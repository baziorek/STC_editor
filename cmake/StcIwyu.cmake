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
option(STC_IWYU_FILTER_NOISE
    "Hide IWYU's harmless '@headername mapping' warnings about libstdc++ headers (UNIX only, see cmake/iwyu-filter.sh)" ON)

find_program(STC_IWYU_PATH NAMES include-what-you-use iwyu)

set(STC_IWYU_MAPPING_FILE "${CMAKE_CURRENT_LIST_DIR}/iwyu.imp")

if(NOT STC_ENABLE_IWYU)
    message(STATUS "IWYU: disabled (STC_ENABLE_IWYU=OFF)")
elseif(NOT STC_IWYU_PATH)
    message(STATUS "IWYU: include-what-you-use not found - skipping (install it, e.g. `sudo apt install iwyu`)")
else()
    message(STATUS "IWYU: found ${STC_IWYU_PATH}; will be run on our own targets only")
    # cmake/iwyu-fix.sh needs compile_commands.json (Qt Creator does not ask for it by default).
    # Must be set before the targets are created; harmless otherwise (one JSON file in the build dir).
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# ------------------ Qt mapping (generated) ------------------
# Without a mapping IWYU suggests Qt's implementation headers (<qstring.h>, <qlist.h>, ...)
# instead of the public forwarding headers (<QString>, <QList>, ...). Qt ships a forwarding
# header per class (QtCore/QString contains just `#include "qstring.h"`), so we derive the
# mapping from the Qt that is actually being built against - it matches whatever Qt version
# and install layout the developer uses. Written to the build directory, not into the repo.
#
# For every forwarder QFoo -> qfoo_impl.h:
#   symbol  QFoo                      -> <QFoo>
#   include qfoo_impl.h (private)     -> <QFoo>   (only when that is unambiguous: the forwarder
#                                                  named after the header, or the only one)
# Headers that several forwarders share without a canonical one (e.g. qobjectdefs.h) and
# headers without any forwarder (e.g. qtypeinfo.h) are handled by hand in iwyu.imp.
set(STC_IWYU_QT_MODULES Core Gui Widgets Network WebEngineCore WebEngineWidgets WebChannel
    CACHE STRING "Qt modules whose forwarding headers are put into the generated IWYU mapping")

# Qt 5.x/6.4 define all of these in one header, <QtGlobal> (qglobal.h); since Qt 6.5 they live in
# many small headers that qglobal.h merely includes (qtypes.h, qlogging.h, qminmax.h, ...), each
# with its own forwarding header (<QtTypes>, <QtLogging>, <QtMinMax>, ...) that exists only in the
# newer versions. IWYU would suggest those, which makes the code depend on the Qt version and
# litters it with includes nobody writes by hand. <QtGlobal> is the documented, portable
# spelling for all of them, so the whole family (headers AND the symbols of their forwarders,
# e.g. QFlags, QOverload, QTypeInfo) is mapped to it. Verified against Qt 6.11.1: qglobal.h includes
# every header in this list (qtconfigmacros/qtcoreexports are reached through qtcoreglobal.h).
set(STC_IWYU_QT_GLOBAL_SATELLITES
    qglobal qtcoreglobal qtconfigmacros qtcoreexports qtypes qlogging qminmax qswap qnumeric
    qassert qtresource qtypeinfo qflags qoverload qtpreprocessorsupport qsystemdetection
    qprocessordetection qcompilerdetection qtdeprecationmarkers qtversion qtversionchecks)

function(stc_generate_qt_iwyu_mapping out_file)
    set(_qt "Qt${QT_VERSION_MAJOR}")
    if(NOT TARGET ${_qt}::Core)
        return()
    endif()

    # <include root>/QtCore is among Qt6::Core's include directories; its parent is the root.
    get_target_property(_core_dirs ${_qt}::Core INTERFACE_INCLUDE_DIRECTORIES)
    set(_root "")
    foreach(_dir IN LISTS _core_dirs)
        get_filename_component(_name "${_dir}" NAME)
        if(_name STREQUAL "QtCore" AND IS_DIRECTORY "${_dir}")
            get_filename_component(_root "${_dir}" DIRECTORY)
            break()
        endif()
    endforeach()
    if(NOT _root)
        message(STATUS "IWYU: could not locate Qt include root - no Qt mapping generated")
        return()
    endif()

    set(_sym_tpl [=[  { symbol: [ "@SYM@", "private", "<@TGT@>", "public" ] },
]=])
    set(_inc_tpl [=[  { include: [ "@[\"<](@MOD@/)?@BASE@\\.h[\">]", "private", "<@FWD@>", "public" ] },
]=])

    set(_symbols "")
    set(_includes "")
    set(_count 0)
    foreach(_mod IN LISTS STC_IWYU_QT_MODULES)
        set(_moddir "${_root}/Qt${_mod}")
        if(NOT IS_DIRECTORY "${_moddir}")
            continue()
        endif()
        # Forwarding headers are the files without an extension.
        file(GLOB _files LIST_DIRECTORIES false "${_moddir}/*")
        list(FILTER _files EXCLUDE REGEX "\\.[^/]*$")

        # header basename -> list of forwarders, collected per module
        set(_headers "")
        foreach(_file IN LISTS _files)
            get_filename_component(_fwd "${_file}" NAME)
            file(STRINGS "${_file}" _lines REGEX "^[ \t]*#[ \t]*include")
            list(LENGTH _lines _n)
            # Umbrella headers (QtCore, QtGuiDepends...) have several/zero includes: skip them.
            if(NOT _n EQUAL 1)
                continue()
            endif()
            # Qt <= 6.4:  #include "qstring.h"
            # Qt 6.11:    #include <QtCore/qstring.h> // IWYU pragma: export
            # (module prefix and quote style vary between versions; a trailing comment is ignored)
            if(NOT _lines MATCHES "include[ \t]*[<\"]([A-Za-z0-9_]+/)?([A-Za-z0-9_]+)\\.h[>\"]")
                continue()
            endif()
            set(_base "${CMAKE_MATCH_2}")
            if(NOT _fwd MATCHES "^[A-Za-z0-9_]+$")
                continue()
            endif()
            list(FIND STC_IWYU_QT_GLOBAL_SATELLITES "${_base}" _sat_idx)
            set(_sym_target "${_fwd}")
            if(NOT _sat_idx EQUAL -1)
                set(_sym_target "QtGlobal")
            endif()
            string(REPLACE "@SYM@" "${_fwd}" _sym_line "${_sym_tpl}")
            string(REPLACE "@TGT@" "${_sym_target}" _sym_line "${_sym_line}")
            string(APPEND _symbols "${_sym_line}")
            list(APPEND _headers "${_base}")
            set(_fwds_of_${_base} ${_fwds_of_${_base}} "${_fwd}")
            math(EXPR _count "${_count} + 1")
        endforeach()
        list(REMOVE_DUPLICATES _headers)
        foreach(_base IN LISTS _headers)
            list(FIND STC_IWYU_QT_GLOBAL_SATELLITES "${_base}" _sat_idx)
            if(NOT _sat_idx EQUAL -1)
                unset(_fwds_of_${_base})
                continue()   # mapped to <QtGlobal> below
            endif()
            # Which forwarder is THE public name of this header? Prefer the one named exactly like
            # the header (qstring.h <- QString); else the "Qt" form (qtypes.h <- QtTypes,
            # qlogging.h <- QtLogging, qcompare.h <- QtCompare), which Qt 6 creates next to
            # class forwarders (QIntegerForSize, QMessageLogger, ...) pointing at the same header.
            set(_chosen "")
            set(_chosen_qt "")
            string(SUBSTRING "${_base}" 1 -1 _base_rest)
            foreach(_fwd IN LISTS _fwds_of_${_base})
                string(TOLOWER "${_fwd}" _lower)
                if(_lower STREQUAL "${_base}")
                    set(_chosen "${_fwd}")
                elseif(_base MATCHES "^q" AND _lower STREQUAL "qt${_base_rest}")
                    set(_chosen_qt "${_fwd}")
                endif()
            endforeach()
            if(NOT _chosen)
                set(_chosen "${_chosen_qt}")
            endif()
            list(LENGTH _fwds_of_${_base} _nf)
            if(NOT _chosen AND _nf EQUAL 1)
                set(_chosen "${_fwds_of_${_base}}")
            endif()
            if(_chosen)
                string(REPLACE "@MOD@" "Qt${_mod}" _line "${_inc_tpl}")
                string(REPLACE "@BASE@" "${_base}" _line "${_line}")
                string(REPLACE "@FWD@" "${_chosen}" _line "${_line}")
                string(APPEND _includes "${_line}")
            endif()
            unset(_fwds_of_${_base})
        endforeach()
    endforeach()

    foreach(_sat IN LISTS STC_IWYU_QT_GLOBAL_SATELLITES)
        string(REPLACE "@MOD@" "QtCore" _line "${_inc_tpl}")
        string(REPLACE "@BASE@" "${_sat}" _line "${_line}")
        string(REPLACE "@FWD@" "QtGlobal" _line "${_line}")
        string(APPEND _includes "${_line}")
    endforeach()

    if(_count EQUAL 0)
        # Qt changed the forwarding-header format again? Say what we saw, so it is easy to fix.
        set(_sample "")
        if(EXISTS "${_root}/QtCore/QString")
            file(READ "${_root}/QtCore/QString" _sample LIMIT 200)
            string(STRIP "${_sample}" _sample)
        endif()
        message(WARNING "IWYU: no Qt forwarding headers recognised in ${_root} - Qt mapping NOT generated, "
                        "IWYU will suggest <qstring.h> instead of <QString>. ${_root}/QtCore/QString contains: "
                        "'${_sample}'. Adjust the pattern in stc_generate_qt_iwyu_mapping() (cmake/StcIwyu.cmake).")
        return()
    endif()

    message(STATUS "IWYU: generated Qt mapping from ${_count} forwarding headers (${_root})")
    file(WRITE "${out_file}" "# Generated by cmake/StcIwyu.cmake from the Qt headers in ${_root} - do not edit.\n[\n${_symbols}${_includes}]\n")
endfunction()

# Attach IWYU to one of OUR targets.
function(stc_enable_iwyu target)
    if(NOT STC_ENABLE_IWYU OR NOT STC_IWYU_PATH)
        return()
    endif()

    # Generated Qt mapping: created once per configure, shared by all targets.
    get_property(_qt_map GLOBAL PROPERTY STC_IWYU_QT_MAPPING_FILE)
    get_property(_qt_map_done GLOBAL PROPERTY STC_IWYU_QT_MAPPING_DONE)
    if(NOT _qt_map_done)
        set(_candidate "${CMAKE_BINARY_DIR}/iwyu-qt.imp")
        stc_generate_qt_iwyu_mapping("${_candidate}")
        if(EXISTS "${_candidate}")
            set(_qt_map "${_candidate}")
            set_property(GLOBAL PROPERTY STC_IWYU_QT_MAPPING_FILE "${_qt_map}")
        endif()
        set_property(GLOBAL PROPERTY STC_IWYU_QT_MAPPING_DONE TRUE)
    endif()

    set(_iwyu_cmd "")
    if(UNIX AND STC_IWYU_FILTER_NOISE AND EXISTS "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/iwyu-filter.sh")
        # `sh script.sh <iwyu> ...` - does not depend on the script's executable bit.
        list(APPEND _iwyu_cmd sh "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/iwyu-filter.sh")
    endif()
    list(APPEND _iwyu_cmd "${STC_IWYU_PATH}")
    foreach(_map IN ITEMS "${STC_IWYU_MAPPING_FILE}" "${_qt_map}")
        if(_map AND EXISTS "${_map}")
            list(APPEND _iwyu_cmd -Xiwyu "--mapping_file=${_map}")
        endif()
    endforeach()
    set_target_properties(${target} PROPERTIES CXX_INCLUDE_WHAT_YOU_USE "${_iwyu_cmd}")
endfunction()
