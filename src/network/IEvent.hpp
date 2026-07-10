
// Type-erased baseclass to store Events in map
#include "network/Message.hpp"
class IEvent {
  public:
	virtual ~IEvent() = default;
	virtual void fire(const Message &msg) = 0;
};