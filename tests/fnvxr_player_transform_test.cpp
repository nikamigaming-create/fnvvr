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

    float openMwYawFromRetailPlayerMatrix(const float matrix[9])
    {
        return -std::atan2(matrix[3], matrix[0]);
    }

    void openMwPublishedYawMatrix(float openMwYaw, float out[9])
    {
        const float c = std::cos(openMwYaw);
        const float s = std::sin(openMwYaw);
        out[0] = c;
        out[1] = s;
        out[2] = 0.f;
        out[3] = -s;
        out[4] = c;
        out[5] = 0.f;
        out[6] = 0.f;
        out[7] = 0.f;
        out[8] = 1.f;
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
}

int main()
{
    const float yaws[] = { -Pi * 0.75f, -Pi * 0.25f, 0.f, Pi * 0.25f, Pi * 0.75f };
    for (float retailYaw : yaws)
    {
        float retailMatrix[9] {};
        retailYawMatrix(retailYaw, retailMatrix);

        const float openMwYaw = openMwYawFromRetailPlayerMatrix(retailMatrix);
        if (!nearlyEqual(openMwYaw, -retailYaw))
            return fail("OpenMW yaw conversion must negate retail matrix yaw");

        float openMwPublishedMatrix[9] {};
        openMwPublishedYawMatrix(openMwYaw, openMwPublishedMatrix);
        if (!matrixNearlyEqual(retailMatrix, openMwPublishedMatrix))
            return fail("OpenMW published yaw matrix must match the retail player matrix after sync");
    }

    std::cout << "player transform contract ok\n";
    return 0;
}
