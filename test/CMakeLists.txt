include_directories(..)
include_directories(.)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/test)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/test)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/test)


add_library(test-tools STATIC
	test_tools.cpp)

add_executable(UnitTest-yaconfig yaconfig-test.cpp)
target_link_libraries(UnitTest-yaconfig test-tools yaconfig)

add_executable(UnitTest-perf yaconfig-perftest.cpp)
target_link_libraries(UnitTest-perf test-tools yaconfig)


configure_file(
	"${PROJECT_SOURCE_DIR}/test/run-tests.sh"
	"${PROJECT_BINARY_DIR}/${file}"
	COPYONLY)
