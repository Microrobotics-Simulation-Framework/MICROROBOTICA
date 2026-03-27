#pragma once

#include <mutex>
#include <condition_variable>
#include <queue>
#include <optional>

namespace microbotica::core {

/// Thread-safe queue for producer-consumer patterns.
///
/// Used by SimulationController to decouple the blocking
/// PhysicsProcess::receiveResult() from the Qt event loop.
template<typename T>
class ThreadSafeQueue {
public:
    /// Push an item and notify one waiting consumer.
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(value));
        }
        cv_.notify_one();
    }

    /// Non-blocking pop. Returns std::nullopt if empty.
    std::optional<T> try_pop() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    /// Blocking pop. Waits until an item is available.
    T wait_pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return !queue_.empty(); });
        T value = std::move(queue_.front());
        queue_.pop();
        return value;
    }

    /// Drain the queue and return only the newest item.
    /// Discards all intermediate items. Single lock acquisition
    /// regardless of queue depth — O(1) lock overhead vs O(N) for
    /// repeated try_pop().
    std::optional<T> drain_latest() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return std::nullopt;
        T latest = std::move(queue_.back());
        // Clear the entire queue (front items are stale)
        std::queue<T> empty;
        queue_.swap(empty);
        return latest;
    }

    /// Check if empty.
    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<T> queue_;
};

} // namespace microbotica::core
