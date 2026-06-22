
cmake_minimum_required(VERSION 3.25)

set(CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION_MAJOR 10)

set (PROJECT_NAME "SMBReader")

set (APP_INCLUDES 
"${CMAKE_CURRENT_SOURCE_DIR}/include/" 
"${CMAKE_CURRENT_SOURCE_DIR}/smb/include/"
)

file(GLOB APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/smb/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
file(GLOB APP_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/smb/include/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")

add_executable(${PROJECT_NAME} ${APP_SOURCES} ${APP_HEADERS})

target_include_directories (${PROJECT_NAME} PRIVATE ${APP_INCLUDES})
target_link_libraries(${PROJECT_NAME} PRIVATE Core)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
        PREFIX "Header Files"
        FILES ${APP_HEADERS}
    )

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
        PREFIX "Source Files"
        FILES ${APP_SOURCES}
    )

if (WIN32)

    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}")

    set_property(TARGET ${PROJECT_NAME} PROPERTY VS_DEBUGGER_COMMAND_ARGUMENTS "-d \"${CMAKE_SOURCE_DIR}/steefnew.smb\"")

    set_source_files_properties(${GLOBAL_SHADERS} PROPERTIES VS_TOOL_OVERRIDE "EmbeddedResource")

    set_property(SOURCE ${GLOBAL_SHADERS} PROPERTY VS_SETTINGS "ExcludedFromBuild=false")

    set_source_files_properties(${GLOBAL_PIPELINE_LAYOUTS} PROPERTIES VS_TOOL_OVERRIDE "EmbeddedResource")

    set_property(SOURCE ${GLOBAL_PIPELINE_LAYOUTS} PROPERTY VS_SETTINGS "ExcludedFromBuild=false")

    set_source_files_properties(${GLOBAL_ATTACHMENT_LAYOUTS} PROPERTIES VS_TOOL_OVERRIDE "EmbeddedResource")

    set_property(SOURCE ${GLOBAL_ATTACHMENT_LAYOUTS} PROPERTY VS_SETTINGS "ExcludedFromBuild=false")

    add_custom_command(
        TARGET ${PROJECT_NAME}    
        PRE_BUILD
        COMMAND cmd /c "shadercompile.bat"
        VERBATIM
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS ${GLOBAL_SHADERS}
        COMMENT "Running shader compile"
    )

endif()

add_custom_command(
TARGET ${PROJECT_NAME}    
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/shaders/compiled/" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}"
COMMENT "Copied compile shaders"
)

add_custom_command(
TARGET ${PROJECT_NAME}    
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/shaders/layouts/" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}"
COMMENT "Copied shader layouts"
)

add_custom_command(
TARGET ${PROJECT_NAME}    
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/pipelinelayout/" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}"
COMMENT "Copied pipeline layouts"
)

add_custom_command(
TARGET ${PROJECT_NAME}    
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/attachmentdescriptions/" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}"
COMMENT "Copied Attachment Descriptions"
)

add_custom_command(
TARGET ${PROJECT_NAME}    
POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets/" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_BUILD_TYPE}"
COMMENT "Copied Assets"
)