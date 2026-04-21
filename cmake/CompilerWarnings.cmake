add_library(rastertoolbox_warnings INTERFACE)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(rastertoolbox_warnings INTERFACE
        -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
    )
elseif(MSVC)
    target_compile_options(rastertoolbox_warnings INTERFACE /W4)
endif()
