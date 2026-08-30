#include "fnvxr_runtime_config_source.h"

#include <cstdlib>

namespace fnvxr::host
{
bool ProcessEnvironmentConfigSource::read(
    const char* name,
    std::string& destination) const
{
    char* value = nullptr;
    std::size_t length = 0;
    const errno_t error = _dupenv_s(&value, &length, name);
    if (error != 0 || value == nullptr)
    {
        std::free(value);
        return false;
    }
    destination.assign(value, length > 0 ? length - 1 : 0);
    std::free(value);
    return true;
}
}
