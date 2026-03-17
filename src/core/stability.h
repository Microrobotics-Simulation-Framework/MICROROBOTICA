#pragma once

#include <spdlog/spdlog.h>

/// Mark a class or function as deprecated with a compiler warning.
#define MBCA_DEPRECATED(msg) [[deprecated(msg)]]

/// Mark a class as experimental.
/// Emits a runtime spdlog::warn on the first instantiation of the class,
/// not at program startup. Place in the class constructor body.
///
/// Usage:
///   MyExperimentalClass::MyExperimentalClass() {
///       MBCA_EXPERIMENTAL_WARN("MyExperimentalClass");
///       // ... rest of constructor
///   }
///
/// Note: The static bool is not thread-safe. Acceptable for Phase 0
/// where experimental classes are only constructed on the main thread.
#define MBCA_EXPERIMENTAL_WARN(class_name) \
    do { \
        static bool _warned = false; \
        if (!_warned) { \
            spdlog::warn("{} is experimental — API may change without notice. " \
                         "Do not rely on this interface in production.", class_name); \
            _warned = true; \
        } \
    } while(0)
