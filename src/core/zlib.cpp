#include <dwarfkit/core/zlib.hpp>

#include <miniz.h>

namespace dwarfkit {

namespace {

Result<std::vector<uint8_t>> deflateImpl(std::span<const uint8_t> data, int windowBits) {
    mz_stream stream{};
    if (mz_deflateInit2(&stream, MZ_DEFAULT_LEVEL, MZ_DEFLATED, windowBits, 9,
                        MZ_DEFAULT_STRATEGY) != MZ_OK) {
        return err(ErrorKind::Internal, "deflate init failed");
    }
    std::vector<uint8_t> out(mz_deflateBound(&stream, static_cast<mz_ulong>(data.size())));
    stream.next_in = data.data();
    stream.avail_in = static_cast<unsigned int>(data.size());
    stream.next_out = out.data();
    stream.avail_out = static_cast<unsigned int>(out.size());
    const int status = mz_deflate(&stream, MZ_FINISH);
    mz_deflateEnd(&stream);
    if (status != MZ_STREAM_END) {
        return err(ErrorKind::Internal, "deflate failed");
    }
    out.resize(stream.total_out);
    return out;
}

Result<std::vector<uint8_t>> inflateImpl(std::span<const uint8_t> data, int windowBits) {
    mz_stream stream{};
    if (mz_inflateInit2(&stream, windowBits) != MZ_OK) {
        return err(ErrorKind::Internal, "inflate init failed");
    }
    std::vector<uint8_t> out;
    out.resize(data.size() * 4 + 64);
    stream.next_in = data.data();
    stream.avail_in = static_cast<unsigned int>(data.size());
    size_t written = 0;
    for (;;) {
        stream.next_out = out.data() + written;
        stream.avail_out = static_cast<unsigned int>(out.size() - written);
        const int status = mz_inflate(&stream, MZ_NO_FLUSH);
        written = stream.total_out;
        if (status == MZ_STREAM_END) {
            break;
        }
        if (status == MZ_OK || status == MZ_BUF_ERROR) {
            if (stream.avail_out == 0) {
                out.resize(out.size() * 2);
                continue;
            }
            if (status == MZ_BUF_ERROR) {
                mz_inflateEnd(&stream);
                return err(ErrorKind::Invalid, "invalid compressed data");
            }
            continue;
        }
        mz_inflateEnd(&stream);
        return err(ErrorKind::Invalid, "invalid compressed data");
    }
    mz_inflateEnd(&stream);
    out.resize(written);
    return out;
}

}  // namespace

Result<std::vector<uint8_t>> zlibCompress(std::span<const uint8_t> data) {
    return deflateImpl(data, MZ_DEFAULT_WINDOW_BITS);
}

Result<std::vector<uint8_t>> zlibUncompress(std::span<const uint8_t> data) {
    return inflateImpl(data, MZ_DEFAULT_WINDOW_BITS);
}

Result<std::vector<uint8_t>> deflateRaw(std::span<const uint8_t> data) {
    return deflateImpl(data, -MZ_DEFAULT_WINDOW_BITS);
}

Result<std::vector<uint8_t>> inflateRaw(std::span<const uint8_t> data) {
    return inflateImpl(data, -MZ_DEFAULT_WINDOW_BITS);
}

}  // namespace dwarfkit
