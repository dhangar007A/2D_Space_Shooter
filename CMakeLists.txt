cmake_minimum_required(VERSION 3.10)
project(NebulDefender CXX)

set(CMAKE_CXX_STANDARD 11)

# Find OpenGL and GLUT
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

add_executable(nebula_defender main.cpp)

target_include_directories(nebula_defender PRIVATE
    ${OPENGL_INCLUDE_DIRS}
    ${GLUT_INCLUDE_DIRS}
)

target_link_libraries(nebula_defender
    ${OPENGL_LIBRARIES}
    ${GLUT_LIBRARIES}
)

# Windows-specific: copy freeglut DLL next to executable
if(WIN32)
    message(STATUS "Windows build: make sure freeglut.dll is in the exe directory")
endif()
