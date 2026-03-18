# CompilerWarnings.cmake
# Provides target_enable_warnings(target) for MICROBOTICA targets only.
#
# Note: -Wpedantic is intentionally omitted. MICROBOTICA uses designated
# initializers (a C++20 feature supported by GCC/Clang as an extension in
# C++17 mode) throughout ComponentMeta definitions for readability.
# -Wall -Wextra -Werror is sufficient for production quality.
#
# OpenUSD headers trigger several warnings (-Wunused-parameter,
# -Wdeprecated-copy, -Wclass-memaccess, -Wdeprecated, -Wmissing-field-initializers)
# that cannot be suppressed by SYSTEM includes alone (template instantiations
# in our TUs still trigger them). These are excluded from -Werror.

function(target_enable_warnings target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:
            -Wall
            -Wextra
            -Werror
            # Suppress warnings originating from OpenUSD headers that
            # propagate through template instantiations in our code.
            -Wno-error=unused-parameter
            -Wno-error=deprecated-copy
            -Wno-error=class-memaccess
            -Wno-error=missing-field-initializers
            -Wno-error=cpp
            -Wno-deprecated
        >
    )
endfunction()
