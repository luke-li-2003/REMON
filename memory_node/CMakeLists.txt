cmake_minimum_required(VERSION 3.16)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -frecord-gcc-switches")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -frecord-gcc-switches")

project(memory_node)

set(CMAKE_CXX_STANDARD 20)
include_directories("../compute_node/shared")
set(SHARED_SRC ../compute_node/shared)
add_executable(memory_node main.cpp
        ${SHARED_SRC}/SocketServer.cpp
        ${SHARED_SRC}/communication.cpp
        ${SHARED_SRC}/log.cpp
        ${SHARED_SRC}/config.cpp
        #${SHARED_SRC}/pod.cpp
        #${SHARED_SRC}/FreeListAllocator.cpp
        #${SHARED_SRC}/RemoteConnectionClient.cpp
        #${SHARED_SRC}/RemoteConnectionBundle.cpp
        ${SHARED_SRC}/helperFunctions.cpp
        ${SHARED_SRC}/SocketClient.cpp
        ${SHARED_SRC}/RamWatch.cpp
        MemoryNodeServer.cpp TenantContext.cpp TenantContext.h
        tenantPod.cpp
        tenantPod.h)
target_link_libraries(memory_node pthread)
target_link_options(memory_node PRIVATE -static-libgcc -static-libstdc++ -static)