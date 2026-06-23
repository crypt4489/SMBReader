cmake_minimum_required(VERSION 3.25)

set (PROJECT_NAME "vklib")

find_package(Vulkan REQUIRED)

set (
    PROJECT_INCLUDES 
    "${CMAKE_CURRENT_SOURCE_DIR}/include/" 
    "${CMAKE_CURRENT_SOURCE_DIR}/vklib/" 
    "${Vulkan_INCLUDE_DIRS}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../common/include/"
    )

get_filename_component(VULKAN_LIB_DIR ${Vulkan_LIBRARIES} DIRECTORY)

set (
    PROJECT_LIBRARY_PATHS
    "${VULKAN_LIB_DIR}"
    )

file(GLOB PROJECT_SOURCES "src/*.cpp" "vklib/pch.cpp") 

file(GLOB PROJECT_HEADERS "include/*.h" "vklib/pch.h")

file(GLOB COMMON_PROJECT_HEADERS "${CMAKE_SOURCE_DIR}/common/include/*.h")

set (PROJECT_DEFINES ${USE_SHADERC_COMBINED})

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

option(USE_SHADERC_COMBINED "Enable ShaderC compilation" OFF)

target_compile_definitions(${PROJECT_NAME} PUBLIC ${PROJECT_DEFINES})



if(WIN32)
    set(LIBVULKAN vulkan-1)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601)
    target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_WIN32_KHR)

    if (USE_SHADERC_COMBINED)
        set(SHADERC_DEBUG_LIB "$<$<CONFIG:Debug>:shaderc_combinedd>")
        set(SHADERC_RELEASE_LIB "$<$<CONFIG:Release>:shaderc_combined>")
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_SHADERC_COMBINED)
    endif()

elseif(UNIX)
    set(LIBVULKAN vulkan)
    if (WINDOW_USE_X11)
        target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_XCB_KHR)
    elseif (WINDOW_USE_WAYLAND)
        target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_WAYLAND_KHR)
    endif()

    if (USE_SHADERC_COMBINED)
        set(SHADERC_RELEASE_LIB shaderc_combined)
        target_compile_definitions(${PROJECT_NAME} PRIVATE USE_SHADERC_COMBINED)
    endif()

endif()

target_include_directories (${PROJECT_NAME} PUBLIC ${PROJECT_INCLUDES})
target_link_directories(${PROJECT_NAME} PUBLIC ${PROJECT_LIBRARY_PATHS})
target_link_libraries(${PROJECT_NAME} PUBLIC
    ${LIBVULKAN}
    ${SHADERC_DEBUG_LIB}
    ${SHADERC_RELEASE_LIB}
)
