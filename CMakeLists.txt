cmake_minimum_required(VERSION 3.10)
project(OpenGL_Project C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(OpenGL_GL_PREFERENCE GLVND)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Force small binary size optimizations and strip debug symbols in Release mode
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -s")

# 1. Locate system dependencies FIRST
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

include_directories(include)

# 2. Explicitly track your source files (keeps compilation snappy and reliable)
set(SOURCES
    src/main.cpp
    src/EBO.cpp
    src/glad.c
    src/shaderclass.cpp
    src/VAO.cpp
    src/VBO.cpp
    src/UI/UIElements.cpp
)

# 3. Create the executable target using the verified sources
add_executable(opengl_app ${SOURCES})

# 4. Safely link your graphics backends
target_link_libraries(opengl_app glfw OpenGL::GL ${CMAKE_DL_LIBS})

# 5. Automatically package your assets
add_custom_command(
    TARGET opengl_app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    "${CMAKE_CURRENT_BINARY_DIR}/shaders"
    COMMENT "Copying shaders to the build directory"
)
