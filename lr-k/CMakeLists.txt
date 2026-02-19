cmake_minimum_required(VERSION 3.28)

set(CMAKE_C_COMPILER "gcc")
set(CMAKE_CXX_COMPILER "g++")

project(LRkParser LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(googletest GIT_REPOSITORY https://github.com/google/googletest.git
                         GIT_TAG v1.16.0)
FetchContent_Declare(
    argparse GIT_REPOSITORY https://github.com/p-ranav/argparse.git
    GIT_TAG v3.2
)
FetchContent_MakeAvailable(argparse)
FetchContent_MakeAvailable(googletest)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(ENABLE_SANITIZER "Enable Address and Undefined Behavior Sanitizers" OFF)
option(ENABLE_COVERAGE "Enable code coverage" OFF)
set(K "1" CACHE STRING "Lookahead depth for the parser")

if(ENABLE_SANITIZER)
    message(STATUS "Sanitizers enabled! (Address + Undefined)")

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
        add_link_options(-fsanitize=address,undefined)
    else()
        message(WARNING "Sanitizers are mainly supported for GCC/Clang on Linux")
    endif()
endif()

add_library(Builder STATIC
  ${CMAKE_SOURCE_DIR}/src/grammar.cpp
  ${CMAKE_SOURCE_DIR}/src/utils.cpp
)

target_include_directories(Builder PUBLIC
  ${CMAKE_SOURCE_DIR}/include
)

add_executable(Compiler ${CMAKE_SOURCE_DIR}/src/compiler.cpp)

target_link_directories(Compiler PRIVATE
    ${CMAKE_SOURCE_DIR}/include    
)

target_link_libraries(Compiler PRIVATE argparse Builder)

target_compile_definitions(Compiler PRIVATE 
    K=${K}
)

add_executable(Parser ${CMAKE_SOURCE_DIR}/src/parser.cpp)

target_link_directories(Parser PRIVATE
    ${CMAKE_SOURCE_DIR}/include    
)

target_link_libraries(Parser PRIVATE argparse Builder)

target_compile_definitions(Parser PRIVATE 
    K=${K}
)

enable_testing()

add_executable(AllTests
  ${CMAKE_SOURCE_DIR}/tests/test_grammar.cpp
  ${CMAKE_SOURCE_DIR}/tests/test_first.cpp
  ${CMAKE_SOURCE_DIR}/tests/test_builder.cpp
  ${CMAKE_SOURCE_DIR}/tests/test_io.cpp
  ${CMAKE_SOURCE_DIR}/tests/test_parser.cpp
)

target_link_libraries(AllTests
  PRIVATE Builder gtest_main gtest gmock pthread
)

target_compile_definitions(AllTests PRIVATE 
    TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data"
)

add_test(NAME Tests COMMAND AllTests)

# target_link_libraries(utils PUBLIC Boost::graph Boost::program_options Boost::json)


# add_executable(my_program ${CMAKE_SOURCE_DIR}/src/main.cpp)

# target_link_libraries(my_program PRIVATE utils)

if(ENABLE_COVERAGE)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(STATUS "Code coverage enabled for GCC/Clang.")

        set(COVERAGE_FLAGS "-g -O0 -fprofile-arcs -ftest-coverage -fno-inline")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
        
        add_custom_target(coverage
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
            COMMAND gcovr --root ${CMAKE_SOURCE_DIR}
                          --filter "${CMAKE_SOURCE_DIR}/src/"
                          --filter "${CMAKE_SOURCE_DIR}/include/" 
                          --exclude "${CMAKE_SOURCE_DIR}/src/parser.cpp"
                          --exclude "${CMAKE_SOURCE_DIR}/src/compiler.cpp"
                          --print-summary
                          --html-details coverage/coverage_report.html
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS AllTests
            COMMENT "Generating code coverage report..."
        )
    else()
        message(WARNING "Code coverage is only supported for GCC and Clang.")
    endif()
endif()