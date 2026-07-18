if(NOT DEFINED ROOT_CMAKE OR NOT EXISTS "${ROOT_CMAKE}")
    message(FATAL_ERROR "ROOT_CMAKE must name the root CMakeLists.txt")
endif()
if(NOT DEFINED PROJECT_BINARY_DIR)
    message(FATAL_ERROR "PROJECT_BINARY_DIR is required")
endif()
if(NOT DEFINED EXPECT_VCXPROJ)
    set(EXPECT_VCXPROJ FALSE)
endif()

file(READ "${ROOT_CMAKE}" root_cmake)
string(REPLACE "\r\n" "\n" root_cmake "${root_cmake}")

function(require_text haystack needle reason)
    string(FIND "${haystack}" "${needle}" found_at)
    if(found_at EQUAL -1)
        message(FATAL_ERROR "${reason}: missing '${needle}'")
    endif()
endfunction()

set(expected_target_list [=[set(_fnvxr_deterministic_win32_dll_targets
        nvse_fnvxr
        fnvxr_d3d9_proxy
        fnvxr_dinput8_proxy
        fnvxr_xinput_proxy)]=])
require_text(
    "${root_cmake}"
    "${expected_target_list}"
    "The deterministic artifact policy must cover exactly the four retail DLLs")
require_text(
    "${root_cmake}"
    [=[target_compile_options(
                ${_fnvxr_deterministic_target} PRIVATE /Brepro)]=]
    "Every retail DLL must compile reproducibly")
require_text(
    "${root_cmake}"
    [=[target_link_options(
                ${_fnvxr_deterministic_target} PRIVATE
                /Brepro
                /INCREMENTAL:NO
                "$<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:/PDBALTPATH:$<TARGET_PDB_FILE_NAME:${_fnvxr_deterministic_target}>>")]=]
    "Every retail DLL must link non-incrementally and reproducibly")

if(NOT EXPECT_VCXPROJ)
    message(STATUS
        "Deterministic Win32 DLL source policy PASS (no vcxproj expected)")
    return()
endif()

set(artifact_targets
    nvse_fnvxr
    fnvxr_d3d9_proxy
    fnvxr_dinput8_proxy
    fnvxr_xinput_proxy)
set(configurations Debug Release MinSizeRel RelWithDebInfo)

function(extract_region output haystack begin_marker end_marker context)
    string(FIND "${haystack}" "${begin_marker}" begin_at)
    if(begin_at EQUAL -1)
        message(FATAL_ERROR "${context}: missing '${begin_marker}'")
    endif()
    string(SUBSTRING "${haystack}" ${begin_at} -1 region_tail)
    string(FIND "${region_tail}" "${end_marker}" relative_end_at)
    if(relative_end_at EQUAL -1 OR relative_end_at EQUAL 0)
        message(FATAL_ERROR "${context}: missing '${end_marker}'")
    endif()
    string(LENGTH "${end_marker}" end_marker_length)
    math(EXPR region_length "${relative_end_at} + ${end_marker_length}")
    string(SUBSTRING "${haystack}" ${begin_at} ${region_length} region)
    set(${output} "${region}" PARENT_SCOPE)
endfunction()

foreach(artifact_target IN LISTS artifact_targets)
    if(artifact_target STREQUAL "nvse_fnvxr")
        set(expected_pdb_name "nvse_fnvxr.pdb")
    elseif(artifact_target STREQUAL "fnvxr_d3d9_proxy")
        set(expected_pdb_name "d3d9.pdb")
    elseif(artifact_target STREQUAL "fnvxr_dinput8_proxy")
        set(expected_pdb_name "dinput8.pdb")
    elseif(artifact_target STREQUAL "fnvxr_xinput_proxy")
        set(expected_pdb_name "xinput1_3.pdb")
    else()
        message(FATAL_ERROR "Unhandled deterministic artifact ${artifact_target}")
    endif()
    set(project_file "${PROJECT_BINARY_DIR}/${artifact_target}.vcxproj")
    if(NOT EXISTS "${project_file}")
        message(FATAL_ERROR
            "Missing generated Visual Studio project for ${artifact_target}: "
            "${project_file}")
    endif()
    file(READ "${project_file}" project)
    string(REPLACE "\r\n" "\n" project "${project}")

    foreach(configuration IN LISTS configurations)
        set(condition
            "'$(Configuration)|$(Platform)'=='${configuration}|Win32'")
        require_text(
            "${project}"
            "<LinkIncremental Condition=\"${condition}\">false</LinkIncremental>"
            "${artifact_target}/${configuration} must disable incremental linking")

        set(group_begin "<ItemDefinitionGroup Condition=\"${condition}\">")
        extract_region(
            definition_group
            "${project}"
            "${group_begin}"
            "</ItemDefinitionGroup>"
            "${artifact_target}/${configuration}")
        extract_region(
            compile_group
            "${definition_group}"
            "<ClCompile>"
            "</ClCompile>"
            "${artifact_target}/${configuration} compiler settings")
        extract_region(
            link_group
            "${definition_group}"
            "<Link>"
            "</Link>"
            "${artifact_target}/${configuration} linker settings")
        require_text(
            "${compile_group}"
            "/Brepro"
            "${artifact_target}/${configuration} compiler must receive /Brepro")
        require_text(
            "${link_group}"
            "/Brepro"
            "${artifact_target}/${configuration} linker must receive /Brepro")

        string(FIND "${link_group}" "/PDBALTPATH:" any_pdb_alt_at)
        string(FIND
            "${link_group}"
            "/PDBALTPATH:${expected_pdb_name}"
            pdb_alt_at)
        if(configuration STREQUAL "Debug"
            OR configuration STREQUAL "RelWithDebInfo")
            if(any_pdb_alt_at EQUAL -1 OR pdb_alt_at EQUAL -1)
                message(FATAL_ERROR
                    "${artifact_target}/${configuration} must embed only the PDB basename")
            endif()
        elseif(NOT any_pdb_alt_at EQUAL -1)
            message(FATAL_ERROR
                "${artifact_target}/${configuration} unexpectedly carries /PDBALTPATH")
        endif()
    endforeach()
endforeach()

message(STATUS
    "Deterministic Win32 DLL policy PASS (source and generated vcxproj)")
