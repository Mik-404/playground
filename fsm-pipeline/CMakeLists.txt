cmake_minimum_required(VERSION 3.28)

project(FormalPain LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add_compile_options(-g)
# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)
# add_compile_options(-fsanitize=undefined)
# add_link_options(-fsanitize=undefined)
# add_compile_options(-fno-omit-frame-pointer)

include(FetchContent)

FetchContent_Declare(
  Boost
  URL https://github.com/boostorg/boost/releases/download/boost-1.89.0/boost-1.89.0-cmake.tar.xz
  URL_HASH SHA256=67acec02d0d118b5de9eb441f5fb707b3a1cdd884be00ca24b9a73c995511f74
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

set(BOOST_INCLUDE_LIBRARIES graph program_options json)
FetchContent_MakeAvailable(Boost)

add_library(utils STATIC
  ${CMAKE_SOURCE_DIR}/src/regex2eps_nfa.cpp
  ${CMAKE_SOURCE_DIR}/src/eps_nfa2nfa.cpp
  ${CMAKE_SOURCE_DIR}/src/nfa2dfa.cpp
  ${CMAKE_SOURCE_DIR}/src/dfa2min_dfa.cpp
  ${CMAKE_SOURCE_DIR}/src/min_dfa2regex.cpp
  ${CMAKE_SOURCE_DIR}/src/graph_type.cpp
)

target_include_directories( utils PUBLIC
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(utils PUBLIC Boost::graph Boost::program_options Boost::json)


add_executable(my_program ${CMAKE_SOURCE_DIR}/src/main.cpp)

target_link_libraries(my_program PRIVATE utils)