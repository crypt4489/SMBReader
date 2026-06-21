cmake_minimum_required(VERSION 3.28)

set (PROJECT_NAME "vklib")

set (
    PROJECT_INCLUDES 
    "${CMAKE_CURRENT_SOURCE_DIR}/include/" 
    "${CMAKE_CURRENT_SOURCE_DIR}/vklib/" 
    "C:/VulkanSDK/1.3.290.0/Include/" 
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/include/"
    )

set (
    PROJECT_LIBRARY_PATHS
    "C:/VulkanSDK/1.3.290.0/Lib/"
    )

file(GLOB PROJECT_SOURCES "src/*.cpp" "vklib/pch.cpp") 

file(GLOB PROJECT_HEADERS "include/*.h" "vklib/pch.h")

file(GLOB COMMON_PROJECT_HEADERS "${CMAKE_SOURCE_DIR}/common/include/*.h")

set (PROJECT_DEFINES)

add_library(${PROJECT_NAME} STATIC ${PROJECT_SOURCES} ${PROJECT_HEADERS} ${COMMON_PROJECT_HEADERS})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX "Source Files"
    FILES ${PROJECT_SOURCES}
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX "Header Files"
    FILES ${PROJECT_HEADERS}
)

source_group(TREE "${CMAKE_SOURCE_DIR}/common/include/"
    PREFIX "Common Header Files"
    FILES ${COMMON_PROJECT_HEADERS}
)

target_compile_definitions(${PROJECT_NAME} PUBLIC ${PROJECT_DEFINES})
target_include_directories (${PROJECT_NAME} PUBLIC ${PROJECT_INCLUDES})
target_link_directories(${PROJECT_NAME} PUBLIC ${PROJECT_LIBRARY_PATHS})
target_link_libraries(${PROJECT_NAME} PUBLIC
    vulkan-1
    $<$<CONFIG:Debug>:shaderc_combinedd>
    $<$<CONFIG:Release>:shaderc_combined>
)

if (WIN32)
    target_compile_definitions(
        ${PROJECT_NAME}
        PRIVATE
        _WIN32_WINNT=0x0601
    )
endif()