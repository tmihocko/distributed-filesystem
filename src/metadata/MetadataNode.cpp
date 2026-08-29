#include "MetadataNode.hpp"
#include "network/Message.hpp"
#include "network/Node.hpp"
#include "rpc/ClientProtocol.hpp"
#include "rpc/StorageProtocol.hpp"
#include "MetadataWorker.hpp"
#include "rpc/Rpc.hpp"
#include <print>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <variant>

MetadataNode::MetadataNode(
	NodeConfig config,
	std::span<const Endpoint> seed_nodes)
	: config_(std::move(config)),
	  network_(config_),
	  store_(config_.data_directory),
	  worker_(config_, network_, store_, storage_nodes_) {
	const auto expected = store_.load_from_storage();
	if (!expected) {
		std::println("MetadataStoreError: {}", static_cast<int>(expected.error()));
		throw std::runtime_error("Failed to load metadata file");
	}
	network_.start(seed_nodes);
}

void MetadataNode::setup() {
	// idk if this needs stuff;
}

// Network producer
void MetadataNode::start() {
	setup();

	network_producer_ = std::jthread([this](std::stop_token stop) {
		while (!stop.stop_requested()) {
			auto message = network_.receive_if(network_receive_timeout_, [this](const Message &message) {
				return Rpc::message_is<ClientJob, RpcKind::Request>(message) || message.header.type == MessageType::STORAGE_HEARTBEAT;
			});

			if (message) {
				push_worker_job(std::move(*message));
			}
		}
	});

	// client events -> client, storage events -> storage
	worker_thread_ = std::jthread([this](std::stop_token stop) {
		while (!stop.stop_requested()) {
			auto event = job_queue_.pop_with_timeout(heartbeat_check_interval_);
			if (event) {
				std::visit(
					[this](auto &&event) {
						using T = std::decay_t<decltype(event)>;

						if constexpr (std::same_as<T, ClientEvent>) {
							std::visit(
								[this](auto &&request) {
									worker_.handle(std::move(request));
								},
								std::move(event));
						} else { // StorageHeartbeat
							handle_heartbeat(event);
						}
					},
					std::move(*event));
			}
			check_heartbeat_timeouts();
		}
	});
}

void MetadataNode::check_heartbeat_timeouts() {
	const auto now = std::chrono::steady_clock::now();

	for (auto &[node_id, info] : storage_nodes_) {
		if (!info.available) continue;

		const auto elapsed = now - info.last_seen;

		if (elapsed >= heartbeat_timeout_) {
			info.available = false;
		}
	}
}

void MetadataNode::stop() {
	worker_thread_.request_stop();
	network_producer_.request_stop();
}

void MetadataNode::handle_heartbeat(StorageHeartbeat heartbeat) {
	const auto now = std::chrono::steady_clock::now();
	const auto &node_id = heartbeat.sender.id;

	auto [it, inserted] = storage_nodes_.try_emplace(
		node_id,
		StorageNodeInfo{
			.last_seen = now,
			.bytes_left = 0, // start with 0, storage should always respond with bytes left
			.available = true,
		});

	if (inserted) {
		std::println("Registered storage heartbeat: {}", node_id);
		return;
	} else {
		const bool recovered = !it->second.available;

		it->second.last_seen = now;
		it->second.available = true;

		if (recovered) std::println("Storage node recovered: {}", node_id);
	}
}

void MetadataNode::push_worker_job(Message message) {
	if (message.header.type == MessageType::CLIENT_RPC) {
		auto rpc_message = Rpc::read_message<ClientJob>(std::move(message));

		switch (rpc_message.rpc_header.job) {

		case ClientJob::CREATE_FILE:
			job_queue_.push(ClientProtocol::decode_create_file_request(rpc_message));
			break;
		case ClientJob::WRITE_FILE:
			job_queue_.push(ClientProtocol::decode_write_file_request(rpc_message));
			break;
		case ClientJob::WRITE_CHUNK:
			job_queue_.push(ClientProtocol::decode_write_chunk_request(rpc_message));
			break;
		case ClientJob::REMOVE:
			job_queue_.push(ClientProtocol::decode_remove_request(rpc_message));
			break;
		case ClientJob::LIST:
			job_queue_.push(ClientProtocol::decode_list_request(rpc_message));
			break;
		case ClientJob::MKDIR:
			job_queue_.push(ClientProtocol::decode_mkdir_request(rpc_message));
			break;
		case ClientJob::RENAME:
			job_queue_.push(ClientProtocol::decode_rename_request(rpc_message));
			break;
		case ClientJob::STAT:
			job_queue_.push(ClientProtocol::decode_stat_request(rpc_message));
			break;
		default:
			throw std::runtime_error("Job type not handled.");
		}
	} else if (message.header.type == MessageType::STORAGE_HEARTBEAT) {
		job_queue_.push(StorageHeartbeat{
			.sender = std::move(message.sender),
		});
		// add stuff here
	}
}