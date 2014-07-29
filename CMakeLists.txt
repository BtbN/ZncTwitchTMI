cmake_minimum_required(VERSION 2.8.12)

project(znctwitch)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules/")

find_package(ZNC REQUIRED)

set(twitch_SOURCES
	twitchtmi.cpp)

add_library(twitch MODULE
	${twitch_SOURCES})
target_link_libraries(twitch
	${ZNC_LIBRARIES})
target_include_directories(twitch SYSTEM
	PRIVATE ${ZNC_INCLUDE_DIRS})
target_compile_options(twitch
	PRIVATE ${ZNC_DEFINITIONS})

set_target_properties(twitch PROPERTIES
	PREFIX "")

