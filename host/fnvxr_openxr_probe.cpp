#define WIN32_LEAN_AND_MEAN

#include <windows.h>

#include <openxr/openxr.h>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
HMODULE loadOpenXrLoader()
{
    HMODULE module = LoadLibraryA("openxr_loader.dll");
    if (module)
        return module;

#ifdef FNVXR_OPENXR_LOADER_HINT
    std::string hint = FNVXR_OPENXR_LOADER_HINT;
    const std::string::size_type slash = hint.find_last_of("/\\");
    if (slash != std::string::npos)
    {
        const std::string dir = hint.substr(0, slash);
        SetDllDirectoryA(dir.c_str());
    }

    module = LoadLibraryExA(FNVXR_OPENXR_LOADER_HINT, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (module)
        return module;

    std::cerr << "failed loader hint " << FNVXR_OPENXR_LOADER_HINT
              << " GetLastError=" << GetLastError() << "\n";
#else
    return nullptr;
#endif

    return nullptr;
}

const char* resultName(XrResult result)
{
    switch (result)
    {
        case XR_SUCCESS: return "XR_SUCCESS";
        case XR_ERROR_RUNTIME_UNAVAILABLE: return "XR_ERROR_RUNTIME_UNAVAILABLE";
        case XR_ERROR_FORM_FACTOR_UNAVAILABLE: return "XR_ERROR_FORM_FACTOR_UNAVAILABLE";
        case XR_ERROR_INITIALIZATION_FAILED: return "XR_ERROR_INITIALIZATION_FAILED";
        case XR_ERROR_VALIDATION_FAILURE: return "XR_ERROR_VALIDATION_FAILURE";
        default: return "XR_RESULT_OTHER";
    }
}

void writeProbeStep(const char* step)
{
    std::ofstream out("local/openxr-probe-step.txt", std::ios::app);
    out << step << "\n";
}
}

int main()
{
    CreateDirectoryA("local", nullptr);
    DeleteFileA("local/openxr-probe-step.txt");
    writeProbeStep("start");

    HMODULE loader = loadOpenXrLoader();
    if (!loader)
    {
        std::cerr << "OpenXR loader not found\n";
        writeProbeStep("loader-not-found");
        return 2;
    }
    writeProbeStep("loader-loaded");

    auto enumerateExtensions = reinterpret_cast<PFN_xrEnumerateInstanceExtensionProperties>(
        GetProcAddress(loader, "xrEnumerateInstanceExtensionProperties"));
    auto enumerateLayers = reinterpret_cast<PFN_xrEnumerateApiLayerProperties>(
        GetProcAddress(loader, "xrEnumerateApiLayerProperties"));
    auto createInstance = reinterpret_cast<PFN_xrCreateInstance>(
        GetProcAddress(loader, "xrCreateInstance"));
    auto destroyInstance = reinterpret_cast<PFN_xrDestroyInstance>(
        GetProcAddress(loader, "xrDestroyInstance"));
    auto getSystem = reinterpret_cast<PFN_xrGetSystem>(
        GetProcAddress(loader, "xrGetSystem"));

    if (!enumerateExtensions || !enumerateLayers || !createInstance || !destroyInstance || !getSystem)
    {
        std::cerr << "OpenXR loader is missing expected exports\n";
        writeProbeStep("missing-exports");
        FreeLibrary(loader);
        return 3;
    }
    writeProbeStep("exports-found");

    uint32_t layerCount = 0;
    writeProbeStep("before-enumerate-layers");
    XrResult layerResult = enumerateLayers(0, &layerCount, nullptr);
    writeProbeStep("after-enumerate-layers");
    std::cout << "xrEnumerateApiLayerProperties: " << resultName(layerResult)
              << " count=" << layerCount << "\n";
    std::cout.flush();

    uint32_t extensionCount = 0;
    writeProbeStep("before-enumerate-extensions");
    XrResult extensionResult = enumerateExtensions(nullptr, 0, &extensionCount, nullptr);
    writeProbeStep("after-enumerate-extensions");
    std::cout << "xrEnumerateInstanceExtensionProperties: " << resultName(extensionResult)
              << " count=" << extensionCount << "\n";
    std::cout.flush();

    if (extensionResult == XR_SUCCESS && extensionCount > 0)
    {
        std::vector<XrExtensionProperties> extensions(extensionCount);
        for (auto& extension : extensions)
            extension.type = XR_TYPE_EXTENSION_PROPERTIES;

        extensionResult = enumerateExtensions(nullptr, extensionCount, &extensionCount, extensions.data());
        if (extensionResult == XR_SUCCESS)
        {
            const uint32_t maxToPrint = extensionCount < 12 ? extensionCount : 12;
            for (uint32_t index = 0; index < maxToPrint; ++index)
                std::cout << "  " << extensions[index].extensionName << "\n";
            std::cout.flush();
        }
    }

    XrInstanceCreateInfo createInfo { XR_TYPE_INSTANCE_CREATE_INFO };
    strcpy_s(createInfo.applicationInfo.applicationName, "FNVXR Probe");
    createInfo.applicationInfo.applicationVersion = 1;
    strcpy_s(createInfo.applicationInfo.engineName, "fnvxr-bridge-experiment");
    createInfo.applicationInfo.engineVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    XrInstance instance = XR_NULL_HANDLE;
    writeProbeStep("before-create-instance");
    XrResult createResult = createInstance(&createInfo, &instance);
    writeProbeStep("after-create-instance");
    std::cout << "xrCreateInstance: " << resultName(createResult) << "\n";

    XrResult systemResult = XR_ERROR_RUNTIME_UNAVAILABLE;
    if (createResult == XR_SUCCESS)
    {
        XrSystemGetInfo systemInfo { XR_TYPE_SYSTEM_GET_INFO };
        systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        XrSystemId systemId = XR_NULL_SYSTEM_ID;

        writeProbeStep("before-get-system");
        systemResult = getSystem(instance, &systemInfo, &systemId);
        writeProbeStep("after-get-system");
        std::cout << "xrGetSystem(HMD): " << resultName(systemResult)
                  << " systemId=" << systemId << "\n";

        destroyInstance(instance);
    }
    std::cout.flush();

    FreeLibrary(loader);
    writeProbeStep("complete");
    return extensionResult == XR_SUCCESS && (createResult == XR_SUCCESS || createResult == XR_ERROR_RUNTIME_UNAVAILABLE) ? 0 : 4;
}
