cmake_minimum_required(VERSION 3.16)
project(levelprobe CXX)
set(CMAKE_CXX_STANDARD 17)
# Built against the local OpenFHE install in ../../openfhe/install (see README).
find_package(OpenFHE REQUIRED PATHS ${CMAKE_CURRENT_SOURCE_DIR}/../../openfhe/install NO_DEFAULT_PATH)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
include_directories(${OPENMP_INCLUDES} ${OpenFHE_INCLUDE} ${OpenFHE_INCLUDE}/third-party/include
                    ${OpenFHE_INCLUDE}/core ${OpenFHE_INCLUDE}/pke ${OpenFHE_INCLUDE}/binfhe)
link_directories(${OpenFHE_LIBDIR} ${OPENMP_LIBRARIES})
add_executable(levelprobe levelprobe.cpp)
target_link_libraries(levelprobe ${OpenFHE_SHARED_LIBRARIES})
set_target_properties(levelprobe PROPERTIES BUILD_RPATH ${OpenFHE_LIBDIR})
