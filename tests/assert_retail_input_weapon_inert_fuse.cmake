if(NOT DEFINED SOURCE_ROOT OR NOT IS_DIRECTORY "${SOURCE_ROOT}")
    message(FATAL_ERROR "SOURCE_ROOT must name the repository root")
endif()

set(input_safety "${SOURCE_ROOT}/renderhook/fnvxr_input_proxy_safety.h")
set(retail_safety "${SOURCE_ROOT}/runtime/fnvxr_retail_safety.h")
set(plugin "${SOURCE_ROOT}/plugin/fnvxr_nvse_plugin.cpp")
foreach(path IN LISTS input_safety retail_safety plugin)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "required retail mutation source is missing: ${path}")
    endif()
endforeach()

file(READ "${input_safety}" input_safety_text)
file(READ "${retail_safety}" retail_safety_text)
file(READ "${plugin}" plugin_text)

function(require_source_text text_value needle failure)
    string(FIND "${text_value}" "${needle}" offset)
    if(offset EQUAL -1)
        message(FATAL_ERROR "${failure}")
    endif()
endfunction()

require_source_text(
    "${input_safety_text}"
    "inline constexpr bool ProductionInputMutationProofComplete = false;"
    "Shipping XInput/DInput mutation is no longer source-fused")
require_source_text(
    "${input_safety_text}"
    "inline constexpr bool ProductInputControllerIntegrated = false;"
    "The experimental proxy mapper is being represented as the product controller")
require_source_text(
    "${retail_safety_text}"
    "inline constexpr bool RetailMutationProofComplete = false;"
    "Camera/weapon/rig mutation is no longer source-fused")
require_source_text(
    "${plugin_text}"
    "fnvxr::safety::RetailMutationEvidenceToken g_retailMutationEvidence {};"
    "The plugin's mutation evidence must begin empty")
require_source_text(
    "${plugin_text}"
    "rejectUntilCurrentProcessIntegrityValidatorImplemented"
    "The deliberately rejecting live mutation validator disappeared")
require_source_text(
    "${plugin_text}"
    "return false;\n}\n\nbool retailMutationAllowedForCurrentProcess"
    "The live plugin mutation validator no longer visibly rejects")

# No production code currently populates the old all-product evidence token.
# If that changes, this fuse forces an explicit review instead of silently
# turning camera/animation writes on through retained booleans.
string(REGEX MATCHALL "g_retailMutationEvidence[ \t\r\n]*=" evidence_assignments
    "${plugin_text}")
list(LENGTH evidence_assignments evidence_assignment_count)
if(NOT evidence_assignment_count EQUAL 0)
    message(FATAL_ERROR
        "Plugin mutation evidence gained a production assignment without a reviewed authority path")
endif()

string(FIND "${plugin_text}" "bool installCameraHook()" camera_start)
string(FIND "${plugin_text}" "Matrix33 transposeMatrix33" camera_end)
if(camera_start EQUAL -1 OR camera_end EQUAL -1 OR camera_end LESS camera_start)
    message(FATAL_ERROR "Could not isolate installCameraHook")
endif()
math(EXPR camera_length "${camera_end} - ${camera_start}")
string(SUBSTRING "${plugin_text}" ${camera_start} ${camera_length} camera_body)
string(FIND "${camera_body}"
    "if (!retailMutationAllowedForCurrentProcess(requested))" camera_gate)
string(FIND "${camera_body}"
    "writeJump(PlayerCharacterUpdateCameraAddress" camera_write)
if(camera_gate EQUAL -1 OR camera_write EQUAL -1 OR camera_gate GREATER camera_write)
    message(FATAL_ERROR
        "Camera hook write is not downstream of the hard mutation gate")
endif()

string(FIND "${plugin_text}" "bool installRetailRigHook()" rig_start)
string(FIND "${plugin_text}" "TileValue* getTileValue" rig_end)
if(rig_start EQUAL -1 OR rig_end EQUAL -1 OR rig_end LESS rig_start)
    message(FATAL_ERROR "Could not isolate installRetailRigHook")
endif()
math(EXPR rig_length "${rig_end} - ${rig_start}")
string(SUBSTRING "${plugin_text}" ${rig_start} ${rig_length} rig_body)
string(FIND "${rig_body}"
    "envEnabled(\"FNVXR_RETAIL_RIG_ENABLE\", false)" rig_default)
string(FIND "${rig_body}"
    "if (!retailMutationAllowedForCurrentProcess(requested))" rig_gate)
string(FIND "${rig_body}"
    "writeJump(PlayerAnimationApplyCallSiteAddress" rig_write)
if(rig_default EQUAL -1
    OR rig_gate EQUAL -1
    OR rig_write EQUAL -1
    OR rig_gate GREATER rig_write)
    message(FATAL_ERROR
        "Retail weapon-rig hook is not disabled-by-default and downstream of the hard gate")
endif()

require_source_text(
    "${plugin_text}"
    "envEnabled(\"FNVXR_RETAIL_WEAPON_APPLY\", false)"
    "Retail weapon transform writes are no longer independently disabled by default")

message(STATUS
    "Retail input/weapon inert fuse PASS (exact bridge authority does not authorize proxy or rig mutation)")
