#pragma once
#include "Vectors/Vector3.hpp"
#include "Quaternion/Quaternion.hpp"

class AimBot {
public:
    static Quaternion GetRotationToLocation(const Vector3& targetLocation, float y_bias, const Vector3& myLoc) {
        Vector3 targget = targetLocation + Vector3(0, y_bias, 0);
        Vector3 forwards = targget - myLoc;
        Vector3 upwards(0, 1, 0);

        return Quaternion::LookRotation(forwards, upwards);
    }
};