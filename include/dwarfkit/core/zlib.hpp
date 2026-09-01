#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <dwarfkit/core/result.hpp>

namespace dwarfkit {

// Ceiling on how much an inflate call may produce. Compressed input is
// untrusted (an esr: URI arrives from whoever sent the link) and deflate
// reaches roughly 1000:1, so decompression is capped rather than allowed to
// grow until the allocator fails. Real signing requests and packed
// transactions are kilobytes.
inline constexpr size_t maxInflateOutput = 16 * 1024 * 1024;

// zlib-wrapped deflate (pako.deflate / pako.inflate equivalents), via miniz.
Result<std::vector<uint8_t>> zlibCompress(std::span<const uint8_t> data);
Result<std::vector<uint8_t>> zlibUncompress(std::span<const uint8_t> data,
                                            size_t maxOutput = maxInflateOutput);

// raw deflate without the zlib wrapper (pako deflateRaw / inflateRaw), used by
// the ESR encoding.
Result<std::vector<uint8_t>> deflateRaw(std::span<const uint8_t> data);
Result<std::vector<uint8_t>> inflateRaw(std::span<const uint8_t> data,
                                        size_t maxOutput = maxInflateOutput);

}  // namespace dwarfkit
