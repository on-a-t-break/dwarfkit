# Warnings-as-errors for dwarfkit's own targets. Never applied to third_party.
function(dk_warnings target)
  if(MSVC)
    # /Zc:preprocessor: conformant preprocessor, required for DK_FIELDS variadic macros
    target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8 /Zc:preprocessor)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()
