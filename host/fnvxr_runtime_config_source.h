#pragma once

#include <string>

namespace fnvxr::host
{
class RuntimeConfigSource
{
public:
    virtual ~RuntimeConfigSource() = default;
    virtual bool read(const char* name, std::string& destination) const = 0;
};

class ProcessEnvironmentConfigSource final : public RuntimeConfigSource
{
public:
    bool read(const char* name, std::string& destination) const override;
};
}
