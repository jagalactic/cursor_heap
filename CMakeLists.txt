
cmake_minimum_required(VERSION 3.1)

project(libcursorheap)

##
### Source definitions ###
##

include_directories("${PROJECT_SOURCE_DIR}/include")

file(GLOB sources "${PROJECT_SOURCE_DIR}/src/*.c")

#add_executable(testy ${sources})
add_library(cheap_dax cursor_heap.c cheap_dax.c )
