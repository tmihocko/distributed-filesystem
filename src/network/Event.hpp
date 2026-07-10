#ifndef EVENT_HPP
#define EVENT_HPP
#include "network/Message.hpp"
#include <functional>
#include "Packet.hpp"
#include "IEvent.hpp"

template <TriviallyCopyable T>
class Event : public IEvent {
  public:
	// Call callbacks on THIS MACHINE
	void fire(const MessageHeader &header, const T &value) override {
		for (const auto &callback : connections) {
			callback(header, value);
		}
	}

	void connect(std::function<void(const MessageHeader &header, const T &)> callback) {
		connections.push_back(std::move(callback));
	}

  private:
	std::vector<std::function<void(const MessageHeader &header, const T &)>> connections;
};

#endif
