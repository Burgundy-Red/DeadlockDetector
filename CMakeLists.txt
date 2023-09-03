cmake_minimum_required(VERSION 3.5)

project(ddetector)

set(CMAKE_CXX_STANDARD 14)

add_executable(${PROJECT_NAME}
                main.cpp
                LockManager.cpp)
target_link_libraries(${PROJECT_NAME} pthread)
