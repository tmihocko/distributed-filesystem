#ifndef EVENT_HPP
#define EVENT_HPP
#include "network/Message.hpp"
#include <functional>
#include <type_traits>

template <typename T>
concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

template <TriviallyCopyable T>
class Event {
  public:
	Event();

	bool fire(const T &value /*, address*/) {
	}
	bool broadcast(const T &value) {
	}
	void connect(std::function<void(const T &)>) {
	}

  private:
};

#endif
