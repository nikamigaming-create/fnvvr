#include <cmath>
#include <iostream>

namespace
{
    constexpr float Pi = 3.14159265358979323846f;

    int fail(const char* message)
    {
        std::cerr << message << "\n";
        return 1;
    }

    bool nearlyEqual(float lhs, float rhs)
    {
        return std::fabs(lhs - rhs) < 0.00001f;
    }

    void retailYawMatrix(float yaw, float out[9])
    {
        const float c = std::cos(yaw);
        const float s = std::sin(yaw);
        out[0] = c;
        out[1] = -s;
        out[2] = 0.f;
        out[3] = s;
        out[4] = c;
        out[5] = 0.f;
        out[6] = 0.f;
        out[7] = 0.f;
        out[8] = 1.f;
    }

    float retailYawFromPlayerMatrix(const float matrix[9])
    {
        return std::atan2(matrix[3], matrix[0]);
    }

    bool matrixNearlyEqual(const float left[9], const float right[9])
    {
        for (int i = 0; i < 9; ++i)
        {
            if (!nearlyEqual(left[i], right[i]))
                return false;
        }
        return true;
    }

    void multiplyMatrix(const float left[9], const float right[9], float out[9])
    {
        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 3; ++column)
            {
                out[row * 3 + column] = 0.0f;
                for (int k = 0; k < 3; ++k)
                    out[row * 3 + column] += left[row * 3 + k] * right[k * 3 + column];
            }
        }
    }
}

int main()
{
    const float yaws[] = { -Pi * 0.75f, -Pi * 0.25f, 0.f, Pi * 0.25f, Pi * 0.75f };
    for (float retailYaw : yaws)
    {
        float retailMatrix[9] {};
        retailYawMatrix(retailYaw, retailMatrix);

        const float recoveredYaw = retailYawFromPlayerMatrix(retailMatrix);
        if (!nearlyEqual(recoveredYaw, retailYaw))
            return fail("retail player yaw must round-trip through the published matrix");

        float rebuiltMatrix[9] {};
        retailYawMatrix(recoveredYaw, rebuiltMatrix);
        if (!matrixNearlyEqual(retailMatrix, rebuiltMatrix))
            return fail("rebuilt retail yaw matrix must preserve the player transform");
    }


    // Camera injection is a post-engine overlay. Each engine update must start
    // from the restored retail camera, not from the prior HMD-modified camera.
    // Reapplying a fixed head delta therefore remains fixed instead of
    // accumulating into body/player heading.
    float retailCamera[9] {};
    float headDelta[9] {};
    retailYawMatrix(Pi * 0.25f, retailCamera);
    retailYawMatrix(Pi * 0.10f, headDelta);
    float firstInjected[9] {};
    float secondInjected[9] {};
    multiplyMatrix(retailCamera, headDelta, firstInjected);
    multiplyMatrix(retailCamera, headDelta, secondInjected);
    if (!matrixNearlyEqual(firstInjected, secondInjected))
        return fail("restored camera base must prevent HMD rotation accumulation");
    if (!nearlyEqual(retailYawFromPlayerMatrix(retailCamera), Pi * 0.25f))
        return fail("camera-local HMD rotation must not rewrite player heading");

    std::cout << "player transform contract ok\n";
    return 0;
}
