#ifndef BLOCKINGQUEUE_HPP
#define BLOCKINGQUEUE_HPP

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>

template <typename T>
class BlockingQueue {
  public:
	void push(T item) {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			queue_.push(std::move(item));
		}
		cv_.notify_one();
	}

	template <typename Rep, typename Period>
	std::optional<T> pop(std::chrono::duration<Rep, Period> timeout) {
		std::unique_lock<std::mutex> lock(mutex_);

		const bool avaiable = cv_.wait_for(lock, timeout, [this]() {
			return !queue_.empty();
		});

		if (!avaiable) return std::nullopt;

		T item = std::move(queue_.front());
		queue_.pop();

		return item;
	}

	// Blocks until item is available
	T pop() {
		std::unique_lock<std::mutex> lock(mutex_);

		cv_.wait(lock, [this]() {
			return !queue_.empty();
		});

		T item = std::move(queue_.front());
		queue_.pop();

		return item;
	}

  private:
	std::queue<T> queue_;
	std::mutex mutex_;
	std::condition_variable cv_;
};

#endif // BLOCKINGQUEUE_HPP