if(NOT DEFINED PROXY_SOURCE OR NOT EXISTS "${PROXY_SOURCE}")
    message(FATAL_ERROR "Missing D3D9 proxy source: ${PROXY_SOURCE}")
endif()

if(NOT DEFINED ACTIVATION_HEADER OR NOT EXISTS "${ACTIVATION_HEADER}")
    message(FATAL_ERROR "Missing D3D9 activation contract: ${ACTIVATION_HEADER}")
endif()

file(READ "${PROXY_SOURCE}" proxy_source)
file(READ "${ACTIVATION_HEADER}" activation_header)
string(REPLACE "\r\n" "\n" proxy_source "${proxy_source}")
string(REPLACE "\r\n" "\n" activation_header "${activation_header}")

function(require_text haystack needle reason)
    string(FIND "${haystack}" "${needle}" found_at)
    if(found_at EQUAL -1)
        message(FATAL_ERROR "${reason}: missing '${needle}'")
    endif()
endfunction()

function(extract_region output haystack begin_marker end_marker)
    string(FIND "${haystack}" "${begin_marker}" begin_at)
    string(FIND "${haystack}" "${end_marker}" end_at)
    if(begin_at EQUAL -1 OR end_at EQUAL -1 OR end_at LESS_EQUAL begin_at)
        message(FATAL_ERROR
            "Could not isolate source region '${begin_marker}' through '${end_marker}'")
    endif()
    math(EXPR region_length "${end_at} - ${begin_at}")
    string(SUBSTRING "${haystack}" ${begin_at} ${region_length} region)
    set(${output} "${region}" PARENT_SCOPE)
endfunction()

require_text(
    "${activation_header}"
    "inline constexpr ProductionRendererProof CompiledProductionRendererProof {}"
    "The checked-in D3D9 renderer proof must initialize every gate false")
require_text(
    "${activation_header}"
    "inline constexpr bool ProductionRendererAuthorized ="
    "The D3D9 side-effect decision must be compile-time data")

require_text(
    "${proxy_source}"
    "#include \"fnvxr_d3d9_activation.h\""
    "The proxy must consume the pure activation contract")
require_text(
    "${proxy_source}"
    "if constexpr (!fnvxr::d3d9::ProductionRendererAuthorized)\n        return real"
    "Direct3DCreate9 must return the untouched system interface in the fused build")
require_text(
    "${proxy_source}"
    "if constexpr (!fnvxr::d3d9::ProductionRendererAuthorized)\n        return gRealDirect3DCreate9Ex(sdkVersion, out)"
    "Direct3DCreate9Ex must directly forward in the fused build")
require_text(
    "${proxy_source}"
    "if constexpr (!fnvxr::d3d9::ProductionRendererAuthorized)\n        return false"
    "Device-vtable installation must have its own compile-time fuse")
require_text(
    "${proxy_source}"
    "if constexpr (!fnvxr::d3d9::ProductionRendererAuthorized)\n        return"
    "Proxy logging must be a compile-time no-op in the fused build")
require_text(
    "${proxy_source}"
    "return fnvxr::d3d9::ProductionRendererAuthorized\n        && StereoWorldProductionProofComplete"
    "Legacy runtime environment switches must be subordinate to exact renderer authorization")

extract_region(
    create9_body
    "${proxy_source}"
    "extern \"C\" IDirect3D9* WINAPI FNVXR_Direct3DCreate9(UINT sdkVersion)\n{"
    "extern \"C\" HRESULT WINAPI FNVXR_Direct3DCreate9Ex")
string(FIND "${create9_body}" "if constexpr (!fnvxr::d3d9::ProductionRendererAuthorized)" create9_fuse_at)
string(FIND "${create9_body}" "ensureD3D9ProxyInitialized()" create9_initialize_at)
string(FIND "${create9_body}" "new (std::nothrow) Direct3D9Proxy" create9_wrap_at)
if(create9_fuse_at EQUAL -1
    OR create9_initialize_at EQUAL -1
    OR create9_wrap_at EQUAL -1
    OR create9_fuse_at GREATER create9_initialize_at
    OR create9_fuse_at GREATER create9_wrap_at)
    message(FATAL_ERROR
        "Direct3DCreate9 must return the system interface before initialization or wrapping")
endif()

extract_region(
    create9ex_body
    "${proxy_source}"
    "extern \"C\" HRESULT WINAPI FNVXR_Direct3DCreate9Ex(UINT sdkVersion, IDirect3D9Ex** out)\n{"
    "extern \"C\" int WINAPI FNVXR_D3DPERF_BeginEvent")
string(FIND "${create9ex_body}" "if constexpr (!fnvxr::d3d9::ProductionRendererAuthorized)" create9ex_fuse_at)
string(FIND "${create9ex_body}" "ensureD3D9ProxyInitialized()" create9ex_initialize_at)
string(FIND "${create9ex_body}" "logLine(" create9ex_log_at)
if(create9ex_fuse_at EQUAL -1
    OR create9ex_initialize_at EQUAL -1
    OR create9ex_log_at EQUAL -1
    OR create9ex_fuse_at GREATER create9ex_initialize_at
    OR create9ex_fuse_at GREATER create9ex_log_at)
    message(FATAL_ERROR
        "Direct3DCreate9Ex must forward before initialization or logging")
endif()

extract_region(
    install_body
    "${proxy_source}"
    "bool installDeviceHooks(IDirect3DDevice9* device)\n{"
    "bool buildLogPath(char* path, size_t pathSize, const char* leafName)\n{")
string(FIND "${install_body}" "if constexpr (!fnvxr::d3d9::ProductionRendererAuthorized)" install_fuse_at)
string(FIND "${install_body}" "patchVTableSlot(" first_patch_at)
if(install_fuse_at EQUAL -1 OR first_patch_at EQUAL -1 OR install_fuse_at GREATER first_patch_at)
    message(FATAL_ERROR "Device vtable mutation is reachable before the production fuse")
endif()

extract_region(
    log_body
    "${proxy_source}"
    "void logLine(const char* text)\n{"
    "void loadStereoConfig()\n{")
string(FIND "${log_body}" "if constexpr (!fnvxr::d3d9::ProductionRendererAuthorized)" log_fuse_at)
string(FIND "${log_body}" "buildLogPath(" log_open_at)
if(log_fuse_at EQUAL -1 OR log_open_at EQUAL -1 OR log_fuse_at GREATER log_open_at)
    message(FATAL_ERROR "Proxy logging is reachable before the production fuse")
endif()

string(FIND "${proxy_source}" "bool loadRealD3D9()\n{" loader_begin)
string(FIND "${proxy_source}" "class Direct3D9Proxy final" loader_end)
if(loader_begin EQUAL -1 OR loader_end EQUAL -1 OR loader_end LESS_EQUAL loader_begin)
    message(FATAL_ERROR "Could not isolate loadRealD3D9")
endif()
math(EXPR loader_length "${loader_end} - ${loader_begin}")
string(SUBSTRING "${proxy_source}" ${loader_begin} ${loader_length} loader_body)
foreach(forbidden IN ITEMS
    "ensureD3D9ProxyInitialized()"
    "logLine("
    "loadStereoConfig()"
    "OpenFileMapping"
    "CreateFileMapping"
    "GetRenderTargetData"
    "LockRect")
    string(FIND "${loader_body}" "${forbidden}" forbidden_at)
    if(NOT forbidden_at EQUAL -1)
        message(FATAL_ERROR
            "System D3D9 loading must be side-effect-free; found '${forbidden}'")
    endif()
endforeach()

message(STATUS "D3D9 proxy inert-source fuse PASS")
