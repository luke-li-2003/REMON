cmake_minimum_required(VERSION 3.16.3)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -frecord-gcc-switches")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frecord-gcc-switches")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frecord-gcc-switches -fsanitize=thread")
#set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
project(remon)

set(CMAKE_CXX_STANDARD 20)

include_directories("./shared")

add_library(remon SHARED remon.cpp
        shared/communication.cpp
        shared/helperFunctions.cpp
        shared/log.cpp
        shared/pod.cpp
        shared/SocketClient.cpp
        shared/SocketServer.cpp
        shared/timer.cpp
        shared/IPCSharedMemory.cpp
        shared/RemoteConnectionClient.cpp
        shared/FreeListAllocator.cpp
        SegregatedPageList.cpp
        SegregatedPageList.h shared/RamWatch.cpp shared/RamWatch.h shared/config.cpp
        shared/RemoteConnectionBundle.cpp
        shared/RemoteConnectionBundle.h
        SplMux.cpp
        SplMux.h
        SPLBundle.cpp
        SPLBundle.h
        TopLevelAllocator.cpp
        TopLevelAllocator.h
        TopLevelAllocatorBundle.cpp
        TopLevelAllocatorBundle.h
        PodBoundedBuffer.cpp
        PodBoundedBuffer.h
        BoundedBufferBundle.cpp
        BoundedBufferBundle.h
        shared/SpinLock.cpp
        shared/SpinLock.h)

set(CMAKE_BUILD_TYPE "Release")
#set(CMAKE_BUILD_TYPE "RelWithDebInfo")
find_package(OpenSSL REQUIRED)
target_link_libraries(remon PRIVATE)
set_target_properties(remon PROPERTIES POSITION_INDEPENDENT_CODE ON)
