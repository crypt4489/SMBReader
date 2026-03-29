cmake_minimum_required(VERSION 3.28)

set (PROJECT_TYPE_EXECUTABLE "exe")

set (PROJECT_TYPE_STATIC_LIBRARY "static-lib")

set (PROJECT_NAME "SMBReader")

project(${PROJECT_NAME} LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(vulkan)

set (PROJECT_INCLUDES "include/" 
"smb/include/" 
"vulkan/include/" 
"C:/VulkanSDK/1.3.290.0/Include/"
"platform/"
"platform/windows/include"
"common/include/"
)

set (PROJECT_LIBRARY_PATHS
 "C:/VulkanSDK/1.3.290.0/Lib/"
 "vulkan/lib/"
 "lib/windows/"
)

file(GLOB PROJECT_SOURCES "platform/windows/src/*.cpp" "src/*.cpp" "smb/src/*.cpp" "main.cpp") 

file(GLOB PROJECT_HEADERS "include/*.h" "smb/include/*.h" "platform/*.h" "platform/windows/include/*.h" "common/include/*.h")

file(GLOB PROJECT_SHADERS "shaders/layouts/*" "shaders/*.vert" "shaders/*.comp" "shaders/*.frag")

file(GLOB PROJECT_PIPELINE_LAYOUTS "pipelineLayout/*.xml")

file(GLOB PROJECT_ATTACHMENT_LAYOUTS "attachmentdescriptions/*.xml")

set (PROJECT_DEFINES)

set (PROJECT_TYPE "exe")

set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION_MAJOR 10)



if (${PROJECT_TYPE} STREQUAL ${PROJECT_TYPE_EXECUTABLE})
    add_executable(${PROJECT_NAME} ${PROJECT_SOURCES} ${PROJECT_HEADERS} ${PROJECT_SHADERS} ${PROJECT_PIPELINE_LAYOUTS} ${PROJECT_ATTACHMENT_LAYOUTS})
elseif (${PROJECT_TYPE} STREQUAL ${PROJECT_TYPE_STATIC_LIBRARY})
    add_library(${PROJECT_NAME} STATIC ${PROJECT_SOURCES} ${PROJECT_HEADERS} ${PROJECT_SHADERS} ${PROJECT_PIPELINE_LAYOUTS} ${PROJECT_ATTACHMENT_LAYOUTS})
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX "Source Files"
    FILES ${PROJECT_SOURCES}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    PREFIX "Shaders"
    FILES ${PROJECT_SHADERS}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX "Header Files"
    FILES ${PROJECT_HEADERS}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX "Pipeline Layouts"
    FILES ${PROJECT_PIPELINE_LAYOUTS}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX "Attachment Descriptions"
    FILES ${PROJECT_ATTACHMENT_LAYOUTS}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE ${PROJECT_DEFINES})
target_include_directories (${PROJECT_NAME} PRIVATE ${PROJECT_INCLUDES})
target_link_directories(${PROJECT_NAME} PRIVATE ${PROJECT_LIBRARY_PATHS})

if (WIN32)
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_libraries(${PROJECT_NAME} PRIVATE shaderc_combinedd vulkan-1 vklib)
    elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
        target_link_libraries(${PROJECT_NAME} PRIVATE shaderc_combined vulkan-1 vklib)
    endif()

    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
            _WIN32_WINNT=0x0A00
    )

    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}")

    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_COMMAND_ARGUMENTS "-d \"${CMAKE_CURRENT_SOURCE_DIR}/steefnew.smb\"")

    set_source_files_properties(${PROJECT_SHADERS} PROPERTIES VS_TOOL_OVERRIDE "EmbeddedResource")

    set_property(SOURCE ${PROJECT_SHADERS} PROPERTY VS_SETTINGS "ExcludedFromBuild=false")

    set_source_files_properties(${PROJECT_PIPELINE_LAYOUTS} PROPERTIES VS_TOOL_OVERRIDE "EmbeddedResource")

    set_property(SOURCE ${PROJECT_PIPELINE_LAYOUTS} PROPERTY VS_SETTINGS "ExcludedFromBuild=false")

    set_source_files_properties(${PROJECT_ATTACHMENT_LAYOUTS} PROPERTIES VS_TOOL_OVERRIDE "EmbeddedResource")

    set_property(SOURCE ${PROJECT_ATTACHMENT_LAYOUTS} PROPERTY VS_SETTINGS "ExcludedFromBuild=false")

endif()

add_custom_command(
TARGET ${PROJECT_NAME}    
PRE_BUILD
COMMAND cmd /c "shadercompile.bat"
VERBATIM
WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
DEPENDS ${PROJECT_SHADERS}
COMMENT "Running shader compile"
)

add_custom_command(
TARGET ${PROJECT_NAME}    
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders/compiled/" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}"
COMMENT "Copied compile shaders"
)

add_custom_command(
TARGET ${PROJECT_NAME}    
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/shaders/layouts/" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}"
COMMENT "Copied shader layouts"
)

add_custom_command(
TARGET ${PROJECT_NAME}    
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/pipelinelayout/" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}"
COMMENT "Copied pipeline layouts"
)

add_custom_command(
TARGET ${PROJECT_NAME}    
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/attachmentdescriptions/" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}"
COMMENT "Copied Attachment Descriptions"
)

add_custom_command(
TARGET ${PROJECT_NAME}    
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets/" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}"
COMMENT "Copied Assets"
)
