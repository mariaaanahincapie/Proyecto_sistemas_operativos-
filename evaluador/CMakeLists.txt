cmake_minimum_required(VERSION 3.16)

project(evaluador LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

find_package(Threads REQUIRED)

file(GLOB COMMON_SRC
    src/shared/*.cpp
    src/queues/*.cpp
    src/concurrency/*.cpp)

# Añade cada programa cuando exista su archivo principal.
function(add_prog name main)
    if(EXISTS ${CMAKE_SOURCE_DIR}/${main})
        add_executable(${name} ${main} ${COMMON_SRC})
        target_link_libraries(${name} Threads::Threads)
        if(UNIX AND NOT APPLE)
            target_link_libraries(${name} rt)
        endif()
    endif()
endfunction()

add_prog(evaluator       src/main/evaluator.cpp)
add_prog(evaluator-init  src/evaluator/init.cpp)
add_prog(evaluator-reg   src/registration/reg.cpp)
add_prog(evaluator-ctrl  src/main/ctrl.cpp)
add_prog(evaluator-rep   src/output/rep.cpp)
