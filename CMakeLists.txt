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
    # Wyłącz budowanie CLI i budowanie jako shared
    set(BUILD_BINARY OFF CACHE BOOL "Do not build uchardet CLI tool" FORCE)
    # set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build uchardet as static library" FORCE)
    FetchContent_Declare(
        uchardet
        GIT_REPOSITORY https://gitlab.freedesktop.org/uchardet/uchardet.git
        GIT_TAG master
    )
    FetchContent_Populate(uchardet)
    # Patch: zakomentuj add_subdirectory(test) w CMakeLists.txt uchardet
    file(READ "${uchardet_SOURCE_DIR}/CMakeLists.txt" _uchardet_cmake)
    string(REPLACE "add_subdirectory(test)" "# add_subdirectory(test)" _uchardet_cmake "${_uchardet_cmake}")
    file(WRITE "${uchardet_SOURCE_DIR}/CMakeLists.txt" "${_uchardet_cmake}")
    add_subdirectory(${uchardet_SOURCE_DIR} ${uchardet_BINARY_DIR})

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

# ------------------ Nuspell (system or FetchContent) ------------------
# --- Try to find system Nuspell ---
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

    # Ustaw alias, by mieć zawsze Nuspell::nuspell
    if(NOT TARGET Nuspell::nuspell)
        add_library(Nuspell::nuspell ALIAS nuspell)
    endif()
endif()

# ------------------ Źródła projektu ------------------
include_directories(${PROJECT_SOURCE_DIR}) # dla plików .ui

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
    widgets/FilteredTagTableWidget.h widgets/FilteredTagTableWidget.cpp
    widgets/CodeBlocksTableWidget.h widgets/CodeBlocksTableWidget.cpp
    widgets/TodosTrackerTableWidget.h widgets/TodosTrackerTableWidget.cpp
    widgets/LoginDialog.h widgets/LoginDialog.cpp
    widgets/StcPreview.h widgets/StcPreview.cpp
    widgets/DiffViewerWidget.h widgets/DiffViewerWidget.cpp
    widgets/DiffReviewDialog.h widgets/DiffReviewDialog.cpp
    widgets/BreadcrumbTextBrowser.h widgets/BreadcrumbTextBrowser.cpp
    widgets/RenameFileDialog.h widgets/RenameFileDialog.cpp widgets/RenameFileDialog.ui

    utils/DiffCalculation.h utils/DiffCalculation.cpp
    utils/FileEncodingHandler.h utils/FileEncodingHandler.cpp
    utils/STCSyntaxHighlighter.h utils/STCSyntaxHighlighter.cpp
    utils/SpellChecker.h utils/SpellChecker.cpp
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

set(PROJECT_FILES ${PROJECT_SOURCES} ${TEXT_FILES})

set(app_icon_resource_linux "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png")

# ------------------ Tworzenie targetu ------------------
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

# ------------------ Linkowanie bibliotek ------------------
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/libs
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::WebEngineWidgets
)

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

# ------------------ Właściwości targetu ------------------
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

# ------------------ Instalacja ------------------
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

# ------------------ Testy (Linux tylko) ------------------
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
