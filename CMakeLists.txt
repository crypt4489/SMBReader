cmake_minimum_required(VERSION 3.28)

set (CORE_LIBRARY_NAME "Core")

project(CoreEngine LANGUAGES C CXX)  
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set (GLOBAL_INCLUDES 
"include/" 
"vulkan/include/" 
"platform/"
"common/include/"
)

set (GLOBAL_LIBRARY_PATHS "vulkan/lib/")

file(GLOB GLOBAL_SOURCES "src/*.cpp" "common/src/*/*.cpp") 

file(GLOB GLOBAL_HEADERS "common/include/*/*.h" "include/*.h" "platform/*.h" "common/include/*.h")

file(GLOB GLOBAL_SHADERS "shaders/layouts/*.sgr" "shaders/*.vert" "shaders/*.comp" "shaders/*.frag")

file(GLOB GLOBAL_PIPELINE_LAYOUTS "pipelineLayout/*.pld")

file(GLOB GLOBAL_ATTACHMENT_LAYOUTS "attachmentdescriptions/*.adf")

if (WIN32)
    
    list (APPEND GLOBAL_LIBRARY_PATHS "lib/windows")
    list (APPEND GLOBAL_INCLUDES "platform/windows/include/")
    
    file(GLOB WIN_GLOBAL_SOURCES "platform/windows/src/*.cpp") 
    list (APPEND GLOBAL_SOURCES ${WIN_GLOBAL_SOURCES})

    file(GLOB WIN_GLOBAL_HEADERS "platform/windows/include/*.h")
    list (APPEND GLOBAL_HEADERS ${WIN_GLOBAL_HEADERS})

endif()

set (PROJECT_DEFINES)

add_subdirectory(vulkan)

add_library(${CORE_LIBRARY_NAME} OBJECT ${GLOBAL_SOURCES} ${GLOBAL_HEADERS} ${GLOBAL_SHADERS} ${GLOBAL_PIPELINE_LAYOUTS} ${GLOBAL_ATTACHMENT_LAYOUTS})
target_include_directories(${CORE_LIBRARY_NAME} PUBLIC ${GLOBAL_INCLUDES})
target_compile_definitions(${CORE_LIBRARY_NAME} PUBLIC ${PROJECT_DEFINES})
target_link_libraries(${CORE_LIBRARY_NAME} PUBLIC vklib)

if (WIN32)
    target_compile_definitions(
        ${CORE_LIBRARY_NAME}
        PUBLIC
        _WIN32_WINNT=0x0A00
    )
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
        PREFIX "Source Files"
        FILES ${GLOBAL_SOURCES}
    )

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/shaders
        PREFIX "Shaders"
        FILES ${GLOBAL_SHADERS}
    )

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
        PREFIX "Header Files"
        FILES ${GLOBAL_HEADERS}
    )

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
        PREFIX "Pipeline Layouts"
        FILES ${GLOBAL_PIPELINE_LAYOUTS}
    )

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
        PREFIX "Attachment Descriptions"
        FILES ${GLOBAL_ATTACHMENT_LAYOUTS}
    )

add_subdirectory(examples)






