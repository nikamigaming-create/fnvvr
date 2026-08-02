if(NOT DEFINED ROOT OR NOT IS_DIRECTORY "${ROOT}")
    message(FATAL_ERROR "ROOT must name the repository root")
endif()

set(adr "${ROOT}/docs/adr/0001-gpu-color-v5-product-v1.md")
set(matrix "${ROOT}/docs/capability-matrix.md")
set(inventory "${ROOT}/docs/mutation-site-inventory.md")
set(capabilities "${ROOT}/protocol/fnvxr_product_capabilities.h")
set(product_contract "${ROOT}/protocol/fnvxr_product_contract.h")
set(retail_safety "${ROOT}/runtime/fnvxr_retail_safety.h")
foreach(path IN LISTS adr matrix inventory capabilities product_contract retail_safety)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "required Phase 0 record is missing: ${path}")
    endif()
endforeach()

file(READ "${adr}" adr_text)
file(READ "${matrix}" matrix_text)
file(READ "${inventory}" inventory_text)
file(READ "${capabilities}" capabilities_text)
file(READ "${product_contract}" product_contract_text)
file(READ "${retail_safety}" retail_safety_text)

function(require_text source needle reason)
    string(FIND "${source}" "${needle}" found_at)
    if(found_at EQUAL -1)
        message(FATAL_ERROR "${reason}: ${needle}")
    endif()
endfunction()

require_text("${adr_text}" "Status: Accepted" "GPU transport ADR is not accepted")
require_text("${adr_text}" "ABI v5" "ADR does not select GPU color ABI v5")
require_text("${adr_text}" "renderLocalDepthPairComplete" "ADR does not keep depth render-local")
require_text("${adr_text}" "not a v1 release gate" "ADR still requires OpenXR depth submission")

foreach(capability IN ITEMS
        "CameraStereoVisual"
        "ControllerInput"
        "UiInteraction"
        "VisualRig"
        "CombatAim"
        "GpuPresentation"
        "FullProduct")
    require_text("${matrix_text}" "${capability}" "Capability matrix is incomplete")
endforeach()
require_text("${matrix_text}" "Production-authorized" "Capability matrix lacks authorization column")
require_text("${matrix_text}" "InputOwner::NvseMainGameLoop" "Matrix does not record the selected input owner")

require_text("${inventory_text}" "RenderWorldSceneGraph" "Mutation inventory lacks world seam")
require_text("${inventory_text}" "PlayerCharacter::UpdateCamera" "Mutation inventory lacks camera seam")
require_text("${inventory_text}" "D2355FF1593FD9D843C0C61FE95205C1B2C4F1FB6D560499B6FA4EE9C312AEAE" "Mutation inventory lacks exact world hash")
require_text("${inventory_text}" "DirectInput/XInput proxies" "Mutation inventory lacks proxy ownership boundary")

require_text("${capabilities_text}" "SelectedProductInputOwner" "Capability lease contract lacks selected input owner")
require_text("${capabilities_text}" "InputOwner::NvseMainGameLoop" "Capability lease contract selected the wrong input owner")
require_text("${capabilities_text}" "assessFullProductLease" "Full-product lease aggregation is missing")
require_text("${product_contract_text}" "renderLocalDepthPairComplete" "Product contract still conflates local depth with transport")
require_text("${retail_safety_text}" "renderLocalDepthPairComplete" "Retail mutation safety still requires depth transport")

string(FIND "${retail_safety_text}" "gpuDepthTransportComplete" obsolete_depth_gate)
if(NOT obsolete_depth_gate EQUAL -1)
    message(FATAL_ERROR "Retail mutation safety still contains the obsolete GPU depth transport gate")
endif()

message(STATUS "Phase 0 records and color-v5/render-local-depth boundary PASS")
