# ------------------ include-what-you-use (IWYU) ------------------
#
# IWYU is attached PER TARGET (target property CXX_INCLUDE_WHAT_YOU_USE), never through the
# global CMAKE_CXX_INCLUDE_WHAT_YOU_USE. That is what keeps external code out of it:
# QCodeEditor, StripCppComments, uchardet and nuspell are separate targets we simply never
# call stc_enable_iwyu() on. (IWYU also only reports on the .cpp being compiled and its own
# header, so headers of Qt etc. are not "analysed" either - they only appear as suggested includes.)
#
#   -DSTC_ENABLE_IWYU=OFF   turns it off
#
# IWYU only prints suggestions (warnings) during compilation; it never fails the build.
# It needs the `include-what-you-use` binary (Ubuntu: `sudo apt install iwyu`). If it is
# not installed this module does nothing.

include_guard(GLOBAL)

option(STC_ENABLE_IWYU "Run include-what-you-use on STC_editor's own sources (never on libs/ or fetched deps)" ON)

find_program(STC_IWYU_PATH NAMES include-what-you-use iwyu)

set(STC_IWYU_MAPPING_FILE "${CMAKE_CURRENT_LIST_DIR}/iwyu.imp")

if(NOT STC_ENABLE_IWYU)
    message(STATUS "IWYU: disabled (STC_ENABLE_IWYU=OFF)")
elseif(NOT STC_IWYU_PATH)
    message(STATUS "IWYU: include-what-you-use not found - skipping (install it, e.g. `sudo apt install iwyu`)")
else()
    message(STATUS "IWYU: found ${STC_IWYU_PATH}; will be run on our own targets only")
endif()

# Attach IWYU to one of OUR targets.
function(stc_enable_iwyu target)
    if(NOT STC_ENABLE_IWYU OR NOT STC_IWYU_PATH)
        return()
    endif()
    set(_iwyu_cmd "${STC_IWYU_PATH}")
    if(EXISTS "${STC_IWYU_MAPPING_FILE}")
        list(APPEND _iwyu_cmd -Xiwyu "--mapping_file=${STC_IWYU_MAPPING_FILE}")
    endif()
    set_target_properties(${target} PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "${_iwyu_cmd}")
endfunction()
