// Port of antelope test/chain.ts. Cases needing types from later Phase 1 items
// (Action, Transaction, Authority, keys, ABI, Serializer) join as those land.
#include <doctest/doctest.h>

#include <dwarfkit/antelope/chain/asset.hpp>
#include <dwarfkit/antelope/chain/block_id.hpp>
#include <dwarfkit/antelope/chain/blob.hpp>
#include <dwarfkit/antelope/chain/bytes.hpp>
#include <dwarfkit/antelope/chain/checksum.hpp>
#include <dwarfkit/antelope/chain/float.hpp>
#include <dwarfkit/antelope/chain/name.hpp>
#include <dwarfkit/antelope/chain/time.hpp>

using namespace dwarfkit;

static_assert("foo"_n == Name::from("foo"));
static_assert("eosio.token"_n.value == Name::from("eosio.token").value);
static_assert("1.0000 EOS"_asset.units == 10000);
static_assert("4,EOS"_symbol.precision() == 4);

TEST_SUITE("chain") {
    TEST_CASE("asset") {
        CHECK(Asset::from("-1.2345 NEGS").value().toString() == "-1.2345 NEGS");
        CHECK(Asset::from("-0.2345 NEGS").value().toString() == "-0.2345 NEGS");
        CHECK(Asset::from("0.0000000000000 DUCKS").value().toString() == "0.0000000000000 DUCKS");
        CHECK(Asset::from("99999999999 DUCKS").value().toString() == "99999999999 DUCKS");
        CHECK(Asset::from("-99999999999 DUCKS").value().toString() == "-99999999999 DUCKS");
        CHECK(Asset::from("-0.0000000000001 DUCKS").value().toString() == "-0.0000000000001 DUCKS");

        auto asset = Asset::from("1.000000000 FOO").value();
        CHECK(asset.value() == 1.0);
        REQUIRE(asset.setValue(asset.value() + 0.000000001).has_value());
        CHECK(asset.value() == 1.000000001);
        REQUIRE(asset.setValue(-100).has_value());
        CHECK(asset.toString() == "-100.000000000 FOO");
        CHECK(asset.units == -100000000000);

        const auto symbol = Asset::Symbol::from("10,K").value();
        CHECK(symbol.name() == "K");
        CHECK(symbol.precision() == 10);
        CHECK(Asset::Symbol::from(symbol.value).value().toString() == symbol.toString());

        CHECK_FALSE(Asset::Symbol::from("0,0").has_value());

        const auto nft_symbol = Asset::Symbol::from("0,").value();
        CHECK(nft_symbol.name() == "");
        CHECK(nft_symbol.precision() == 0);
        CHECK(nft_symbol.value == 0);
        CHECK(nft_symbol.code().value == 0);
        CHECK(Asset::Symbol::from(nft_symbol.value).value().toString() == nft_symbol.toString());

        // test null asset
        asset = Asset::from("0 ").value();
        CHECK(asset.value() == 0);
        CHECK(asset.toString() == "0 ");

        asset = Asset::from(10, "4,POX").value();
        CHECK(asset.value() == 10);
        CHECK(asset.units == 100000);

        asset = Asset::fromUnits(1, "10,KEK").value();
        CHECK(asset.value() == 0.0000000001);
        REQUIRE(asset.setValue(asset.value() + 0.0000000001).has_value());
        CHECK(asset.units == 2);

        asset = Asset::from(3.004, "4,RAR").value();
        REQUIRE(asset.setValue(asset.value() + 1).has_value());
        CHECK(asset.toString() == "4.0040 RAR");
        CHECK(asset.value() == 4.004);

        asset = Asset::from(3.004, "8,RAR").value();
        REQUIRE(asset.setValue(asset.value() + 1).has_value());
        CHECK(asset.units == 400400000);
        CHECK(asset.toString() == "4.00400000 RAR");
        CHECK(asset.value() == 4.004);

        CHECK(symbol.convertUnits(9223372036854775807ll).error().message ==
              "Number can only safely store up to 53 bits");
        CHECK_FALSE(Asset::from("").has_value());
        CHECK_FALSE(Asset::from("1POP").has_value());
        CHECK_FALSE(Asset::from("1.0000000000000000000000 BIGS").has_value());
        CHECK_FALSE(Asset::from("1.2 horse").has_value());
        CHECK_FALSE(Asset::Symbol::from("12").has_value());
    }

    TEST_CASE("block id") {
        const std::string string =
            "048865fb643bca3b644647177f0cf363f7956794d0a7ec3bc6d29d93d9637308";
        const auto blockId = BlockId::from(string).value();
        CHECK(blockId.toString() == string);
        CHECK(blockId.blockNum() == 76047867);
        const auto blockId2 =
            BlockId::fromBlockChecksum(
                "61375f2d5fbe6bbad86e424962a190e8309394b7bff4bf3e16b0a2a71e5a617c", 7)
                .value();
        CHECK(blockId2.toString() ==
              "000000075fbe6bbad86e424962a190e8309394b7bff4bf3e16b0a2a71e5a617c");
        CHECK(blockId2.blockNum() == 7);
    }

    TEST_CASE("blob") {
        const auto expected = Bytes(std::vector<uint8_t>{0xbe, 0xef, 0xfa, 0xce});

        // Correct
        CHECK(Bytes::from(Blob::from("vu/6zg==").value().array).equals(expected));
        // Wrong padding, ensure it still works
        CHECK(Bytes::from(Blob::from("vu/6zg=").value().array).equals(expected));
        CHECK(Bytes::from(Blob::from("vu/6zg").value().array).equals(expected));
        CHECK(Bytes::from(Blob::from("vu/6zg===").value().array).equals(expected));
        // round trip
        CHECK(Blob::from("vu/6zg==").value().base64String() == "vu/6zg==");
    }

    TEST_CASE("bytes") {
        CHECK(Bytes::from("hello", BytesEncoding::utf8).value().toString(BytesEncoding::hex) ==
              "68656c6c6f");
        CHECK(Bytes::equal(Bytes::from("beef").value(), Bytes::from("beef").value()) == true);
        CHECK(Bytes::equal(Bytes::from("beef").value(), Bytes::from("face").value()) == false);
        CHECK(Bytes::from("68656c6c6f").value().toString(BytesEncoding::utf8) == "hello");
        CHECK(Bytes(std::vector<uint8_t>{0xff, 0x00, 0xff, 0x00}).copy().hexString() == "ff00ff00");
        CHECK(Checksum256::hash(Bytes::from("hello world", BytesEncoding::utf8).value())
                  .hexString() ==
              "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
        CHECK(Checksum512::hash(Bytes::from("hello world", BytesEncoding::utf8).value())
                  .hexString() ==
              "309ecc489c12d6eb4cc40f50c902f2b4d0ed77ee511a7c7a9bcd3ca86d4cd86f"
              "989dd35bc5ff499670da34255b45b0cfd830e81f605dcf7dc5542e93ae9cd76f");
        CHECK(Checksum160::hash(Bytes::from("hello world", BytesEncoding::utf8).value())
                  .hexString() == "98c615784ccb5fe5936fbc0cbe9dfdb408d92f0f");
        CHECK(Bytes::from("beef").value().zeropadded(4).toString() == "0000beef");
        CHECK(Bytes::from("beef").value().zeropadded(2).toString() == "beef");
        CHECK(Bytes::from("beef").value().zeropadded(1).toString() == "beef");
        CHECK(Bytes::from("beef").value().zeropadded(1, true).toString() == "be");
        CHECK(Bytes::from("beef").value().zeropadded(2, true).toString() == "beef");
        CHECK(Bytes::from("beef").value().zeropadded(3, true).toString() == "00beef");
    }

    TEST_CASE("time") {
        const int64_t now = 1629864000123;
        CHECK(TimePoint::fromMilliseconds(static_cast<double>(now)).toMilliseconds() == now);
        CHECK(TimePointSec::from(TimePointSec::fromMilliseconds(static_cast<double>(now)))
                  .toMilliseconds() == 1629864000000);
        CHECK_FALSE(TimePoint::from("blah").has_value());
        CHECK(BlockTimestamp::from("2021-08-25T02:37:24.500").value().toString() ==
              "2021-08-25T02:37:24.500");
        CHECK(BlockTimestamp::fromMilliseconds(static_cast<double>(now)).toMilliseconds() ==
              1629864000000);
        CHECK(TimePoint::from("2021-08-25T04:00:59").value().toString() ==
              "2021-08-25T04:00:59.000");
        // upstream builds "<value>ZZ" for strings that already carry a zone
        CHECK_FALSE(TimePoint::from("2021-08-25T04:00:59Z").has_value());
        CHECK_FALSE(TimePointSec::from("2021-08-25T04:00:59+01:00").has_value());
    }

    TEST_CASE("equality helpers") {
        const auto name = Name::from("foo");
        CHECK(name.equals("foo") == true);
        CHECK(name.equals(Name::from(6712615244595724288ull)) == true);
        CHECK(name.equals(Name::from(uint64_t(12345))) == false);
        CHECK(name.equals("bar") == false);

        const auto checksum = Checksum160::hash(Bytes::from("hello", BytesEncoding::utf8).value());
        CHECK(checksum.equals("108f07b8382412612c048d07d13f814118445acd") == true);
        CHECK(checksum.equals("108f07b8382412612c048d07d13f814118445abe") == false);

        const auto time = TimePointSec::from(1);
        CHECK(time.equals(time) == true);
        CHECK(time.equals("1970-01-01T00:00:01") == true);
        CHECK(time.equals("2020-02-20T02:20:20") == false);
        CHECK(time.equals(TimePointSec::from(1)) == true);
        CHECK(time.equals(TimePointSec::from(2)) == false);
        CHECK(time.equals(TimePoint::from(int64_t(1) * 1000000)) == true);
    }

    TEST_CASE("name") {
        CHECK(Name::from("foo").toString() == "foo");
        CHECK(Name::from("foo").value == 6712615244595724288ull);
        CHECK(Name::from(".gems").toString() == ".gems");
        CHECK(Name::from("foo....").toString() == "foo");
        CHECK(Name::from("").value == 0);
        CHECK(Name::from("").toString() == "");
        CHECK(Name::from("eosio.token").toString() == "eosio.token");
    }

    TEST_CASE("float128") {
        const std::string hex = "0100000000000000000000000000ff7f";
        const auto value = Float128::from(hex).value();
        CHECK(value.toString() == "0x" + hex);
        CHECK(Float128::from("0x" + hex).value() == value);
        CHECK(Float128::from("beef").error().message == "Invalid float128");
    }

    TEST_CASE("extended asset") {
        const auto ext = ExtendedAsset::from(Asset::from("1.0000 EOS").value(), "eosio.token"_n);
        CHECK(ext.toJSON().dump() == R"({"quantity":"1.0000 EOS","contract":"eosio.token"})");
        const auto extSym = ExtendedSymbol::from("4,EOS"_symbol, "eosio.token"_n);
        CHECK(extSym.toJSON().dump() == R"({"sym":"4,EOS","contract":"eosio.token"})");
    }
}
