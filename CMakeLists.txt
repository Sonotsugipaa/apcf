# Declare the project

	cmake_minimum_required(VERSION 3.20)
	project(APCF
		VERSION "0.2.2"
		DESCRIPTION "Almost Plain-text Configuration Format library, designed to be user friendly.")


# Set the project's structure

	set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib-runtime)
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)

	include_directories(include)
	link_directories(lib bin)


# Set warning levels

	if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
		add_compile_options(-Wall -Wextra -Wpedantic)
	elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
		# add_compile_options(\\Wall) # How...?
	else()
		message(WARNING "Toolchain \"${CMAKE_CXX_COMPILER_ID}\" not recognized by the project")
	endif()


# Define the C++ standard to use

	set(CMAKE_CXX_STANDARD 20)
	set(CMAKE_CXX_STANDARD_REQUIRED True)


# Setup targets

	# Windows needs a lot of fluff in order to create a DLL, and I'm not going to copy paste base64-like macros everywhere if I'm targeting Linux.
	# Solution: Windows uses a static build, while Linux (and other unsupported environments) use shared builds.
	# Windows is already bloated all the way down to hell, anyway.
	if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
		set(APCF_LIB_TYPE STATIC)
	else()
		set(APCF_LIB_TYPE SHARED)
	endif()
	add_library(apcf ${APCF_LIB_TYPE}
		src/apcf.cpp
		src/apcf_io.cpp
		src/apcf_config.cpp
		src/apcf_hierarchy.cpp
		src/apcf_util.cpp
		src/apcf_num.cpp
		src/apcf_parse.cpp
		src/apcf_serialize.cpp )

	set_target_properties(
		apcf PROPERTIES
		VERSION "${PROJECT_VERSION}" )

	set_property(TARGET apcf PROPERTY UNITY_BUILD true)
	set_property(TARGET apcf PROPERTY UNITY_BUILD_BATCH_SIZE 32) # Way more time is spent compiling STL templates than actually using them


# Add unit test module

	add_subdirectory(test)


# Setup `apcfConfig.cmake` holy fuck this is copypasty

add_library(apcf::apcf ALIAS apcf)

include(GNUInstallDirs)
install(
	TARGETS apcf
	EXPORT apcfTargets
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} )

install(
	EXPORT apcfTargets
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/apcf
	NAMESPACE apcf::apcf
	FILE apcfTargets.cmake )

include(CMakePackageConfigHelpers)
configure_package_config_file(
	"Config.cmake.in"
	"apcfConfig.cmake"
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/apcf
	PATH_VARS CMAKE_INSTALL_LIBDIR )

write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/apcfConfigVersion.cmake
	VERSION "${PROJECT_VERSION}"
	COMPATIBILITY SameMajorVersion )

install(
	FILES
		"${CMAKE_CURRENT_BINARY_DIR}/apcfConfig.cmake"
		"${CMAKE_CURRENT_BINARY_DIR}/apcfConfigVersion.cmake"
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/apcf" )
