add_library(dwarfkit STATIC
  core/version.cpp
)

target_include_directories(dwarfkit PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)

target_link_libraries(dwarfkit
  PUBLIC dk_expected dk_json
  PRIVATE dk_trezor_crypto secp256k1 dk_miniz)

target_compile_features(dwarfkit PUBLIC cxx_std_20)
dk_warnings(dwarfkit)
