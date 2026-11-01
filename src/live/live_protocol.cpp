#include "live/live_protocol.h"

#include <algorithm>
#include <cstring>

namespace microbotica::live {

namespace {

std::string text(const nlohmann::json& object, const char* key, const std::string& fallback = std::string()) {
    if (!object.is_object()) {
        return fallback;
    }
    const auto it = object.find(key);
    return (it != object.end() && it->is_string()) ? it->get<std::string>() : fallback;
}

template <typename T>
T number(const nlohmann::json& object, const char* key, T fallback) {
    if (!object.is_object()) {
        return fallback;
    }
    const auto it = object.find(key);
    return (it != object.end() && it->is_number()) ? it->get<T>() : fallback;
}

std::vector<std::size_t> shapeOf(const nlohmann::json& shape) {
    std::vector<std::size_t> out;
    if (shape.is_array()) {
        for (const auto& n : shape) {
            if (!n.is_number_integer() || n.get<long long>() < 0) {
                throw ProtocolError("a shape holds something that is not a non-negative integer: " + shape.dump());
            }
            out.push_back(n.get<std::size_t>());
        }
    }
    return out;
}

bool hostIsLittleEndian() {
    const std::uint16_t probe = 1;
    std::uint8_t first = 0;
    std::memcpy(&first, &probe, 1);
    return first == 1;
}

template <typename T>
T readLe(const std::uint8_t* p) {
    // The protocol's bytes are little-endian; every host MICROROBOTICA builds for is too (checked at decode).
    T value;
    std::memcpy(&value, p, sizeof(T));
    return value;
}

/// One element of dtype at p, widened to double.
double element(const std::string& dtype, const std::uint8_t* p) {
    if (dtype == "<f4") return static_cast<double>(readLe<float>(p));
    if (dtype == "<f8") return readLe<double>(p);
    if (dtype == "<i4") return static_cast<double>(readLe<std::int32_t>(p));
    if (dtype == "<i8") return static_cast<double>(readLe<std::int64_t>(p));
    if (dtype == "<u4") return static_cast<double>(readLe<std::uint32_t>(p));
    if (dtype == "<u8") return static_cast<double>(readLe<std::uint64_t>(p));
    if (dtype == "<i2") return static_cast<double>(readLe<std::int16_t>(p));
    if (dtype == "<u2") return static_cast<double>(readLe<std::uint16_t>(p));
    if (dtype == "|u1" || dtype == "|b1") return static_cast<double>(*p);
    if (dtype == "|i1") return static_cast<double>(static_cast<std::int8_t>(*p));
    throw ProtocolError("a field of dtype " + dtype + ", which this client does not read");
}

void flatten(const nlohmann::json& value, std::vector<double>& out) {
    if (value.is_array()) {
        for (const auto& v : value) {
            flatten(v, out);
        }
    } else if (value.is_number()) {
        out.push_back(value.get<double>());
    } else if (value.is_boolean()) {
        out.push_back(value.get<bool>() ? 1.0 : 0.0);
    } else {
        throw ProtocolError("a JSON frame's field holds something that is not a number: " + value.dump());
    }
}

SessionStatus statusOf(const nlohmann::json& message) {
    SessionStatus s;
    s.state = text(message, "state");
    s.step = number<std::uint64_t>(message, "step", 0);
    s.time = number<double>(message, "time", 0.0);
    s.frames = number<std::uint64_t>(message, "frames", 0);
    s.stepping = number<std::uint64_t>(message, "stepping", 0);
    if (message.contains("pace") && message["pace"].is_number()) {
        s.pace = message["pace"].get<double>();
    }
    s.recording = message.value("recording", false);
    s.nonFinite = message.contains("non_finite") ? message["non_finite"] : nlohmann::json();
    s.activity = text(message, "activity");
    if (message.contains("pending") && message["pending"].is_array()) {
        for (const auto& c : message["pending"]) {
            if (c.is_string()) {
                s.pending.push_back(c.get<std::string>());
            }
        }
    }
    return s;
}

} // namespace

// ── Requests ─────────────────────────────────────────────────────────────────

nlohmann::json request(const std::string& command, const nlohmann::json& arguments) {
    nlohmann::json message = arguments.is_object() ? arguments : nlohmann::json::object();
    message["protocol"] = kProtocol;
    message["command"] = command;
    return message;
}

nlohmann::json helloRequest() { return request("hello"); }
nlohmann::json statusRequest() { return request("status"); }
nlohmann::json startRequest() { return request("start"); }
nlohmann::json pauseRequest() { return request("pause"); }
nlohmann::json stepRequest(int chunks) { return request("step", {{"chunks", chunks}}); }
nlohmann::json stopRequest() { return request("stop"); }
nlohmann::json setRequest(const nlohmann::json& values) { return request("set", {{"values", values}}); }
nlohmann::json recordRequest(bool on) { return request("record", {{"on", on}}); }
nlohmann::json paceRequest(std::optional<double> rtf) {
    return request("pace", {{"rtf", rtf ? nlohmann::json(*rtf) : nlohmann::json(nullptr)}});
}

ReplyWait replyWait(const std::string& command) {
    if (command == "stop") return ReplyWait::RunFolder;
    if (command == "set" || command == "record" || command == "pace" || command == "start" || command == "step") {
        return ReplyWait::ChunkEnd;
    }
    return ReplyWait::AtOnce;
}

// ── Replies ──────────────────────────────────────────────────────────────────

bool SessionStatus::isPending(const std::string& command) const {
    return std::find(pending.begin(), pending.end(), command) != pending.end();
}

bool Reply::stopPending() const {
    return ok && command == "stop" && !folder && (taken || status.isPending("stop"));
}

Reply parseReply(const nlohmann::json& message) {
    if (!message.is_object()) {
        throw ProtocolError("a reply that is not a JSON object");
    }
    const std::string protocol = text(message, "protocol");
    if (protocol != kProtocol) {
        throw ProtocolError("a reply of protocol '" + protocol + "'; this client speaks " + kProtocol);
    }
    Reply reply;
    reply.raw = message;
    reply.ok = message.value("ok", false);
    reply.command = text(message, "command");
    reply.status = statusOf(message);
    if (message.contains("errors")) {
        reply.errors = core::refusalsFromJson(message["errors"]);
    }
    if (message.contains("folder") && message["folder"].is_string()) {
        reply.folder = message["folder"].get<std::string>();
    }
    reply.taken = message.contains("taken") && message["taken"].is_boolean() && message["taken"].get<bool>();
    reply.message = text(message, "message");
    return reply;
}

Reply parseReply(const std::string& text) {
    nlohmann::json message;
    try {
        message = nlohmann::json::parse(text);
    } catch (const nlohmann::json::parse_error& e) {
        throw ProtocolError(std::string("a reply that is not JSON: ") + e.what());
    }
    return parseReply(message);
}

// ── Schema and frames ────────────────────────────────────────────────────────

std::size_t FieldSpec::count() const {
    std::size_t n = 1;
    for (std::size_t d : shape) {
        n *= d;
    }
    return n;
}

std::size_t FieldSpec::itemBytes() const {
    if (dtype.size() < 3) {
        return 0;
    }
    try {
        return static_cast<std::size_t>(std::stoul(dtype.substr(2)));
    } catch (const std::exception&) {
        return 0;
    }
}

const FieldSpec* FrameSchema::find(const std::string& path) const {
    for (const auto& f : fields) {
        if (f.path == path) {
            return &f;
        }
    }
    return nullptr;
}

FrameSchema parseSchema(const nlohmann::json& schema) {
    if (!schema.is_object() || !schema.contains("fields") || !schema["fields"].is_array()) {
        throw ProtocolError("hello has no frame schema with fields");
    }
    if (text(schema, "byte_order", "little") != "little") {
        throw ProtocolError("a frame schema of byte order " + text(schema, "byte_order"));
    }
    FrameSchema out;
    out.id = number<std::uint32_t>(schema, "id", 0);
    out.headerBytes = number<std::size_t>(schema, "header_bytes", kFrameHeaderBytes);
    out.frameBytes = number<std::size_t>(schema, "frame_bytes", 0);
    std::size_t payload = 0;
    for (const auto& f : schema["fields"]) {
        FieldSpec spec;
        spec.path = text(f, "path");
        spec.node = text(f, "node");
        spec.field = text(f, "field");
        spec.shape = shapeOf(f.contains("shape") ? f["shape"] : nlohmann::json::array());
        spec.dtype = text(f, "dtype");
        spec.unit = text(f, "unit");
        spec.offset = number<std::size_t>(f, "offset", 0);
        if (spec.path.empty() || spec.itemBytes() == 0) {
            throw ProtocolError("a frame schema field without a path or a dtype this client reads: " + f.dump());
        }
        payload = std::max(payload, spec.offset + spec.bytes());
        out.fields.push_back(std::move(spec));
    }
    if (out.headerBytes != kFrameHeaderBytes) {
        throw ProtocolError("a frame header of " + std::to_string(out.headerBytes) + " bytes; this client reads " +
                            std::to_string(kFrameHeaderBytes));
    }
    if (out.frameBytes == 0) {
        out.frameBytes = out.headerBytes + payload;
    } else if (out.headerBytes + payload > out.frameBytes) {
        throw ProtocolError("a frame schema whose fields run past its frame_bytes");
    }
    return out;
}

bool isBinaryFrame(const std::uint8_t* data, std::size_t size) {
    return size >= 4 && std::memcmp(data, "MIMF", 4) == 0;
}

FrameHeader readFrameHeader(const std::uint8_t* data, std::size_t size) {
    if (size < kFrameHeaderBytes || !isBinaryFrame(data, size)) {
        throw ProtocolError("not a mime.live/1 binary frame (no MIMF header)");
    }
    if (!hostIsLittleEndian()) {
        throw ProtocolError("this client reads frames only on a little-endian host");
    }
    FrameHeader h;
    h.version = readLe<std::uint16_t>(data + 4);
    h.flags = readLe<std::uint16_t>(data + 6);
    h.schemaId = readLe<std::uint32_t>(data + 8);
    h.number = readLe<std::uint32_t>(data + 12);
    h.step = readLe<std::uint64_t>(data + 16);
    h.time = readLe<double>(data + 24);
    if (h.version != 1) {
        throw ProtocolError("a frame of version " + std::to_string(h.version) + "; this client reads version 1");
    }
    return h;
}

LiveFrame decodeBinaryFrame(const std::uint8_t* data, std::size_t size, const FrameSchema& schema) {
    const FrameHeader h = readFrameHeader(data, size);
    if (h.schemaId != schema.id) {
        throw ProtocolError("a frame of schema " + std::to_string(h.schemaId) + "; this session's is " +
                            std::to_string(schema.id));
    }
    if (size != schema.frameBytes) {
        throw ProtocolError("a frame of " + std::to_string(size) + " bytes; this schema's are " +
                            std::to_string(schema.frameBytes));
    }
    LiveFrame frame;
    frame.number = h.number;
    frame.step = h.step;
    frame.time = h.time;
    frame.final = h.final();
    frame.values.reserve(schema.fields.size());
    for (const auto& spec : schema.fields) {
        const std::uint8_t* p = data + schema.headerBytes + spec.offset;
        const std::size_t item = spec.itemBytes();
        std::vector<double> values(spec.count());
        for (std::size_t i = 0; i < values.size(); ++i) {
            values[i] = element(spec.dtype, p + i * item);
        }
        frame.values.emplace(spec.path, std::move(values));
    }
    return frame;
}

LiveFrame decodeJsonFrame(const nlohmann::json& message, const FrameSchema& schema) {
    if (!message.is_object() || text(message, "protocol") != kProtocol) {
        throw ProtocolError("not a mime.live/1 JSON frame");
    }
    if (number<std::uint32_t>(message, "schema", 0) != schema.id) {
        throw ProtocolError("a JSON frame of another schema");
    }
    LiveFrame frame;
    frame.number = number<std::uint32_t>(message, "frame", 0);
    frame.step = number<std::uint64_t>(message, "step", 0);
    frame.time = number<double>(message, "time", 0.0);
    frame.final = message.value("final", false);
    const auto fields = message.find("fields");
    if (fields == message.end() || !fields->is_object()) {
        throw ProtocolError("a JSON frame without fields");
    }
    for (const auto& spec : schema.fields) {
        const auto it = fields->find(spec.path);
        if (it == fields->end()) {
            throw ProtocolError("a JSON frame without the schema's field " + spec.path);
        }
        std::vector<double> values;
        values.reserve(spec.count());
        flatten(*it, values);
        if (values.size() != spec.count()) {
            throw ProtocolError("a JSON frame's " + spec.path + " has " + std::to_string(values.size()) +
                                " numbers; its shape holds " + std::to_string(spec.count()));
        }
        frame.values.emplace(spec.path, std::move(values));
    }
    return frame;
}

LiveFrame decodeFrame(const std::uint8_t* data, std::size_t size, const FrameSchema& schema) {
    if (isBinaryFrame(data, size)) {
        return decodeBinaryFrame(data, size, schema);
    }
    nlohmann::json message;
    try {
        message = nlohmann::json::parse(data, data + size);
    } catch (const nlohmann::json::parse_error& e) {
        throw ProtocolError(std::string("a frame that is neither binary nor JSON: ") + e.what());
    }
    return decodeJsonFrame(message, schema);
}

std::string FrameOrder::see(std::uint32_t number) {
    if (!seenAny_) {
        seenAny_ = true;
        expected_ = static_cast<std::uint64_t>(number) + 1;
        return {};
    }
    std::string problem;
    if (number > expected_) {
        problem = "frames " + std::to_string(expected_) + " to " + std::to_string(number - 1) +
                  " were dropped before frame " + std::to_string(number);
        dropped_ += number - expected_;
    } else if (number < expected_) {
        problem = "frame " + std::to_string(number) + " arrived after frame " + std::to_string(expected_ - 1) +
                  ": out of order";
        ++outOfOrder_;
    }
    expected_ = std::max<std::uint64_t>(expected_, static_cast<std::uint64_t>(number) + 1);
    return problem;
}

// ── Binding ──────────────────────────────────────────────────────────────────

std::vector<BindingEntry> parseBinding(const nlohmann::json& binding) {
    const nlohmann::json* entries = &binding;
    if (binding.is_object()) {
        const auto it = binding.find("entries");
        if (it == binding.end()) {
            return {};
        }
        entries = &*it;
    }
    std::vector<BindingEntry> out;
    if (!entries->is_array()) {
        return out;
    }
    for (const auto& e : *entries) {
        if (!e.is_object()) {
            continue;
        }
        BindingEntry b;
        b.prim = text(e, "prim");
        b.node = text(e, "node");
        b.field = text(e, "field");
        if (e.contains("index") && e["index"].is_number_integer() && e["index"].get<long long>() >= 0) {
            b.index = e["index"].get<std::size_t>();
        }
        b.kind = text(e, "kind");
        b.thing = text(e, "thing");
        b.name = text(e, "name");
        b.unit = text(e, "unit");
        b.space = text(e, "space", "world");
        if (e.contains("at") && e["at"].is_object()) {
            b.atNode = text(e["at"], "node");
            b.atField = text(e["at"], "field");
        }
        if (!b.prim.empty() && !b.node.empty() && !b.field.empty()) {
            out.push_back(std::move(b));
        }
    }
    return out;
}

// ── hello ────────────────────────────────────────────────────────────────────

std::size_t LiveInputSpec::count() const {
    std::size_t n = 1;
    for (std::size_t d : shape) {
        n *= d;
    }
    return n;
}

std::vector<double> LiveInputSpec::defaults() const {
    std::vector<double> values;
    if (!defaultValue.is_null()) {
        try {
            flatten(defaultValue, values);
        } catch (const ProtocolError&) {
            values.clear();
        }
    }
    if (values.size() != count()) {
        values.assign(count(), 0.0);  // MIME's own default when a controller declares none: zeros
    }
    return values;
}

std::vector<std::string> VisualsInfo::noticeLines() const {
    std::vector<std::string> lines;
    auto add = [&lines](const nlohmann::json& item) {
        if (item.is_string()) {
            lines.push_back(item.get<std::string>());
        } else if (item.is_object()) {
            for (const char* key : {"text", "notice", "credit", "terms"}) {
                if (item.contains(key) && item[key].is_string()) {
                    lines.push_back(item[key].get<std::string>());
                }
            }
        }
    };
    if (notice.is_array()) {
        for (const auto& item : notice) {
            add(item);
        }
    } else {
        add(notice);
    }
    return lines;
}

Hello parseHello(const nlohmann::json& reply) {
    const Reply r = parseReply(reply);
    if (!r.ok) {
        std::string why = "hello was refused";
        if (!r.errors.empty()) {
            why += ": " + r.errors.front().code + ": " + r.errors.front().message;
        }
        throw ProtocolError(why);
    }
    Hello h;
    h.raw = reply;
    h.protocol = text(reply, "protocol");
    h.mimeVersion = text(reply, "mime");
    h.experiment = text(reply, "experiment");
    if (reply.contains("hashes") && reply["hashes"].is_object()) {
        for (const auto& [k, v] : reply["hashes"].items()) {
            if (v.is_string()) {
                h.hashes[k] = v.get<std::string>();
            }
        }
    }
    h.dt = number<double>(reply, "dt", 0.0);
    h.chunkSteps = number<std::int64_t>(reply, "chunk_steps", 0);
    if (reply.contains("controller") && reply["controller"].is_object()) {
        const auto& c = reply["controller"];
        ControllerInfo info;
        info.entry = text(c, "entry");
        info.mode = text(c, "mode");
        info.every = number<std::int64_t>(c, "every", 0);
        info.latency = number<std::int64_t>(c, "latency", 0);
        h.controller = info;
    }
    if (reply.contains("live_inputs") && reply["live_inputs"].is_object()) {
        for (const auto& [label, spec] : reply["live_inputs"].items()) {
            LiveInputSpec in;
            in.label = label;
            in.units = text(spec, "units");
            in.shape = shapeOf(spec.is_object() && spec.contains("shape") ? spec["shape"] : nlohmann::json::array());
            in.meaning = text(spec, "meaning");
            in.defaultValue = spec.is_object() && spec.contains("default") ? spec["default"] : nlohmann::json();
            h.liveInputs.push_back(std::move(in));
        }
    }
    h.stage = text(reply, "stage");
    if (reply.contains("visuals") && reply["visuals"].is_object()) {
        const auto& v = reply["visuals"];
        VisualsInfo info;
        if (v.contains("overlay") && v["overlay"].is_string()) {
            info.overlay = v["overlay"].get<std::string>();
        }
        info.drawn = number<long>(v, "drawn", 0);
        if (v.contains("missing") && v["missing"].is_object()) {
            for (const auto& [set, count] : v["missing"].items()) {
                info.missing[set] = count.is_number() ? count.get<long>() : 0;
            }
        }
        info.notice = v.contains("notice") ? v["notice"] : nlohmann::json();
        h.visuals = info;
    }
    h.binding = parseBinding(reply.contains("binding") ? reply["binding"] : nlohmann::json());
    h.topology = reply.contains("topology") ? reply["topology"] : nlohmann::json::object();
    h.report = runs::parseReport(reply.contains("report") ? reply["report"] : nlohmann::json());
    if (!reply.contains("schema")) {
        throw ProtocolError("hello has no frame schema");
    }
    h.schema = parseSchema(reply["schema"]);
    if (reply.contains("commands") && reply["commands"].is_array()) {
        for (const auto& c : reply["commands"]) {
            if (c.is_string()) {
                h.commands.push_back(c.get<std::string>());
            }
        }
    }
    h.status = r.status;
    return h;
}

// ── Ready file ───────────────────────────────────────────────────────────────

ReadyFile parseReadyFile(const std::string& content) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(content);
    } catch (const nlohmann::json::parse_error& e) {
        throw ProtocolError(std::string("a ready file that is not JSON: ") + e.what());
    }
    ReadyFile r;
    r.protocol = text(j, "protocol", kProtocol);
    r.commands = text(j, "commands");
    r.frames = text(j, "frames");
    r.out = text(j, "out");
    if (r.protocol != kProtocol) {
        throw ProtocolError("a ready file of protocol '" + r.protocol + "'; this client speaks " + kProtocol);
    }
    if (r.commands.empty() || r.frames.empty()) {
        throw ProtocolError("a ready file without both endpoints");
    }
    return r;
}

} // namespace microbotica::live
