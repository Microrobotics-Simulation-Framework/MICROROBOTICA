#include "runs/npz.h"

#include <cstring>
#include <fstream>
#include <iterator>

#include <zlib.h>

namespace microbotica::runs {

namespace {

template <typename T>
T le(const std::uint8_t* p) {
    T value;
    std::memcpy(&value, p, sizeof(T));
    return value;  // zip and npy are little-endian, as every host MICROROBOTICA builds for
}

double element(const std::string& dtype, const std::uint8_t* p) {
    if (dtype == "<f8") return le<double>(p);
    if (dtype == "<f4") return static_cast<double>(le<float>(p));
    if (dtype == "<i8") return static_cast<double>(le<std::int64_t>(p));
    if (dtype == "<i4") return static_cast<double>(le<std::int32_t>(p));
    if (dtype == "<i2") return static_cast<double>(le<std::int16_t>(p));
    if (dtype == "<u8") return static_cast<double>(le<std::uint64_t>(p));
    if (dtype == "<u4") return static_cast<double>(le<std::uint32_t>(p));
    if (dtype == "<u2") return static_cast<double>(le<std::uint16_t>(p));
    if (dtype == "|u1" || dtype == "|b1") return static_cast<double>(*p);
    if (dtype == "|i1") return static_cast<double>(static_cast<std::int8_t>(*p));
    throw NpzError("an array of dtype " + dtype + ", which this reader does not read");
}

std::size_t itemBytes(const std::string& dtype) {
    if (dtype.size() < 3) return 0;
    try {
        return static_cast<std::size_t>(std::stoul(dtype.substr(2)));
    } catch (const std::exception&) {
        return 0;
    }
}

/// The value of `key` in a .npy header's Python dict literal, as text: `'<f4'`, `False`, `(121, 7)`.
std::string headerValue(const std::string& header, const std::string& key) {
    const std::string needle = "'" + key + "'";
    const std::size_t at = header.find(needle);
    if (at == std::string::npos) {
        throw NpzError("a .npy header without " + key + ": " + header);
    }
    std::size_t start = header.find(':', at + needle.size());
    if (start == std::string::npos) throw NpzError("a .npy header that is not a dict: " + header);
    ++start;
    while (start < header.size() && header[start] == ' ') ++start;
    std::size_t end = start;
    if (header[start] == '(') {
        end = header.find(')', start);
        if (end == std::string::npos) throw NpzError("a .npy shape without its ')': " + header);
        return header.substr(start, end - start + 1);
    }
    if (header[start] == '\'' || header[start] == '"') {
        end = header.find(header[start], start + 1);
        return header.substr(start + 1, end - start - 1);
    }
    while (end < header.size() && header[end] != ',' && header[end] != '}') ++end;
    return header.substr(start, end - start);
}

std::vector<std::size_t> parseShape(const std::string& text) {
    std::vector<std::size_t> shape;
    std::string digits;
    for (char c : text) {
        if (c >= '0' && c <= '9') {
            digits.push_back(c);
        } else if (!digits.empty()) {
            shape.push_back(static_cast<std::size_t>(std::stoull(digits)));
            digits.clear();
        }
    }
    return shape;
}

} // namespace

std::size_t NpyArray::count() const {
    std::size_t n = 1;
    for (std::size_t d : shape) n *= d;
    return n;
}

NpyArray readNpy(const std::uint8_t* data, std::size_t size) {
    if (size < 10 || std::memcmp(data, "\x93NUMPY", 6) != 0) {
        throw NpzError("not a .npy file");
    }
    const int major = data[6];
    std::size_t headerLength = 0;
    std::size_t headerStart = 0;
    if (major == 1) {
        headerLength = le<std::uint16_t>(data + 8);
        headerStart = 10;
    } else if (major == 2 || major == 3) {
        if (size < 12) throw NpzError("a truncated .npy header");
        headerLength = le<std::uint32_t>(data + 8);
        headerStart = 12;
    } else {
        throw NpzError("a .npy file of version " + std::to_string(major));
    }
    if (headerStart + headerLength > size) throw NpzError("a truncated .npy header");
    const std::string header(reinterpret_cast<const char*>(data + headerStart), headerLength);

    NpyArray array;
    array.dtype = headerValue(header, "descr");
    if (headerValue(header, "fortran_order").find("True") != std::string::npos) {
        throw NpzError("a Fortran-order array, which MIME does not write");
    }
    array.shape = parseShape(headerValue(header, "shape"));
    const std::size_t item = itemBytes(array.dtype);
    if (item == 0) throw NpzError("a .npy dtype this reader does not read: " + array.dtype);
    const std::uint8_t* payload = data + headerStart + headerLength;
    const std::size_t n = array.count();
    if (headerStart + headerLength + n * item > size) throw NpzError("a truncated .npy payload");
    array.values.resize(n);
    for (std::size_t i = 0; i < n; ++i) {
        array.values[i] = element(array.dtype, payload + i * item);
    }
    return array;
}

NpzFile::NpzFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw NpzError("cannot open " + path);
    bytes_.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    const std::size_t n = bytes_.size();
    if (n < 22) throw NpzError(path + " is not a zip archive");

    // The end of central directory record, searched from the end (a comment may follow it).
    std::size_t eocd = std::string::npos;
    for (std::size_t i = n - 22 + 1; i-- > 0 && n - i <= 22 + 65535;) {
        if (le<std::uint32_t>(&bytes_[i]) == 0x06054b50u) {
            eocd = i;
            break;
        }
    }
    if (eocd == std::string::npos) throw NpzError(path + " is not a zip archive (no directory)");
    std::uint64_t entries = le<std::uint16_t>(&bytes_[eocd + 10]);
    std::uint64_t dirOffset = le<std::uint32_t>(&bytes_[eocd + 16]);
    // Zip64: its own end record, located by the locator just before.
    if ((entries == 0xFFFF || dirOffset == 0xFFFFFFFFu) && eocd >= 20
        && le<std::uint32_t>(&bytes_[eocd - 20]) == 0x07064b50u) {
        const std::uint64_t at = le<std::uint64_t>(&bytes_[eocd - 20 + 8]);
        if (at + 56 > n || le<std::uint32_t>(&bytes_[at]) != 0x06064b50u) throw NpzError("a broken zip64 record");
        entries = le<std::uint64_t>(&bytes_[at + 32]);
        dirOffset = le<std::uint64_t>(&bytes_[at + 48]);
    }

    std::size_t p = static_cast<std::size_t>(dirOffset);
    for (std::uint64_t e = 0; e < entries; ++e) {
        if (p + 46 > n || le<std::uint32_t>(&bytes_[p]) != 0x02014b50u) throw NpzError("a broken zip directory");
        Member m;
        m.method = le<std::uint16_t>(&bytes_[p + 10]);
        m.compressed = le<std::uint32_t>(&bytes_[p + 20]);
        m.size = le<std::uint32_t>(&bytes_[p + 24]);
        const std::uint16_t nameLength = le<std::uint16_t>(&bytes_[p + 28]);
        const std::uint16_t extraLength = le<std::uint16_t>(&bytes_[p + 30]);
        const std::uint16_t commentLength = le<std::uint16_t>(&bytes_[p + 32]);
        m.localHeader = le<std::uint32_t>(&bytes_[p + 42]);
        std::string name(reinterpret_cast<const char*>(&bytes_[p + 46]), nameLength);
        // Zip64 extra field: the sizes and offset that did not fit, in this order.
        std::size_t x = p + 46 + nameLength;
        const std::size_t xEnd = x + extraLength;
        while (x + 4 <= xEnd && xEnd <= n) {
            const std::uint16_t id = le<std::uint16_t>(&bytes_[x]);
            const std::uint16_t length = le<std::uint16_t>(&bytes_[x + 2]);
            if (id == 0x0001) {
                std::size_t f = x + 4;
                if (m.size == 0xFFFFFFFFu) { m.size = le<std::uint64_t>(&bytes_[f]); f += 8; }
                if (m.compressed == 0xFFFFFFFFu) { m.compressed = le<std::uint64_t>(&bytes_[f]); f += 8; }
                if (m.localHeader == 0xFFFFFFFFu) { m.localHeader = le<std::uint64_t>(&bytes_[f]); }
            }
            x += 4 + length;
        }
        if (name.size() > 4 && name.compare(name.size() - 4, 4, ".npy") == 0) {
            name.resize(name.size() - 4);
        }
        members_[name] = m;
        p += 46 + nameLength + extraLength + commentLength;
    }
}

std::vector<std::string> NpzFile::names() const {
    std::vector<std::string> out;
    out.reserve(members_.size());
    for (const auto& [name, member] : members_) out.push_back(name);
    return out;
}

bool NpzFile::contains(const std::string& name) const {
    return members_.count(name) != 0;
}

NpyArray NpzFile::read(const std::string& name) const {
    const auto it = members_.find(name);
    if (it == members_.end()) throw NpzError("no array " + name + " in the archive");
    const Member& m = it->second;
    const std::size_t h = static_cast<std::size_t>(m.localHeader);
    if (h + 30 > bytes_.size() || le<std::uint32_t>(&bytes_[h]) != 0x04034b50u) throw NpzError("a broken zip member");
    const std::size_t start = h + 30 + le<std::uint16_t>(&bytes_[h + 26]) + le<std::uint16_t>(&bytes_[h + 28]);
    if (start + m.compressed > bytes_.size()) throw NpzError("a truncated zip member: " + name);
    if (m.method == 0) {
        return readNpy(&bytes_[start], static_cast<std::size_t>(m.size));
    }
    if (m.method != 8) throw NpzError("a zip member compressed by method " + std::to_string(m.method));
    std::vector<std::uint8_t> out(static_cast<std::size_t>(m.size));
    z_stream z{};
    z.next_in = const_cast<Bytef*>(&bytes_[start]);
    z.avail_in = static_cast<uInt>(m.compressed);
    z.next_out = out.data();
    z.avail_out = static_cast<uInt>(out.size());
    if (inflateInit2(&z, -MAX_WBITS) != Z_OK) throw NpzError("zlib could not start");
    const int result = inflate(&z, Z_FINISH);
    inflateEnd(&z);
    if (result != Z_STREAM_END) throw NpzError("a deflated member that does not inflate: " + name);
    return readNpy(out.data(), out.size());
}

} // namespace microbotica::runs
