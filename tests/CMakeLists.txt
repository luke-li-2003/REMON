cmake_minimum_required(VERSION 3.16.3)
project(tests)

find_package(TBB REQUIRED)



set(COMPUTE_NODE_LIB_ROOT ../compute_node/)
set(MEMORY_NODE_ROOT ../memory_node/)
include_directories(${COMPUTE_NODE_LIB_ROOT})
#include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../compute_node/shared)
include_directories(${COMPUTE_NODE_LIB_ROOT}/shared)
include_directories(${MEMORY_NODE_ROOT})
link_directories(${COMPUTE_NODE_LIB_ROOT}build-release/)

find_library(COMPUTE_NODE_LIB_ROOT remon)

set(CMAKE_CXX_STANDARD 20)

# Memory node sources needed for unit tests (TenantContext, tenantPod)
set(MEMORY_NODE_SRCS
    ${MEMORY_NODE_ROOT}/TenantContext.cpp
    ${MEMORY_NODE_ROOT}/tenantPod.cpp
)

# Shared sources needed for unit tests that aren't in libremon
set(SHARED_SRCS
    ${COMPUTE_NODE_LIB_ROOT}/shared/SpinLock.cpp
)

add_executable(tests main.cpp tests.cpp tests.h unit_tests.cpp unit_tests.h ${MEMORY_NODE_SRCS} ${SHARED_SRCS})
target_link_libraries(tests PUBLIC remon pthread tbb)
