cmake_minimum_required(VERSION 3.20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(App LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE Release)

include(FetchContent)

FetchContent_Declare(
    glm
    GIT_REPOSITORY  https://github.com/g-truc/glm.git
    GIT_TAG         1.0.1
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY  https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG         v2.0.0rc13
)
FetchContent_MakeAvailable(tinyobjloader)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE 
)
FetchContent_GetProperties(stb)
if(NOT stb_POPULATED)
    FetchContent_Populate(stb)
    add_library(stb INTERFACE)
    target_include_directories(stb INTERFACE ${stb_SOURCE_DIR})
    add_library(stb::stb ALIAS stb)
endif()

FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY  https://github.com/syoyo/tinygltf.git
    GIT_TAG         v3.0.1
)
FetchContent_GetProperties(tinygltf)
if(NOT tinygltf_POPULATED)
    FetchContent_Populate(tinygltf)
    add_library(tinygltf STATIC ${tinygltf_SOURCE_DIR}/tiny_gltf_v3.c)
    target_include_directories(tinygltf PUBLIC ${tinygltf_SOURCE_DIR})
    target_compile_definitions(tinygltf PUBLIC
        TINYGLTF3_ENABLE_FS
        TINYGLTF3_ENABLE_STB_IMAGE
    )
    target_link_libraries(tinygltf PRIVATE stb::stb)
endif()

add_executable(App 
    main.cpp
    misc.h
    application.h
    application.cpp
    plane.h
    plane.cpp
    glb_reader.h
    glb_reader.cpp
    bindgroup_manager.h
)

set_property(TARGET App APPEND PROPERTY 
    LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/shader.wgsl"
)

if(EMSCRIPTEN)
    execute_process(
        COMMAND embuilder build emdawnwebgpu
    )

    target_include_directories(App PRIVATE
        "${EMSCRIPTEN_ROOT_PATH}/cache/ports/emdawnwebgpu/emdawnwebgpu_pkg/webgpu/include"
        "${EMSCRIPTEN_ROOT_PATH}/cache/ports/emdawnwebgpu/emdawnwebgpu_pkg/webgpu_cpp/include"
    )

    target_compile_options(App PRIVATE
        "--use-port=emdawnwebgpu"
    )

    target_link_options(App PRIVATE
        "--use-port=emdawnwebgpu"
        "-sUSE_GLFW=3"
        "-sASYNCIFY"
        "-sFETCH=1"
        "-sASSERTIONS=1"
        "-sINITIAL_MEMORY=33554432"
        "--preload-file" "${CMAKE_CURRENT_SOURCE_DIR}/resources@resources"
        "--shell-file" "${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
    )

    target_link_libraries(App PRIVATE 
        glm::glm 
        tinyobjloader 
        stb::stb 
        tinygltf
    )

    target_compile_definitions(App PRIVATE
        RESOURCE_DIR="resources"
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        GLM_FORCE_LEFT_HANDED
    )
    set_target_properties(App PROPERTIES SUFFIX ".html")
else()
    message(FATAL_ERROR "Ten projekt wymaga kompilacji przez Emscripten (użyj emcmake cmake...)")
endif()