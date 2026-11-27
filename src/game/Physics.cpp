#include "game/Physics.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace game::Physics
{
bool intersectSegmentOBB(
    const glm::vec3& start,
    const glm::vec3& end,
    const OBB& obb,
    float& outFraction,
    glm::vec3& outNormal)
{
    constexpr float kParallelEps = 1e-8f;

    const glm::vec3 delta = end - start;
    const glm::vec3 relStart = start - obb.center;

    float tMin = -std::numeric_limits<float>::infinity();
    float tMax = std::numeric_limits<float>::infinity();
    glm::vec3 enterNormal{0.0f, 1.0f, 0.0f};
    bool hasEnterAxis = false;

    for (int axisIdx = 0; axisIdx < 3; ++axisIdx)
    {
        const glm::vec3 axis = obb.axes[axisIdx];
        const float s0 = glm::dot(relStart, axis);
        const float sd = glm::dot(delta, axis);
        const float extent = obb.halfExtents[axisIdx];

        if (std::abs(sd) < kParallelEps)
        {
            if (std::abs(s0) > extent)
                return false;
            continue;
        }

        float t1 = (-extent - s0) / sd;
        float t2 = (extent - s0) / sd;
        glm::vec3 candidateNormal = (sd > 0.0f) ? -axis : axis;

        if (t1 > t2)
            std::swap(t1, t2);

        if (t1 > tMin)
        {
            tMin = t1;
            enterNormal = candidateNormal;
            hasEnterAxis = true;
        }
        tMax = std::min(tMax, t2);

        if (tMin > tMax)
            return false;
    }

    if (tMax < 0.0f || tMin > 1.0f)
        return false;

    if (tMin >= 0.0f)
    {
        outFraction = tMin;
        outNormal = hasEnterAxis ? enterNormal : glm::vec3{0.0f, 1.0f, 0.0f};
    }
    else
    {
        outFraction = 0.0f;
        const float lenSq = glm::dot(delta, delta);
        outNormal = (lenSq > kParallelEps) ? -glm::normalize(delta) : glm::vec3{0.0f, 1.0f, 0.0f};
    }
    return true;
}

SegmentHit traceSegment(
    const glm::vec3& start,
    const glm::vec3& end,
    std::span<const TraceCollider> colliders,
    EntityId ignoreOwnerId)
{
    SegmentHit best{};
    best.fraction = std::numeric_limits<float>::infinity();

    for (const TraceCollider& collider : colliders)
    {
        if (ignoreOwnerId != kInvalidEntityId && collider.ownerId == ignoreOwnerId)
            continue;

        float fraction = 1.0f;
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        if (!intersectSegmentOBB(start, end, collider.box, fraction, normal))
            continue;

        if (fraction < best.fraction)
        {
            best.hit = true;
            best.fraction = fraction;
            best.point = glm::mix(start, end, fraction);
            best.normal = normal;
            best.entityId = collider.ownerId;
            best.part = collider.part;
        }
    }

    if (!best.hit)
        best.fraction = 1.0f;

    return best;
}
} // namespace game::Physics
