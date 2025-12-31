cmake_minimum_required(VERSION 3.28)

set (PROJECT_TYPE_EXECUTABLE "exe")

set (PROJECT_TYPE_STATIC_LIBRARY "static-lib")

set (PROJECT_NAME "vklib")

project(${PROJECT_NAME} LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set (PROJECT_INCLUDES "include/" "vklib/" "C:/VulkanSDK/1.3.290.0/Include/"
)

set (PROJECT_LIBRARY_PATHS
)

file(GLOB PROJECT_SOURCES "src/*.cpp" "vklib/pch.cpp") 

file(GLOB PROJECT_HEADERS "include/*.h" "vklib/pch.h"
)



set (PROJECT_DEFINES)

set (PROJECT_TYPE "static-lib")



if (${PROJECT_TYPE} STREQUAL ${PROJECT_TYPE_EXECUTABLE})
    add_executable(${PROJECT_NAME} ${PROJECT_SOURCES} ${PROJECT_HEADERS})
elseif (${PROJECT_TYPE} STREQUAL ${PROJECT_TYPE_STATIC_LIBRARY})
    add_library(${PROJECT_NAME} STATIC ${PROJECT_SOURCES} ${PROJECT_HEADERS})
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX "Source Files"
    FILES ${PROJECT_SOURCES}
)



source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}
    PREFIX "Header Files"
    FILES ${PROJECT_HEADERS}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE ${PROJECT_DEFINES})
target_include_directories (${PROJECT_NAME} PRIVATE ${PROJECT_INCLUDES})
target_link_directories(${PROJECT_NAME} PRIVATE ${PROJECT_LIBRARY_PATHS})

if (WIN32)
    target_compile_definitions(${PROJECT_NAME}
        PRIVATE
            _WIN32_WINNT=0x0601
    )
endif()