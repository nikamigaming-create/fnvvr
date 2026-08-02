#include "fnvxr_retail_runtime_publication_win32.h"

#include <cstring>

namespace fnvxr::engine
{
RetailRuntimePublicationWin32Reader::
    ~RetailRuntimePublicationWin32Reader() noexcept
{
    reset();
}

bool RetailRuntimePublicationWin32Reader::initialize(
    const char* mappingName) noexcept
{
    reset();
    mMappingName = mappingName && mappingName[0] != '\0'
        ? mappingName
        : RetailRuntimePublicationMappingName;
    mInitialized = true;
    return openMapping();
}

void RetailRuntimePublicationWin32Reader::reset() noexcept
{
#if defined(_WIN32)
    if (mState)
        UnmapViewOfFile(mState);
    if (mMapping)
        CloseHandle(mMapping);
#endif
    mMappingName = nullptr;
    mMapping = nullptr;
    mState = nullptr;
    mFailure =
        RetailRuntimePublicationReadinessFailure::ReaderNotInitialized;
    mInitialized = false;
}

bool RetailRuntimePublicationWin32Reader::initialized() const noexcept
{
    return mInitialized && mMappingName;
}

bool RetailRuntimePublicationWin32Reader::mappingReady() const noexcept
{
    return initialized() && mMapping && mState;
}

bool RetailRuntimePublicationWin32Reader::openMapping() noexcept
{
#if defined(_WIN32)
    if (!initialized())
    {
        mFailure =
            RetailRuntimePublicationReadinessFailure::ReaderNotInitialized;
        return false;
    }
    if (mappingReady())
        return true;

    mMapping = OpenFileMappingA(
        FILE_MAP_READ,
        FALSE,
        mMappingName);
    if (!mMapping)
    {
        mFailure =
            RetailRuntimePublicationReadinessFailure::MappingUnavailable;
        return false;
    }
    mState = static_cast<const shared::SharedRuntimeState*>(
        MapViewOfFile(
            mMapping,
            FILE_MAP_READ,
            0u,
            0u,
            sizeof(shared::SharedRuntimeState)));
    if (!mState)
    {
        CloseHandle(mMapping);
        mMapping = nullptr;
        mFailure =
            RetailRuntimePublicationReadinessFailure::MappingUnavailable;
        return false;
    }
    return true;
#else
    mFailure =
        RetailRuntimePublicationReadinessFailure::MappingUnavailable;
    return false;
#endif
}

bool RetailRuntimePublicationWin32Reader::readReadyPublication(
    shared::SharedRuntimeState& state,
    LONG& sequence) noexcept
{
    state = {};
    sequence = 0;
    if (!initialized())
    {
        mFailure =
            RetailRuntimePublicationReadinessFailure::ReaderNotInitialized;
        return false;
    }
    if (!mappingReady() && !openMapping())
        return false;

#if defined(_WIN32)
    RetailRuntimePublicationReadinessFailure lastFailure =
        RetailRuntimePublicationReadinessFailure::PublicationUnstable;
    for (int attempt = 0; attempt < 8; ++attempt)
    {
        RetailRuntimePublicationObservation observation {};
        observation.sequenceBefore = mState->sequence;
        MemoryBarrier();
        std::memcpy(
            &observation.state,
            mState,
            sizeof(observation.state));
        MemoryBarrier();
        observation.sequenceAfter = mState->sequence;

        const RetailRuntimePublicationReadiness readiness =
            assessRetailRuntimePublicationReadiness(observation);
        lastFailure = readiness.failure;
        if (readiness.complete())
        {
            state = observation.state;
            sequence = observation.sequenceAfter;
            mFailure = RetailRuntimePublicationReadinessFailure::None;
            return true;
        }
        if (readiness.failure
                == RetailRuntimePublicationReadinessFailure::HeaderInvalid
            || readiness.failure
                == RetailRuntimePublicationReadinessFailure::
                    NeutralPublication
            || readiness.failure
                == RetailRuntimePublicationReadinessFailure::PayloadInvalid)
        {
            mFailure = readiness.failure;
            return false;
        }
        YieldProcessor();
    }
    mFailure = lastFailure
            == RetailRuntimePublicationReadinessFailure::SequenceUnpublished
        || lastFailure
            == RetailRuntimePublicationReadinessFailure::PublicationChanged
        ? RetailRuntimePublicationReadinessFailure::PublicationUnstable
        : lastFailure;
#else
    mFailure =
        RetailRuntimePublicationReadinessFailure::MappingUnavailable;
#endif
    return false;
}

RetailRuntimePublicationReadinessFailure
RetailRuntimePublicationWin32Reader::failure() const noexcept
{
    return mFailure;
}
}
