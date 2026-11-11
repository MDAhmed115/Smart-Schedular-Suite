cmake_minimum_required(VERSION 3.21)

project(SmartSchedulerSuite LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets Concurrent)

include_directories(${CMAKE_SOURCE_DIR})

# ---- Engine: pure C++, no Qt ----

file(GLOB ENGINE_SRC CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/engine/*.cpp"
)

if(ENGINE_SRC)
    add_library(engine STATIC ${ENGINE_SRC})
else()
    add_library(engine INTERFACE)
endif()

# ---- Main Qt application ----

file(GLOB GUI_SRC CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/gui/*.cpp"
)

file(GLOB GUI_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/gui/*.h"
    "${CMAKE_SOURCE_DIR}/gui/*.hpp"
)

add_executable(SmartScheduler ${GUI_SRC} ${GUI_HEADERS})

# Embed the application images in the Qt executable.
# Supports both resources.qrc and app_resources.qrc depending on file name.
if(EXISTS "${CMAKE_SOURCE_DIR}/resources.qrc")
    target_sources(SmartScheduler PRIVATE "${CMAKE_SOURCE_DIR}/resources.qrc")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/app_resources.qrc")
    target_sources(SmartScheduler PRIVATE "${CMAKE_SOURCE_DIR}/app_resources.qrc")
endif()

# Embed native Windows executable icon (.ico via .rc file).
if(WIN32)
    enable_language(RC)
    if(EXISTS "${CMAKE_SOURCE_DIR}/app_icon.rc")
        target_sources(SmartScheduler PRIVATE "${CMAKE_SOURCE_DIR}/app_icon.rc")
    endif()
endif()

target_link_libraries(SmartScheduler PRIVATE
    engine
    Qt6::Widgets
    Qt6::Concurrent
)

set_target_properties(SmartScheduler PROPERTIES
    WIN32_EXECUTABLE ON
)

# ---- Manual integration test ----

if(EXISTS "${CMAKE_SOURCE_DIR}/tests/main.cpp")
    add_executable(SmartSchedulerTest
        tests/main.cpp
    )
    target_link_libraries(SmartSchedulerTest PRIVATE engine)
endif()

# ---- Console tests ----

enable_testing()

foreach(t
    test_ds
    test_auto
    test_sorting
    test_tournament
    test_timetable
    test_study
    test_event
)
    if(EXISTS "${CMAKE_SOURCE_DIR}/tests/${t}.cpp")
        add_executable(${t} "tests/${t}.cpp")
        target_link_libraries(${t} PRIVATE engine)
        add_test(NAME ${t} COMMAND ${t})
    endif()
endforeach()