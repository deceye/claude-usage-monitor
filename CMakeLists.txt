cmake_minimum_required(VERSION 3.22)
project(ClaudeUsageMonitor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

set(SOURCES
    src/main.cpp
    src/OverlayWindow.cpp
    src/SystemTrayIcon.cpp
    src/ClaudeApiClient.cpp
    src/SettingsDialog.cpp
    src/SecureStorage.cpp
    src/resources/resources.qrc
)

if(WIN32)
    list(APPEND SOURCES src/resources/app.rc)
    set_source_files_properties(src/resources/app.rc PROPERTIES
        COMPILE_FLAGS "-I${CMAKE_SOURCE_DIR}/src/resources"
    )
endif()

qt_add_executable(ClaudeUsageMonitor ${SOURCES})

target_link_libraries(ClaudeUsageMonitor PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

if(WIN32)
    set_target_properties(ClaudeUsageMonitor PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    target_link_libraries(ClaudeUsageMonitor PRIVATE crypt32)
endif()
