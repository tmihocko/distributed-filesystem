#include "Client.hpp"
#include "network/Node.hpp"
#include "rpc/ClientProtocol.hpp"
#include "rpc/Rpc.hpp"
#include "Yaml.hpp"
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

constexpr std::chrono::seconds TIMEOUT{ 3 };
constexpr std::chrono::seconds READ_CHUNK_TIMEOUT{ 7 };

Client::Client(Endpoint self, std::span<const Endpoint> seed_nodes) : self_(self), network_(self) {
	network_.start(seed_nodes);
	for (const auto &node : seed_nodes) {
		if (node.role == NodeRole::METADATA) {
			metadata_node_id_ = node.node_id;
			return;
		}
	}
}

Client::Client(const std::string &config_file) : Client(Yaml::get_node_info(config_file), Yaml::get_seed_nodes(config_file)) {}

ClientOperation<void> Client::create_file(std::string path) {

	fs::path temp{ path };
	auto clean_path = temp.lexically_normal().string(); // Remove trailing slashes and other bs

	auto request_id = next_id();

	CreateFileRequest request{
		.path = std::move(clean_path),
		.context = {}
	};

	auto frame = ClientProtocol::encode_create_file_request(request_id, request);

	network_.send(metadata_node_id_, std::move(frame));

	auto message = network_.receive_if(TIMEOUT, Rpc::make_message_is(request_id, ClientJob::CREATE_FILE, RpcKind::Response, metadata_node_id_));

	if (!message) return std::unexpected(ClientError::Timeout);

	auto rpc_message = Rpc::read_message<ClientJob>(*message);

	if (rpc_message.rpc_header.request_id != request_id) return std::unexpected(ClientError::BadResponse);
	if (rpc_message.rpc_header.kind != RpcKind::Response) return std::unexpected(ClientError::BadResponse);
	if (rpc_message.rpc_header.job != ClientJob::CREATE_FILE) return std::unexpected(ClientError::BadResponse);

	CreateFileResponse response = ClientProtocol::decode_create_file_response(rpc_message);

	if (response.status == ClientStatus::Success) {
		return {};
	} else {
		return status_to_error<void>(response.status);
	}
}

ClientOperation<void> Client::read_file(std::string from_path, std::string to_path) {
	auto request_id = next_id();

	ReadFileRequest request{
		.path = std::move(from_path),
		.context = {},
	};

	Frame frame = ClientProtocol::encode_read_file_request(request_id, request);

	network_.send(metadata_node_id_, std::move(frame));

	auto message = network_.receive_if(TIMEOUT, Rpc::make_message_is(request_id, ClientJob::READ_FILE, RpcKind::Response, metadata_node_id_));

	if (!message) return std::unexpected(ClientError::Timeout);

	auto rpc_message = Rpc::read_message<ClientJob>(std::move(*message));

	if (!validate_rpc_header<ClientJob::READ_FILE, RpcKind::Response>(rpc_message.rpc_header, request_id)) {
		return std::unexpected(ClientError::BadResponse);
	}

	auto response = ClientProtocol::decode_read_file_response(rpc_message);

	if (response.status != ClientStatus::Success) return status_to_error<void>(response.status);

	const std::uint64_t expected_chunk_count =
		response.file_size / CHUNK_SIZE +
		(response.file_size % CHUNK_SIZE != 0);

	if (expected_chunk_count != response.chunk_count) return std::unexpected(ClientError::BadResponse);

	if (to_path.empty()) return std::unexpected(ClientError::BadInput);

	const fs::path destination{ to_path };
	fs::path temporary = destination;
	temporary += ".dfs-read-" + std::to_string(request_id) + ".tmp";

	std::ofstream output(temporary, std::ios::binary | std::ios::trunc);

	if (!output) return std::unexpected(ClientError::BadInput);

	auto discard_temporary = [&] {
		output.close();

		std::error_code ignored;
		fs::remove(temporary, ignored);
	};

	for (std::uint32_t chunk_index = 0; chunk_index < response.chunk_count; chunk_index++) {
		ReadChunkRequest chunk_request{
			.chunk_index = chunk_index,
			.context = {},
		};

		Frame chunk_frame = ClientProtocol::encode_read_chunk_request(request_id, chunk_request);

		network_.send(metadata_node_id_, std::move(chunk_frame));

		auto message = network_.receive_if(
			READ_CHUNK_TIMEOUT,
			Rpc::make_message_is(
				request_id,
				ClientJob::READ_CHUNK,
				RpcKind::Response,
				metadata_node_id_,
				[chunk_index](BinaryReader &reader) {
					return reader.read<std::uint32_t>() == chunk_index;
				}));

		if (!message) {
			discard_temporary();
			return std::unexpected(ClientError::Timeout);
		}

		auto rpc_chunk = Rpc::read_message<ClientJob>(std::move(*message));

		if (!validate_rpc_header<ClientJob::READ_CHUNK, RpcKind::Response>(rpc_chunk.rpc_header, request_id)) {
			discard_temporary();
			return std::unexpected(ClientError::BadResponse);
		}

		ReadChunkResponse chunk_response = ClientProtocol::decode_read_chunk_response(rpc_chunk);

		if (chunk_response.status != ClientStatus::Success) {
			discard_temporary();
			return status_to_error<void>(chunk_response.status);
		}

		const std::uint64_t offset = static_cast<std::uint64_t>(chunk_index) * CHUNK_SIZE;

		const std::size_t expected_size = static_cast<std::size_t>(std::min<std::uint64_t>(CHUNK_SIZE, response.file_size - offset));

		if (chunk_response.chunk_index != chunk_index || chunk_response.data.size() != expected_size) {
			discard_temporary();
			return std::unexpected(ClientError::BadResponse);
		}

		output.write(reinterpret_cast<const char *>(chunk_response.data.data()), static_cast<std::streamsize>(chunk_response.data.size()));

		if (!output) {
			discard_temporary();
			return std::unexpected(ClientError::ReadError);
		}
	}

	output.flush();
	output.close();

	if (!output) {
		std::error_code ignored;
		fs::remove(temporary, ignored);

		return std::unexpected(ClientError::ReadError);
	}

	std::error_code rename_error;
	fs::rename(temporary, destination, rename_error);

	if (rename_error) {
		std::error_code ignored;
		fs::remove(temporary, ignored);

		return std::unexpected(ClientError::ReadError);
	} else {
		return {};
	}
}

ClientOperation<void> Client::write_file(std::string local_path, std::string path) {
	auto request_id = next_id();

	std::ifstream file(local_path, std::ios::binary);

	if (!file) return std::unexpected(ClientError::BadInput);

	const std::uint64_t file_size = fs::file_size(local_path);
	const std::uint64_t chunks = file_size / CHUNK_SIZE + (file_size % CHUNK_SIZE != 0);

	if (chunks > std::numeric_limits<std::uint32_t>::max()) return std::unexpected(ClientError::StorageFull);
	// maybe error category? if it needs this much chunks it probably wont fit anyway

	const auto chunk_count = static_cast<std::uint32_t>(chunks);

	WriteFileRequest write_req{
		.file_size = file_size,
		.chunk_count = chunk_count,
		.path = path,
		.context = {}
	};

	const auto write_req_frame = ClientProtocol::encode_write_file_request(request_id, write_req);

	network_.send(metadata_node_id_, write_req_frame);

	const auto write_frame = network_.receive_if(TIMEOUT, Rpc::make_message_is(request_id, ClientJob::WRITE_FILE, RpcKind::Response, metadata_node_id_));

	if (!write_frame) return std::unexpected(ClientError::Timeout);
	const auto rpc_write_response = Rpc::read_message<ClientJob>(*write_frame);

	if (!validate_rpc_header<ClientJob::WRITE_FILE, RpcKind::Response>(rpc_write_response.rpc_header, request_id)) {
		return std::unexpected(ClientError::BadResponse);
	}

	const auto write_response = ClientProtocol::decode_write_file_response(rpc_write_response);

	if (write_response.status != ClientStatus::Success) {
		return status_to_error<void>(write_response.status);
	}

	for (std::uint32_t chunk_index = 0; chunk_index < chunk_count; chunk_index++) {
		std::vector<std::byte> data(CHUNK_SIZE);

		file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(CHUNK_SIZE));

		const auto bytes_read = static_cast<std::size_t>(file.gcount());

		if (bytes_read == 0) return std::unexpected(ClientError::ReadError);
		data.resize(bytes_read);

		WriteChunkRequest write_chunk{
			.chunk_index = chunk_index,
			.data = std::move(data),
			.context = {}
		};

		auto frame = ClientProtocol::encode_write_chunk_request(request_id, write_chunk);

		network_.send(metadata_node_id_, frame);

		const auto chunk_frame = network_.receive_if(
			TIMEOUT,
			Rpc::make_message_is(
				request_id, ClientJob::WRITE_CHUNK, RpcKind::Response, metadata_node_id_,
				[chunk_index](BinaryReader &reader) {
					return reader.read<std::uint32_t>() == chunk_index;
				}));

		if (!chunk_frame) return std::unexpected(ClientError::Timeout);
		auto rpc_chunk_response = Rpc::read_message<ClientJob>(*chunk_frame);

		if (!validate_rpc_header<ClientJob::WRITE_CHUNK, RpcKind::Response>(rpc_chunk_response.rpc_header, request_id)) {
			return std::unexpected(ClientError::BadResponse);
		}

		const auto chunk_response = ClientProtocol::decode_write_chunk_response(rpc_chunk_response);
		if (chunk_response.chunk_index != chunk_index) return std::unexpected(ClientError::BadResponse);

		// this might need more stuff to check responses i didnt think it thourgh much
		if (chunk_response.status != ClientStatus::Success) {
			return status_to_error<void>(chunk_response.status);
		}
	}

	return {};
}

ClientOperation<void> Client::remove(std::string path) {
	auto request_id = next_id();

	RemoveRequest request{
		.path = path,
		.context = {},
	};

	auto frame = ClientProtocol::encode_remove_request(request_id, request);

	network_.send(metadata_node_id_, frame);

	auto message = network_.receive_if(TIMEOUT, Rpc::make_message_is(request_id, ClientJob::REMOVE, RpcKind::Response, metadata_node_id_));

	if (!message) return std::unexpected(ClientError::Timeout);

	auto rpc_message = Rpc::read_message<ClientJob>(*message);

	if (!validate_rpc_header<ClientJob::REMOVE, RpcKind::Response>(rpc_message.rpc_header, request_id)) {
		return std::unexpected(ClientError::BadResponse);
	}

	auto response = ClientProtocol::decode_remove_response(rpc_message);

	if (response.status == ClientStatus::Success) {
		return {};
	} else {
		return status_to_error<void>(response.status);
	}
}

ClientOperation<std::vector<FileInfo>> Client::list(std::string directory) {
	auto request_id = next_id();

	ListRequest request{
		.path = directory,
		.context = {}
	};

	auto frame = ClientProtocol::encode_list_request(request_id, request);

	network_.send(metadata_node_id_, std::move(frame));

	auto message = network_.receive_if(TIMEOUT, Rpc::make_message_is(request_id, ClientJob::LIST, RpcKind::Response, metadata_node_id_));

	if (!message) return std::unexpected(ClientError::Timeout);

	auto rpc_message = Rpc::read_message<ClientJob>(*message);

	if (!validate_rpc_header<ClientJob::LIST, RpcKind::Response>(rpc_message.rpc_header, request_id)) {
		return std::unexpected(ClientError::BadResponse);
	}

	auto response = ClientProtocol::decode_list_response(rpc_message);

	if (response.status == ClientStatus::Success) {
		return response.info_vec;
	} else {
		return status_to_error<std::vector<FileInfo>>(response.status);
	}
}

ClientOperation<void> Client::mkdir(std::string path) {
	auto request_id = next_id();

	MakeDirRequest request{
		.path = path,
		.context = {}
	};

	Frame frame = ClientProtocol::encode_mkdir_request(request_id, request);

	network_.send(metadata_node_id_, std::move(frame));

	auto message = network_.receive_if(TIMEOUT, Rpc::make_message_is(request_id, ClientJob::MKDIR, RpcKind::Response, metadata_node_id_));

	if (!message) return std::unexpected(ClientError::Timeout);

	auto rpc_message = Rpc::read_message<ClientJob>(*message);

	if (!validate_rpc_header<ClientJob::MKDIR, RpcKind::Response>(rpc_message.rpc_header, request_id)) {
		return std::unexpected(ClientError::BadResponse);
	}

	auto response = ClientProtocol::decode_mkdir_response(rpc_message);

	if (response.status == ClientStatus::Success) {
		return {};
	} else {
		return status_to_error<void>(response.status);
	}
}

ClientOperation<void> Client::rename(std::string old_path, std::string new_path) {
	auto request_id = next_id();

	RenameRequest request{
		.old_path = old_path,
		.new_path = new_path,
		.context = {}
	};

	Frame frame = ClientProtocol::encode_rename_request(request_id, request);

	network_.send(metadata_node_id_, std::move(frame));

	auto message = network_.receive_if(TIMEOUT, Rpc::make_message_is(request_id, ClientJob::RENAME, RpcKind::Response, metadata_node_id_));

	if (!message) return std::unexpected(ClientError::Timeout);

	auto rpc_message = Rpc::read_message<ClientJob>(*message);

	if (!validate_rpc_header<ClientJob::RENAME, RpcKind::Response>(rpc_message.rpc_header, request_id)) {
		return std::unexpected(ClientError::BadResponse);
	}

	auto response = ClientProtocol::decode_rename_response(rpc_message);

	if (response.status == ClientStatus::Success) {
		return {};
	} else {
		return status_to_error<void>(response.status);
	}
}

ClientOperation<FileStat> Client::stat(std::string path) {
	auto request_id = next_id();

	StatRequest request{
		.path = path,
		.context = {}
	};

	Frame frame = ClientProtocol::encode_stat_request(request_id, request);

	network_.send(metadata_node_id_, std::move(frame));

	auto message = network_.receive_if(TIMEOUT, Rpc::make_message_is(request_id, ClientJob::STAT, RpcKind::Response, metadata_node_id_));

	if (!message) return std::unexpected(ClientError::Timeout);

	auto rpc_message = Rpc::read_message<ClientJob>(*message);

	if (!validate_rpc_header<ClientJob::STAT, RpcKind::Response>(rpc_message.rpc_header, request_id)) {
		return std::unexpected(ClientError::BadResponse);
	}

	auto response = ClientProtocol::decode_stat_response(rpc_message);

	if (response.status == ClientStatus::Success) {
		return response.stat;
	} else {
		return status_to_error<FileStat>(response.status);
	}
}

std::uint64_t Client::next_id() {
	return ++current_id_;
}
