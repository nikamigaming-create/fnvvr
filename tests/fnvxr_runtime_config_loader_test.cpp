#include "../host/fnvxr_runtime_config_loader.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>

namespace
{
class FakeSource final : public fnvxr::host::RuntimeConfigSource
{
public:
    using Entry = std::pair<const char*, const char*>;

    explicit FakeSource(std::vector<Entry> entries = {})
        : entries_(std::move(entries))
    {
    }

    bool read(const char* name, std::string& destination) const override
    {
        ++readCount;
        for (const Entry& entry : entries_)
        {
            if (std::strcmp(entry.first, name) == 0)
            {
                destination = entry.second;
                return true;
            }
        }
        destination.clear();
        return false;
    }

    mutable std::size_t readCount = 0;

private:
    std::vector<Entry> entries_;
};

int fail(const char* message)
{
    std::cerr << message << '\n';
    return EXIT_FAILURE;
}
}

int main()
{
    using namespace fnvxr;

    FakeSource defaults;
    const host::RuntimeConfigLoadResult loadedDefaults =
        host::loadRuntimeConfig(defaults);
    if (!loadedDefaults || !loadedDefaults.config)
        return fail("an empty source must load production defaults");
    if (loadedDefaults.config->get().performance.targetRefreshHz != 90)
        return fail("the host boundary must preserve the single production default");
    const std::size_t readsAfterLoad = defaults.readCount;
    (void)loadedDefaults.config->get().bodyRig.shoulderWidthMeters;
    (void)loadedDefaults.config->frameBudgetMilliseconds();
    if (defaults.readCount != readsAfterLoad)
        return fail("validated config access must not return to the environment source");

    FakeSource overrides({
        { "FNVXR_TARGET_REFRESH_HZ", "120" },
        { "FNVXR_STEREO_MAX_SOURCE_POSE_AGE_MS", "8.0" },
        { "FNVXR_CPU_STEREO_MAX_SOURCE_POSE_AGE_MS", "250" },
        { "FNVXR_ENABLE_ENGINE_CENTER_STEREO", "1" },
        { "FNVXR_GAME_PLANE_WIDTH", "3.15" },
        { "FNVXR_GAME_PLANE_HEIGHT", "2.15" },
        { "FNVXR_GAME_PLANE_OFFSET_Z", "-3.35" },
        { "FNVXR_BODY_SHOULDER_WIDTH", "0.46" },
        { "FNVXR_BODY_SHOULDER_DROP", "0.26" },
        { "FNVXR_BODY_SHOULDER_BACK", "0.10" },
        { "FNVXR_PIPBOY_WRIST_UI_WIDTH", "0.12" },
        { "FNVXR_PIPBOY_UI_MAX_CONTENT_AGE_MS", "180" },
    });
    const host::RuntimeConfigLoadResult loadedOverrides =
        host::loadRuntimeConfig(overrides);
    if (!loadedOverrides || !loadedOverrides.config)
        return fail("valid typed overrides must load");
    const kernel::RuntimeConfig& config = loadedOverrides.config->get();
    if (config.performance.targetRefreshHz != 120
        || config.performance.maximumCpuPoseAgeMilliseconds != 250.0F
        || config.performance.eyeTransport
            != kernel::EyeTransport::GpuColorV5
        || config.presentation.menuWidthMeters != 3.15F
        || config.presentation.menuHeightMeters != 2.15F
        || config.presentation.menuDistanceMeters != 3.35F
        || config.bodyRig.shoulderWidthMeters != 0.46F
        || config.bodyRig.shoulderDropMeters != 0.26F
        || config.bodyRig.shoulderBackMeters != 0.10F
        || config.wristUi.widthMeters != 0.12F
        || config.wristUi.maximumContentAgeMilliseconds != 180u)
        return fail("the host boundary must map overrides to typed fields");

    FakeSource typedMenuPrecedence({
        { "FNVXR_MENU_WIDTH_METERS", "1.50" },
        { "FNVXR_MENU_HEIGHT_METERS", "0.80" },
        { "FNVXR_MENU_DISTANCE_METERS", "1.60" },
        { "FNVXR_GAME_PLANE_WIDTH", "malformed-legacy" },
        { "FNVXR_GAME_PLANE_HEIGHT", "malformed-legacy" },
        { "FNVXR_GAME_PLANE_OFFSET_Z", "malformed-legacy" },
    });
    const host::RuntimeConfigLoadResult typedMenu =
        host::loadRuntimeConfig(typedMenuPrecedence);
    if (!typedMenu || !typedMenu.config
        || typedMenu.config->get().presentation.menuWidthMeters != 1.50F
        || typedMenu.config->get().presentation.menuHeightMeters != 0.80F
        || typedMenu.config->get().presentation.menuDistanceMeters != 1.60F)
        return fail("typed menu names must override legacy aliases atomically");

    FakeSource malformed({ { "FNVXR_TARGET_REFRESH_HZ", "90hz" } });
    const host::RuntimeConfigLoadResult malformedResult =
        host::loadRuntimeConfig(malformed);
    if (malformedResult
        || malformedResult.textError != host::RuntimeConfigTextError::InvalidUnsignedInteger
        || malformedResult.variable == nullptr
        || std::strcmp(malformedResult.variable, "FNVXR_TARGET_REFRESH_HZ") != 0)
        return fail("malformed values must identify their exact source variable");

    FakeSource emptyValue({ { "FNVXR_TARGET_REFRESH_HZ", "" } });
    if (host::loadRuntimeConfig(emptyValue).textError !=
        host::RuntimeConfigTextError::EmptyValue)
        return fail("an explicitly empty variable must not silently select a default");
    if (std::strcmp(host::runtimeConfigTextErrorName(
            host::RuntimeConfigTextError::EmptyValue), "empty-value") != 0)
        return fail("text failures must expose stable diagnostic names");

    FakeSource invalidBoolean({ { "FNVXR_ENABLE_ENGINE_CENTER_STEREO", "yes" } });
    if (!host::loadRuntimeConfig(invalidBoolean))
        return fail("retired CPU-transport selector must not affect GPU configuration");

    FakeSource invalidConfig({
        { "FNVXR_TARGET_REFRESH_HZ", "120" },
        { "FNVXR_STEREO_MAX_SOURCE_POSE_AGE_MS", "26" },
    });
    const host::RuntimeConfigLoadResult invalidConfigResult =
        host::loadRuntimeConfig(invalidConfig);
    if (invalidConfigResult
        || invalidConfigResult.validationError != kernel::ConfigError::InvalidPoseAge
        || invalidConfigResult.config.has_value())
        return fail("cross-field failures must not expose a partial config");

    return EXIT_SUCCESS;
}
