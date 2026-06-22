cmake_minimum_required(VERSION 3.25)

if(WIN32)
    add_subdirectory(SMBReader)
endif()

add_subdirectory(LinuxTest)