cmake_minimum_required(VERSION 3.20)
project(fnvxr_openmw_adapter_smoke LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
add_executable(fnvxr_openmw_adapter_smoke main.cpp)
target_include_directories(fnvxr_openmw_adapter_smoke PRIVATE
    "${OPENMW_SOURCE_DIR}" "${OSG_INCLUDE_DIR}")
target_link_libraries(fnvxr_openmw_adapter_smoke PRIVATE "${FNVXR_RIG_LIBRARY}" "${OSG_LIBRARY}")
