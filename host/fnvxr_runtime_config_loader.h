#pragma once

#include "fnvxr_runtime_config_source.h"
#include "../kernel/config_validation.h"

#include <optional>

namespace fnvxr::host
{
enum class RuntimeConfigTextError
{
    None,
    EmptyValue,
    InvalidUnsignedInteger,
    IntegerOutOfRange,
    InvalidFloat,
    InvalidBoolean,
};

struct RuntimeConfigLoadResult
{
    RuntimeConfigTextError textError = RuntimeConfigTextError::None;
    kernel::ConfigError validationError = kernel::ConfigError::None;
    const char* variable = nullptr;
    std::optional<kernel::ValidatedRuntimeConfig> config;

    explicit operator bool() const noexcept;
};

RuntimeConfigLoadResult loadRuntimeConfig(
    const RuntimeConfigSource& source);
const char* runtimeConfigTextErrorName(RuntimeConfigTextError error) noexcept;
}
