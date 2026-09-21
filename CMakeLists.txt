cmake_minimum_required(VERSION 3.20)
project(STC_editor VERSION 0.2 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets WebView WebEngineWidgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets WebEngineWidgets)

# ------------------ uchardet (system or FetchContent) ------------------
include(FetchContent)

find_package(uchardet QUIET)

set(UCHARDET_LIB_NAME "")
set(UCHARDET_INCLUDE_DIR "")

if(uchardet_FOUND)
    message(STATUS "Found system uchardet")
    set(UCHARDET_LIB_NAME uchardet::libuchardet)
else()
    message(STATUS "uchardet not found – using FetchContent to download")
    # Disable CLI tool building and shared library building
    set(BUILD_BINARY OFF CACHE BOOL "Do not build uchardet CLI tool" FORCE)
    # set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build uchardet as static library" FORCE)
    FetchContent_Declare(
        uchardet
        GIT_REPOSITORY https://gitlab.freedesktop.org/uchardet/uchardet.git
        GIT_TAG master
    )
    # NOTE: FetchContent_MakeAvailable() calls add_subdirectory() internally,
    # so any patch applied *after* it runs too late to matter - the original
    # file has already been parsed. Populate (fetch source only, no
    # configure) -> patch -> THEN add_subdirectory ourselves, so whatever we
    # patch actually takes effect. FetchContent_Populate() is deprecated on
    # CMake >= 3.30 (prints a harmless dev warning) but still fully
    # functional; it remains the correct way to patch a dependency's source
    # before it is configured.
    #
    # The actual reason this matters: uchardet's own top-level CMakeLists.txt
    # contains this, completely unconditionally (not gated on BUILD_TESTING
    # or anything test-related - its own test/ subdirectory is already
    # commented out upstream):
    #     if (CMAKE_BUILD_TYPE MATCHES Debug)
    #         add_compile_options("-fsanitize=address")
    #         add_link_options("-fsanitize=address")
    #     endif (CMAKE_BUILD_TYPE MATCHES Debug)
    # add_compile_options()/add_link_options() are directory-scoped: they
    # apply to uchardet's own directory tree (src/, where BOTH the shared
    # "libuchardet" and static "libuchardet_static" targets are defined) but
    # never propagate up to us. So in ANY Debug build, both uchardet variants
    # silently become ASan-instrumented while the rest of the project isn't -
    # the shared libuchardet.so ends up with a hard runtime dependency on
    # libasan.so.8 baked into its own ELF metadata, which triggers the
    # classic "ASan runtime does not come first in initial library list"
    # crash at startup, since nothing preloads it first. Confirmed directly
    # against the fetched source, not guessed.
    # Fix: build uchardet's own subtree as if it were a Release build,
    # regardless of what the top-level CMAKE_BUILD_TYPE is, so its ASan
    # branch never triggers. This only affects uchardet's own optimization
    # level/debug info, not ours - perfectly fine for a vendored dependency.
    FetchContent_GetProperties(uchardet)
    if(NOT uchardet_POPULATED)
        FetchContent_Populate(uchardet)
        set(BUILD_TESTING OFF CACHE BOOL "Disable uchardet's own test suite" FORCE)
        # uchardet's own test/ subdirectory registers ~150 of its own CTest
        # cases unconditionally (enable_testing() + add_subdirectory(test) in
        # its root CMakeLists.txt aren't actually gated on BUILD_TESTING).
        # With the test binary excluded from the default build below, those
        # would show up as spurious "Not Run" entries and a non-zero exit
        # code from a plain `ctest` invocation. Comment out that
        # add_subdirectory(test) call before it's ever parsed - this only
        # works because we Populate (fetch source only) before configuring
        # the subtree ourselves further down, unlike FetchContent_MakeAvailable
        # which would have already processed the original file by this point.
        file(READ "${uchardet_SOURCE_DIR}/CMakeLists.txt" _uchardet_cmake)
        string(REPLACE "add_subdirectory(test)" "# add_subdirectory(test)" _uchardet_cmake "${_uchardet_cmake}")
        file(WRITE "${uchardet_SOURCE_DIR}/CMakeLists.txt" "${_uchardet_cmake}")
        set(_stc_saved_build_type "${CMAKE_BUILD_TYPE}")
        set(CMAKE_BUILD_TYPE "Release")
        add_subdirectory(${uchardet_SOURCE_DIR} ${uchardet_BINARY_DIR})
        set(CMAKE_BUILD_TYPE "${_stc_saved_build_type}")
        # Belt-and-braces: if some future revision moves that add_subdirectory
        # call elsewhere (it has happened before), whatever target ends up
        # defined for uchardet's own test binary still won't build by default.
        if(TARGET uchardet-tests)
            set_target_properties(uchardet-tests PROPERTIES EXCLUDE_FROM_ALL TRUE)
        endif()
        # nsMBCSGroupProber.cpp's HandleData() trips GCC's -Wmaybe-uninitialized
        # on the local "st" variable - every code path actually assigns it
        # before use, this is GCC being unable to prove that through the
        # nested loop/branch structure. It's third-party, upstream code we
        # don't maintain (and re-fetched fresh from a floating branch on
        # every clean build, so patching the source itself wouldn't stick
        # reliably) - just silence this specific warning class for
        # uchardet's own targets instead.
        foreach(_uchardet_tgt libuchardet libuchardet_static)
            if(TARGET ${_uchardet_tgt})
                target_compile_options(${_uchardet_tgt} PRIVATE -Wno-maybe-uninitialized)
            endif()
        endforeach()
    endif()

    if(TARGET libuchardet)
        add_library(uchardet::libuchardet ALIAS libuchardet)
        set(UCHARDET_LIB_NAME uchardet::libuchardet)

        get_target_property(UCHARDET_SOURCE_DIR libuchardet SOURCE_DIR)
        if(UCHARDET_SOURCE_DIR)
            set(UCHARDET_INCLUDE_DIR "${UCHARDET_SOURCE_DIR}/src")
            message(STATUS "Detected include path for uchardet: ${UCHARDET_INCLUDE_DIR}")

            add_library(uchardet_headers INTERFACE)
            target_include_directories(uchardet_headers INTERFACE "${UCHARDET_INCLUDE_DIR}")
        else()
            message(FATAL_ERROR "Could not determine SOURCE_DIR for libuchardet")
        endif()
    endif()
endif()

# ------------------ Nuspell (VCPKG or FetchContent) ------------------
find_package(Nuspell QUIET)

# --- If not found, fetch and build it ---
if(NOT Nuspell_FOUND)
    message(STATUS "Nuspell not found, fetching and building it from source...")

    FetchContent_Declare(
        nuspell
        GIT_REPOSITORY https://github.com/nuspell/nuspell.git
        GIT_TAG v5.1.6
    )

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(BUILD_TOOLS OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(nuspell)

    # Set alias to always have Nuspell::nuspell
    if(NOT TARGET Nuspell::nuspell)
        add_library(Nuspell::nuspell ALIAS nuspell)
    endif()
endif()

# ------------------ Project Sources ------------------
include_directories(${PROJECT_SOURCE_DIR}) # for .ui files

set(PROJECT_SOURCES
    main.cpp
    CodeEditor.h CodeEditor.cpp
    stcSyntaxPatterns.h stcSyntaxPatterns.cpp

    ui/mainwindow.h ui/mainwindow.cpp ui/mainwindow.ui
    ui/finddialog.h ui/finddialog.ui ui/finddialog.cpp
    ui/errorlist.h ui/errorlist.cpp ui/errorlist.ui
    ui/gotolinewidget.h ui/gotolinewidget.cpp ui/gotolinewidget.ui
    ui/stctagsbuttons.h ui/stctagsbuttons.cpp ui/stctagsbuttons.ui
    ui/shortcutsdialog.h ui/shortcutsdialog.cpp
    ui/highlightdelegate.h ui/highlightdelegate.cpp
    ui/cppcompilerdialog.h ui/cppcompilerdialog.cpp
    ui/WorkAwareStopwatch.h ui/WorkAwareStopwatch.cpp ui/WorkAwareStopwatch.ui

    checkers/PairedTagsChecker.h checkers/PairedTagsChecker.cpp

    types/stcTags.h types/stcTags.cpp
    types/CodeBlock.h
    types/documentstatistics.h types/documentstatistics.cpp

    widgets/LineNumberArea.h widgets/LineNumberArea.cpp
    widgets/DocumentationBrowserDialog.h widgets/DocumentationBrowserDialog.cpp
    widgets/FilteredTagTableWidget.h widgets/FilteredTagTableWidget.cpp
    widgets/CodeBlocksTableWidget.h widgets/CodeBlocksTableWidget.cpp
    widgets/TodosTrackerTableWidget.h widgets/TodosTrackerTableWidget.cpp
    widgets/LoginDialog.h widgets/LoginDialog.cpp
    widgets/StcPreview.h widgets/StcPreview.cpp
    widgets/DiffViewerWidget.h widgets/DiffViewerWidget.cpp
    widgets/DiffReviewDialog.h widgets/DiffReviewDialog.cpp
    widgets/BreadcrumbTextBrowser.h widgets/BreadcrumbTextBrowser.cpp
    widgets/RenameFileDialog.h widgets/RenameFileDialog.cpp widgets/RenameFileDialog.ui
    widgets/StcTablesCreator.h widgets/StcTablesCreator.cpp widgets/StcTablesCreator.ui
    widgets/BackupRecoveryDialog.h widgets/BackupRecoveryDialog.cpp

    utils/DiffCalculation.h utils/DiffCalculation.cpp
    utils/FileEncodingHandler.h utils/FileEncodingHandler.cpp
    utils/STCSyntaxHighlighter.h utils/STCSyntaxHighlighter.cpp
    utils/SpellChecker.h utils/SpellChecker.cpp
    utils/BackupManager.h utils/BackupManager.cpp
)

set(TEXT_FILES
    LICENSE
    README.md
    README_ang.md
    .github/workflows/release-ubuntu22.04.yml
    .github/workflows/release-windows.yml
    .github/workflows/test-linux.yml
    .github/workflows/README.md
    dictionaries/pl/pl_PL.aff
    dictionaries/pl/pl_PL.dic
    dictionaries/pl/README_en.txt
    dictionaries/pl/README_pl.txt
)

# ------------------ Translation Files ------------------
set(TRANSLATION_FILES
    translations/stc_editor_en.ts
    translations/stc_editor_pl.ts
)

set(PROJECT_FILES ${PROJECT_SOURCES} ${TEXT_FILES})

set(app_icon_resource_linux "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png")

# ------------------ Create Target ------------------
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(${PROJECT_NAME}
        MANUAL_FINALIZATION
        ${PROJECT_FILES}
        resources.qrc
        ${app_icon_resource_linux}
    )
else()
    if(ANDROID)
        add_library(${PROJECT_NAME} SHARED ${PROJECT_FILES})
    else()
        add_executable(${PROJECT_NAME} ${PROJECT_FILES})
    endif()
endif()

# ------------------ Application icon for windows ------------------
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
        RC_COMPILE_DEFINITIONS "ICON_PATH=\"${CMAKE_SOURCE_DIR}/resources/icon.ico\""
    )
endif()

# ------------------ Library Linking ------------------
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/libs
)

add_library(StripCppComments OBJECT
    ${PROJECT_SOURCE_DIR}/libs/StripCppComments/CommentStripper.cpp
    ${PROJECT_SOURCE_DIR}/libs/StripCppComments/CommentStripper.h
)
target_include_directories(StripCppComments PRIVATE ${PROJECT_SOURCE_DIR}/libs/StripCppComments)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::WebEngineWidgets
        StripCppComments
)

# ------------------ QCodeEditor: patch upstream sources in place ------------------
# libs/QCodeEditor is a git submodule pointing at a third-party upstream
# (ArsMasiuk/QCodeEditor) that we don't maintain, so instead of forking it we
# patch its known-broken bits here at configure time, mirroring the approach
# already used above for uchardet's CMakeLists.txt. Patches are idempotent
# (string(REPLACE) is a no-op, and the file is only rewritten, if the old text
# is no longer present - e.g. already patched, or fixed upstream).
set(QCE_INTERNAL_DIR "${PROJECT_SOURCE_DIR}/libs/QCodeEditor/src/internal")

# QXmlStreamReader::name() / QXmlStreamAttributes::value() return QStringView.
# Comparing a QStringView directly against a string literal is ambiguous under
# Qt 6.4 (multiple candidate implicit QChar conversions) and fails to compile.
# Fix: compare against QString via .toString() instead.
set(QCE_SYNTAXSTYLE_CPP "${QCE_INTERNAL_DIR}/QSyntaxStyle.cpp")
if(EXISTS "${QCE_SYNTAXSTYLE_CPP}")
    file(READ "${QCE_SYNTAXSTYLE_CPP}" _qce_content)
    set(_qce_original "${_qce_content}")
    string(REPLACE "reader.name() == \"style-scheme\""
                    "reader.name().toString() == \"style-scheme\""
                    _qce_content "${_qce_content}")
    string(REPLACE "reader.name() == \"style\")"
                    "reader.name().toString() == \"style\")"
                    _qce_content "${_qce_content}")
    string(REPLACE "attributes.value(\"bold\") == \"true\")"
                    "attributes.value(\"bold\").toString() == \"true\")"
                    _qce_content "${_qce_content}")
    string(REPLACE "attributes.value(\"italic\") == \"true\")"
                    "attributes.value(\"italic\").toString() == \"true\")"
                    _qce_content "${_qce_content}")
    string(REPLACE "auto underline = attributes.value(\"underlineStyle\");"
                    "auto underline = attributes.value(\"underlineStyle\").toString();"
                    _qce_content "${_qce_content}")
    if(NOT _qce_content STREQUAL _qce_original)
        message(STATUS "Patching QCodeEditor/QSyntaxStyle.cpp (ambiguous QStringView comparisons)")
        file(WRITE "${QCE_SYNTAXSTYLE_CPP}" "${_qce_content}")
    endif()
endif()

set(QCE_LANGUAGE_CPP "${QCE_INTERNAL_DIR}/QLanguage.cpp")
if(EXISTS "${QCE_LANGUAGE_CPP}")
    file(READ "${QCE_LANGUAGE_CPP}" _qcl_content)
    set(_qcl_original "${_qcl_content}")
    string(REPLACE "reader.name() == \"section\""
                    "reader.name().toString() == \"section\""
                    _qcl_content "${_qcl_content}")
    string(REPLACE "reader.name() == \"name\")"
                    "reader.name().toString() == \"name\")"
                    _qcl_content "${_qcl_content}")
    if(NOT _qcl_content STREQUAL _qcl_original)
        message(STATUS "Patching QCodeEditor/QLanguage.cpp (ambiguous QStringView comparisons)")
        file(WRITE "${QCE_LANGUAGE_CPP}" "${_qcl_content}")
    endif()
endif()

# ------------------ QCodeEditor ------------------
# The vendored example/demo app isn't part of STC_editor itself and its
# MainWindow.cpp uses the deprecated QCheckBox::stateChanged(int) signal,
# producing warnings on every build for no benefit to us - skip building it.
set(BUILD_EXAMPLE OFF CACHE BOOL "Skip QCodeEditor's demo app" FORCE)
add_subdirectory(libs/QCodeEditor)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        QCodeEditor
)

# Override the standard if their CMake sets -ansi or an old standard
target_compile_features(QCodeEditor PUBLIC cxx_std_23)

# libs/QCodeEditor's own CMakeLists.txt unconditionally adds "-ansi" to its
# PRIVATE compile options. For GCC/Clang, "-ansi" is equivalent to
# "-std=c++98", and since compiler flags are resolved left-to-right, it is
# emitted *after* our "-std=gnu++23" on the command line and silently wins,
# putting the whole library back into C++98 mode (breaking nullptr, override,
# std::true_type, etc. inside Qt's generated moc code). Strip it out so the
# C++23 requirement above actually takes effect.
get_target_property(QCODEEDITOR_COMPILE_OPTIONS QCodeEditor COMPILE_OPTIONS)
if(QCODEEDITOR_COMPILE_OPTIONS)
    list(REMOVE_ITEM QCODEEDITOR_COMPILE_OPTIONS "-ansi")
    set_target_properties(QCodeEditor PROPERTIES COMPILE_OPTIONS "${QCODEEDITOR_COMPILE_OPTIONS}")
endif()

# ------------------ uchardet ------------------
if(UCHARDET_INCLUDE_DIR)
    target_include_directories(${PROJECT_NAME} PRIVATE "${UCHARDET_INCLUDE_DIR}")
endif()

if(UCHARDET_LIB_NAME)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${UCHARDET_LIB_NAME})
endif()

if(TARGET uchardet_headers)
    target_link_libraries(${PROJECT_NAME} PRIVATE uchardet_headers)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE Nuspell::nuspell)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    DICTIONARY_PATH="${CMAKE_CURRENT_SOURCE_DIR}/dictionaries/pl/pl_PL.aff"
)

# ------------------ Target Properties ------------------
if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.Cpp0x)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# ------------------ Installation ------------------
include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(FILES
    ${PROJECT_SOURCE_DIR}/dictionaries/pl/pl_PL.aff
    ${PROJECT_SOURCE_DIR}/dictionaries/pl/pl_PL.dic
    DESTINATION ${CMAKE_INSTALL_DATADIR}/STC_editor/dictionaries/pl
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()

# ------------------ Translation Processing ------------------
# Custom target to update translation files
add_custom_target(update_translations
    COMMAND Qt${QT_VERSION_MAJOR}::lupdate ${PROJECT_SOURCES} -ts ${TRANSLATION_FILES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Updating translation files"
)

# Custom target to compile translation files
add_custom_target(compile_translations
    COMMAND Qt${QT_VERSION_MAJOR}::lrelease ${TRANSLATION_FILES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Compiling translation files"
)

# ------------------ Tests (Linux only) ------------------
if(NOT WIN32)
    enable_testing()
    find_package(GTest REQUIRED)
    include_directories(${GTEST_INCLUDE_DIRS})

    set(TEST_SOURCES
        tests/PairedTagsCheckerTests.cpp
    )

    add_executable(${PROJECT_NAME}Tests
        ${TEST_SOURCES}
        checkers/PairedTagsChecker.cpp
    )

    target_link_libraries(${PROJECT_NAME}Tests PRIVATE ${GTEST_LIBRARIES} pthread gtest_main)
    add_test(NAME ${PROJECT_NAME}Tests COMMAND ${PROJECT_NAME}Tests)
endif()

# ------------------ AddressSanitizer: static runtime (avoid LD_PRELOAD ordering issues) ------------------
# When ASan is enabled - e.g. via Qt Creator's Projects > Build Settings > "Sanitizers"
# panel, or manually with -DCMAKE_CXX_FLAGS=-fsanitize=address - the dynamic ASan
# runtime (libasan.so) has to be the very first shared library the dynamic linker
# loads, or the binary aborts at startup with:
#   "ASan runtime does not come first in initial library list; you should either
#    link runtime to your application or manually preload it with LD_PRELOAD."
# Statically linking the runtime into the executable sidesteps that requirement
# entirely, so this is applied automatically whenever ASan is detected, regardless
# of whether it was turned on via CMAKE_CXX_FLAGS or CMAKE_C_FLAGS.
if(CMAKE_CXX_FLAGS MATCHES "-fsanitize=address" OR CMAKE_C_FLAGS MATCHES "-fsanitize=address")
    message(STATUS "AddressSanitizer detected - linking its runtime statically (-static-libasan)")
    target_link_options(${PROJECT_NAME} PRIVATE -static-libasan)
    if(TARGET ${PROJECT_NAME}Tests)
        target_link_options(${PROJECT_NAME}Tests PRIVATE -static-libasan)
    endif()
endif()
