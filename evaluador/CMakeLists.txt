cmake_minimum_required(VERSION 3.16)
project(evaluador LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Los ejecutables quedan en bin/ (también con generadores multi-configuración como Visual Studio).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER ${cfg} CFG)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${CFG} ${CMAKE_SOURCE_DIR}/bin)
endforeach()

find_package(Threads REQUIRED)

add_library(evaluator_common STATIC
    src/shared/platform.cpp
    src/shared/shm.cpp
    src/concurrency/semaphore.cpp
    src/queues/ring.cpp)
target_include_directories(evaluator_common PUBLIC src)
target_link_libraries(evaluator_common PUBLIC Threads::Threads)
if(UNIX AND NOT APPLE)
    target_link_libraries(evaluator_common PUBLIC rt)
endif()
if(WIN32)
    target_compile_definitions(evaluator_common PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
if(MSVC)
    target_compile_options(evaluator_common PUBLIC /W4 /utf-8)
else()
    target_compile_options(evaluator_common PUBLIC -Wall -Wextra -Wpedantic)
endif()

add_executable(evaluator      src/main/evaluator.cpp)
add_executable(evaluator-init src/evaluator/init.cpp src/evaluator/evaluator.cpp)
add_executable(evaluator-reg  src/registration/reg.cpp)
add_executable(evaluator-ctrl src/main/ctrl.cpp)
add_executable(evaluator-rep  src/output/rep.cpp)
foreach(prog evaluator evaluator-init evaluator-reg evaluator-ctrl evaluator-rep)
    target_link_libraries(${prog} PRIVATE evaluator_common)
endforeach()

install(TARGETS evaluator evaluator-init evaluator-reg evaluator-ctrl evaluator-rep RUNTIME DESTINATION bin)
