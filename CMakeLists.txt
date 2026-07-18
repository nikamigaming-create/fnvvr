cmake_minimum_required(VERSION 3.20)

project(fnvvr LANGUAGES CXX)

enable_testing()

if(WIN32)
    add_test(
        NAME fnvxr_launcher_live_fuse
        COMMAND ${CMAKE_COMMAND}
            -DLAUNCHER=${CMAKE_CURRENT_SOURCE_DIR}/scripts/start-openxr-retail-sidecar.ps1
            -DPOWERSHELL=$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe
            -DRUN_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/local/openxr-retail-sidecar-runs
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_launcher_live_fuse.cmake
    )
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(fnvxr_protocol INTERFACE)
target_include_directories(fnvxr_protocol INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol
)

file(GLOB FNVXR_OPENXR_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/sources/OpenXR-SDK-release-1.1.60/*/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/sources/OpenXR-SDK-release-1.1.*/*/include"
)

if(FNVXR_OPENXR_INCLUDE_DIR AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_executable(fnvxr_openxr_probe host/fnvxr_openxr_probe.cpp)
    target_include_directories(fnvxr_openxr_probe PRIVATE ${FNVXR_OPENXR_INCLUDE_DIR})

    add_executable(fnvxr_openxr_pose_host host/fnvxr_openxr_pose_host.cpp)
    target_include_directories(fnvxr_openxr_pose_host PRIVATE
        ${FNVXR_OPENXR_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol
        ${CMAKE_CURRENT_SOURCE_DIR}/renderhook
    )
    target_link_libraries(fnvxr_openxr_pose_host PRIVATE
        d3d11
        d3dcompiler
        dxgi
        windowscodecs
        ole32
        bcrypt
    )
    if(MSVC)
        target_compile_options(fnvxr_openxr_pose_host PRIVATE /fp:strict)
    endif()
    add_test(
        NAME fnvxr_openxr_host_live_fuse
        COMMAND ${CMAKE_COMMAND}
            -DHOST_EXE=$<TARGET_FILE:fnvxr_openxr_pose_host>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_openxr_host_fuse.cmake
    )

    set(FNVXR_OPENXR_LOADER_HINT "$ENV{FNVXR_OPENXR_LOADER_HINT}" CACHE FILEPATH "Optional OpenXR loader DLL hint")
    if(FNVXR_OPENXR_LOADER_HINT AND EXISTS "${FNVXR_OPENXR_LOADER_HINT}")
        target_compile_definitions(fnvxr_openxr_probe PRIVATE
            FNVXR_OPENXR_LOADER_HINT="${FNVXR_OPENXR_LOADER_HINT}"
        )
        target_compile_definitions(fnvxr_openxr_pose_host PRIVATE
            FNVXR_OPENXR_LOADER_HINT="${FNVXR_OPENXR_LOADER_HINT}"
        )
    endif()

endif()

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    # FalloutNV.exe and NVSE are 32-bit. Building a same-named x64 plugin is a
    # load-incompatible artifact hazard and makes x64 CI look more complete
    # than the runtime pipeline really is.
    add_library(nvse_fnvxr MODULE plugin/fnvxr_nvse_plugin.cpp)
    target_include_directories(nvse_fnvxr PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    )
    target_link_libraries(nvse_fnvxr PRIVATE fnvxr_protocol dbghelp)
    set_target_properties(nvse_fnvxr PROPERTIES
        PREFIX ""
        OUTPUT_NAME "nvse_fnvxr"
    )

    add_library(fnvxr_d3d9_proxy MODULE renderhook/fnvxr_d3d9_proxy.cpp)
    target_include_directories(fnvxr_d3d9_proxy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/renderhook)
    target_sources(fnvxr_d3d9_proxy PRIVATE renderhook/fnvxr_d3d9_proxy.def)
    target_link_libraries(fnvxr_d3d9_proxy PRIVATE dxguid bcrypt)
    if(MSVC)
        target_compile_options(fnvxr_d3d9_proxy PRIVATE /fp:strict /arch:SSE2)
    endif()
    set_target_properties(fnvxr_d3d9_proxy PROPERTIES
        PREFIX ""
        OUTPUT_NAME "d3d9"
    )

    add_library(fnvxr_xinput_proxy MODULE renderhook/fnvxr_xinput_proxy.cpp)
    target_sources(fnvxr_xinput_proxy PRIVATE renderhook/fnvxr_xinput_proxy.def)
    set_target_properties(fnvxr_xinput_proxy PROPERTIES
        PREFIX ""
        OUTPUT_NAME "xinput1_3"
    )

    add_library(fnvxr_dinput8_proxy MODULE renderhook/fnvxr_dinput8_proxy.cpp)
    target_sources(fnvxr_dinput8_proxy PRIVATE renderhook/fnvxr_dinput8_proxy.def)
    target_link_libraries(fnvxr_dinput8_proxy PRIVATE dxguid)
    set_target_properties(fnvxr_dinput8_proxy PROPERTIES
        PREFIX ""
        OUTPUT_NAME "dinput8"
    )
endif()

add_executable(fnvxr_protocol_layout_test tests/fnvxr_protocol_layout_test.cpp)
target_link_libraries(fnvxr_protocol_layout_test PRIVATE fnvxr_protocol)
add_test(NAME fnvxr_protocol_layout_test COMMAND fnvxr_protocol_layout_test)

add_executable(fnvxr_product_contract_test tests/fnvxr_product_contract_test.cpp)
target_link_libraries(fnvxr_product_contract_test PRIVATE fnvxr_protocol)
add_test(NAME fnvxr_product_contract_test COMMAND fnvxr_product_contract_test)

add_executable(fnvxr_stereo_transaction_test tests/fnvxr_stereo_transaction_test.cpp)
target_include_directories(fnvxr_stereo_transaction_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
target_link_libraries(fnvxr_stereo_transaction_test PRIVATE fnvxr_protocol)
add_test(NAME fnvxr_stereo_transaction_test COMMAND fnvxr_stereo_transaction_test)

add_executable(fnvxr_engine_stereo_schedule_test tests/fnvxr_engine_stereo_schedule_test.cpp)
target_include_directories(fnvxr_engine_stereo_schedule_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
add_test(NAME fnvxr_engine_stereo_schedule_test COMMAND fnvxr_engine_stereo_schedule_test)

add_executable(fnvxr_engine_capability_test tests/fnvxr_engine_capability_test.cpp)
target_include_directories(fnvxr_engine_capability_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
add_test(NAME fnvxr_engine_capability_test COMMAND fnvxr_engine_capability_test)

add_executable(fnvxr_retail_engine_manifest_test tests/fnvxr_retail_engine_manifest_test.cpp)
target_include_directories(fnvxr_retail_engine_manifest_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
add_test(NAME fnvxr_retail_engine_manifest_test COMMAND fnvxr_retail_engine_manifest_test)

add_executable(fnvxr_retail_safety_test tests/fnvxr_retail_safety_test.cpp)
target_include_directories(fnvxr_retail_safety_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
add_test(NAME fnvxr_retail_safety_test COMMAND fnvxr_retail_safety_test)

if(WIN32)
    add_executable(fnvxr_retail_runtime_probe tools/fnvxr_retail_runtime_probe.cpp)
    target_include_directories(fnvxr_retail_runtime_probe PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    )
    target_link_libraries(fnvxr_retail_runtime_probe PRIVATE bcrypt)
    add_test(
        NAME fnvxr_retail_runtime_probe_help
        COMMAND fnvxr_retail_runtime_probe --help
    )
endif()

add_executable(fnvxr_gpu_transport_test tests/fnvxr_gpu_transport_test.cpp)
target_link_libraries(fnvxr_gpu_transport_test PRIVATE fnvxr_protocol)
add_test(NAME fnvxr_gpu_transport_test COMMAND fnvxr_gpu_transport_test)

add_executable(fnvxr_shared_state_probe tools/fnvxr_shared_state_probe.cpp)
target_link_libraries(fnvxr_shared_state_probe PRIVATE fnvxr_protocol)
add_test(NAME fnvxr_shared_state_probe_smoke COMMAND fnvxr_shared_state_probe)

add_executable(fnvxr_pose_fixture tools/fnvxr_pose_fixture.cpp)
target_link_libraries(fnvxr_pose_fixture PRIVATE fnvxr_protocol)
add_test(NAME fnvxr_pose_fixture_help COMMAND fnvxr_pose_fixture --help)

add_executable(fnvxr_command tools/fnvxr_command.cpp)
target_link_libraries(fnvxr_command PRIVATE fnvxr_protocol)
add_test(NAME fnvxr_command_help COMMAND fnvxr_command --help)

add_executable(fnvxr_input tools/fnvxr_input.cpp)
target_link_libraries(fnvxr_input PRIVATE fnvxr_protocol)
add_test(NAME fnvxr_input_help COMMAND fnvxr_input --help)

add_executable(fnvxr_stereo_math_test tests/fnvxr_stereo_math_test.cpp)
target_include_directories(fnvxr_stereo_math_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/renderhook)
if(MSVC)
    target_compile_options(fnvxr_stereo_math_test PRIVATE /fp:strict)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_compile_options(fnvxr_stereo_math_test PRIVATE /arch:SSE2)
    endif()
endif()
add_test(NAME fnvxr_stereo_math_test COMMAND fnvxr_stereo_math_test)

add_executable(fnvxr_player_transform_test tests/fnvxr_player_transform_test.cpp)
add_test(NAME fnvxr_player_transform_test COMMAND fnvxr_player_transform_test)

add_executable(fnvxr_fabrik_test tests/fnvxr_fabrik_test.cpp)
target_include_directories(fnvxr_fabrik_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/plugin)
add_test(NAME fnvxr_fabrik_test COMMAND fnvxr_fabrik_test)

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
    add_executable(fnvxr_plugin_exports_test tests/fnvxr_plugin_exports_test.cpp)
    add_test(NAME fnvxr_plugin_exports_test
        COMMAND fnvxr_plugin_exports_test "$<TARGET_FILE:nvse_fnvxr>"
    )

    add_executable(fnvxr_nvse_bridge_test tests/fnvxr_nvse_bridge_test.cpp)
    target_link_libraries(fnvxr_nvse_bridge_test PRIVATE fnvxr_protocol)
    add_test(NAME fnvxr_nvse_bridge_test
        COMMAND fnvxr_nvse_bridge_test "$<TARGET_FILE:nvse_fnvxr>"
    )
endif()
