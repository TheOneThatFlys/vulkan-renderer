# CMakeList.txt : CMake project for VulkanRenderer, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)

set(CMAKE_CXX_SCAN_FOR_MODULES ON)

project ("VulkanRenderer")

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(Stb REQUIRED)


# set up Vulkan C++ module
add_library(VulkanCppModule)
add_library(Vulkan::cppm ALIAS VulkanCppModule)

target_compile_definitions(VulkanCppModule PUBLIC
        VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
        VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
        VULKAN_HPP_NO_SPACESHIP_OPERATOR=1
)
target_include_directories(VulkanCppModule
        PRIVATE
        "${Vulkan_INCLUDE_DIR}"
)
target_link_libraries(VulkanCppModule
        PUBLIC
        Vulkan::Vulkan
)

set_target_properties(VulkanCppModule PROPERTIES CXX_STANDARD 20)

target_sources(VulkanCppModule
        PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS
        "${Vulkan_INCLUDE_DIR}"
        FILES
        "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm"
)

# Add source to this project's executable.
add_executable (VulkanRenderer
        src/VulkanEngine.cpp
        src/VulkanEngine.h
        src/ControlledCameraSystem.cpp
        src/ControlledCameraSystem.h
        src/InputManager.cpp
        src/InputManager.h
        src/Common.h
        src/DebugWindow.cpp
        src/DebugWindow.h
        src/AssetManager.cpp
        src/AssetManager.h
        src/Logger.cpp
        src/Logger.h
        src/Mesh.cpp
        src/Mesh.h
        src/Vertex.h
        src/Texture.cpp
        src/Texture.h
        src/Material.cpp
        src/Material.h
        src/UniformBufferBlock.h
        src/ECS.h
        src/Components.h
        src/Renderer3D.cpp
        src/Renderer3D.h
        src/Pipeline.cpp
        src/Pipeline.h
        src/TransformSystem.cpp
        src/TransformSystem.h
        src/EntitySearcher.cpp
        src/EntitySearcher.h
        src/LightSystem.cpp
        src/LightSystem.h
        src/Volumes.h
        src/BoundingVolumeRenderer.cpp
        src/BoundingVolumeRenderer.h
)

set_property(TARGET VulkanRenderer PROPERTY CXX_STANDARD 20)

target_link_libraries(VulkanRenderer PRIVATE glfw)
target_link_libraries(VulkanRenderer PRIVATE glm::glm)
target_link_libraries(VulkanRenderer PRIVATE imgui::imgui)
target_link_libraries(VulkanRenderer PRIVATE Vulkan::cppm)

find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
target_include_directories(VulkanRenderer PRIVATE ${TINYGLTF_INCLUDE_DIRS})
target_include_directories(VulkanRenderer PRIVATE ${Stb_INCLUDE_DIR})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET VulkanRenderer PROPERTY CXX_STANDARD 20)
endif()

# Compile the list of shaders
function(add_shaders TARGET_NAME FOLDER_PATH)
    set(SHADER_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    set(SHADER_SOURCES ${ARGN})
    set(SHADER_PRODUCTS)
    add_custom_command(
        OUTPUT ${SHADER_OUT_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_OUT_DIR}
    )

    foreach(SHADER_SOURCE IN LISTS SHADER_SOURCES)
        get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME)
        set(SHADER_OUTPUT_PATH "${CMAKE_CURRENT_BINARY_DIR}/shaders/${SHADER_NAME}.spv")
        set(SHADER_INPUT_PATH "${FOLDER_PATH}/${SHADER_SOURCE}")

        add_custom_command(OUTPUT ${SHADER_OUTPUT_PATH} 
            COMMAND glslc ${SHADER_INPUT_PATH} -o ${SHADER_OUTPUT_PATH}
            DEPENDS ${SHADER_INPUT_PATH}
            COMMENT "Compiling ${SHADER_NAME}"
        )
        list(APPEND SHADER_PRODUCTS ${SHADER_OUTPUT_PATH})
    endforeach()

    add_custom_target(${TARGET_NAME} ALL DEPENDS ${SHADER_PRODUCTS})
endfunction()

set(SHADER_FILES
        model.vert
        model.frag
        line.vert
        line.frag
)
add_shaders(Shaders ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders ${SHADER_FILES})
add_dependencies(VulkanRenderer Shaders)

# Copy assets folder
add_custom_target(Assets COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/assets)
add_dependencies(VulkanRenderer Assets)
