add_library(dwarfkit STATIC
  core/hash.cpp
  core/random.cpp
  core/version.cpp
  antelope/base58.cpp
  antelope/utils.cpp
  antelope/chain/bytes.cpp
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
