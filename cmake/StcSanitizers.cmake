# ------------------ Sanitizers (AddressSanitizer + UndefinedBehaviorSanitizer) ------------------
#
# On a crash or an invalid memory access the sanitizer runtime prints the kind of
# error together with a stack trace (function, file:line) - no gdb needed.
#
#   -DSTC_ENABLE_SANITIZERS=OFF   turns it off
#
# Design notes:
#  * Applied PER TARGET (via the INTERFACE library "stc_sanitizers"), never through
#    CMAKE_CXX_FLAGS. So only our own targets are instrumented - not QCodeEditor,
#    uchardet, nuspell or anything else fetched/vendored.
#  * Active in every configuration except Release / MinSizeRel (so Debug,
#    RelWithDebInfo and the "empty" build type that plain `cmake ..` gives).
#  * "-g" is added so reports contain file:line even when CMAKE_BUILD_TYPE is empty.
#  * The ASan runtime is linked statically; otherwise the binary aborts at start-up
#    with "ASan runtime does not come first in initial library list".
#  * If the user already passes -fsanitize=... through CMAKE_CXX_FLAGS (e.g. Qt Creator's
#    "Sanitizers" panel, possibly with ThreadSanitizer, which cannot be combined with
#    ASan) we stay out of the way.

include_guard(GLOBAL)

option(STC_ENABLE_SANITIZERS
    "Build STC_editor and its tests with AddressSanitizer + UBSan (not in Release/MinSizeRel)" ON)

# Link the sanitizer flags into an executable target (no-op when sanitizers are unavailable/disabled).
function(stc_enable_sanitizers target)
    if(TARGET stc_sanitizers)
        target_link_libraries(${target} PRIVATE stc_sanitizers)
    endif()
endfunction()

# Make ctest runs actually fail on sanitizer findings and print readable traces.
# Appends to ENVIRONMENT, so it does not clobber e.g. QT_QPA_PLATFORM=offscreen.
# Call after all add_test()/set_tests_properties() calls.
function(stc_sanitizers_test_environment)
    if(NOT TARGET stc_sanitizers)
        return()
    endif()
    get_directory_property(_stc_tests TESTS)
    foreach(_stc_test IN LISTS _stc_tests)
        set_property(TEST ${_stc_test} APPEND PROPERTY ENVIRONMENT
            "ASAN_OPTIONS=strict_string_checks=1:detect_stack_use_after_return=1"
            "UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1"
        )
    endforeach()
endfunction()

if(NOT STC_ENABLE_SANITIZERS)
    message(STATUS "Sanitizers: disabled (STC_ENABLE_SANITIZERS=OFF)")
    return()
endif()

if(WIN32 OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "Sanitizers: skipped (needs GCC or Clang on a non-Windows system)")
    return()
endif()

if(CMAKE_CXX_FLAGS MATCHES "-fsanitize=")
    message(STATUS "Sanitizers: -fsanitize=... already present in CMAKE_CXX_FLAGS - leaving it to the user")
    return()
endif()

# Does the toolchain really have the runtimes (libasan/libubsan)? Try to compile AND link.
include(CheckCXXSourceCompiles)
set(CMAKE_REQUIRED_FLAGS "-fsanitize=address,undefined")
set(CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address,undefined")
check_cxx_source_compiles("int main() { return 0; }" STC_SANITIZERS_SUPPORTED)
unset(CMAKE_REQUIRED_FLAGS)
unset(CMAKE_REQUIRED_LINK_OPTIONS)

if(NOT STC_SANITIZERS_SUPPORTED)
    message(WARNING "Sanitizers requested, but this toolchain cannot build with -fsanitize=address,undefined "
                    "(libasan/libubsan missing?). Continuing without them.")
    return()
endif()

set(_stc_sanitize_cfg "$<NOT:$<CONFIG:Release,MinSizeRel>>")

add_library(stc_sanitizers INTERFACE)
target_compile_options(stc_sanitizers INTERFACE
    "$<${_stc_sanitize_cfg}:-fsanitize=address,undefined>"
    "$<${_stc_sanitize_cfg}:-fno-omit-frame-pointer>"
    "$<${_stc_sanitize_cfg}:-g>"
)
target_link_options(stc_sanitizers INTERFACE
    "$<${_stc_sanitize_cfg}:-fsanitize=address,undefined>"
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(stc_sanitizers INTERFACE "$<${_stc_sanitize_cfg}:-static-libasan>")
else()
    target_link_options(stc_sanitizers INTERFACE "$<${_stc_sanitize_cfg}:-static-libsan>")
endif()

message(STATUS "Sanitizers: AddressSanitizer + UBSan enabled for our own targets (all configs except Release/MinSizeRel)")
