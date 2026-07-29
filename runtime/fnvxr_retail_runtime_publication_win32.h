#pragma once

#include "fnvxr_retail_runtime_publication.h"

namespace fnvxr::engine
{
#if defined(_WIN32)
inline constexpr bool RetailRuntimePublicationWin32ReaderAvailable = true;
#else
inline constexpr bool RetailRuntimePublicationWin32ReaderAvailable = false;
#endif

inline constexpr char RetailRuntimePublicationMappingName[] =
    "Local\\FNVXR_Runtime_State";

class RetailRuntimePublicationWin32Reader final
{
public:
    RetailRuntimePublicationWin32Reader() noexcept = default;
    ~RetailRuntimePublicationWin32Reader() noexcept;

    RetailRuntimePublicationWin32Reader(
        const RetailRuntimePublicationWin32Reader&) = delete;
    RetailRuntimePublicationWin32Reader& operator=(
        const RetailRuntimePublicationWin32Reader&) = delete;

    bool initialize(const char* mappingName = nullptr) noexcept;
    void reset() noexcept;
    bool initialized() const noexcept;
    bool mappingReady() const noexcept;
    bool readReadyPublication(
        shared::SharedRuntimeState& state,
        LONG& sequence) noexcept;
    RetailRuntimePublicationReadinessFailure failure() const noexcept;

private:
    bool openMapping() noexcept;

    const char* mMappingName = nullptr;
    HANDLE mMapping = nullptr;
    const shared::SharedRuntimeState* mState = nullptr;
    RetailRuntimePublicationReadinessFailure mFailure =
        RetailRuntimePublicationReadinessFailure::ReaderNotInitialized;
    bool mInitialized = false;
};
}
