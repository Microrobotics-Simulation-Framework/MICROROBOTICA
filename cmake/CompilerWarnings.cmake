# CompilerWarnings.cmake
# Provides target_enable_warnings(target) for MICROBOTICA targets only.
#
# Note: -Wpedantic is intentionally omitted. MICROBOTICA uses designated
# initializers (a C++20 feature supported by GCC/Clang as an extension in
# C++17 mode) throughout ComponentMeta definitions for readability.
# -Wall -Wextra -Werror is sufficient for production quality.

function(target_enable_warnings target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:
            -Wall
            -Wextra
            -Werror
        >
    )
endfunction()
