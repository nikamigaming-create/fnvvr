foreach(required_variable IN ITEMS
        PROXY_SOURCE
        PLUGIN_SOURCE
        CONTRACT_HEADER
        READER_HEADER
        READER_SOURCE)
    if(NOT DEFINED ${required_variable}
        OR NOT EXISTS "${${required_variable}}")
        message(FATAL_ERROR
            "${required_variable} is missing or does not name a file")
    endif()
endforeach()

file(READ "${PROXY_SOURCE}" proxy_text)
file(READ "${PLUGIN_SOURCE}" plugin_text)
file(READ "${CONTRACT_HEADER}" contract_text)
file(READ "${READER_HEADER}" reader_header_text)
file(READ "${READER_SOURCE}" reader_source_text)
foreach(text_name IN ITEMS
        proxy_text
        plugin_text
        contract_text
        reader_header_text
        reader_source_text)
    string(REPLACE "\r\n" "\n" ${text_name} "${${text_name}}")
endforeach()

function(require_text haystack needle reason)
    string(FIND "${haystack}" "${needle}" found_at)
    if(found_at EQUAL -1)
        message(FATAL_ERROR "${reason}: missing '${needle}'")
    endif()
endfunction()

function(forbid_text haystack needle reason)
    string(FIND "${haystack}" "${needle}" found_at)
    if(NOT found_at EQUAL -1)
        message(FATAL_ERROR "${reason}: found '${needle}'")
    endif()
endfunction()

function(extract_region output haystack begin_marker end_marker)
    string(FIND "${haystack}" "${begin_marker}" begin_at)
    string(FIND "${haystack}" "${end_marker}" end_at)
    if(begin_at EQUAL -1 OR end_at EQUAL -1 OR end_at LESS_EQUAL begin_at)
        message(FATAL_ERROR
            "Could not isolate '${begin_marker}' through '${end_marker}'")
    endif()
    math(EXPR region_length "${end_at} - ${begin_at}")
    string(SUBSTRING
        "${haystack}"
        ${begin_at}
        ${region_length}
        region)
    set(${output} "${region}" PARENT_SCOPE)
endfunction()

require_text(
    "${plugin_text}"
    "#include \"fnvxr_stereo_visual_trial_automation_authority.h\""
    "Publication-only automation must compile the fixed command authority gate")
require_text(
    "${plugin_text}"
    "#include \"fnvxr_retail_fixture_automation_authority.h\""
    "Retail fixture automation must compile its independent command authority gate")

foreach(required IN ITEMS
        "RetailRuntimePublicationObservation"
        "RetailPluginMainLoopDisposition"
        "PublishRuntimeOnly"
        "retailRuntimeCameraActive"
        "sequenceBefore"
        "sequenceAfter"
        "sequencedValueIsPublished"
        "state.frame == 0u"
        "RuntimePhaseUnknown"
        "NeutralPublication")
    require_text(
        "${contract_text}"
        "${required}"
        "Runtime readiness must reject torn, cold, and neutral publications")
endforeach()

require_text(
    "${reader_header_text}"
    [[Local\\FNVXR_Runtime_State]]
    "The readiness reader must consume only the plugin-owned runtime mapping")
foreach(required IN ITEMS
        "OpenFileMappingA("
        "FILE_MAP_READ"
        "MapViewOfFile("
        "assessRetailRuntimePublicationReadiness(observation)")
    require_text(
        "${reader_source_text}"
        "${required}"
        "Runtime readiness reader is not a read-only stable-snapshot consumer")
endforeach()
foreach(forbidden IN ITEMS
        "CreateFileMapping"
        "FILE_MAP_WRITE"
        "FILE_MAP_ALL_ACCESS"
        "InterlockedExchange"
        "beginSequencedSharedWrite"
        "endSequencedSharedWrite")
    forbid_text(
        "${reader_source_text}"
        "${forbidden}"
        "Runtime readiness reader gained publication or write authority")
endforeach()

extract_region(
    present_bootstrap
    "${proxy_text}"
    "bool initializeRetailVrPresentBootstrap(IDirect3DDevice9* device) noexcept\n{"
    "void __cdecl retailVrAccumulateSceneAdapter(")
string(FIND
    "${present_bootstrap}"
    "gRetailRuntimePublicationReadiness.initialize()"
    reader_initialize_at)
string(FIND
    "${present_bootstrap}"
    "gRetailUiPresentHook.initializeAuthorizedDevice("
    present_hook_at)
if(reader_initialize_at EQUAL -1
    OR present_hook_at EQUAL -1
    OR reader_initialize_at GREATER present_hook_at)
    message(FATAL_ERROR
        "CreateDevice Present bootstrap must arm the lazy read-only reader before leasing Present")
endif()

extract_region(
    bridge_bootstrap
    "${proxy_text}"
    "bool initializeRetailVrBridge(IDirect3DDevice9* device) noexcept\n{"
    "// Fallout: New Vegas 1.4.0.525 retail render entry point")
string(FIND
    "${bridge_bootstrap}"
    "gRetailRuntimePublicationReadiness.readReadyPublication("
    readiness_at)
string(FIND
    "${bridge_bootstrap}"
    "prepareRetailEngineEyeTargetOperations(device, eyeTargets)"
    eye_targets_at)
string(FIND
    "${bridge_bootstrap}"
    "new (std::nothrow) RetailVrBridge"
    bridge_allocation_at)
string(FIND
    "${bridge_bootstrap}"
    "bridge->initialize("
    authority_bridge_at)
if(readiness_at EQUAL -1
    OR eye_targets_at EQUAL -1
    OR bridge_allocation_at EQUAL -1
    OR authority_bridge_at EQUAL -1
    OR readiness_at GREATER eye_targets_at
    OR readiness_at GREATER bridge_allocation_at
    OR readiness_at GREATER authority_bridge_at)
    message(FATAL_ERROR
        "Runtime publication readiness must precede eye resources, bridge allocation, and engine authority")
endif()
require_text(
    "${bridge_bootstrap}"
    "engine authority not attempted"
    "A deferred readiness result must explicitly preserve inert engine authority")

extract_region(
    observation_bootstrap
    "${plugin_text}"
    "bool ensureAuthorizedRuntimeObservationStarted()\n{"
    "RuntimeObservation observeAndPublishRuntime()")
string(FIND
    "${observation_bootstrap}"
    "if (g_authorizedRuntimeObservationStarted)"
    retained_observation_at)
string(FIND
    "${observation_bootstrap}"
    "proveCurrentRetailCompatibilityAtDecisionPoint()"
    observation_proof_at)
if(retained_observation_at EQUAL -1
    OR observation_proof_at EQUAL -1
    OR retained_observation_at GREATER observation_proof_at)
    message(FATAL_ERROR
        "Authenticated observation authority must persist after the renderer installs its known hook")
endif()

extract_region(
    runtime_publication
    "${plugin_text}"
    "RuntimeObservation observeAndPublishRuntime()\n{"
    "bool ensureAuthorizedDesktopAssistBridgeStarted()")
foreach(required IN ITEMS
        "visualTrialPublicationOnly"
        "readOnlyCameraPublicationAuthorized"
        "retailFixtureAutomationRequested()"
        "cameraAllowedForMenuBits(observation.menuBits)"
        "activeGameCameraObject() != nullptr"
        "retailRuntimeCameraActive("
        "observation.cameraActive")
    require_text(
        "${runtime_publication}"
        "${required}"
        "Publication-only profiles must report only a currently observed retail camera")
endforeach()

extract_region(
    visual_trial_profile
    "${plugin_text}"
    "bool stereoVisualTrialProfileSelected()\n{"
    "bool desktopAssistAutomationRequested()")
foreach(required IN ITEMS
        "runProfileIs(\"stereo-visual-trial-v5\")"
        "FNVXR_ENABLE_ENGINE_CENTER_STEREO"
        "retailPluginMainLoopDisposition(")
    require_text(
        "${visual_trial_profile}"
        "${required}"
        "Plugin visual-trial publication-only policy is incomplete")
endforeach()

extract_region(
    visual_trial_automation_opt_in
    "${plugin_text}"
    "bool stereoVisualTrialAutomationRequested()\n{"
    "bool desktopAssistAutomationRequested()")
foreach(required IN ITEMS
        "PublishRuntimeOnly"
        "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_RECOVERY_LOAD"
        "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_FRESH_CHARACTER"
        "FNVXR_STEREO_VISUAL_TRIAL_ACK_TRIBAL_PACK_POPUP"
        "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_RECOVERY_SAVE_NAME"
        "recoveryLoadRequested != freshCharacterRequested"
        "false")
    require_text(
        "${visual_trial_automation_opt_in}"
        "${required}"
        "Visual-trial fixed-save automation is no longer explicit publication-only opt-in")
endforeach()

extract_region(
    visual_trial_selected_retail_save
    "${plugin_text}"
    "stereoVisualTrialSelectedRetailSave()\n{"
    "bool desktopAssistAutomationRequested()")
foreach(required IN ITEMS
        "FNVXR_STEREO_VISUAL_TRIAL_AUTOMATE_RECOVERY_SAVE_NAME"
        "getenv_s("
        "required == 0u"
        "required > sizeof(saveName)"
        "findApprovedRetailSave")
    require_text(
        "${visual_trial_selected_retail_save}"
        "${required}"
        "Visual-trial selected retail save is no longer compile-time allowlisted and fail-closed")
endforeach()

foreach(required IN ITEMS
        "fixedCommandAutomationRequested"
        "desktopAssistAutomationRequested()"
        "stereoVisualTrialAutomationRequested()"
        "initSharedCommand()"
        "&& g_commandState != nullptr")
    require_text(
        "${observation_bootstrap}"
        "${required}"
        "Runtime observation did not keep the optional fixed-command mailbox narrow and fail-closed")
endforeach()

extract_region(
    visual_trial_automation_support
    "${plugin_text}"
    "bool stereoVisualTrialRecoveryLoadCommandIsExact("
    "void closeExactRetailFixtureOfficialPackMessageMenu(")
extract_region(
    visual_trial_automation_execution
    "${plugin_text}"
    "void processStereoVisualTrialFixedSaveAutomation(\n"
    "bool ensureAuthorizedDesktopAssistBridgeStarted()")
set(
    visual_trial_automation
    "${visual_trial_automation_support}\n${visual_trial_automation_execution}")
foreach(required IN ITEMS
        "CommandTypeConsole"
        "FreshCharacterLoadCommand"
        "FreshCharacterStartCommand"
        "FreshCharacterSetNameCommand"
        "FreshCharacterSaveCommand"
        "std::memcmp("
        "fixedCommand.size()"
        "request.saveName[fixedCommand.size()] == '\\0'"
        "stereoVisualTrialSelectedRetailSave"
        "selectedRetailSave != nullptr"
        "realStereoVisualTrialStartMenuState(observation)"
        "realStereoVisualTrialFreshGameplayState(observation)"
        "observation.frame != 0u"
        "observation.phase == RuntimePhase::Menu"
        "observation.uiInputAllowed"
        "!g_showroomActive"
        "RuntimeBlockingMenuBits"
        "RuntimeStartMenuBit"
        "if (!stereoVisualTrialAutomationRequested())"
        "automation::State authorityState"
        "automation::Action::LoadFixedRecoverySave"
        "automation::Action::StartFreshCharacter"
        "automation::Action::NameFreshCharacter"
        "automation::Action::SaveFreshCharacter"
        "selectedRetailSave->name"
        "automation::decide(authorityState, authorityRequest)"
        "decision.command == selectedRetailSave->loadCommand"
        "authorityState = decision.nextState"
        "fnvxrStereoVisualTrialRecoveryLoad")
    require_text(
        "${visual_trial_automation}"
        "${required}"
        "Publication-only automation lost an exact command, evidence, or one-shot authority bound")
endforeach()
foreach(forbidden IN ITEMS
        "CloseAllMenus"
        "FixedCloseAllMenusCommand"
        "authorityRequest.runtime"
        "recoveryLoadAuthorizedFrame")
    forbid_text(
        "${visual_trial_automation}"
        "${forbidden}"
        "Fixed visual-trial automation must not broadly control in-game menus after loading a save")
endforeach()

extract_region(
    tribal_pack_message_target
    "${plugin_text}"
    "struct ExactOfficialPackMessageMenuMatch\n{"
    "void collectMenuButtons(")
foreach(required IN ITEMS
        "copyPrintableTileValueStringReadOnly("
        "OfficialPackNotifications"
        "MessageMenuOkText"
        "observedTitleMask"
        "observedBodyMask"
        "officialPackNotificationMask"
        "exactlyOneOfficialPackNotification"
        "firstButtonOkTileCount == 1u"
        "getTileButtonId(tile) == 0u"
        "validatedVisibleMenu(kMenuTypeMessage"
        "tileRootFromMenu(menu, tileMenu)")
    require_text(
        "${tribal_pack_message_target}"
        "${required}"
        "Exact Tribal Pack acknowledgement target lost its visible-message guard")
endforeach()
foreach(forbidden IN ITEMS
        "setTileFloat("
        "setTileFloatByName("
        "dispatchMenuClick("
        "SendInput(")
    forbid_text(
        "${tribal_pack_message_target}"
        "${forbidden}"
        "Exact Tribal Pack acknowledgement target must remain read-only until the native invocation")
endforeach()

extract_region(
    tribal_pack_message_acknowledgement
    "${plugin_text}"
    "void acknowledgeExactOfficialPackMessageMenu(\n"
    "void completeStereoVisualTrialFreshCharacterRequest(")
foreach(required IN ITEMS
        "exactOfficialPackAcknowledgementRequested()"
        "exactOfficialPackAcknowledgementAuthorized("
        "acknowledgedOfficialPackNotificationMask"
        "RuntimeGenericMenuBit"
        "findExactOfficialPackMessageMenuTarget("
        "setTileFloatByName("
        "okTile, \"mouseover\", TileValueMouseover, 1.0f"
        "okTile, \"clicked\", TileValueClicked, 1.0f"
        "armExactOfficialPackMessageMenuSelection("
        "nativeSelectionArmed"
        "HandleMouseoverFn"
        "vtable[4]"
        "HandleClickFn"
        "vtable[3]"
        "menu,\n                    0u,\n                    okTile"
        "fnvxrStereoVisualTrialOfficialPackNativeMouseoverAndClick"
        "fnvxrStereoVisualTrialOfficialPackAcknowledgement")
    require_text(
        "${tribal_pack_message_acknowledgement}"
        "${required}"
        "Exact Tribal Pack acknowledgement lost its one-shot native-menu guard")
endforeach()
foreach(forbidden IN ITEMS
        "dispatchMenuClick("
        "SendInput("
        "keybd_event("
        "mouse_event("
        "SetCursorPos("
        "CloseAllMenus")
    forbid_text(
        "${tribal_pack_message_acknowledgement}"
        "${forbidden}"
        "Exact Tribal Pack acknowledgement must not use a general or OS input path")
endforeach()

extract_region(
    tribal_pack_native_selection
    "${plugin_text}"
    "bool armExactOfficialPackMessageMenuSelection(void* menu, void* okTile)\n{"
    "void acknowledgeExactOfficialPackMessageMenu(")
foreach(required IN ITEMS
        "InterfaceManagerAddress"
        "InterfaceManagerActiveTileOffset"
        "InterfaceManagerActiveMenuOffset"
        "*activeTile = okTile;"
        "*activeMenu = menu;"
        "fnvxrStereoVisualTrialOfficialPackNativeSelection")
    require_text(
        "${tribal_pack_native_selection}"
        "${required}"
        "Exact Tribal Pack native selection lost its fixed retail menu fields")
endforeach()
foreach(forbidden IN ITEMS
        "SendInput("
        "keybd_event("
        "mouse_event("
        "SetCursorPos("
        "CloseAllMenus")
    forbid_text(
        "${tribal_pack_native_selection}"
        "${forbidden}"
        "Exact Tribal Pack native selection must not use a general or OS input path")
endforeach()
foreach(forbidden IN ITEMS
        "buildSharedCommandLine("
        "consumeSharedCommand("
        "consumeExternalXInputBridge("
        "consumeExternalDInputBridge("
        "executeAcceptClickOnGameThread("
        "SendInput("
        "installCameraHook("
        "installRetailRigHook("
        "ensureAuthorizedSharedBridgeStarted("
        "ensureAuthorizedDesktopAssistBridgeStarted("
        "ensureAuthorizedTrackedPropAssistBridgeStarted("
        "processMainGameLoop("
        "updateWeaponAlignment("
        "applyRetailRigPose(")
    forbid_text(
        "${visual_trial_automation}"
        "${forbidden}"
        "Fixed visual-trial automation gained bridge, input, camera, rig, or weapon authority")
endforeach()
string(REGEX MATCHALL
    "runPluginConsoleCommand\\("
    visual_trial_console_calls
    "${visual_trial_automation}")
list(LENGTH visual_trial_console_calls visual_trial_console_call_count)
if(NOT visual_trial_console_call_count EQUAL 4)
    message(FATAL_ERROR
        "Fixed visual-trial automation must have exactly the recovery plus fixed fresh-character COC/name/save console execution sites")
endif()
string(REGEX MATCHALL
    "automation::decide\\("
    visual_trial_authority_decisions
    "${visual_trial_automation}")
list(LENGTH visual_trial_authority_decisions visual_trial_authority_decision_count)
if(NOT visual_trial_authority_decision_count EQUAL 3)
    message(FATAL_ERROR
        "Fixed visual-trial automation must use the authority gate for its recovery or fresh-character transitions")
endif()

# Owned retail fixtures are intentionally a separate path from the legacy
# fixed-save visual trial.  Keep both the opt-in and every console execution
# site constrained so adding a fixture cannot silently broaden the legacy
# automation or acquire desktop/simulator input authority.
extract_region(
    retail_fixture_opt_in
    "${plugin_text}"
    "bool retailFixtureAutomationRequested()\n{"
    "bool stereoVisualTrialTribalPackAcknowledgementRequested()")
foreach(required IN ITEMS
        "retailFixtureProfileSelected()"
        "FNVXR_RETAIL_FIXTURE_AUTOMATION"
        "FNVXR_RETAIL_FIXTURE_ACTION"
        "FNVXR_RETAIL_FIXTURE_SAVE_NAME"
        "FNVXR_RETAIL_FIXTURE_TRAIT_ONE"
        "FNVXR_RETAIL_FIXTURE_TRAIT_TWO"
        "requestedAction = fixture::Action::Create"
        "requestedAction = fixture::Action::Load"
        "fixture::authorized(output.plan)")
    require_text(
        "${retail_fixture_opt_in}"
        "${required}"
        "Retail fixture automation must remain publication-only, explicit, and authority-gated")
endforeach()

require_text(
    "${plugin_text}"
    "runProfileIs(\"retail-fixture-v1\")"
    "Retail fixture automation must use its own non-OpenXR run profile")
extract_region(
    retail_fixture_main_loop
    "${plugin_text}"
    "if (retailFixtureProfileSelected())\n        {\n            processRetailFixtureAutomation(observation);"
    "if (headsetDemoFixtureProfileSelected())")
foreach(required IN ITEMS
        "processRetailFixtureAutomation(observation)"
        "no OpenXR, simulator, bridge, input, camera, rig, renderer, or weapon authority is active"
        "return;")
    require_text(
        "${retail_fixture_main_loop}"
        "${required}"
        "Retail fixture profile must publish/automate only its owned save lifecycle")
endforeach()
foreach(forbidden IN ITEMS
        "ensureAuthorizedSharedBridgeStarted("
        "ensureAuthorizedDesktopAssistBridgeStarted("
        "ensureAuthorizedTrackedPropAssistBridgeStarted("
        "processMainGameLoop("
        "SendInput("
        "tapKey("
        "installCameraHook("
        "installRetailRigHook(")
    forbid_text(
        "${retail_fixture_main_loop}"
        "${forbidden}"
        "Retail fixture profile must return before any OpenXR, input, camera, rig, or bridge authority")
endforeach()

extract_region(
    headset_demo_fixture_main_loop
    "${plugin_text}"
    "if (headsetDemoFixtureProfileSelected())\n        {\n            processRetailFixtureAutomation(observation);"
    "if (visualTrialDisposition\n            == fnvxr::engine::RetailPluginMainLoopDisposition::\n                PublishRuntimeOnly)")
foreach(required IN ITEMS
        "processRetailFixtureAutomation(observation)"
        "recoverFocusLossPause("
        "processHeadsetDemoFixtureUi(observation)"
        "OpenXR display remains host-owned"
        "fixed Pip-Boy open/close taps"
        "return;")
    require_text(
        "${headset_demo_fixture_main_loop}"
        "${required}"
        "Headset demo fixture must keep its own bounded in-game UI path")
endforeach()
foreach(forbidden IN ITEMS
        "ensureAuthorizedSharedBridgeStarted("
        "ensureAuthorizedDesktopAssistBridgeStarted("
        "ensureAuthorizedTrackedPropAssistBridgeStarted("
        "processMainGameLoop("
        "SendInput("
        "keybd_event("
        "mouse_event("
        "SetCursorPos("
        "installCameraHook("
        "installRetailRigHook(")
    forbid_text(
        "${headset_demo_fixture_main_loop}"
        "${forbidden}"
        "Headset demo fixture must not acquire desktop, controller, simulator, camera, rig, or bridge authority")
endforeach()

extract_region(
    retail_fixture_automation
    "${plugin_text}"
    "enum class RetailFixtureAutomationStage"
    "void processStereoVisualTrialFixedSaveAutomation(\n")
foreach(required IN ITEMS
        "RetailFixtureAutomationStage::AwaitingStartMenu"
        "RetailFixtureAutomationStage::AwaitingLoadGameplay"
        "RetailFixtureAutomationStage::AwaitingGameplayName"
        "RetailFixtureAutomationStage::AwaitingFirstTrait"
        "RetailFixtureAutomationStage::AwaitingSecondTrait"
        "RetailFixtureAutomationStage::AwaitingSave"
        "retailFixtureCommandIsExact"
        "fixture::CreateStartCommand"
        "fixture::LoadCommandPrefix"
        "fixture::SetFixturePlayerNameCommand"
        "fixture::SaveCommandPrefix"
        "fixture::addPerkCommand(trait)"
        "acknowledgeExactOfficialPackMessageMenu(observation)"
        "closeExactRetailFixtureOfficialPackMessageMenu(observation)"
        "realStereoVisualTrialStartMenuState(observation)"
        "realRetailFixtureFreshGameplayState(observation)"
        "FixtureLoadGameplaySettlingFrames = 1u"
        "FixtureMutationSettlingFrames = 20u"
        "fnvxrRetailFixtureLoadDispatched"
        "fnvxrRetailFixtureLoadGameplay"
        "\"load-gameplay\""
        "fnvxrRetailFixtureComplete")
    require_text(
        "${retail_fixture_automation}"
        "${required}"
        "Retail fixture automation lost an exact lifecycle, command, or real-state guard")
endforeach()
foreach(forbidden IN ITEMS
        "SendInput("
        "keybd_event("
        "mouse_event("
        "SetCursorPos("
        "CloseAllMenus"
        "setTileFloatByName("
        "dispatchMenuClick("
        "ensureAuthorizedSharedBridgeStarted("
        "ensureAuthorizedDesktopAssistBridgeStarted("
        "ensureAuthorizedTrackedPropAssistBridgeStarted("
        "installCameraHook("
        "installRetailRigHook("
        "updateWeaponAlignment("
        "HeadsetDemoMutationSettlingFrames"
        "mutationSettlingFrames = headsetDemoFixtureProfileSelected()")
    forbid_text(
        "${retail_fixture_automation}"
        "${forbidden}"
        "Retail fixture automation must not acquire input, menu, bridge, camera, rig, or weapon authority")
endforeach()
string(REGEX MATCHALL
    "runPluginConsoleCommand\\("
    retail_fixture_console_calls
    "${retail_fixture_automation}")
list(LENGTH retail_fixture_console_calls retail_fixture_console_call_count)
if(NOT retail_fixture_console_call_count EQUAL 4)
    message(FATAL_ERROR
        "Retail fixture automation must have exactly its start/load, name, trait, and save console execution sites")
endif()

extract_region(
    retail_fixture_exact_pack_close
    "${plugin_text}"
    "void closeExactRetailFixtureOfficialPackMessageMenu(\n"
    "enum class RetailFixtureAutomationStage")
foreach(required IN ITEMS
        "retailFixtureOfficialPackAcknowledgementRequested()"
        "findExactOfficialPackMessageMenuTarget("
        "fixture::exactOfficialPackCloseAuthorized("
        "closeAttemptCount"
        "visibleOfficialPackNotificationMask"
        "exactOfficialPackVisibleLastFrame"
        "fixture::MaxExactOfficialPackCloseAttemptsPerRun"
        "fixture::CloseExactOfficialPackMessageCommand.data()"
        "runPluginConsoleCommand("
        "fnvxrRetailFixtureOfficialPackClose")
    require_text(
        "${retail_fixture_exact_pack_close}"
        "${required}"
        "Retail fixture stock popup fallback lost an exact owned-fixture guard")
endforeach()
foreach(forbidden IN ITEMS
        "dispatchMenuClick("
        "SendInput("
        "keybd_event("
        "mouse_event("
        "SetCursorPos("
        "setTileFloatByName(")
    forbid_text(
        "${retail_fixture_exact_pack_close}"
        "${forbidden}"
        "Retail fixture stock popup fallback must not acquire menu/device input authority")
endforeach()

extract_region(
    retail_fixture_pack_acknowledgement_opt_in
    "${plugin_text}"
    "bool retailFixtureOfficialPackAcknowledgementRequested()\n{"
    "bool exactOfficialPackAcknowledgementRequested()")
foreach(required IN ITEMS
        "retailFixtureAutomationRequested()"
        "FNVXR_RETAIL_FIXTURE_ACK_OFFICIAL_PACK_POPUP")
    require_text(
        "${retail_fixture_pack_acknowledgement_opt_in}"
        "${required}"
        "Retail fixture official-pack acknowledgement must remain an explicit owned-fixture opt-in")
endforeach()
foreach(forbidden IN ITEMS
        "SendInput("
        "keybd_event("
        "mouse_event("
        "SetCursorPos("
        "CloseAllMenus")
    forbid_text(
        "${retail_fixture_pack_acknowledgement_opt_in}"
        "${forbidden}"
        "Retail fixture official-pack acknowledgement must not gain OS/menu-wide input authority")
endforeach()

extract_region(
    full_bridge_bootstrap
    "${plugin_text}"
    "bool ensureAuthorizedSharedBridgeStarted()\n{"
    "void processMainGameLoop(")
string(FIND
    "${full_bridge_bootstrap}"
    "stereoVisualTrialProfileSelected()"
    visual_trial_reject_at)
string(FIND
    "${full_bridge_bootstrap}"
    "authorizeCurrentRetailRuntimeAtDecisionPoint()"
    full_authority_at)
string(FIND
    "${full_bridge_bootstrap}"
    "installCameraHook()"
    camera_hook_at)
string(FIND
    "${full_bridge_bootstrap}"
    "installRetailRigHook()"
    rig_hook_at)
if(visual_trial_reject_at EQUAL -1
    OR full_authority_at EQUAL -1
    OR camera_hook_at EQUAL -1
    OR rig_hook_at EQUAL -1
    OR visual_trial_reject_at GREATER full_authority_at
    OR visual_trial_reject_at GREATER camera_hook_at
    OR visual_trial_reject_at GREATER rig_hook_at)
    message(FATAL_ERROR
        "Visual-trial profile must reject the plugin full bridge before authority or camera/rig hooks")
endif()

extract_region(
    main_loop_handler
    "${plugin_text}"
    "void handleNvseMessage(NVSEMessagingInterface::Message* message)\n{"
    "void tapKey(WORD virtualKey)")
string(FIND
    "${main_loop_handler}"
    "ensureAuthorizedRuntimeObservationStarted()"
    observation_start_at)
string(FIND
    "${main_loop_handler}"
    "observeAndPublishRuntime()"
    runtime_publish_at)
string(FIND
    "${main_loop_handler}"
    "RetailPluginMainLoopDisposition::\n                PublishRuntimeOnly"
    publication_only_at)
string(FIND
    "${main_loop_handler}"
    "ensureAuthorizedSharedBridgeStarted()"
    full_bridge_at)
if(observation_start_at EQUAL -1
    OR runtime_publish_at EQUAL -1
    OR publication_only_at EQUAL -1
    OR full_bridge_at EQUAL -1
    OR observation_start_at GREATER runtime_publish_at
    OR runtime_publish_at GREATER publication_only_at
    OR publication_only_at GREATER full_bridge_at
    OR runtime_publish_at GREATER full_bridge_at)
    message(FATAL_ERROR
        "Main-loop observation must publish, then return publication-only for visual trial before the full bridge")
endif()

extract_region(
    publication_only_branch
    "${main_loop_handler}"
    "if (visualTrialDisposition\n            == fnvxr::engine::RetailPluginMainLoopDisposition::\n                PublishRuntimeOnly)"
    "if (desktopAssistProfileRequested())")
require_text(
    "${publication_only_branch}"
    "return;"
    "Visual-trial runtime publication must return before plugin bridge work")
foreach(required IN ITEMS
        "if (stereoVisualTrialAutomationRequested())"
        "processStereoVisualTrialFixedSaveAutomation(observation)")
    require_text(
        "${publication_only_branch}"
        "${required}"
        "Publication-only branch lost its explicit narrow automation guard")
endforeach()
foreach(forbidden IN ITEMS
        "ensureAuthorizedSharedBridgeStarted("
        "ensureAuthorizedDesktopAssistBridgeStarted("
        "ensureAuthorizedTrackedPropAssistBridgeStarted("
        "installCameraHook("
        "installRetailRigHook("
        "processMainGameLoop("
        "processDesktopAssistMainLoop("
        "processTrackedPropAssistMainLoop("
        "consumeSharedCommand("
        "consumeExternalXInputBridge("
        "consumeExternalDInputBridge("
        "executeAcceptClickOnGameThread("
        "updateWeaponAlignment("
        "applyRetailRigPose("
        "SendInput(")
    forbid_text(
        "${publication_only_branch}"
        "${forbidden}"
        "Visual-trial publication-only branch reached plugin mutation or input")
endforeach()

extract_region(
    camera_hook
    "${plugin_text}"
    "bool installCameraHook()\n{"
    "Matrix33 transposeMatrix33(")
string(FIND
    "${camera_hook}"
    "if (stereoVisualTrialProfileSelected())"
    camera_visual_reject_at)
string(FIND
    "${camera_hook}"
    "writeJump(PlayerCharacterUpdateCameraAddress"
    camera_write_at)
if(camera_visual_reject_at EQUAL -1
    OR camera_write_at EQUAL -1
    OR camera_visual_reject_at GREATER camera_write_at)
    message(FATAL_ERROR
        "Camera-hook mutation point must reject the visual-trial profile before writing protected retail code")
endif()

extract_region(
    rig_hook
    "${plugin_text}"
    "bool installRetailRigHook()\n{"
    "TileValue* getTileValue(")
string(FIND
    "${rig_hook}"
    "if (stereoVisualTrialProfileSelected())"
    rig_visual_reject_at)
string(FIND
    "${rig_hook}"
    "writeJump(PlayerAnimationApplyCallSiteAddress"
    rig_write_at)
if(rig_visual_reject_at EQUAL -1
    OR rig_write_at EQUAL -1
    OR rig_visual_reject_at GREATER rig_write_at)
    message(FATAL_ERROR
        "Rig-hook mutation point must reject the visual-trial profile before writing retail code")
endif()

extract_region(
    plugin_load
    "${plugin_text}"
    [[extern "C" __declspec(dllexport) bool NVSEPlugin_Load(const NVSEInterface* nvse)
{]]
    "BOOL APIENTRY DllMain")
foreach(forbidden IN ITEMS
        "ensureAuthorizedRuntimeObservationStarted("
        "ensureAuthorizedSharedBridgeStarted("
        "authorizeCurrentRetailRuntimeAtDecisionPoint("
        "proveCurrentRetailCompatibilityAtDecisionPoint("
        "initSharedRuntime("
        "observeAndPublishRuntime(")
    forbid_text(
        "${plugin_load}"
        "${forbidden}"
        "NVSEPlugin_Load must not race Present by acquiring authority or publishing early")
endforeach()
require_text(
    "${plugin_load}"
    "stereo visual-trial profile selected: deferring to publication-only main-loop authority; plugin full bridge, input, camera, and rig hooks are disabled"
    "NVSEPlugin_Load must document the visual-trial publication-only barrier")

message(STATUS "Retail runtime-publication bootstrap fuse PASS")
