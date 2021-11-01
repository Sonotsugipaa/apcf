# Declare the project

	cmake_minimum_required(VERSION 3.20)
	project(yaconfig
		VERSION "0.1.0"
		DESCRIPTION "Yet Another CONFIGuration format library, designed to be user friendly.")


# Set the project's structure

	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib-runtime)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

	include_directories(include)
	link_directories(lib)
	link_directories(bin)


# Set warning levels

	if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
		add_compile_options(-Wall -Wextra -Wpedantic)
	elseif(${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
		add_compile_options(\Wall)
	else()
		message(WARNING "Toolchain \"${CMAKE_CXX_COMPILER_ID}\" not recognized by the project")
	endif()


# Define the C++ standard to use

	set(CMAKE_CXX_STANDARD 20)
	set(CMAKE_CXX_STANDARD_REQUIRED True)


# Setup targets

	add_library(yaconfig SHARED src/yaconfig.cpp)
	set_target_properties(
		yaconfig PROPERTIES
		VERSION "${PROJECT_VERSION}"
		SOVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")


# Add unit test module

	add_subdirectory(test)
