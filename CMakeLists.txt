cmake_minimum_required(VERSION 3.25)
project(raytracer)

set(CMAKE_CXX_STANDARD 17)

find_package(glm CONFIG REQUIRED)
add_executable(raytracer main.cpp)
target_link_libraries(raytracer PRIVATE glm::glm)
