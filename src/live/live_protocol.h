#pragma once

// mime.live/1, MIME 0.3's live-session protocol (MIME's docs/live.md; `mime serve`), as MICROROBOTICA speaks it.
//
// THE ISOLATED LAYER. Every message shape of the protocol lives in this header and its .cpp: the requests, the
// replies and their status, `hello` (the session's description: experiment, controller, live inputs, stage, visuals,
// stage binding, topology, frame schema), the binary frame's header and payload, the JSON frame, and the ready file
// `mime serve --ready-file` writes. Nothing else in MICROROBOTICA reads the protocol's JSON or bytes: when MIME changes
// a message (its client note's "Changes since the first note"), the change is made here and in
// tests/test_live_protocol.cpp, and nowhere else.
//
// Parsing is tolerant within the major version, as the protocol asks: unknown fields are ignored, absent optional
// fields take their defaults, and the raw JSON is kept beside what was read. What a client cannot do without (the
// protocol name, the frame schema) is refused with a ProtocolError that says what was missing.
//
// Free of Qt, USD and ZMQ: nlohmann_json and the standard library only.

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/mime_error.h"
#include "runs/build_report.h"

namespace microbotica::live {

/// The protocol this client speaks: its name and major version, as requests and replies carry it.
inline constexpr const char* kProtocol = "mime.live/1";

/// A message that is not what mime.live/1 says it is (or that this client cannot use): what was wrong.
class ProtocolError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// ── Requests ─────────────────────────────────────────────────────────────────

/// A request: a JSON object with the protocol, the command and its arguments.
nlohmann::json request(const std::string& command, const nlohmann::json& arguments = nlohmann::json::object());

nlohmann::json helloRequest();
nlohmann::json statusRequest();
nlohmann::json startRequest();                       ///< start or resume stepping, paced
nlohmann::json pauseRequest();                       ///< stop stepping after the chunk in hand
nlohmann::json stepRequest(int chunks);              ///< step that many chunks now (from a session not running)
nlohmann::json stopRequest();                        ///< stop in every state; the reply names the run folder
nlohmann::json setRequest(const nlohmann::json& values);  ///< live inputs by label, from the next tick
nlohmann::json recordRequest(bool on);               ///< the recording layer on or off
nlohmann::json paceRequest(std::optional<double> rtf);    ///< a factor of real time, or unpaced (nullopt)

/// When a command's reply comes (MIME's docs/live.md, "It never goes silent"): the server answers while a chunk runs.
enum class ReplyWait {
    AtOnce,     ///< hello, status, pause: at once (a pause taken while a chunk runs is pending, acting at its end)
    ChunkEnd,   ///< set, record, pace, start, step: at the end of the chunk in hand, where they act (a session's first
                ///< chunk compiles its programs: seconds)
    RunFolder,  ///< stop: at once when a chunk runs (pending), else once the run folder it writes is written
};
ReplyWait replyWait(const std::string& command);

// ── Replies ──────────────────────────────────────────────────────────────────

/// The session's status, which every accepting reply carries.
struct SessionStatus {
    std::string state;                 ///< "ready", "running", "paused", "stopped", ...
    std::uint64_t step = 0;            ///< base steps taken (while a chunk runs: the step it started at)
    double time = 0.0;                 ///< simulated seconds
    std::uint64_t frames = 0;          ///< frames sent
    std::uint64_t stepping = 0;        ///< chunks a step in hand has still to take (0 otherwise)
    std::optional<double> pace;        ///< factor of real time; nullopt when unpaced
    bool recording = false;
    nlohmann::json nonFinite;          ///< null, or what MIME says of a state that is not finite
    /// What the session is doing: "compiling" (its first chunk runs, compiling the run's programs), "stepping" (a
    /// later chunk runs) or "idle" (between chunks); empty from a MIME before phase 7n-b.
    std::string activity;
    /// The stop and pause commands taken while the chunk in hand runs, to act at its end, in order.
    std::vector<std::string> pending;

    bool compiling() const { return activity == "compiling"; }
    bool isPending(const std::string& command) const;
};

/// A reply: accepted (ok, with the status) or refused (with MIME's coded errors).
struct Reply {
    bool ok = false;
    std::string command;
    SessionStatus status;
    std::vector<core::MimeRefusal> errors;
    std::optional<std::string> folder;  ///< stop's run folder, once written (never in a pending stop's reply)
    bool taken = false;                 ///< a stop taken while a chunk runs, to act at its end
    std::string message;                ///< what MIME says of it, e.g. "stop pending: ..."
    nlohmann::json raw;

    /// A stop taken while a chunk ran: the session stops at its end. The run folder does not exist yet; the final
    /// frame (or the connection closing) says the session has stopped, and no status is answered after it.
    bool stopPending() const;
};

/// A reply's text, read. Throws ProtocolError when it is not a JSON object of this protocol.
Reply parseReply(const std::string& text);
Reply parseReply(const nlohmann::json& message);

// ── The frame schema and frames ──────────────────────────────────────────────

/// One field a frame carries: its path (`<node>.<field>`), node and field, shape, dtype as NumPy spells it
/// (`<f4`), unit where MIME knows one, and its byte offset after the header.
struct FieldSpec {
    std::string path;
    std::string node;
    std::string field;
    std::vector<std::size_t> shape;
    std::string dtype;
    std::string unit;
    std::size_t offset = 0;

    std::size_t count() const;      ///< elements (1 for a scalar, shape [])
    std::size_t itemBytes() const;  ///< bytes per element, from dtype
    std::size_t bytes() const { return count() * itemBytes(); }
};

/// The fields a session's frames carry, fixed for the session.
struct FrameSchema {
    std::uint32_t id = 0;          ///< what every binary frame of it carries in its header
    std::size_t frameBytes = 0;    ///< header and payload
    std::size_t headerBytes = 32;
    std::vector<FieldSpec> fields;

    const FieldSpec* find(const std::string& path) const;
};

/// hello's `schema`, read and checked (offsets and sizes consistent with frameBytes). Throws ProtocolError.
FrameSchema parseSchema(const nlohmann::json& schema);

/// A binary frame's header (32 bytes, little-endian): magic "MIMF", version, flags, schema id, frame number, step,
/// time.
struct FrameHeader {
    std::uint16_t version = 0;
    std::uint16_t flags = 0;
    std::uint32_t schemaId = 0;
    std::uint32_t number = 0;  ///< from 0, one per frame sent: a gap is a dropped frame
    std::uint64_t step = 0;
    double time = 0.0;
    bool final() const { return (flags & 1u) != 0; }
};

inline constexpr std::size_t kFrameHeaderBytes = 32;

/// Whether a message is a binary frame (it starts "MIMF"; a JSON frame starts '{').
bool isBinaryFrame(const std::uint8_t* data, std::size_t size);

/// The header of a binary frame. Throws ProtocolError when it is not one of version 1.
FrameHeader readFrameHeader(const std::uint8_t* data, std::size_t size);

/// A frame as the client keeps it: its header's numbers and each field's values as doubles, by path, row-major in the
/// field's shape (the run's own type, widened: a float32 state is held exactly).
struct LiveFrame {
    std::uint32_t number = 0;
    std::uint64_t step = 0;
    double time = 0.0;
    bool final = false;
    std::unordered_map<std::string, std::vector<double>> values;
};

/// A binary frame of `schema`, read. Throws ProtocolError when it is of another schema or length.
LiveFrame decodeBinaryFrame(const std::uint8_t* data, std::size_t size, const FrameSchema& schema);

/// A JSON frame (`mime serve --json-frames`) of `schema`, read. Throws ProtocolError.
LiveFrame decodeJsonFrame(const nlohmann::json& message, const FrameSchema& schema);

/// Either kind of frame, by its first bytes.
LiveFrame decodeFrame(const std::uint8_t* data, std::size_t size, const FrameSchema& schema);

/// What a client checks of the frames it receives, by their numbers: each the one after the last. A gap is a dropped
/// frame, a number at or below one seen is out of order. It counts from the first frame it sees, whatever its number:
/// a client that joins a running session (or whose subscription lands after the first frames) has missed nothing of
/// its own.
class FrameOrder {
public:
    /// The problem with this frame's place, or empty when it is the next.
    std::string see(std::uint32_t number);
    std::uint64_t dropped() const { return dropped_; }
    std::uint64_t outOfOrder() const { return outOfOrder_; }

private:
    bool seenAny_ = false;
    std::uint64_t expected_ = 0;
    std::uint64_t dropped_ = 0;
    std::uint64_t outOfOrder_ = 0;
};

// ── The stage binding ────────────────────────────────────────────────────────

/// One prim the state moves: which field (and row of a stacked field) sets it, and how.
struct BindingEntry {
    std::string prim;          ///< e.g. "/World/rpm/arm/link_1"
    std::string node;
    std::string field;
    std::optional<std::size_t> index;  ///< the row of a stacked field (frame_poses is N x 7)
    std::string kind;          ///< "pose" (x y z, then w x y z) or "vector" (three numbers, world axes)
    std::string thing;
    std::string name;
    std::string unit;          ///< "m; 1" for a pose, the field's unit for a vector
    std::string space;         ///< "world": a pose is the prim's translate and orient as they stand
    std::string atNode;        ///< a vector: the pose it is drawn at (node, field)
    std::string atField;

    std::string path() const { return node + "." + field; }
    std::string atPath() const { return atNode.empty() ? std::string() : atNode + "." + atField; }
};

/// hello's `binding` (`{format, entries}`, or the entries alone).
std::vector<BindingEntry> parseBinding(const nlohmann::json& binding);

// ── hello ────────────────────────────────────────────────────────────────────

/// A controller's live input, as hello lists it.
struct LiveInputSpec {
    std::string label;
    std::string units;
    std::vector<std::size_t> shape;
    std::string meaning;
    /// The value at which it changes nothing the step does, which the session hands the step until it is set (null
    /// from a MIME before phase 7n-b).
    nlohmann::json defaultValue;

    std::size_t count() const;                 ///< numbers it holds (1 for a scalar)
    std::vector<double> defaults() const;      ///< its default, flattened row-major; zeros when hello gives none
};

/// The session's controller.
struct ControllerInfo {
    std::string entry;      ///< e.g. "arm_tracking 0.1.0"
    std::string mode;       ///< "callback" (live inputs apply) or "graph" (inside the compiled step: they do not)
    std::int64_t every = 0; ///< base steps between ticks
    std::int64_t latency = 0;
    bool takesLiveInputs() const { return mode == "callback"; }
};

/// What the asset cache draws of the stage's visuals.
struct VisualsInfo {
    std::optional<std::string> overlay;    ///< a layer to add over the stage, or none
    long drawn = 0;
    std::map<std::string, long> missing;   ///< visual sets the cache lacks, and how many meshes each
    nlohmann::json notice;                 ///< the credit and terms text (its shape may still change)
    /// The notice as lines of text, whatever shape it arrives in (a string, a list of strings, a list of objects).
    std::vector<std::string> noticeLines() const;
};

/// The session as `hello` describes it.
struct Hello {
    std::string protocol;
    std::string mimeVersion;
    std::string experiment;
    std::map<std::string, std::string> hashes;
    double dt = 0.0;
    std::int64_t chunkSteps = 0;
    std::optional<ControllerInfo> controller;
    std::vector<LiveInputSpec> liveInputs;
    std::string stage;                     ///< the served saved folder's scene/scene.usda, on the server's machine
    std::optional<VisualsInfo> visuals;
    std::vector<BindingEntry> binding;
    nlohmann::json topology;               ///< mime.topology/1, kept as JSON for the graph inspector
    runs::ReportRead report;               ///< the build report's summary (mime.report/1), or why there is none
    FrameSchema schema;
    std::vector<std::string> commands;
    SessionStatus status;
    nlohmann::json raw;
};

/// hello's reply, read. Throws ProtocolError when it is refused, of another protocol, or has no usable schema.
Hello parseHello(const nlohmann::json& reply);

// ── The security mechanism ───────────────────────────────────────────────────

/// The security mechanism a session's sockets use (ZMTP's): "null", none, as every session uses today, or "curve"
/// (CurveZMQ: encrypted, the server known by its public key; MIME's remote sessions, designs #214 and #215). The
/// ready file names it, for a manual endpoint the settings do, and never `hello`, which travels over the connection
/// the mechanism secures. Absent means null.
struct Security {
    std::string mechanism = "null";  ///< lower case
    std::string serverKey;           ///< the server's public key, Z85 (40 characters), for curve
};

/// A mechanism as named (in any case; empty for none) and its server key.
Security securityOf(const std::string& mechanism, const std::string& serverKey = {});

/// Why this client cannot speak `security`'s mechanism, said for the person, or nullopt when it can. A client that
/// cannot speak the mechanism a session names refuses it: it never falls back to null. This one speaks null alone.
std::optional<std::string> mechanismRefusal(const Security& security);

// ── mime serve's ready file ──────────────────────────────────────────────────

/// The ready file's key for the server's public key: Z85 text (40 characters), present only alongside
/// `"mechanism": "curve"`. Reserved in MIME's docs/live.md from dev1, written from dev2's S5.
inline constexpr const char* kReadyServerKey = "server_key";

/// What `mime serve --ready-file` writes once its sockets listen. Fields may be added: a reader ignores any key it does
/// not know (mime.live/1's rule for the ready file).
struct ReadyFile {
    std::string protocol;
    std::string commands;  ///< the REP endpoint, e.g. "tcp://127.0.0.1:41233"
    std::string frames;    ///< the PUB endpoint
    std::string out;       ///< the run folder it will write
    Security security;     ///< `mechanism` and the server's key; null when the file names none
};

/// The ready file's text, read. Throws ProtocolError when it lacks an endpoint or names another protocol. A mechanism
/// this client cannot speak is read, not refused here: connecting refuses it (mechanismRefusal).
ReadyFile parseReadyFile(const std::string& text);

} // namespace microbotica::live
