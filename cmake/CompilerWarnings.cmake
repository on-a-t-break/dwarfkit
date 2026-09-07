# Warnings-as-errors for dwarfkit's own targets. Never applied to third_party.
function(dk_warnings target)
  if(MSVC)
    # /Zc:preprocessor: conformant preprocessor. The DK_FIELDS macros no longer
    # need it (they rescan through DK_EXPAND); kept so the library itself is
    # compiled conformantly
    target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8 /Zc:preprocessor)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()
