if(NOT DEFINED PROXY_SOURCE OR NOT EXISTS "${PROXY_SOURCE}")
    message(FATAL_ERROR "Missing D3D9 proxy source: ${PROXY_SOURCE}")
endif()

if(NOT DEFINED ACTIVATION_HEADER OR NOT EXISTS "${ACTIVATION_HEADER}")
    message(FATAL_ERROR "Missing D3D9 activation contract: ${ACTIVATION_HEADER}")
endif()

if(NOT DEFINED BRIDGE_HEADER OR NOT EXISTS "${BRIDGE_HEADER}")
    message(FATAL_ERROR "Missing retail VR bridge source: ${BRIDGE_HEADER}")
endif()

file(READ "${PROXY_SOURCE}" proxy_source)
file(READ "${ACTIVATION_HEADER}" activation_header)
file(READ "${BRIDGE_HEADER}" bridge_header)
string(REPLACE "\r\n" "\n" proxy_source "${proxy_source}")
string(REPLACE "\r\n" "\n" activation_header "${activation_header}")
string(REPLACE "\r\n" "\n" bridge_header "${bridge_header}")

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
    "${activation_header}"
    "struct RetailVrVisualTrialRequest"
    "The bounded engine-center trial must have a separate request contract")
require_text(
    "${activation_header}"
    "constexpr bool retailVrVisualTrialAuthorized("
    "The bounded engine-center trial must use a pure activation predicate")
foreach(required IN ITEMS
        "policy.exactCurrentProcessAuthorityRequired"
        "!policy.exBackedGameDevice"
        "policy.retailWorldHookOnly"
        "!policy.replaceD3D9DeviceVtablePointer"
        "policy.leaseNativePresentSlot"
        "policy.cpuImageTransfer"
        "!policy.legacyDrawReplay"
        "request.exactProfileMatched"
        "request.engineCenterStereoRequested"
        "!request.stereoWorldDisabled"
        "!request.legacyImageDiagnosticsRequested"
        "!request.retainedStereoGameTexturesRequested"
        "!request.unprovenColorOnlyStereoDiagnosticRequested"
        "!request.allowStereoWorld2dFallback"
        "!request.showGamePlaneOnStereoLoss"
        "!request.stereoFallbackMonoFullscreen")
    require_text(
        "${activation_header}"
        "${required}"
        "The bounded visual-trial predicate widened its authority")
endforeach()

require_text(
    "${proxy_source}"
    "#include \"fnvxr_d3d9_activation.h\""
    "The proxy must consume the pure activation contract")
require_text(
    "${proxy_source}"
    "if (!currentExecutableIsFalloutNv())\n        return gRealDirect3DCreate9(sdkVersion)"
    "Non-retail processes must receive the untouched system interface")
require_text(
    "${bridge_header}"
    "engine::authorizeCurrentRetailRuntimeAtDecisionPoint()"
    "The retail bridge must require current-process authority")
require_text(
    "${bridge_header}"
    "if (!mAuthority.complete())\n            return fail(RetailVrBridgeFailure::RuntimeAuthorityRejected)"
    "The retail bridge must reject before resolving or mutating engine state")
require_text(
    "${proxy_source}"
    "currentLoadedExecutableMatchesSupportedRetail()"
    "The early D3D bootstrap must verify the exact loaded retail PE")
require_text(
    "${proxy_source}"
    "IDirect3D9* real = gRealDirect3DCreate9(sdkVersion);"
    "Retail startup must preserve Fallout's ordinary D3D9 enumerator")
require_text(
    "${proxy_source}"
    "#include \"fnvxr_retail_vr_bridge_win32.h\""
    "The exact retail path must enter through the isolated bridge")
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
    visual_trial_request_body
    "${proxy_source}"
    "bool retailVrVisualTrialRequested()\n{"
    "bool rockSolidProfile()\n{")
foreach(required IN ITEMS
        "runProfileIs(\"stereo-visual-trial-v5\")"
        "FNVXR_ENABLE_ENGINE_CENTER_STEREO"
        "FNVXR_DISABLE_STEREO_WORLD"
        "FNVXR_ENABLE_LEGACY_IMAGE_DIAGNOSTICS"
        "FNVXR_USE_STEREO_GAME_TEXTURES"
        "FNVXR_ENABLE_UNPROVEN_COLOR_ONLY_STEREO_DIAGNOSTIC"
        "FNVXR_ALLOW_STEREO_WORLD_2D_FALLBACK"
        "FNVXR_SHOW_GAME_PLANE_ON_STEREO_LOSS"
        "FNVXR_STEREO_FALLBACK_MONO_FULLSCREEN"
        "retailVrVisualTrialAuthorized("
        "CompiledRetailVrBridgePolicy")
    require_text(
        "${visual_trial_request_body}"
        "${required}"
        "The proxy visual-trial request is not exact and fallback-free")
endforeach()

extract_region(
    create9_body
    "${proxy_source}"
    "extern \"C\" IDirect3D9* WINAPI FNVXR_Direct3DCreate9(UINT sdkVersion)\n{"
    "extern \"C\" HRESULT WINAPI FNVXR_Direct3DCreate9Ex")
string(FIND "${create9_body}" "if (!currentExecutableIsFalloutNv())" create9_nonretail_at)
string(FIND "${create9_body}" "assessGameD3D9Bootstrap(" create9_authority_at)
string(FIND "${create9_body}" "if (!bootstrap.authorized())" create9_complete_at)
string(FIND "${create9_body}" "authorizeCurrentRetailRuntimeAtDecisionPoint()" create9_mutation_authority_at)
string(FIND "${create9_body}" "IDirect3D9* real = gRealDirect3DCreate9(sdkVersion);" create9_ordinary_at)
string(FIND "${create9_body}" "new (std::nothrow) Direct3D9Proxy" create9_wrap_at)
if(create9_nonretail_at EQUAL -1
    OR create9_authority_at EQUAL -1
    OR create9_complete_at EQUAL -1
    OR NOT create9_mutation_authority_at EQUAL -1
    OR create9_ordinary_at EQUAL -1
    OR create9_wrap_at EQUAL -1
    OR create9_nonretail_at GREATER create9_authority_at
    OR create9_authority_at GREATER create9_complete_at
    OR create9_complete_at GREATER create9_ordinary_at
    OR create9_ordinary_at GREATER create9_wrap_at
    OR create9_complete_at GREATER create9_wrap_at)
    message(FATAL_ERROR
        "Direct3DCreate9 must use exact bootstrap authority, never premature mutation authority, before creating and wrapping the ordinary retail enumerator")
endif()

extract_region(
    game_proxy_body
    "${proxy_source}"
    "class Direct3D9Proxy final : public IDirect3D9"
    "bool currentExecutableIsFalloutNv() noexcept")
foreach(required IN ITEMS
        "explicit Direct3D9Proxy(IDirect3D9* real)"
        "const HRESULT result = mReal->CreateDevice("
        "if constexpr (fnvxr::d3d9::ProductionRendererAuthorized)"
        "if (!installDeviceHooks(*returnedDevice)"
        "else if (retailVrVisualTrialRequested())"
        "initializeRetailVrPresentBootstrap(*returnedDevice)"
        "static_cast<void>(initializeRetailVrBridge(*returnedDevice))")
    string(FIND "${game_proxy_body}" "${required}" required_at)
    if(required_at EQUAL -1)
        message(FATAL_ERROR
            "The exact-retail ordinary-D3D9 bridge is incomplete: missing '${required}'")
    endif()
endforeach()
foreach(forbidden IN ITEMS "mRealEx" "CreateDeviceEx(")
    string(FIND "${game_proxy_body}" "${forbidden}" forbidden_at)
    if(NOT forbidden_at EQUAL -1)
        message(FATAL_ERROR
            "Fallout's ordinary game device was replaced by an Ex path: '${forbidden}'")
    endif()
endforeach()
string(FIND "${game_proxy_body}" "const HRESULT result = mReal->CreateDevice(" create_device_at)
string(FIND "${game_proxy_body}" "if constexpr (fnvxr::d3d9::ProductionRendererAuthorized)" production_gate_at)
string(FIND "${game_proxy_body}" "if (!installDeviceHooks(*returnedDevice)" install_hooks_at)
string(FIND "${game_proxy_body}" "else if (retailVrVisualTrialRequested())" visual_trial_gate_at)
string(FIND "${game_proxy_body}" "initializeRetailVrPresentBootstrap(*returnedDevice)" present_bootstrap_at)
string(FIND "${game_proxy_body}" "static_cast<void>(initializeRetailVrBridge(*returnedDevice))" bridge_attempt_at)
if(create_device_at GREATER present_bootstrap_at
    OR production_gate_at EQUAL -1
    OR install_hooks_at EQUAL -1
    OR visual_trial_gate_at EQUAL -1
    OR production_gate_at GREATER install_hooks_at
    OR install_hooks_at GREATER visual_trial_gate_at
    OR visual_trial_gate_at GREATER present_bootstrap_at
    OR present_bootstrap_at GREATER bridge_attempt_at)
    message(FATAL_ERROR
        "Legacy device hooks and the narrow visual-trial bridge are not independently ordered and gated")
endif()
require_text(
    "${proxy_source}"
    "header->producerMode =\n        fnvxr::shared::StereoProducerEngineCenter;"
    "The isolated CPU publisher must identify the exact engine-center producer")
require_text(
    "${proxy_source}"
    "device->GetRenderTargetData(\n        gLeftEyeSurface"
    "The ordinary-D3D9 bridge must read back the private left eye")
require_text(
    "${proxy_source}"
    "operations.publishCpuPair = &publishRetailVrCpuPair;"
    "The retail bridge must select the isolated CPU pair publisher")
require_text(
    "${proxy_source}"
    "fnvxrRetailEngineCenterFrame"
    "Engine-center acceptance must receive an exact completed-render transaction event")
require_text(
    "${proxy_source}"
    "fnvxrRetailEngineCenterCpuStereo"
    "Engine-center acceptance must receive a source-pixel event from the CPU pair publisher")

# The verifier treats source order as part of the transaction proof: the CPU
# pixels are logged by publishCpuPair during the in-scope accumulation
# dispatch, and the E8 adapter emits the successful completion record only
# after that dispatch returns.
extract_region(
    retail_adapter_body
    "${proxy_source}"
    "void __cdecl retailVrAccumulateSceneAdapter("
    "bool initializeRetailVrBridge(IDirect3DDevice9* device) noexcept\n{")
string(FIND "${retail_adapter_body}" "bridge->dispatchFromAccumulationAdapter(" adapter_dispatch_at)
string(FIND "${retail_adapter_body}" "fnvxrRetailEngineCenterFrame" adapter_completion_at)
if(adapter_dispatch_at EQUAL -1
    OR adapter_completion_at EQUAL -1
    OR adapter_dispatch_at GREATER adapter_completion_at)
    message(FATAL_ERROR
        "The engine-center completion event must be emitted after its dispatch returns")
endif()
extract_region(
    cpu_pair_publisher_body
    "${proxy_source}"
    "bool publishRetailVrCpuPair("
    "void publishSharedStereoInvalid(bool uiActive, const char* reason)\n{")
string(FIND "${cpu_pair_publisher_body}" "fnvxrRetailEngineCenterCpuStereo" cpu_pair_event_at)
if(cpu_pair_event_at EQUAL -1)
    message(FATAL_ERROR
        "The publishCpuPair callback must emit the source-pixel lineage event")
endif()
require_text(
    "${cpu_pair_publisher_body}"
    [[\"publicationGeneration\":\"%llu\"]]
    "The engine-center CPU event must carry the exact nonzero 64-bit publication identity")

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
