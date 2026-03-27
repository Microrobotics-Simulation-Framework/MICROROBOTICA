#pragma once

/// Per-frame profiling infrastructure for MICROBOTICA.
///
/// Enabled by -DMICROBOTICA_PROFILING=1 (debug presets enable by default).
/// Zero overhead when disabled — macros compile to nothing.
///
/// Architecture:
///   - ScopedTimer (RAII): records duration on the hot path (render thread).
///     Writes to a lock-free ring buffer — no spdlog, no mutex, no allocation.
///   - ProfileReporter: called from the Qt main thread (TimelinePanel timer)
///     to compute min/avg/max and log via spdlog. The render thread never
///     touches spdlog.
///
/// Usage:
///   MBCA_PROFILE_SCOPE("paintGL");  // at top of function
///   // ... function body runs, duration recorded on scope exit

#include <atomic>
#include <chrono>
#include <array>
#include <string>
#include <unordered_map>

#ifdef MICROBOTICA_PROFILING
#include <spdlog/spdlog.h>
#endif

namespace microbotica::core {

/// Fixed-size ring buffer for one profiling tag.
/// Lock-free: producer (render thread) does atomic fetch_add on writeIdx_.
/// Consumer (reporter thread) reads non-atomically — tolerates stale reads.
struct ProfileRingBuffer {
    static constexpr size_t kCapacity = 128;

    // Index uses uint64_t. At 60fps this wraps after ~9.7 billion years.
    // A uint32_t would wrap after ~18 hours of continuous profiling.
    std::atomic<uint64_t> writeIdx{0};
    std::array<int64_t, kCapacity> samples{};  // nanoseconds

    void record(int64_t duration_ns) {
        uint64_t idx = writeIdx.fetch_add(1, std::memory_order_relaxed);
        samples[idx % kCapacity] = duration_ns;
    }
};

/// Global registry of profiling tags → ring buffers.
/// Tags are registered on first use (static local in getBuffer).
class ProfileRegistry {
public:
    static ProfileRegistry& instance() {
        static ProfileRegistry reg;
        return reg;
    }

    ProfileRingBuffer& getBuffer(const char* tag) {
        // First-use registration is not lock-free, but only happens once
        // per tag (at static init time). Hot path is the returned reference.
        auto it = buffers_.find(tag);
        if (it != buffers_.end()) return it->second;
        return buffers_[tag];
    }

    /// Iterate all registered tags. Called from reporter thread only.
    const std::unordered_map<std::string, ProfileRingBuffer>& buffers() const {
        return buffers_;
    }

private:
    ProfileRegistry() = default;
    std::unordered_map<std::string, ProfileRingBuffer> buffers_;
};

/// RAII timer that records duration to a ring buffer on destruction.
/// Used on the render thread — no locks, no spdlog, no allocation.
class ScopedTimer {
public:
    explicit ScopedTimer(const char* tag)
        : tag_(tag)
        , start_(std::chrono::high_resolution_clock::now())
    {}

    ~ScopedTimer() {
        auto end = std::chrono::high_resolution_clock::now();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start_).count();
        ProfileRegistry::instance().getBuffer(tag_).record(ns);
    }

    ScopedTimer(const ScopedTimer&) = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    const char* tag_;
    std::chrono::high_resolution_clock::time_point start_;
};

/// Computes and logs profiling summaries. Called from the Qt main thread
/// (e.g., TimelinePanel timer) — never from the render thread.
class ProfileReporter {
public:
    /// Log a summary of all tags if at least `interval_ms` has elapsed
    /// since the last report. Safe to call every frame — internally
    /// throttles to the configured interval.
    static void reportIfDue(int64_t interval_ms = 1000) {
#ifdef MICROBOTICA_PROFILING
        auto now = std::chrono::steady_clock::now();
        static auto lastReport = now;

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastReport).count();
        if (elapsed < interval_ms) return;
        lastReport = now;

        for (const auto& [tag, buf] : ProfileRegistry::instance().buffers()) {
            uint64_t count = buf.writeIdx.load(std::memory_order_relaxed);
            if (count == 0) continue;

            size_t n = std::min(static_cast<size_t>(count), ProfileRingBuffer::kCapacity);
            int64_t total = 0, minVal = INT64_MAX, maxVal = 0;
            for (size_t i = 0; i < n; ++i) {
                int64_t s = buf.samples[i];
                total += s;
                if (s < minVal) minVal = s;
                if (s > maxVal) maxVal = s;
            }
            double avgMs = (total / static_cast<double>(n)) / 1e6;
            double minMs = minVal / 1e6;
            double maxMs = maxVal / 1e6;

            spdlog::info("[profile] {}: avg={:.2f}ms min={:.2f}ms max={:.2f}ms ({} samples)",
                         tag, avgMs, minMs, maxMs, n);
        }
#else
        (void)interval_ms;
#endif
    }
};

} // namespace microbotica::core

// --- Public macros ---

#ifdef MICROBOTICA_PROFILING
// Use __COUNTER__ to ensure unique variable names even on same line
#define MBCA_PROFILE_SCOPE(tag) \
    ::microbotica::core::ScopedTimer _mbca_timer_##__COUNTER__(tag)
#else
#define MBCA_PROFILE_SCOPE(tag)
#endif
