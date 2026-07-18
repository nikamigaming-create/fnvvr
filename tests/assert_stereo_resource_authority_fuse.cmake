if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT is required")
endif()

set(FORBIDDEN_MACRO FNVXR_STEREO_RESOURCE_LIFECYCLE_TEST_AUTHORITY)

file(READ "${SOURCE_ROOT}/CMakeLists.txt" ROOT_CMAKE)
string(FIND "${ROOT_CMAKE}" "${FORBIDDEN_MACRO}" MACRO_IN_CMAKE)
if(NOT MACRO_IN_CMAKE EQUAL -1)
    message(FATAL_ERROR
        "The stereo lifecycle test-authority macro reappeared in CMake")
endif()

file(GLOB_RECURSE PRODUCTION_SOURCES
    "${SOURCE_ROOT}/host/*.cpp"
    "${SOURCE_ROOT}/host/*.h"
    "${SOURCE_ROOT}/plugin/*.cpp"
    "${SOURCE_ROOT}/plugin/*.h"
    "${SOURCE_ROOT}/protocol/*.cpp"
    "${SOURCE_ROOT}/protocol/*.h"
    "${SOURCE_ROOT}/renderhook/*.cpp"
    "${SOURCE_ROOT}/renderhook/*.h"
    "${SOURCE_ROOT}/runtime/*.cpp"
    "${SOURCE_ROOT}/runtime/*.h"
    "${SOURCE_ROOT}/tools/*.cpp"
    "${SOURCE_ROOT}/tools/*.h")

foreach(SOURCE_FILE IN LISTS PRODUCTION_SOURCES)
    file(READ "${SOURCE_FILE}" SOURCE_TEXT)
    string(FIND "${SOURCE_TEXT}" "${FORBIDDEN_MACRO}" MACRO_IN_SOURCE)
    if(NOT MACRO_IN_SOURCE EQUAL -1)
        message(FATAL_ERROR
            "Forbidden stereo test-authority macro in ${SOURCE_FILE}")
    endif()
    string(FIND
        "${SOURCE_TEXT}"
        "StereoResourceLifecycleTestAuthority"
        TEST_ISSUER_IN_SOURCE)
    if(NOT TEST_ISSUER_IN_SOURCE EQUAL -1
        AND NOT SOURCE_FILE STREQUAL
            "${SOURCE_ROOT}/runtime/fnvxr_engine_stereo_resources.h")
        message(FATAL_ERROR
            "Stereo test authority implementation escaped into ${SOURCE_FILE}")
    endif()
endforeach()

file(READ
    "${SOURCE_ROOT}/tests/fnvxr_engine_stereo_resources_test.cpp"
    LIFECYCLE_TEST_SOURCE)
string(FIND
    "${LIFECYCLE_TEST_SOURCE}"
    "struct StereoResourceLifecycleTestAuthority final"
    TEST_ISSUER_DEFINITION)
if(TEST_ISSUER_DEFINITION EQUAL -1)
    message(FATAL_ERROR
        "The isolated lifecycle test issuer definition is missing")
endif()
