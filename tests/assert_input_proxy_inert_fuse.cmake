if(NOT DEFINED DINPUT_SOURCE OR NOT EXISTS "${DINPUT_SOURCE}")
    message(FATAL_ERROR "DINPUT_SOURCE must name the DirectInput proxy source")
endif()
if(NOT DEFINED XINPUT_SOURCE OR NOT EXISTS "${XINPUT_SOURCE}")
    message(FATAL_ERROR "XINPUT_SOURCE must name the XInput proxy source")
endif()

file(READ "${DINPUT_SOURCE}" dinput_source)
file(READ "${XINPUT_SOURCE}" xinput_source)

set(shared_include "#include \"fnvxr_input_proxy_safety.h\"")
set(auth_call "fnvxr::input_proxy::productionInputMutationAuthorizedForCurrentBuild()")

foreach(source_name IN ITEMS dinput_source xinput_source)
    string(FIND "${${source_name}}" "${shared_include}" include_offset)
    string(FIND "${${source_name}}" "${auth_call}" auth_offset)
    if(include_offset EQUAL -1 OR auth_offset EQUAL -1)
        message(FATAL_ERROR
            "${source_name} does not consume the shared production input source fuse")
    endif()
endforeach()

set(dinput_forward
    "if (!vrInputMutationAuthorized())\n        return fn(instance, version, riid, out, outer);")
string(FIND "${dinput_source}" "${dinput_forward}" dinput_forward_offset)
string(FIND "${dinput_source}" "void* realOut = nullptr;" dinput_wrap_offset)
if(dinput_forward_offset EQUAL -1
    OR dinput_wrap_offset EQUAL -1
    OR dinput_forward_offset GREATER dinput_wrap_offset)
    message(FATAL_ERROR
        "DirectInput8Create does not forward before allocating a wrapper result")
endif()

set(xinput_required_forwarders
    "if (!vrInputMutationAuthorized())\n        return forwardXInputGetState(dwUserIndex, pState);"
    "if (!vrInputMutationAuthorized())\n        return forwardXInputGetCapabilities(dwUserIndex, dwFlags, pCapabilities);"
    "if (!vrInputMutationAuthorized())\n        return forwardXInputGetBatteryInformation(dwUserIndex, devType, pBatteryInformation);"
    "if (!vrInputMutationAuthorized())\n        return forwardXInputGetKeystroke(dwUserIndex, dwReserved, pKeystroke);"
    "if (!vrInputMutationAuthorized())\n        return forwardXInputGetStateEx(dwUserIndex, pState);")
foreach(forwarder IN LISTS xinput_required_forwarders)
    string(FIND "${xinput_source}" "${forwarder}" forwarder_offset)
    if(forwarder_offset EQUAL -1)
        message(FATAL_ERROR "XInput transparent forwarding is missing: ${forwarder}")
    endif()
endforeach()

string(FIND "${xinput_source}" "DllMain attach xinput proxy" loader_log_offset)
if(NOT loader_log_offset EQUAL -1)
    message(FATAL_ERROR
        "The source-fused XInput forwarder still logs while under loader lock")
endif()

message(STATUS
    "Input proxy inert fuse PASS (DInput/XInput forward before mutation paths)")
