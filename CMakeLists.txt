cmake_minimum_required(VERSION 3.25)

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

set (GLOBAL_LIBRARIES_USED)

file(GLOB GLOBAL_SOURCES "src/*.cpp" "common/src/*/*.cpp") 

file(GLOB GLOBAL_HEADERS "common/include/*/*.h" "include/*.h" "platform/*.h" "common/include/*.h")


if (WIN32)
    list (APPEND GLOBAL_LIBRARY_PATHS "lib/windows")
    list (APPEND GLOBAL_INCLUDES "platform/windows/include/")
    
    file(GLOB WIN_GLOBAL_SOURCES "platform/windows/src/*.cpp") 
    list (APPEND GLOBAL_SOURCES ${WIN_GLOBAL_SOURCES})

    file(GLOB WIN_GLOBAL_HEADERS "platform/windows/include/*.h")
    list (APPEND GLOBAL_HEADERS ${WIN_GLOBAL_HEADERS})
elseif(UNIX)
    list (APPEND GLOBAL_INCLUDES "platform/linux/include/")
    
    file(GLOB LINUX_GLOBAL_SOURCES "platform/linux/src/*.cpp") 
    list (APPEND GLOBAL_SOURCES ${LINUX_GLOBAL_SOURCES})

    file(GLOB LINUX_GLOBAL_HEADERS "platform/linux/include/*.h")
    list (APPEND GLOBAL_HEADERS ${LINUX_GLOBAL_HEADERS})

    list (APPEND GLOBAL_LIBRARIES_USED pthread)
endif()

option(WINDOW_USE_WAYLAND "Enable wayland backend" OFF)

option(WINDOW_USE_X11 "Enable X11 backend" OFF)

set (PROJECT_DEFINES)

add_subdirectory(vulkan)

add_library(${CORE_LIBRARY_NAME} OBJECT ${GLOBAL_SOURCES} ${GLOBAL_HEADERS})

target_include_directories(${CORE_LIBRARY_NAME} PUBLIC ${GLOBAL_INCLUDES})
target_compile_definitions(${CORE_LIBRARY_NAME} PUBLIC ${PROJECT_DEFINES})
target_link_directories(${CORE_LIBRARY_NAME} PUBLIC ${GLOBAL_LIBRARY_PATHS})
target_link_libraries(${CORE_LIBRARY_NAME} PUBLIC ${GLOBAL_LIBRARIES_USED})

if (WINDOW_USE_WAYLAND)
    target_compile_definitions(${CORE_LIBRARY_NAME} PUBLIC WINDOW_USE_WAYLAND)
    target_link_libraries(${CORE_LIBRARY_NAME} PUBLIC wayland-client)
endif()

if (WINDOW_USE_X11)
    target_compile_definitions(${CORE_LIBRARY_NAME} PUBLIC WINDOW_USE_X11)
endif()

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

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
        PREFIX "Header Files"
        FILES ${GLOBAL_HEADERS}
    )

add_subdirectory(examples)






