
#ifndef EVENTREGISTRY_HPP
#define EVENTREGISTRY_HPP
#include "network/Message.hpp"
#include "network/Event.hpp"
#include "util/Singleton.hpp"
#include <unordered_map>

template <MessageType>
struct MessageTraits;

template <>
struct MessageTraits<MessageType::Heartbeat> {
	using type = std::byte; // doesnt support void :(
};

template <>
struct MessageTraits<MessageType::FindPeers> {
	using type = std::byte;
};

// Ex.
// EventRegistry::get().get<MessageType::Heartbeat>().connect()
// Type in connect should be proper now based on message traits up here

class EventRegistry : public Singleton<EventRegistry> {
  public:
	template <MessageType Type>
	Event<typename MessageTraits<Type>::type> &get() {
		using T = typename MessageTraits<Type>::type;

		auto it = events.find(Type);
		if (it == events.end()) {
			it = events.emplace(Type, std::make_unique<Event<T>>()).first;
		}

		return static_cast<Event<T> &>(*it->second);
	}

	void dispatch(const Message &msg) {
		auto it = events.find(msg.header.type);
		if (it != events.end()) {
			it->second->fire(msg); // virtual dispatch -> Event<T>::fire
		}
	}

  private:
	std::unordered_map<MessageType, std::unique_ptr<IEvent>> events;
};

#endif // EVENTREGISTRY_HPP