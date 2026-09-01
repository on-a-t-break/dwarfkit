// Regression tests for the pre-release hardening pass: every case here is a
// crash, hang or unbounded allocation reachable from untrusted input (a chain
// response, an esr: URI, a p2p frame) before it was fixed. The library's
// public API is exception-free, so "returns an error" is the contract being
// asserted; several of these used to throw or abort instead.
#include <doctest/doctest.h>

#include <dwarfkit/antelope.hpp>
#include <dwarfkit/core/zlib.hpp>
#include <dwarfkit/resources.hpp>

using namespace dwarfkit;

namespace {

ABI fieldAbi(const std::string& fieldType) {
    return ABI::from(json{{"version", "eosio::abi/1.2"},
                          {"structs", json::array({{{"name", "s"},
                                                    {"base", ""},
                                                    {"fields", json::array({{{"name", "f"},
                                                                             {"type", fieldType}}})}}})}})
        .value();
}

}  // namespace

TEST_SUITE("hardening") {
    TEST_CASE("array length prefix cannot drive an allocation") {
        // 5 bytes claiming 0xFFFFFFFF elements of 32 bytes each: reserving
        // from the wire length asked for ~137 GB
        const auto data = Bytes::from("ffffffff0f").value();
        const auto rv = Serializer::decode<std::vector<Checksum256>>(data.array);
        CHECK_FALSE(rv.has_value());
    }

    TEST_CASE("zero-width array elements are bounded by the input") {
        // an empty struct decodes without consuming bytes, so the element
        // count was limited only by the attacker's varuint32
        const ABI abi =
            ABI::from(json{{"version", "eosio::abi/1.2"},
                           {"structs", json::array({{{"name", "empty"},
                                                     {"base", ""},
                                                     {"fields", json::array()}},
                                                    {{"name", "s"},
                                                     {"base", ""},
                                                     {"fields", json::array({{{"name", "f"},
                                                                              {"type", "empty[]"}}})}}})}})
                .value();
        const auto data = Bytes::from("e807").value();  // varuint 1000
        const auto rv = Serializer::decode(data.array, "s", abi);
        REQUIRE_FALSE(rv.has_value());
        CHECK(rv.error().message.find("exceeds remaining data") != std::string::npos);
    }

    TEST_CASE("float fields error instead of throwing") {
        const ABI abi = fieldAbi("float32");
        CHECK_FALSE(Serializer::decodeObject(json{{"f", "abc"}}, "s", abi).has_value());
        CHECK_FALSE(Serializer::decodeObject(json{{"f", "1e999999"}}, "s", abi).has_value());
        CHECK_FALSE(Serializer::decodeObject(json{{"f", json::object()}}, "s", abi).has_value());
        CHECK_FALSE(Serializer::encode(json{{"f", "xxx"}}, "s", fieldAbi("float64")).has_value());
    }

    TEST_CASE("wrongly typed json for chain types errors instead of throwing") {
        // types whose json form is only ever a string
        for (const char* type : {"name", "asset", "symbol", "checksum256", "public_key",
                                 "signature", "bytes", "float128"}) {
            CAPTURE(type);
            const ABI abi = fieldAbi(type);
            CHECK_FALSE(Serializer::decodeObject(json{{"f", 123}}, "s", abi).has_value());
            CHECK_FALSE(Serializer::decodeObject(json{{"f", json::array()}}, "s", abi)
                            .has_value());
            CHECK_FALSE(Serializer::encode(json{{"f", true}}, "s", abi).has_value());
        }
        // types that accept a number or a string, but nothing else
        for (const char* type : {"time_point", "block_timestamp_type", "uint128", "int128"}) {
            CAPTURE(type);
            const ABI abi = fieldAbi(type);
            CHECK_FALSE(Serializer::decodeObject(json{{"f", json::array()}}, "s", abi)
                            .has_value());
            CHECK_FALSE(Serializer::decodeObject(json{{"f", json::object()}}, "s", abi)
                            .has_value());
            CHECK_FALSE(Serializer::encode(json{{"f", true}}, "s", abi).has_value());
        }
        // extended_asset reads two members that may be missing entirely
        const ABI ext = fieldAbi("extended_asset");
        CHECK_FALSE(Serializer::decodeObject(json{{"f", json::object()}}, "s", ext).has_value());
        CHECK_FALSE(
            Serializer::decodeObject(json{{"f", {{"quantity", 1}}}}, "s", ext).has_value());
    }

    TEST_CASE("oversized fixed array size in a type name errors") {
        const ABI abi = fieldAbi("uint8[99999999999]");
        CHECK_FALSE(
            Serializer::decodeObject(json{{"f", json::array()}}, "s", abi).has_value());
    }

    TEST_CASE("deep type chains do not overflow the stack") {
        json types = json::array();
        const int chain = 20000;
        for (int i = 0; i < chain; i++) {
            types.push_back({{"new_type_name", "t" + std::to_string(i)},
                             {"type", i + 1 < chain ? "t" + std::to_string(i + 1) : "uint8"}});
        }
        const ABI abi =
            ABI::from(json{{"version", "eosio::abi/1.2"}, {"types", types}}).value();
        CHECK_FALSE(Serializer::decodeObject(json(1), "t0", abi).has_value());
        CHECK_FALSE(Serializer::encode(json(1), "t0", abi).has_value());
    }

    TEST_CASE("overlong varuint32 is rejected rather than shifting out of range") {
        // seven continuation bytes would shift a uint32 by 42
        const auto data = Bytes::from("ffffffffffff7f").value();
        ABIDecoder decoder(data.array);
        CHECK_FALSE(decoder.readVaruint32().has_value());
    }

    TEST_CASE("inflate is bounded") {
        // 8 MB of zeros compresses to a few KB: it inflates under the default
        // ceiling, but is refused once the ceiling is lowered beneath it
        std::vector<uint8_t> zeros(8u * 1024 * 1024, 0);
        const auto compressed = deflateRaw(zeros).value();
        CHECK(compressed.size() < 64u * 1024);
        const auto full = inflateRaw(compressed);
        REQUIRE(full.has_value());
        CHECK(full->size() == zeros.size());
        const auto capped = inflateRaw(compressed, 1024u * 1024);
        REQUIRE_FALSE(capped.has_value());
        CHECK(capped.error().message.find("too large") != std::string::npos);
        // and the shipped default refuses a payload well inside a bomb's reach
        std::vector<uint8_t> huge(64u * 1024 * 1024, 0);
        const auto bomb = deflateRaw(huge).value();
        CHECK(bomb.size() < 512u * 1024);
        CHECK_FALSE(inflateRaw(bomb).has_value());
    }

    TEST_CASE("short WA public keys do not over-read") {
        // WA keys carry no enforced length, so the 33-byte compressed slice
        // used to run past the end of a short buffer
        const auto key = PublicKey::from(KeyType::WA, Bytes::from("aabb").value());
        const auto compressed = key.getCompressedKeyBytes();
        CHECK(compressed.array.size() == 2);
    }

    TEST_CASE("nist256p1 rejects public keys it cannot read") {
        // trezor reads 32 bytes at +1 and, for an 0x04 prefix, 32 more at +33
        const std::vector<uint8_t> uncompressedPrefix(33, 0x04);
        const auto secret = std::vector<uint8_t>(32, 0x11);
        CHECK_FALSE(crypto::sharedSecret(secret, uncompressedPrefix, KeyType::R1).has_value());
        CHECK_FALSE(crypto::verify(std::vector<uint8_t>(65, 0x20), std::vector<uint8_t>(32, 0x01),
                                   uncompressedPrefix, KeyType::R1));
    }

    TEST_CASE("asset parsing handles the int64 boundary") {
        // -2^63 used to be produced by negating INT64_MIN
        const auto min = Asset::from("-9223372036854775808 EOS");
        REQUIRE(min.has_value());
        CHECK(min->units == std::numeric_limits<int64_t>::min());
        CHECK_FALSE(Asset::Symbol::from("99999999999,EOS").has_value());
    }

    TEST_CASE("time conversion handles the int64 boundary") {
        CHECK(TimePoint(std::numeric_limits<int64_t>::max()).toMilliseconds() > 0);
        CHECK(TimePoint(std::numeric_limits<int64_t>::min()).toMilliseconds() < 0);
    }

    TEST_CASE("non-finite and out-of-range doubles do not cast to int64 as UB") {
        // the double->int64 chokepoint for every time conversion
        (void)TimePoint::fromMilliseconds(1e300);
        (void)TimePoint::fromMilliseconds(-1e300);
        (void)TimePoint::fromMilliseconds(std::nan(""));
        // the saturating helper the resources kit routes its double casts through
        using resources_detail::saturateInt64;
        CHECK(saturateInt64(std::nan("")) == 0);
        CHECK(saturateInt64(1e300) == std::numeric_limits<int64_t>::max());
        CHECK(saturateInt64(-1e300) == std::numeric_limits<int64_t>::min());
        CHECK(saturateInt64(42.0) == 42);
        CHECK(saturateInt64(-7.9) == -7);
    }

    TEST_CASE("p2p framing drops an oversized frame instead of buffering it") {
        struct Sink final : p2p::P2PProvider {
            void write(std::span<const uint8_t>, p2p::P2PHandler) override {}
            void end(p2p::P2PHandler) override {}
            void destroy(const std::optional<Error>&) override {}
            void onData(p2p::P2PDataHandler handler) override { data.push_back(handler); }
            void onError(p2p::P2PErrorHandler handler) override { errors.push_back(handler); }
            void onClose(p2p::P2PHandler) override {}
            std::vector<p2p::P2PDataHandler> data;
            std::vector<p2p::P2PErrorHandler> errors;
        };
        Sink sink;
        p2p::SimpleEnvelopeP2PProvider envelope(&sink);
        int errorCount = 0;
        envelope.onError([&](const Error&) { errorCount++; });
        // declare 0xFFFFFFFF bytes, then stream junk forever
        const std::vector<uint8_t> header = {0xff, 0xff, 0xff, 0xff};
        for (const auto& handler : sink.data) {
            handler(header);
        }
        CHECK(errorCount == 1);
        // the bogus header must not linger: a following well-formed frame is
        // delivered normally rather than being appended behind a length that
        // can never be satisfied
        std::vector<uint8_t> received;
        envelope.onData([&](std::span<const uint8_t> message) {
            received.assign(message.begin(), message.end());
        });
        const std::vector<uint8_t> frame = {0x03, 0x00, 0x00, 0x00, 0x61, 0x62, 0x63};
        for (const auto& handler : sink.data) {
            handler(frame);
        }
        CHECK(errorCount == 1);
        CHECK(received == std::vector<uint8_t>{0x61, 0x62, 0x63});
    }
}
