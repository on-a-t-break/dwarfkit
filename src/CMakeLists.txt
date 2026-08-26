add_library(dwarfkit STATIC
  core/base64.cpp
  core/hash.cpp
  core/random.cpp
  core/version.cpp
  core/zlib.cpp
  antelope/base58.cpp
  antelope/crypto.cpp
  antelope/utils.cpp
  antelope/chain/asset.cpp
  antelope/chain/blob.cpp
  antelope/chain/block_id.cpp
  antelope/chain/bytes.cpp
  antelope/chain/checksum.cpp
  antelope/chain/float.cpp
  antelope/chain/integer.cpp
  antelope/chain/name.cpp
  antelope/chain/private_key.cpp
  antelope/chain/public_key.cpp
  antelope/chain/abi.cpp
  antelope/chain/action.cpp
  antelope/chain/authority.cpp
  antelope/chain/permission_level.cpp
  antelope/chain/signature.cpp
  antelope/chain/time.cpp
  antelope/chain/transaction.cpp
  antelope/serializer/builtins.cpp
  antelope/serializer/decoder.cpp
  antelope/serializer/serializer.cpp
  antelope/serializer/traits.cpp
)

target_include_directories(dwarfkit PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

target_link_libraries(dwarfkit
  PUBLIC dk_expected dk_json
  PRIVATE dk_trezor_crypto secp256k1 dk_miniz)
if(WIN32)
  target_link_libraries(dwarfkit PRIVATE bcrypt)
endif()

target_compile_features(dwarfkit PUBLIC cxx_std_20)
dk_warnings(dwarfkit)
