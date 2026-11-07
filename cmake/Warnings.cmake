# Strict warnings for first-party targets (never applied to third-party code).
function(as_enable_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /WX)
  else()
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
    if(EMSCRIPTEN)
      # EM_JS/EM_ASM refer to their arguments as $0, $1... which -Wpedantic flags as '$' in an identifier.
      target_compile_options(${target} PRIVATE -Wno-dollar-in-identifier-extension)
    endif()
  endif()
endfunction()

function(as_enable_sanitizers target)
  if(AS_SANITIZE AND NOT MSVC)
    target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE -fsanitize=address,undefined)
  endif()
endfunction()
