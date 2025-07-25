cmake_minimum_required(VERSION 3.20)
project(Cpp0x VERSION 0.2 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets WebView WebEngineWidgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets WebEngineWidgets)

##################### uchardet:
include(FetchContent)

# Try system installation
find_path(UCHARDET_INCLUDE_DIR uchardet/uchardet.h)
find_library(UCHARDET_LIBRARY NAMES uchardet)

if (UCHARDET_INCLUDE_DIR AND UCHARDET_LIBRARY)
    message(STATUS "Found system uchardet: ${UCHARDET_LIBRARY}")
    add_library(uchardet::uchardet UNKNOWN IMPORTED)
    set_target_properties(uchardet::uchardet PROPERTIES
        IMPORTED_LOCATION "${UCHARDET_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${UCHARDET_INCLUDE_DIR}"
    )
else()
    message(STATUS "uchardet not found – using FetchContent to download")

    FetchContent_Declare(
        uchardet
        GIT_REPOSITORY https://github.com/freedesktop/uchardet.git
        GIT_TAG        master
    )

    FetchContent_MakeAvailable(uchardet)

    # The built target from the original CMakeLists is `uchardet`
    add_library(uchardet::uchardet ALIAS uchardet)
endif()



# In Linux it is possible to have sanitizers for g++ and Clang compilers:
# address sanitizer is not compatible with QWebEngineWidgets
# if (NOT CMAKE_SYSTEM_NAME MATCHES "Windows" AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
#     include(CheckLibraryExists)

#     check_library_exists(asan __asan_init "" HAVE_LIBASAN)
#     check_library_exists(ubsan __ubsan_handle_add_overflow "" HAVE_LIBUBSAN)

#     if (HAVE_LIBASAN AND HAVE_LIBUBSAN)
#         message(STATUS "Sanitizers (AddressSanitizer + UndefinedBehaviorSanitizer) are available")
#         set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined")
#     else()
#         message(WARNING "No required libraries (libasan/libubsan). You can install them, e.g. for Fedora: sudo yum install libasan libubsan")
#     endif()
# endif()

include_directories(${PROJECT_SOURCE_DIR}) # to make sure that promoted types are visible from generated designer's files

set(PROJECT_SOURCES
    main.cpp
    CodeEditor.h CodeEditor.cpp
    stcsyntaxhighlighter.h stcsyntaxhighlighter.cpp
    stcsyntaxpatterns.h stcsyntaxpatterns.cpp

    ui/mainwindow.h ui/mainwindow.cpp ui/mainwindow.ui
    ui/finddialog.h ui/finddialog.ui ui/finddialog.cpp
    ui/errorlist.h ui/errorlist.cpp ui/errorlist.ui
    ui/gotolinewidget.h ui/gotolinewidget.cpp ui/gotolinewidget.ui
    ui/stctagsbuttons.h ui/stctagsbuttons.cpp ui/stctagsbuttons.ui
    ui/shortcutsdialog.h ui/shortcutsdialog.cpp
    ui/highlightdelegate.h ui/highlightdelegate.cpp
    ui/cppcompilerdialog.h ui/cppcompilerdialog.cpp

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
    utils/diffcalculation.h utils/diffcalculation.cpp
    utils/FileEncodingHandler.h utils/FileEncodingHandler.cpp
)

set(TEXT_FILES
    LICENSE
    README.md
    README_ang.md
)

set(PROJECT_FILES ${PROJECT_SOURCES} ${TEXT_FILES})

set(app_icon_resource_linux "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png")


if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(${PROJECT_NAME}
        MANUAL_FINALIZATION
        ${PROJECT_FILES}
        resources.qrc
        ${app_icon_resource_linux}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET Cpp0x APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(${PROJECT_NAME} SHARED
            ${PROJECT_FILES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(${PROJECT_NAME}
            ${PROJECT_FILES}
        )
    endif()
endif()

target_include_directories(${PROJECT_NAME}
        PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/libs
)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::WebEngineWidgets)
target_link_libraries(${PROJECT_NAME} PRIVATE uchardet::uchardet)


# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
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

include(GNUInstallDirs)
install(TARGETS ${PROJECT_NAME}
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()

############ adding tests:
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

add_test(NAME Cpp0xTests COMMAND Cpp0xTests)
