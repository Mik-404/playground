project(00-aplusb)


set(00-source-files "${PROJECT_SOURCE_DIR}/Foo.cpp" CACHE INTERNAL "")
 
add_library(
    00-aplusb SHARED ${00-source-files} ${Testing_SOURCE_DIR}/include/Foo.h
)
