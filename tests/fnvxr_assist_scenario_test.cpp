#include "fnvxr_assist_scenario.h"

#include <cstdlib>
#include <iostream>

int main()
{
    using namespace fnvxr::assist;

    if (!headBodyScenarioIsValid())
    {
        std::cerr << "head/body assist scenario must retain its isolated baseline, yaw, and translation probes\n";
        return EXIT_FAILURE;
    }
    if (!trackedPropScenarioIsValid())
    {
        std::cerr << "tracked-prop assist scenario must retain isolated right controller position and aim probes\n";
        return EXIT_FAILURE;
    }

    if (HeadBodyScenario.front().expectsRotationResponse
        || HeadBodyScenario.front().expectsTranslationResponse
        || HeadBodyScenario.front().positionY != 0.0f)
    {
        std::cerr << "head/body assist scenario must begin with a neutral baseline\n";
        return EXIT_FAILURE;
    }

    if (HeadBodyScenario.back().yawDegrees != 0.0f
        || HeadBodyScenario.back().pitchDegrees != 0.0f
        || HeadBodyScenario.back().rollDegrees != 0.0f
        || HeadBodyScenario.back().positionX != 0.0f
        || HeadBodyScenario.back().positionY != 0.0f
        || HeadBodyScenario.back().positionZ != 0.0f)
    {
        std::cerr << "head/body assist scenario must end at its neutral baseline\n";
        return EXIT_FAILURE;
    }

    bool sawControllerTranslation = false;
    bool sawControllerAim = false;
    for (const PoseStep& step : HeadBodyScenario)
    {
        if (!step.expectsControllerResponse)
            continue;
        if (step.yawDegrees != 0.0f || step.pitchDegrees != 0.0f
            || step.rollDegrees != 0.0f || step.positionX != 0.0f
            || step.positionY != 0.0f || step.positionZ != 0.0f)
        {
            std::cerr << "tracked-prop controller probe must leave the synthetic head neutral\n";
            return EXIT_FAILURE;
        }
        sawControllerTranslation = sawControllerTranslation
            || step.controllerPositionX >= ExpectedControllerTranslationMeters;
        sawControllerAim = sawControllerAim
            || step.controllerYawDegrees >= ExpectedControllerYawDegrees;
    }
    if (!sawControllerTranslation || !sawControllerAim)
    {
        std::cerr << "tracked-prop scenario must include right controller position and aim changes\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
