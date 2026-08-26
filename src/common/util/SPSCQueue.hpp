#ifndef SPSCQUEUE_HPP
#define SPSCQUEUE_HPP

#include <atomic>
#include <cstddef>
#include <optional>
#include <vector>
template <typename T>
class SPSCQueue {
  public:
	// + 1 because of circular buffer logic stuff
	SPSCQueue(std::size_t capacity) : capacity_(capacity + 1), buffer_(capacity + 1), head_(0), tail_(0) {}

	// Returns false if queue is full,
	bool push(T &&value) {
		const auto current_tail = tail_.load(std::memory_order_relaxed);
		const auto next_tail = increment(current_tail);

		if (next_tail == head_.load(std::memory_order_acquire)) {
			return false;
		}

		buffer_[current_tail] = value;

		tail_.store(next_tail, std::memory_order_release);
		return true;
	}

	std::optional<T> pop() {
	}

	bool empty() {}

  private:
	std::size_t increment(std::size_t index) const {
		return (index + 1) % capacity_;
	}

	std::size_t capacity_;
	std::vector<T> buffer_;
	// Avoid false sharing
	alignas(64) std::atomic<std::size_t> head_;
	alignas(64) std::atomic<std::size_t> tail_;
};
#endif // SPSCQUEUE_HPP