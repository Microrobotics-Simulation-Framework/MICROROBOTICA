#pragma once

namespace microbotica::core {

/// Status of a physics backend process.
enum class ProcessStatus {
    Idle,       ///< Not started
    Launching,  ///< Starting up
    Running,    ///< Actively producing results
    Stopped,    ///< Gracefully stopped
    Error       ///< Failed or crashed
};

/// Status of a render session.
enum class SessionStatus {
    Disconnected, ///< No active session
    Connecting,   ///< Establishing connection
    Active,       ///< Session active and rendering
    Error         ///< Session failed
};

/// Render mode for the viewport.
enum class RenderMode {
    LocalHydra,    ///< Local Hydra/Storm rendering
    Software,      ///< Software fallback rendering
    CloudStream    ///< Remote Selkies stream
};

} // namespace microbotica::core
