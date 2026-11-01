#pragma once

// NumPy's .npz (a zip of .npy arrays, as numpy.savez writes) read in C++: MIME 0.3's run folders keep their outputs in
// `outputs.npz` (MIME's results/README.md), written with allow_pickle=False, so every member is a plain numeric array.
// Stored and deflated members are read; arrays come back as doubles, row-major, with their shape.
//
// Free of Qt and USD: the standard library and zlib.

#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace microbotica::runs {

/// A .npz or .npy that cannot be read, and why.
class NpzError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// One array: its dtype as NumPy spells it (`<f4`), its shape, and its values widened to double, row-major.
struct NpyArray {
    std::string dtype;
    std::vector<std::size_t> shape;
    std::vector<double> values;

    std::size_t count() const;
};

/// A .npy file's bytes, read. Throws NpzError (not .npy, Fortran order, a dtype this reader does not read).
NpyArray readNpy(const std::uint8_t* data, std::size_t size);

/// An .npz archive: its members by name (without the `.npy` suffix), read on demand.
class NpzFile {
public:
    /// Open `path` and read its directory. Throws NpzError.
    explicit NpzFile(const std::string& path);

    /// Every member's name, `.npy` removed (e.g. "final/umr/pose").
    std::vector<std::string> names() const;
    bool contains(const std::string& name) const;

    /// One member, read. Throws NpzError.
    NpyArray read(const std::string& name) const;

private:
    struct Member {
        std::uint16_t method = 0;      // 0 stored, 8 deflated
        std::uint64_t compressed = 0;
        std::uint64_t size = 0;
        std::uint64_t localHeader = 0;
    };
    std::vector<std::uint8_t> bytes_;
    std::map<std::string, Member> members_;
};

} // namespace microbotica::runs
