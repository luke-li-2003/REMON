cmake_minimum_required(VERSION 2.8.12)
project(experiments)

set(CMAKE_CXX_STANDARD 11)

# DuckDB paths
include_directories(../duckdb/src/include)

set(DUCKDB_LIB_DIR "" CACHE PATH "Path to directory containing libduckdb.*")
set(DUCKDB_SEARCH_DIRS
	"${CMAKE_SOURCE_DIR}/../duckdb/build-release/src"
	"${CMAKE_SOURCE_DIR}/../duckdb/build/release/src"
	"${CMAKE_SOURCE_DIR}/../duckdb/build/src"
	"${CMAKE_SOURCE_DIR}/../duckdb/cmake-build-release/src"
)
if (DUCKDB_LIB_DIR)
	set(DUCKDB_SEARCH_DIRS "${DUCKDB_LIB_DIR}" ${DUCKDB_SEARCH_DIRS})
endif ()

find_library(DUCKDB_LIB duckdb PATHS ${DUCKDB_SEARCH_DIRS} NO_DEFAULT_PATH)
if (NOT DUCKDB_LIB)
	message(FATAL_ERROR "libduckdb not found. Build DuckDB first. Searched: ${DUCKDB_SEARCH_DIRS}")
endif ()

# libremon.so path (required by libduckdb.so)
# Support both build-release (verify_build.sh) and cmake-build-release (CLion IDE)
link_directories(../compute_node/build-release)
link_directories(../compute_node/cmake-build-release)

# Set rpath so shared libraries can be found at runtime
get_filename_component(DUCKDB_LIB_DIR "${DUCKDB_LIB}" DIRECTORY)
set(RPATH_DIRS
	"${CMAKE_SOURCE_DIR}/../compute_node/build-release"
	"${CMAKE_SOURCE_DIR}/../compute_node/cmake-build-release"
	"${DUCKDB_LIB_DIR}"
)
list(REMOVE_DUPLICATES RPATH_DIRS)
set(CMAKE_BUILD_RPATH "${RPATH_DIRS}")
set(CMAKE_INSTALL_RPATH "${RPATH_DIRS}")

add_executable(experiment main.cpp)
target_link_libraries(experiment "${DUCKDB_LIB}" remon)
