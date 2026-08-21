/**

Generalizes the event queuing in metadata node

```
using SpecificEvent = std::variant<SpecificRpcMessage, Stop, A, B, C>

class SpecificRpc : public RpcService<SpecificEvent, SpecificRpc> {
  public:

	handle(SpecificRpcMessage message);

	handle(A a);
	handle(B b);
	handle(C c);

	Stop isn't handled, will break queue's receive loop,

  private:
};
```

*/
#include "rpc/Rpc.hpp"
#include "util/BlockingQueue.hpp"
#include <type_traits>
#include <variant>

template <typename EventType, typename Derived>
class RpcService {
  public:
	void post(EventType event) {
		queue_.push(std::move(event));
	}

	void run() {
		while (true) {
			auto event = queue_.pop();

			if (std::holds_alternative<Stop>(event)) break;

			std::visit(
				[this](auto &&arg) {
					using T = std::remove_cvref_t<decltype(arg)>;

					// This makes it so we dont need a handle(Stop) overload
					if constexpr (!std::is_same_v<T, Stop>) {
						static_cast<Derived &>(*this).handle(std::forward<decltype(arg)>(arg));
					}
				},
				std::move(event));
		}
	}

  protected:
	BlockingQueue<EventType> queue_;
};