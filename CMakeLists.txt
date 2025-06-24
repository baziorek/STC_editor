cmake_minimum_required(VERSION 3.15)

project(Cpp0x VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

include_directories(${PROJECT_SOURCE_DIR}) # to make sure that promoted types are visible from generated designer's files

set(PROJECT_SOURCES
    main.cpp
    ui/mainwindow.h ui/mainwindow.cpp ui/mainwindow.ui
    ui/finddialog.h ui/finddialog.ui ui/finddialog.cpp
    ui/errorlist.h ui/errorlist.cpp ui/errorlist.ui
    ui/gotolinewidget.h ui/gotolinewidget.cpp ui/gotolinewidget.ui
    ui/stctagsbuttons.h ui/stctagsbuttons.cpp ui/stctagsbuttons.ui
    linenumberarea.h linenumberarea.cpp
    codeeditor.h codeeditor.cpp
    stcsyntaxhighlighter.h stcsyntaxhighlighter.cpp
    checkers/PairedTagsChecker.h checkers/PairedTagsChecker.cpp
    types/stcTags.h types/stcTags.cpp
)

set(TEXT_FILES
    LICENSE
    README.md
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

target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

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

add_executable(Cpp0xTests
    ${TEST_SOURCES}
    checkers/PairedTagsChecker.cpp
)

target_link_libraries(Cpp0xTests PRIVATE ${GTEST_LIBRARIES} pthread gtest_main)

add_test(NAME Cpp0xTests COMMAND Cpp0xTests)
