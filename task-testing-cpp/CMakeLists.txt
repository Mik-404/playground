cmake_minimum_required(VERSION 3.8)
project(Testing CXX)

include(ExternalProject)
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" ${CMAKE_SOURCE_DIR}/cmake)

set(EXTERNAL_INSTALL_LOCATION ${Testing_SOURCE_DIR}/3rdparty)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${Testing_SOURCE_DIR}/bin)

SET(CMAKE_CXX_FLAGS "-g -O0 -fprofile-arcs -ftest-coverage --coverage")
SET(CMAKE_C_FLAGS "-g -O0 -fprofile-arcs -ftest-coverage --coverage")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")

option(NEW_LCOV "Enable new LCOV VERSION" OFF)

if (NEW_LCOV) 
    list(APPEND LCOV_IGNORED "--ignore-errors;unused")
endif()

if (APPLE)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17") 
endif()

SET(CPR_USE_SYSTEM_GTEST ON)

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG main
)

if (APPLE)

	set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl@1.1" CACHE INTERNAL "")
	set(OPENSSL_LIBRARIES "/usr/local/opt/openssl@1.1/lib" CACHE INTERNAL "")
    FetchContent_Declare(
		cpr_repo
		GIT_REPOSITORY https://github.com/libcpr/cpr.git
        GIT_TAG 11e5954
	)
else()
    FetchContent_Declare(
		cpr_repo
		GIT_REPOSITORY https://github.com/libcpr/cpr.git
        GIT_TAG 11e5954
	)
endif()

FetchContent_MakeAvailable(cpr_repo googletest)

include_directories(${EXTERNAL_INSTALL_LOCATION}/include)
link_directories(${EXTERNAL_INSTALL_LOCATION}/lib)
include_directories(${CPR_INCLUDE_DIRS})



include(CodeCoverage)
add_custom_target(coverage_report)

function(run_coverage test_sources sources coverage_target target_name coverage_name)
    message("INFO test sources ${test_sources}")
    set(test_name "${target_name}-test")
    message("INFO sources ${sources}")
    add_executable(${test_name} ${test_sources} ${sources})


    target_link_libraries(
		${test_name} gtest_main gtest gmock Threads::Threads cpr curl
	)

    SETUP_TARGET_FOR_COVERAGE(
            ${coverage_target} tests/${target_name} ${test_name} ${coverage_name}
    )

    add_dependencies(coverage_report ${coverage_target})
endfunction()


include_directories(include)

add_subdirectory(src/00-aplusb)
add_subdirectory(src/01-simple-library)
add_subdirectory(src/02-tree)
add_subdirectory(src/03-weather)
add_subdirectory(examples)

add_subdirectory(tests)


