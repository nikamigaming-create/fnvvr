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
    add_test(
        NAME fnvxr_launch_safety_audit
        COMMAND $ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/scripts/audit-launch-safety.ps1
    )
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(fnvxr_protocol INTERFACE)
target_include_directories(fnvxr_protocol INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol
)

add_executable(fnvxr_openxr_live_authority_test
    tests/fnvxr_openxr_live_authority_test.cpp)
target_include_directories(fnvxr_openxr_live_authority_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/host)
if(MSVC)
    target_compile_options(fnvxr_openxr_live_authority_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_openxr_live_authority_test
    COMMAND fnvxr_openxr_live_authority_test)

add_executable(fnvxr_headset_mirror_capture_test
    tests/fnvxr_headset_mirror_capture_test.cpp)
target_include_directories(fnvxr_headset_mirror_capture_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/host)
if(MSVC)
    target_compile_options(fnvxr_headset_mirror_capture_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_headset_mirror_capture_test
    COMMAND fnvxr_headset_mirror_capture_test)

add_executable(fnvxr_headset_demo_authority_test
    tests/fnvxr_headset_demo_authority_test.cpp)
target_include_directories(fnvxr_headset_demo_authority_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
if(MSVC)
    target_compile_options(fnvxr_headset_demo_authority_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_headset_demo_authority_test
    COMMAND fnvxr_headset_demo_authority_test)

add_executable(fnvxr_physical_input_authority_test
    tests/fnvxr_physical_input_authority_test.cpp)
target_include_directories(fnvxr_physical_input_authority_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
if(MSVC)
    target_compile_options(fnvxr_physical_input_authority_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_physical_input_authority_test
    COMMAND fnvxr_physical_input_authority_test)

add_executable(fnvxr_weapon_frame_contract_test
    tests/fnvxr_weapon_frame_contract_test.cpp)
target_include_directories(fnvxr_weapon_frame_contract_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
if(MSVC)
    target_compile_options(fnvxr_weapon_frame_contract_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_weapon_frame_contract_test
    COMMAND fnvxr_weapon_frame_contract_test)

if(WIN32)
    add_test(
        NAME fnvxr_build_attestation_test
        COMMAND powershell.exe
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/fnvxr_build_attestation_test.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(
        NAME fnvxr_retired_launchers_fuse_test
        COMMAND powershell.exe
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/fnvxr_retired_launchers_fuse_test.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(
        NAME fnvxr_product_launcher_test
        COMMAND powershell.exe
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/fnvxr_product_launcher_test.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(
        NAME fnvxr_phase1_6dof_verifier_test
        COMMAND powershell.exe
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/fnvxr_phase1_6dof_verifier_test.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(
        NAME fnvxr_physical_locomotion_contract_test
        COMMAND powershell.exe
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/fnvxr_physical_locomotion_contract_test.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(
        NAME fnvxr_ttw_retail_sandbox_script_test
        COMMAND powershell.exe
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/fnvxr_ttw_retail_sandbox_script_test.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(
        NAME fnvxr_ttw_baseline_launcher_test
        COMMAND powershell.exe
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/fnvxr_ttw_baseline_launcher_test.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(
        NAME fnvxr_ttw_fixture_launcher_test
        COMMAND powershell.exe
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/fnvxr_ttw_fixture_launcher_test.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(
        NAME fnvxr_ttw_johnnyguitar_staging_test
        COMMAND powershell.exe
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/fnvxr_ttw_johnnyguitar_staging_test.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(
        NAME fnvxr_meta_xr_operator_staging_test
        COMMAND powershell.exe
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/fnvxr_meta_xr_operator_staging_test.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(
        NAME fnvxr_private_stereo_live_verifier_test
        COMMAND powershell.exe
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/fnvxr_private_stereo_live_verifier_test.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(
        NAME fnvxr_desktop_assist_automation_script_test
        COMMAND powershell.exe
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/fnvxr_desktop_assist_automation_script_test.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}
    )
    add_test(
        NAME fnvxr_headless_simulator_cli_test
        COMMAND powershell.exe
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_CURRENT_SOURCE_DIR}/tests/fnvxr_headless_simulator_cli_test.ps1
            -SourceRoot ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

file(GLOB FNVXR_OPENXR_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/sources/OpenXR-SDK-release-1.1.60/*/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/sources/OpenXR-SDK-release-1.1.*/*/include"
)

if(FNVXR_OPENXR_INCLUDE_DIR AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_executable(fnvxr_openxr_probe host/fnvxr_openxr_probe.cpp)
    target_include_directories(fnvxr_openxr_probe PRIVATE ${FNVXR_OPENXR_INCLUDE_DIR})
    add_test(
        NAME fnvxr_openxr_probe_live_fuse
        COMMAND ${CMAKE_COMMAND}
            -DPROBE_EXE=$<TARGET_FILE:fnvxr_openxr_probe>
            -DPROBE_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/host/fnvxr_openxr_probe.cpp
            -DPROBE_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/scripts/run-openxr-probe.ps1
            -DPOWERSHELL=$ENV{SystemRoot}/System32/WindowsPowerShell/v1.0/powershell.exe
            -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/fnvxr-openxr-probe-fuse-test
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_openxr_probe_fuse.cmake
    )

    add_executable(fnvxr_openxr_pose_host host/fnvxr_openxr_pose_host.cpp)
    target_include_directories(fnvxr_openxr_pose_host PRIVATE
        ${FNVXR_OPENXR_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol
        ${CMAKE_CURRENT_SOURCE_DIR}/renderhook
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    )
    target_link_libraries(fnvxr_openxr_pose_host PRIVATE
        fnvxr_gpu_color_consumer
        d3d11
        d3dcompiler
        dxgi
        windowscodecs
        ole32
        bcrypt
    )
    if(MSVC)
        target_compile_options(fnvxr_openxr_pose_host PRIVATE /W4 /WX /fp:strict)
        target_compile_definitions(fnvxr_openxr_pose_host PRIVATE
            _CRT_SECURE_NO_WARNINGS)
    endif()
    add_test(
        NAME fnvxr_openxr_host_live_fuse
        COMMAND ${CMAKE_COMMAND}
            -DHOST_EXE=$<TARGET_FILE:fnvxr_openxr_pose_host>
            -DHOST_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/host/fnvxr_openxr_pose_host.cpp
            -DHOST_AUTHORITY=${CMAKE_CURRENT_SOURCE_DIR}/host/fnvxr_openxr_live_authority.h
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
    target_link_libraries(nvse_fnvxr PRIVATE
        fnvxr_protocol
        fnvxr_retail_runtime_authority
        dbghelp)
    set_target_properties(nvse_fnvxr PROPERTIES
        PREFIX ""
        OUTPUT_NAME "nvse_fnvxr"
    )

    add_library(fnvxr_d3d9ex_color_transport_win32 STATIC
        renderhook/fnvxr_d3d9ex_color_transport_win32.cpp)
    target_include_directories(fnvxr_d3d9ex_color_transport_win32 PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/renderhook
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol)
    if(MSVC)
        target_compile_options(fnvxr_d3d9ex_color_transport_win32 PRIVATE /W4 /WX)
    endif()

    add_library(fnvxr_retail_ui_quad_capture_win32 STATIC
        renderhook/fnvxr_retail_ui_quad_capture_win32.cpp)
    target_include_directories(fnvxr_retail_ui_quad_capture_win32 PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/renderhook
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime
        ${CMAKE_CURRENT_SOURCE_DIR}/protocol)
    if(MSVC)
        target_compile_options(
            fnvxr_retail_ui_quad_capture_win32 PRIVATE /W4 /WX)
    endif()

    add_library(fnvxr_d3d9_proxy MODULE renderhook/fnvxr_d3d9_proxy.cpp)
    target_include_directories(fnvxr_d3d9_proxy PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/renderhook)
    target_sources(fnvxr_d3d9_proxy PRIVATE renderhook/fnvxr_d3d9_proxy.def)
    target_link_libraries(fnvxr_d3d9_proxy PRIVATE
        dxguid
        bcrypt
        fnvxr_d3d9ex_color_transport_win32
        fnvxr_gpu_color_publisher_win32
        fnvxr_retail_ui_quad_capture_win32
        fnvxr_retail_runtime_authority
        fnvxr_retail_tracked_frame_win32
        fnvxr_retail_world_accumulation_hook_win32)
    if(MSVC)
        target_compile_options(fnvxr_d3d9_proxy PRIVATE /fp:strict /arch:SSE2)
    endif()
    set_target_properties(fnvxr_d3d9_proxy PROPERTIES
        PREFIX ""
        OUTPUT_NAME "d3d9"
    )

    add_executable(fnvxr_d3d9_forwarding_test
        tests/fnvxr_d3d9_forwarding_test.cpp
    )
    add_dependencies(fnvxr_d3d9_forwarding_test fnvxr_d3d9_proxy)
    add_test(
        NAME fnvxr_d3d9_forwarding_test
        COMMAND fnvxr_d3d9_forwarding_test $<TARGET_FILE:fnvxr_d3d9_proxy>
    )

    add_executable(fnvxr_retail_ui_quad_capture_win32_test
        tests/fnvxr_retail_ui_quad_capture_win32_test.cpp)
    target_link_libraries(fnvxr_retail_ui_quad_capture_win32_test PRIVATE
        fnvxr_retail_ui_quad_capture_win32)
    if(MSVC)
        target_compile_options(
            fnvxr_retail_ui_quad_capture_win32_test PRIVATE /W4 /WX)
    endif()
    add_test(
        NAME fnvxr_retail_ui_quad_capture_win32_test
        COMMAND fnvxr_retail_ui_quad_capture_win32_test)

    add_library(fnvxr_xinput_proxy MODULE renderhook/fnvxr_xinput_proxy.cpp)
    target_sources(fnvxr_xinput_proxy PRIVATE renderhook/fnvxr_xinput_proxy.def)
    set_target_properties(fnvxr_xinput_proxy PROPERTIES
        PREFIX ""
        OUTPUT_NAME "xinput1_3"
    )

    add_library(fnvxr_dinput8_proxy MODULE renderhook/fnvxr_dinput8_proxy.cpp)
    target_sources(fnvxr_dinput8_proxy PRIVATE renderhook/fnvxr_dinput8_proxy.def)
    target_link_libraries(fnvxr_dinput8_proxy PRIVATE dxguid)
    if(MSVC)
        # A drop-in dinput8 proxy intentionally preserves the system DLL's
        # COM export ordinals. LINK4222 warns about that valid compatibility
        # requirement, so suppress only this specific linker diagnostic.
        target_link_options(fnvxr_dinput8_proxy PRIVATE /IGNORE:4222)
    endif()
    set_target_properties(fnvxr_dinput8_proxy PROPERTIES
        PREFIX ""
        OUTPUT_NAME "dinput8"
    )

    set(_fnvxr_deterministic_win32_dll_targets
        nvse_fnvxr
        fnvxr_d3d9_proxy
        fnvxr_dinput8_proxy
        fnvxr_xinput_proxy)
    if(MSVC)
        foreach(_fnvxr_deterministic_target IN LISTS
                _fnvxr_deterministic_win32_dll_targets)
            target_compile_options(
                ${_fnvxr_deterministic_target} PRIVATE /Brepro)
            target_link_options(
                ${_fnvxr_deterministic_target} PRIVATE
                /Brepro
                /INCREMENTAL:NO
                "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/PDBALTPATH:$<TARGET_PDB_FILE_NAME:${_fnvxr_deterministic_target}>>")
        endforeach()
    endif()
endif()

set(_fnvxr_expect_deterministic_vcxproj FALSE)
if(WIN32
    AND CMAKE_SIZEOF_VOID_P EQUAL 4
    AND CMAKE_GENERATOR MATCHES "^Visual Studio")
    set(_fnvxr_expect_deterministic_vcxproj TRUE)
endif()
add_test(
    NAME fnvxr_deterministic_win32_artifacts
    COMMAND ${CMAKE_COMMAND}
        -DROOT_CMAKE=${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
        -DPROJECT_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}
        -DEXPECT_VCXPROJ=${_fnvxr_expect_deterministic_vcxproj}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_deterministic_win32_artifacts.cmake
)
unset(_fnvxr_expect_deterministic_vcxproj)

add_executable(fnvxr_d3d9_activation_test tests/fnvxr_d3d9_activation_test.cpp)
target_include_directories(fnvxr_d3d9_activation_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/renderhook
)
add_test(NAME fnvxr_d3d9_activation_test COMMAND fnvxr_d3d9_activation_test)

add_executable(fnvxr_d3d9_eye_targets_test
    tests/fnvxr_d3d9_eye_targets_test.cpp)
target_include_directories(fnvxr_d3d9_eye_targets_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/renderhook
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
if(MSVC)
    target_compile_options(fnvxr_d3d9_eye_targets_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_d3d9_eye_targets_test
    COMMAND fnvxr_d3d9_eye_targets_test)

add_executable(fnvxr_gpu_color_producer_test
    tests/fnvxr_gpu_color_producer_test.cpp)
target_include_directories(fnvxr_gpu_color_producer_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/renderhook
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol
)
if(MSVC)
    target_compile_options(fnvxr_gpu_color_producer_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_gpu_color_producer_test
    COMMAND fnvxr_gpu_color_producer_test)

add_library(fnvxr_gpu_color_publisher_win32 STATIC
    renderhook/fnvxr_gpu_color_publisher_win32.cpp)
target_include_directories(fnvxr_gpu_color_publisher_win32 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/renderhook
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol)
if(MSVC)
    target_compile_options(fnvxr_gpu_color_publisher_win32 PRIVATE /W4 /WX)
endif()

add_executable(fnvxr_gpu_color_publisher_win32_test
    tests/fnvxr_gpu_color_publisher_win32_test.cpp)
target_link_libraries(fnvxr_gpu_color_publisher_win32_test PRIVATE
    fnvxr_gpu_color_publisher_win32)
if(MSVC)
    target_compile_options(
        fnvxr_gpu_color_publisher_win32_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_gpu_color_publisher_win32_test
    COMMAND fnvxr_gpu_color_publisher_win32_test)

add_executable(fnvxr_retail_vr_bridge_win32_test
    tests/fnvxr_retail_vr_bridge_win32_test.cpp)
target_include_directories(fnvxr_retail_vr_bridge_win32_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/renderhook
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol)
target_link_libraries(fnvxr_retail_vr_bridge_win32_test PRIVATE
    fnvxr_gpu_color_publisher_win32
    fnvxr_retail_runtime_authority
    fnvxr_retail_tracked_frame_win32
    fnvxr_retail_world_accumulation_hook_win32)
if(MSVC)
    target_compile_options(
        fnvxr_retail_vr_bridge_win32_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_vr_bridge_win32_test
    COMMAND fnvxr_retail_vr_bridge_win32_test)

add_executable(fnvxr_d3d9ex_game_creation_test
    tests/fnvxr_d3d9ex_game_creation_test.cpp)
target_include_directories(fnvxr_d3d9ex_game_creation_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/renderhook)
if(MSVC)
    target_compile_options(fnvxr_d3d9ex_game_creation_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_d3d9ex_game_creation_test
    COMMAND fnvxr_d3d9ex_game_creation_test)

if(WIN32)
    add_executable(fnvxr_d3d11_fence_only_nt_handle_test
        tests/fnvxr_d3d11_fence_only_nt_handle_test.cpp)
    target_link_libraries(fnvxr_d3d11_fence_only_nt_handle_test PRIVATE
        d3d11
        dxgi)
    if(MSVC)
        target_compile_options(fnvxr_d3d11_fence_only_nt_handle_test PRIVATE
            /W4 /WX)
    endif()
    add_test(
        NAME fnvxr_d3d11_fence_only_nt_handle_test
        COMMAND fnvxr_d3d11_fence_only_nt_handle_test)
endif()

add_test(
    NAME fnvxr_d3d9_proxy_inert_fuse
    COMMAND ${CMAKE_COMMAND}
        -DPROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/renderhook/fnvxr_d3d9_proxy.cpp
        -DACTIVATION_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/renderhook/fnvxr_d3d9_activation.h
        -DBRIDGE_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/renderhook/fnvxr_retail_vr_bridge_win32.h
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_d3d9_proxy_inert_fuse.cmake
)

add_test(
    NAME fnvxr_retail_runtime_publication_bootstrap_fuse
    COMMAND ${CMAKE_COMMAND}
        -DPROXY_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/renderhook/fnvxr_d3d9_proxy.cpp
        -DPLUGIN_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/plugin/fnvxr_nvse_plugin.cpp
        -DCONTRACT_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/runtime/fnvxr_retail_runtime_publication.h
        -DREADER_HEADER=${CMAKE_CURRENT_SOURCE_DIR}/runtime/fnvxr_retail_runtime_publication_win32.h
        -DREADER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/runtime/fnvxr_retail_runtime_publication_win32.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_retail_runtime_publication_bootstrap_fuse.cmake
)

add_test(
    NAME fnvxr_retail_ui_quad_capture_source_fuse
    COMMAND ${CMAKE_COMMAND}
        -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_retail_ui_quad_capture_source_fuse.cmake
)

add_executable(fnvxr_protocol_layout_test tests/fnvxr_protocol_layout_test.cpp)
target_link_libraries(fnvxr_protocol_layout_test PRIVATE fnvxr_protocol)
add_test(NAME fnvxr_protocol_layout_test COMMAND fnvxr_protocol_layout_test)

add_executable(fnvxr_product_contract_test tests/fnvxr_product_contract_test.cpp)
target_link_libraries(fnvxr_product_contract_test PRIVATE fnvxr_protocol)
add_test(NAME fnvxr_product_contract_test COMMAND fnvxr_product_contract_test)

add_executable(
    fnvxr_product_capabilities_test
    tests/fnvxr_product_capabilities_test.cpp)
target_link_libraries(fnvxr_product_capabilities_test PRIVATE fnvxr_protocol)
if(MSVC)
    target_compile_options(fnvxr_product_capabilities_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_product_capabilities_test
    COMMAND fnvxr_product_capabilities_test)

add_test(
    NAME fnvxr_phase0_records
    COMMAND ${CMAKE_COMMAND}
        -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_phase0_records.cmake)

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

add_executable(fnvxr_center_renderer_backend_test
    tests/fnvxr_center_renderer_backend_test.cpp)
target_include_directories(fnvxr_center_renderer_backend_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
if(MSVC)
    target_compile_options(fnvxr_center_renderer_backend_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_center_renderer_backend_test
    COMMAND fnvxr_center_renderer_backend_test)

add_executable(fnvxr_engine_stereo_resources_test
    tests/fnvxr_engine_stereo_resources_test.cpp
)
target_include_directories(fnvxr_engine_stereo_resources_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
if(MSVC)
    target_compile_options(fnvxr_engine_stereo_resources_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_engine_stereo_resources_test
    COMMAND fnvxr_engine_stereo_resources_test
)

add_executable(fnvxr_engine_stereo_resources_inert_test
    tests/fnvxr_engine_stereo_resources_inert_test.cpp
)
target_include_directories(fnvxr_engine_stereo_resources_inert_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
if(MSVC)
    target_compile_options(
        fnvxr_engine_stereo_resources_inert_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_engine_stereo_resources_inert_test
    COMMAND fnvxr_engine_stereo_resources_inert_test
)
add_test(
    NAME fnvxr_engine_stereo_authority_fuse
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_stereo_resource_authority_fuse.cmake
)

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

add_executable(fnvxr_module_inventory_acceptance_test
    tests/fnvxr_module_inventory_acceptance_test.cpp)
target_include_directories(fnvxr_module_inventory_acceptance_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
add_test(
    NAME fnvxr_module_inventory_acceptance_test
    COMMAND fnvxr_module_inventory_acceptance_test)

add_executable(fnvxr_windows_module_census_test
    tests/fnvxr_windows_module_census_test.cpp)
target_include_directories(fnvxr_windows_module_census_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
)
if(MSVC)
    target_compile_options(fnvxr_windows_module_census_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_windows_module_census_test
    COMMAND fnvxr_windows_module_census_test)

add_library(fnvxr_retail_abi_revalidator STATIC
    runtime/fnvxr_retail_abi_revalidator.cpp)
target_include_directories(fnvxr_retail_abi_revalidator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
)
if(WIN32)
    target_link_libraries(fnvxr_retail_abi_revalidator PRIVATE bcrypt)
endif()
if(MSVC)
    target_compile_options(fnvxr_retail_abi_revalidator PRIVATE /W4 /WX)
endif()

add_executable(fnvxr_retail_abi_revalidator_test
    tests/fnvxr_retail_abi_revalidator_test.cpp
    runtime/fnvxr_retail_abi_revalidator.cpp
    runtime/fnvxr_retail_compatibility_proof.cpp
)
target_include_directories(fnvxr_retail_abi_revalidator_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
)
target_compile_definitions(fnvxr_retail_abi_revalidator_test PRIVATE
    FNVXR_RETAIL_ABI_REVALIDATOR_TEST_AUTHORITY=1)
if(WIN32)
    target_link_libraries(fnvxr_retail_abi_revalidator_test PRIVATE bcrypt)
endif()
if(MSVC)
    target_compile_options(fnvxr_retail_abi_revalidator_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_abi_revalidator_test
    COMMAND fnvxr_retail_abi_revalidator_test)

add_library(fnvxr_retail_compatibility_proof STATIC
    runtime/fnvxr_retail_compatibility_proof.cpp)
target_include_directories(fnvxr_retail_compatibility_proof PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
if(WIN32)
    target_link_libraries(fnvxr_retail_compatibility_proof PRIVATE bcrypt)
endif()
if(MSVC)
    target_compile_options(fnvxr_retail_compatibility_proof PRIVATE /W4 /WX)
endif()

add_executable(fnvxr_retail_compatibility_proof_test
    tests/fnvxr_retail_compatibility_proof_test.cpp
    runtime/fnvxr_retail_compatibility_proof.cpp)
target_include_directories(fnvxr_retail_compatibility_proof_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
target_compile_definitions(fnvxr_retail_compatibility_proof_test PRIVATE
    FNVXR_RETAIL_COMPATIBILITY_PROOF_TEST_AUTHORITY=1)
if(WIN32)
    target_link_libraries(fnvxr_retail_compatibility_proof_test PRIVATE bcrypt)
endif()
if(MSVC)
    target_compile_options(
        fnvxr_retail_compatibility_proof_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_compatibility_proof_test
    COMMAND fnvxr_retail_compatibility_proof_test)

target_link_libraries(fnvxr_retail_abi_revalidator PRIVATE
    fnvxr_retail_compatibility_proof)

add_library(fnvxr_retail_runtime_authority STATIC
    runtime/fnvxr_retail_runtime_authority.cpp)
target_include_directories(fnvxr_retail_runtime_authority PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
target_link_libraries(fnvxr_retail_runtime_authority PRIVATE
    fnvxr_retail_abi_revalidator
    fnvxr_retail_compatibility_proof)
if(MSVC)
    target_compile_options(fnvxr_retail_runtime_authority PRIVATE /W4 /WX)
endif()

add_executable(fnvxr_retail_runtime_authority_test
    tests/fnvxr_retail_runtime_authority_test.cpp
    runtime/fnvxr_retail_runtime_authority.cpp)
target_include_directories(fnvxr_retail_runtime_authority_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
target_compile_definitions(fnvxr_retail_runtime_authority_test PRIVATE
    FNVXR_RETAIL_RUNTIME_AUTHORITY_TEST_AUTHORITY=1)
target_link_libraries(fnvxr_retail_runtime_authority_test PRIVATE
    fnvxr_retail_abi_revalidator
    fnvxr_retail_compatibility_proof)
if(MSVC)
    target_compile_options(
        fnvxr_retail_runtime_authority_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_runtime_authority_test
    COMMAND fnvxr_retail_runtime_authority_test)

add_executable(fnvxr_retail_abi_map_test tests/fnvxr_retail_abi_map_test.cpp)
target_include_directories(fnvxr_retail_abi_map_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
)
add_test(NAME fnvxr_retail_abi_map_test COMMAND fnvxr_retail_abi_map_test)

add_executable(fnvxr_retail_engine_abi_test tests/fnvxr_retail_engine_abi_test.cpp)
target_include_directories(fnvxr_retail_engine_abi_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
add_test(NAME fnvxr_retail_engine_abi_test COMMAND fnvxr_retail_engine_abi_test)

add_executable(fnvxr_retail_engine_calls_test
    tests/fnvxr_retail_engine_calls_test.cpp)
target_include_directories(fnvxr_retail_engine_calls_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
if(MSVC)
    target_compile_options(fnvxr_retail_engine_calls_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_engine_calls_test
    COMMAND fnvxr_retail_engine_calls_test)

add_executable(fnvxr_retail_engine_resource_adapter_test
    tests/fnvxr_retail_engine_resource_adapter_test.cpp)
target_include_directories(fnvxr_retail_engine_resource_adapter_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
if(MSVC)
    target_compile_options(
        fnvxr_retail_engine_resource_adapter_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_engine_resource_adapter_test
    COMMAND fnvxr_retail_engine_resource_adapter_test)

add_executable(fnvxr_retail_center_renderer_operations_test
    tests/fnvxr_retail_center_renderer_operations_test.cpp)
target_include_directories(fnvxr_retail_center_renderer_operations_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
if(MSVC)
    target_compile_options(
        fnvxr_retail_center_renderer_operations_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_center_renderer_operations_test
    COMMAND fnvxr_retail_center_renderer_operations_test)

add_executable(fnvxr_retail_center_runtime_test
    tests/fnvxr_retail_center_runtime_test.cpp)
target_include_directories(fnvxr_retail_center_runtime_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
if(MSVC)
    target_compile_options(
        fnvxr_retail_center_runtime_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_center_runtime_test
    COMMAND fnvxr_retail_center_runtime_test)

add_executable(fnvxr_retail_eye_camera_transaction_test
    tests/fnvxr_retail_eye_camera_transaction_test.cpp)
target_include_directories(fnvxr_retail_eye_camera_transaction_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/renderhook)
if(MSVC)
    target_compile_options(
        fnvxr_retail_eye_camera_transaction_test PRIVATE /W4 /WX /fp:strict)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_compile_options(
            fnvxr_retail_eye_camera_transaction_test PRIVATE /arch:SSE2)
    endif()
endif()
add_test(
    NAME fnvxr_retail_eye_camera_transaction_test
    COMMAND fnvxr_retail_eye_camera_transaction_test)

add_executable(fnvxr_retail_tracked_frame_test
    tests/fnvxr_retail_tracked_frame_test.cpp)
target_include_directories(fnvxr_retail_tracked_frame_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol)
if(MSVC)
    target_compile_options(fnvxr_retail_tracked_frame_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_tracked_frame_test
    COMMAND fnvxr_retail_tracked_frame_test)

add_executable(fnvxr_retail_runtime_publication_test
    tests/fnvxr_retail_runtime_publication_test.cpp)
target_include_directories(fnvxr_retail_runtime_publication_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol)
if(MSVC)
    target_compile_options(
        fnvxr_retail_runtime_publication_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_runtime_publication_test
    COMMAND fnvxr_retail_runtime_publication_test)

add_library(fnvxr_retail_tracked_frame_win32 STATIC
    runtime/fnvxr_retail_tracked_frame_win32.cpp
    runtime/fnvxr_retail_runtime_publication_win32.cpp)
target_include_directories(fnvxr_retail_tracked_frame_win32 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol)
if(MSVC)
    target_compile_options(fnvxr_retail_tracked_frame_win32 PRIVATE /W4 /WX)
endif()

add_executable(fnvxr_retail_tracked_frame_win32_test
    tests/fnvxr_retail_tracked_frame_win32_test.cpp)
target_link_libraries(fnvxr_retail_tracked_frame_win32_test PRIVATE
    fnvxr_retail_tracked_frame_win32)
if(MSVC)
    target_compile_options(
        fnvxr_retail_tracked_frame_win32_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_tracked_frame_win32_test
    COMMAND fnvxr_retail_tracked_frame_win32_test)

add_executable(fnvxr_retail_world_hook_contract_test
    tests/fnvxr_retail_world_hook_contract_test.cpp)
target_include_directories(fnvxr_retail_world_hook_contract_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
if(MSVC)
    target_compile_options(
        fnvxr_retail_world_hook_contract_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_world_hook_contract_test
    COMMAND fnvxr_retail_world_hook_contract_test)

add_executable(fnvxr_retail_world_controller_test
    tests/fnvxr_retail_world_controller_test.cpp)
target_include_directories(fnvxr_retail_world_controller_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol)
if(MSVC)
    target_compile_options(fnvxr_retail_world_controller_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_world_controller_test
    COMMAND fnvxr_retail_world_controller_test)

add_executable(fnvxr_retail_world_accumulation_controller_test
    tests/fnvxr_retail_world_accumulation_controller_test.cpp)
target_include_directories(fnvxr_retail_world_accumulation_controller_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
if(MSVC)
    target_compile_options(
        fnvxr_retail_world_accumulation_controller_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_world_accumulation_controller_test
    COMMAND fnvxr_retail_world_accumulation_controller_test)

add_executable(fnvxr_presentation_route_transition_test
    tests/fnvxr_presentation_route_transition_test.cpp)
target_include_directories(fnvxr_presentation_route_transition_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/renderhook
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol)
if(MSVC)
    target_compile_options(
        fnvxr_presentation_route_transition_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_presentation_route_transition_test
    COMMAND fnvxr_presentation_route_transition_test)

add_executable(fnvxr_cpu_engine_presentation_test
    tests/fnvxr_cpu_engine_presentation_test.cpp)
target_include_directories(fnvxr_cpu_engine_presentation_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/host
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol)
if(MSVC)
    target_compile_options(
        fnvxr_cpu_engine_presentation_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_cpu_engine_presentation_test
    COMMAND fnvxr_cpu_engine_presentation_test)

add_executable(fnvxr_game_plane_surface_test
    tests/fnvxr_game_plane_surface_test.cpp)
target_include_directories(fnvxr_game_plane_surface_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/host)
if(MSVC)
    target_compile_options(
        fnvxr_game_plane_surface_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_game_plane_surface_test
    COMMAND fnvxr_game_plane_surface_test)

add_executable(fnvxr_retail_world_hook_lease_test
    tests/fnvxr_retail_world_hook_lease_test.cpp)
target_include_directories(fnvxr_retail_world_hook_lease_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
if(MSVC)
    target_compile_options(
        fnvxr_retail_world_hook_lease_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_world_hook_lease_test
    COMMAND fnvxr_retail_world_hook_lease_test)

add_library(fnvxr_retail_world_hook_win32 STATIC
    runtime/fnvxr_retail_world_hook_win32.cpp)
target_include_directories(fnvxr_retail_world_hook_win32 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
if(MSVC)
    target_compile_options(fnvxr_retail_world_hook_win32 PRIVATE /W4 /WX)
endif()

add_executable(fnvxr_retail_world_hook_win32_test
    tests/fnvxr_retail_world_hook_win32_test.cpp)
target_link_libraries(fnvxr_retail_world_hook_win32_test PRIVATE
    fnvxr_retail_world_hook_win32)
if(MSVC)
    target_compile_options(
        fnvxr_retail_world_hook_win32_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_world_hook_win32_test
    COMMAND fnvxr_retail_world_hook_win32_test)

add_library(fnvxr_retail_world_accumulation_hook_win32 STATIC
    runtime/fnvxr_retail_world_accumulation_hook_win32.cpp)
target_include_directories(fnvxr_retail_world_accumulation_hook_win32 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
if(MSVC)
    target_compile_options(
        fnvxr_retail_world_accumulation_hook_win32 PRIVATE /W4 /WX)
endif()

add_executable(fnvxr_retail_world_accumulation_hook_lease_test
    tests/fnvxr_retail_world_accumulation_hook_lease_test.cpp)
target_include_directories(
    fnvxr_retail_world_accumulation_hook_lease_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
if(MSVC)
    target_compile_options(
        fnvxr_retail_world_accumulation_hook_lease_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_world_accumulation_hook_lease_test
    COMMAND fnvxr_retail_world_accumulation_hook_lease_test)

add_executable(fnvxr_retail_first_person_hook_lease_test
    tests/fnvxr_retail_first_person_hook_lease_test.cpp)
target_include_directories(
    fnvxr_retail_first_person_hook_lease_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
if(MSVC)
    target_compile_options(
        fnvxr_retail_first_person_hook_lease_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_first_person_hook_lease_test
    COMMAND fnvxr_retail_first_person_hook_lease_test)

add_executable(fnvxr_retail_world_accumulation_hook_win32_test
    tests/fnvxr_retail_world_accumulation_hook_win32_test.cpp)
target_link_libraries(fnvxr_retail_world_accumulation_hook_win32_test PRIVATE
    fnvxr_retail_world_accumulation_hook_win32)
if(MSVC)
    target_compile_options(
        fnvxr_retail_world_accumulation_hook_win32_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_world_accumulation_hook_win32_test
    COMMAND fnvxr_retail_world_accumulation_hook_win32_test)

add_executable(fnvxr_private_geometry_collector_test
    tests/fnvxr_private_geometry_collector_test.cpp
)
target_include_directories(fnvxr_private_geometry_collector_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
)
add_test(
    NAME fnvxr_private_geometry_collector_test
    COMMAND fnvxr_private_geometry_collector_test
)

add_executable(fnvxr_jip_normalization_test tests/fnvxr_jip_normalization_test.cpp)
target_include_directories(fnvxr_jip_normalization_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
)
if(WIN32)
    target_link_libraries(fnvxr_jip_normalization_test PRIVATE bcrypt)
endif()

add_test(NAME fnvxr_jip_normalization_test COMMAND fnvxr_jip_normalization_test)

add_executable(fnvxr_showoff_compatibility_test
    tests/fnvxr_showoff_compatibility_test.cpp)
target_include_directories(fnvxr_showoff_compatibility_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
)
add_test(
    NAME fnvxr_showoff_compatibility_test
    COMMAND fnvxr_showoff_compatibility_test)

add_executable(fnvxr_mapped_image_proof_test
    tests/fnvxr_mapped_image_proof_test.cpp)
target_include_directories(fnvxr_mapped_image_proof_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/tools
)
if(MSVC)
    target_compile_options(fnvxr_mapped_image_proof_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_mapped_image_proof_test
    COMMAND fnvxr_mapped_image_proof_test)

if(WIN32)
    add_executable(fnvxr_probe_module_lookup_test tests/fnvxr_probe_module_lookup_test.cpp)
    target_include_directories(fnvxr_probe_module_lookup_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tools
    )
    add_test(
        NAME fnvxr_probe_module_lookup_test
        COMMAND fnvxr_probe_module_lookup_test
    )

    add_executable(fnvxr_retail_runtime_probe tools/fnvxr_retail_runtime_probe.cpp)
    target_include_directories(fnvxr_retail_runtime_probe PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime
    )
    target_link_libraries(fnvxr_retail_runtime_probe PRIVATE bcrypt)
    add_test(
        NAME fnvxr_retail_runtime_probe_help
        COMMAND fnvxr_retail_runtime_probe --help
    )
    add_test(
        NAME fnvxr_retail_runtime_probe_dump_guard
        COMMAND ${CMAKE_COMMAND}
            -DPROBE=$<TARGET_FILE:fnvxr_retail_runtime_probe>
            -DEXISTING_DIR=${CMAKE_CURRENT_BINARY_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_retail_runtime_probe_dump_guard.cmake
    )
endif()

add_executable(fnvxr_gpu_transport_test tests/fnvxr_gpu_transport_test.cpp)
target_link_libraries(fnvxr_gpu_transport_test PRIVATE fnvxr_protocol)
add_test(NAME fnvxr_gpu_transport_test COMMAND fnvxr_gpu_transport_test)

add_executable(fnvxr_gpu_color_transport_test
    tests/fnvxr_gpu_color_transport_test.cpp)
target_link_libraries(fnvxr_gpu_color_transport_test PRIVATE fnvxr_protocol)
if(MSVC)
    target_compile_options(fnvxr_gpu_color_transport_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_gpu_color_transport_test
    COMMAND fnvxr_gpu_color_transport_test)

add_library(fnvxr_gpu_color_consumer STATIC
    host/fnvxr_gpu_color_consumer.cpp)
target_include_directories(fnvxr_gpu_color_consumer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/host
    ${CMAKE_CURRENT_SOURCE_DIR}/protocol)
if(WIN32)
    target_link_libraries(fnvxr_gpu_color_consumer PUBLIC d3d11 dxgi)
endif()
if(MSVC)
    target_compile_options(fnvxr_gpu_color_consumer PRIVATE /W4 /WX)
endif()

add_executable(fnvxr_gpu_color_consumer_test
    tests/fnvxr_gpu_color_consumer_test.cpp)
target_link_libraries(fnvxr_gpu_color_consumer_test PRIVATE
    fnvxr_gpu_color_consumer)
if(MSVC)
    target_compile_options(fnvxr_gpu_color_consumer_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_gpu_color_consumer_test
    COMMAND fnvxr_gpu_color_consumer_test)

add_executable(fnvxr_gpu_color_route_test
    tests/fnvxr_gpu_color_route_test.cpp)
target_link_libraries(fnvxr_gpu_color_route_test PRIVATE
    fnvxr_gpu_color_consumer)
if(MSVC)
    target_compile_options(fnvxr_gpu_color_route_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_gpu_color_route_test
    COMMAND fnvxr_gpu_color_route_test)

add_executable(fnvxr_host_ui_capture_gate_test
    tests/fnvxr_host_ui_capture_gate_test.cpp)
target_link_libraries(fnvxr_host_ui_capture_gate_test PRIVATE fnvxr_protocol)
if(MSVC)
    target_compile_options(fnvxr_host_ui_capture_gate_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_host_ui_capture_gate_test
    COMMAND fnvxr_host_ui_capture_gate_test)

add_executable(fnvxr_retail_observation_authority_test
    tests/fnvxr_retail_observation_authority_test.cpp)
target_include_directories(fnvxr_retail_observation_authority_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
if(MSVC)
    target_compile_options(
        fnvxr_retail_observation_authority_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_observation_authority_test
    COMMAND fnvxr_retail_observation_authority_test)

add_executable(
    fnvxr_desktop_assist_authority_test
    tests/fnvxr_desktop_assist_authority_test.cpp)
target_include_directories(fnvxr_desktop_assist_authority_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
if(MSVC)
    target_compile_options(
        fnvxr_desktop_assist_authority_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_desktop_assist_authority_test
    COMMAND fnvxr_desktop_assist_authority_test)

add_executable(
    fnvxr_tracked_prop_assist_authority_test
    tests/fnvxr_tracked_prop_assist_authority_test.cpp)
target_include_directories(fnvxr_tracked_prop_assist_authority_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
if(MSVC)
    target_compile_options(
        fnvxr_tracked_prop_assist_authority_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_tracked_prop_assist_authority_test
    COMMAND fnvxr_tracked_prop_assist_authority_test)

add_executable(
    fnvxr_desktop_assist_automation_authority_test
    tests/fnvxr_desktop_assist_automation_authority_test.cpp)
target_include_directories(fnvxr_desktop_assist_automation_authority_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
if(MSVC)
    target_compile_options(
        fnvxr_desktop_assist_automation_authority_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_desktop_assist_automation_authority_test
    COMMAND fnvxr_desktop_assist_automation_authority_test)

add_executable(
    fnvxr_stereo_visual_trial_automation_authority_test
    tests/fnvxr_stereo_visual_trial_automation_authority_test.cpp)
target_include_directories(
    fnvxr_stereo_visual_trial_automation_authority_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
target_link_libraries(
    fnvxr_stereo_visual_trial_automation_authority_test PRIVATE fnvxr_protocol)
if(MSVC)
    target_compile_options(
        fnvxr_stereo_visual_trial_automation_authority_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_stereo_visual_trial_automation_authority_test
    COMMAND fnvxr_stereo_visual_trial_automation_authority_test)

add_executable(
    fnvxr_retail_fixture_automation_authority_test
    tests/fnvxr_retail_fixture_automation_authority_test.cpp)
target_include_directories(
    fnvxr_retail_fixture_automation_authority_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
if(MSVC)
    target_compile_options(
        fnvxr_retail_fixture_automation_authority_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_retail_fixture_automation_authority_test
    COMMAND fnvxr_retail_fixture_automation_authority_test)

add_executable(
    fnvxr_desktop_assist_ui_authority_test
    tests/fnvxr_desktop_assist_ui_authority_test.cpp)
target_include_directories(fnvxr_desktop_assist_ui_authority_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
if(MSVC)
    target_compile_options(
        fnvxr_desktop_assist_ui_authority_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_desktop_assist_ui_authority_test
    COMMAND fnvxr_desktop_assist_ui_authority_test)

add_executable(
    fnvxr_desktop_assist_ui_evidence_test
    tests/fnvxr_desktop_assist_ui_evidence_test.cpp)
target_link_libraries(fnvxr_desktop_assist_ui_evidence_test PRIVATE fnvxr_protocol)
if(MSVC)
    target_compile_options(
        fnvxr_desktop_assist_ui_evidence_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_desktop_assist_ui_evidence_test
    COMMAND fnvxr_desktop_assist_ui_evidence_test)

add_executable(fnvxr_stereo_visual_trial_test
    tests/fnvxr_stereo_visual_trial_test.cpp)
target_link_libraries(fnvxr_stereo_visual_trial_test PRIVATE
    fnvxr_gpu_color_consumer)
if(MSVC)
    target_compile_options(fnvxr_stereo_visual_trial_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_stereo_visual_trial_test
    COMMAND fnvxr_stereo_visual_trial_test)

add_executable(fnvxr_input_proxy_safety_test
    tests/fnvxr_input_proxy_safety_test.cpp)
target_include_directories(fnvxr_input_proxy_safety_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/renderhook
)
if(MSVC)
    target_compile_options(fnvxr_input_proxy_safety_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_input_proxy_safety_test
    COMMAND fnvxr_input_proxy_safety_test
)
add_test(
    NAME fnvxr_input_proxy_inert_fuse
    COMMAND ${CMAKE_COMMAND}
        -DDINPUT_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/renderhook/fnvxr_dinput8_proxy.cpp
        -DXINPUT_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/renderhook/fnvxr_xinput_proxy.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_input_proxy_inert_fuse.cmake
)
add_test(
    NAME fnvxr_retail_input_weapon_inert_fuse
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_retail_input_weapon_inert_fuse.cmake
)
add_test(
    NAME fnvxr_desktop_assist_camera_fuse
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_desktop_assist_camera_fuse.cmake
)
add_test(
    NAME fnvxr_tracked_prop_assist_fuse
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_tracked_prop_assist_fuse.cmake
)
add_test(
    NAME fnvxr_desktop_assist_preflight_fuse
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_desktop_assist_preflight.cmake
)
add_test(
    NAME fnvxr_desktop_assist_launcher_fuse
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_desktop_assist_launcher.cmake
)
add_test(
    NAME fnvxr_desktop_assist_ui_fuse
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/assert_desktop_assist_ui_fuse.cmake
)

add_executable(fnvxr_shared_state_probe tools/fnvxr_shared_state_probe.cpp)
target_link_libraries(fnvxr_shared_state_probe PRIVATE fnvxr_protocol)
add_test(NAME fnvxr_shared_state_probe_smoke COMMAND fnvxr_shared_state_probe)

add_executable(
    fnvxr_shared_state_probe_ui_quad_test
    tests/fnvxr_shared_state_probe_ui_quad_test.cpp)
target_link_libraries(fnvxr_shared_state_probe_ui_quad_test PRIVATE fnvxr_protocol)
add_dependencies(fnvxr_shared_state_probe_ui_quad_test fnvxr_shared_state_probe)
if(MSVC)
    target_compile_options(
        fnvxr_shared_state_probe_ui_quad_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_shared_state_probe_ui_quad_test
    COMMAND fnvxr_shared_state_probe_ui_quad_test $<TARGET_FILE:fnvxr_shared_state_probe>)

add_executable(
    fnvxr_shared_state_probe_desktop_assist_ready_test
    tests/fnvxr_shared_state_probe_desktop_assist_ready_test.cpp)
target_link_libraries(fnvxr_shared_state_probe_desktop_assist_ready_test PRIVATE fnvxr_protocol)
add_dependencies(fnvxr_shared_state_probe_desktop_assist_ready_test fnvxr_shared_state_probe)
if(MSVC)
    target_compile_options(
        fnvxr_shared_state_probe_desktop_assist_ready_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_shared_state_probe_desktop_assist_ready_test
    COMMAND fnvxr_shared_state_probe_desktop_assist_ready_test $<TARGET_FILE:fnvxr_shared_state_probe>)

add_executable(
    fnvxr_shared_state_probe_stereo_test
    tests/fnvxr_shared_state_probe_stereo_test.cpp)
target_link_libraries(fnvxr_shared_state_probe_stereo_test PRIVATE fnvxr_protocol)
add_dependencies(fnvxr_shared_state_probe_stereo_test fnvxr_shared_state_probe)
if(MSVC)
    target_compile_options(
        fnvxr_shared_state_probe_stereo_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_shared_state_probe_stereo_test
    COMMAND fnvxr_shared_state_probe_stereo_test $<TARGET_FILE:fnvxr_shared_state_probe>)

add_executable(
    fnvxr_shared_state_probe_ui_then_world_test
    tests/fnvxr_shared_state_probe_ui_then_world_test.cpp)
target_link_libraries(
    fnvxr_shared_state_probe_ui_then_world_test PRIVATE fnvxr_protocol)
add_dependencies(
    fnvxr_shared_state_probe_ui_then_world_test fnvxr_shared_state_probe)
if(MSVC)
    target_compile_options(
        fnvxr_shared_state_probe_ui_then_world_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_shared_state_probe_ui_then_world_test
    COMMAND fnvxr_shared_state_probe_ui_then_world_test $<TARGET_FILE:fnvxr_shared_state_probe>)
set_tests_properties(
    fnvxr_shared_state_probe_stereo_test
    fnvxr_shared_state_probe_ui_then_world_test
    PROPERTIES RESOURCE_LOCK fnvxr_d3d9_stereo_frame_mapping)

add_executable(fnvxr_pose_fixture tools/fnvxr_pose_fixture.cpp)
target_link_libraries(fnvxr_pose_fixture PRIVATE fnvxr_protocol)
add_test(NAME fnvxr_pose_fixture_help COMMAND fnvxr_pose_fixture --help)

add_executable(fnvxr_assist tools/fnvxr_assist.cpp)
target_link_libraries(fnvxr_assist PRIVATE fnvxr_protocol)
target_include_directories(fnvxr_assist PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tools)
if(MSVC)
    target_compile_options(fnvxr_assist PRIVATE /W4 /WX)
endif()
add_test(NAME fnvxr_assist_validate_scenario COMMAND fnvxr_assist --validate-scenario)
add_test(NAME fnvxr_assist_self_test COMMAND fnvxr_assist --self-test)
add_test(
    NAME fnvxr_assist_head_body_fixture
    COMMAND fnvxr_assist --scenario head-body --step-ms 1 --period-ms 1 --cycles 1 --no-observe)
add_test(
    NAME fnvxr_assist_tracked_prop_fixture
    COMMAND fnvxr_assist --scenario head-body --tracked-prop --step-ms 1 --period-ms 1 --cycles 1 --no-observe)

add_executable(fnvxr_assist_scenario_test tests/fnvxr_assist_scenario_test.cpp)
target_include_directories(fnvxr_assist_scenario_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tools)
if(MSVC)
    target_compile_options(fnvxr_assist_scenario_test PRIVATE /W4 /WX)
endif()
add_test(NAME fnvxr_assist_scenario_test COMMAND fnvxr_assist_scenario_test)

add_executable(
    fnvxr_assist_mapping_integration_test
    tests/fnvxr_assist_mapping_integration_test.cpp)
target_link_libraries(fnvxr_assist_mapping_integration_test PRIVATE fnvxr_protocol)
add_dependencies(fnvxr_assist_mapping_integration_test fnvxr_assist)
if(MSVC)
    target_compile_options(fnvxr_assist_mapping_integration_test PRIVATE /W4 /WX)
endif()
add_test(
    NAME fnvxr_assist_mapping_integration_test
    COMMAND fnvxr_assist_mapping_integration_test $<TARGET_FILE:fnvxr_assist>)
# The test owns the same process-wide named mappings as a live assist run.
# Keep it alone even when a caller asks CTest to run tests in parallel.
set_tests_properties(
    fnvxr_assist_mapping_integration_test
    PROPERTIES RUN_SERIAL TRUE TIMEOUT 30)

add_executable(fnvxr_command tools/fnvxr_command.cpp)
target_include_directories(fnvxr_command PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/runtime)
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

# The former preprocessor test authority changed inline header definitions and
# could become a live authorization bypass if copied to a production target.
# It is forbidden on every target at configure time; the isolated test issuer
# is now a single test-source definition instead.
string(CONCAT _fnvxr_forbidden_stereo_authority_macro
    "FNVXR_STEREO_RESOURCE_"
    "LIFECYCLE_TEST_AUTHORITY")
get_property(_fnvxr_all_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(_fnvxr_target IN LISTS _fnvxr_all_targets)
    get_target_property(
        _fnvxr_target_definitions
        ${_fnvxr_target}
        COMPILE_DEFINITIONS)
    if(_fnvxr_target_definitions)
        string(JOIN ";" _fnvxr_joined_definitions
            ${_fnvxr_target_definitions})
        string(FIND
            "${_fnvxr_joined_definitions}"
            "${_fnvxr_forbidden_stereo_authority_macro}"
            _fnvxr_forbidden_definition_index)
        if(NOT _fnvxr_forbidden_definition_index EQUAL -1)
            message(FATAL_ERROR
                "Forbidden stereo test authority macro on target "
                "${_fnvxr_target}")
        endif()
    endif()
endforeach()
