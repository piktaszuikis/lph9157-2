cmake_minimum_required(VERSION 2.8)
macro(use_c99)
	if (CMAKE_VERSION VERSION_LESS "3.1")
		set (CMAKE_C_FLAGS "--std=gnu99 ${CMAKE_C_FLAGS}")
	else ()
		set (CMAKE_C_STANDARD 99)
	endif ()
endmacro(use_c99)

project(lph9157-2)
use_c99()

add_executable(${PROJECT_NAME} "main.c")

find_library(SOC_LIB soc)
target_link_libraries(${PROJECT_NAME} "${SOC_LIB}")
