project(dpf_pir)
cmake_minimum_required(VERSION 3.4.1)

enable_language(C CXX)
set(CMAKE_CXX_STANDARD 14)

set(SRCS
    AES.cpp
    Defines.cpp
    Log.cpp
    PRNG.cpp
    dpf.cpp
    hashdatastore.cpp)

set(CMAKE_C_FLAGS "-ffunction-sections -Wall  -maes -msse2 -msse4.1 -mavx2 -mpclmul -Wfatal-errors -pthread -Wno-strict-overflow  -fPIC -Wno-ignored-attributes")
set(CMAKE_CXX_FLAGS  "${CMAKE_C_FLAGS}  -std=c++14 -g")

# Select flags.
# SET(CMAKE_CXX_FLAGS_RELEASE "-O0 -g -DNDEBUG")
SET(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG")
SET(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-ggdb -rdynamic")
SET(CMAKE_CXX_FLAGS_DEBUG  "-ggdb -rdynamic")

add_library(dpf_pir ${SRCS})

set_target_properties(dpf_pir PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

# add_executable(dpf_pir ${SRCS} main.cpp)
# target_link_libraries(dpf_pir crypto)

# add_executable(bench ${SRCS} bench.cpp)
# target_link_libraries(bench crypto)

# add_executable(dpf_tests ${SRCS} test.cpp)
# target_link_libraries(dpf_tests crypto)
