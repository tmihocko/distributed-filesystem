/**
MPMC queue that blocks thread when on pop() when empty
*/
#ifndef BLOCKINGQUEUE_HPP
#define BLOCKINGQUEUE_HPP

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <deque>

template <typename T>
class BlockingQueue {
  public:
	void push(T item) {
		{
			std::lock_guard<std::mutex> lock(mutex_);
			queue_.push_back(std::move(item));
		}
		cv_.notify_all();
	}

	// Blocks until item is available or timeout has passed,
	template <typename Rep, typename Period>
	std::optional<T> pop_with_timeout(std::chrono::duration<Rep, Period> timeout) {
		std::unique_lock lock(mutex_);

		const bool avaiable = cv_.wait_for(lock, timeout, [this]() {
			return !queue_.empty();
		});

		if (!avaiable) return std::nullopt;

		T item = std::move(queue_.front());
		queue_.pop_front();

		return item;
	}

	template <typename Rep, typename Period>
	std::optional<T> pop_if(std::chrono::duration<Rep, Period> timeout, std::function<bool(const T &)> predicate) {
		std::unique_lock lock(mutex_);

		const bool available = cv_.wait_for(lock, timeout, [this, &predicate] {
			return std::any_of(queue_.begin(), queue_.end(), predicate);
		});

		if (!available) return std::nullopt;

		auto it = std::find_if(queue_.begin(), queue_.end(), predicate);

		T result = std::move(*it);
		queue_.erase(it);

		return result;
	}

	// Blocks until item is available
	T pop() {
		std::unique_lock lock(mutex_);

		cv_.wait(lock, [this]() {
			return !queue_.empty();
		});

		T item = std::move(queue_.front());
		queue_.pop_front();

		return item;
	}

  private:
	std::deque<T> queue_;
	std::mutex mutex_;
	std::condition_variable cv_;
};

#endif // BLOCKINGQUEUE_HPP