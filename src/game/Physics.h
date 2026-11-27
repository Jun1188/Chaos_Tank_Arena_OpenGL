#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <span>

namespace game
{
using EntityId = std::uint32_t;
inline constexpr EntityId kInvalidEntityId = 0;

enum class PartKind : std::uint8_t
{
    None = 0,
    Terrain,
    Body,
    Turret,
    Barrel,
};

struct OBB
{
    glm::vec3 center{0.0f};
    glm::mat3 axes{1.0f};
    glm::vec3 halfExtents{0.5f};
};

struct TraceCollider
{
    EntityId ownerId{kInvalidEntityId};
    PartKind part{PartKind::None};
    OBB box{};
};

struct SegmentHit
{
    bool hit{false};
    float fraction{1.0f};
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    EntityId entityId{kInvalidEntityId};
    PartKind part{PartKind::None};
};

namespace Physics
{
bool intersectSegmentOBB(
    const glm::vec3& start,
    const glm::vec3& end,
    const OBB& obb,
    float& outFraction,
    glm::vec3& outNormal);

SegmentHit traceSegment(
    const glm::vec3& start,
    const glm::vec3& end,
    std::span<const TraceCollider> colliders,
    EntityId ignoreOwnerId = kInvalidEntityId);
} // namespace Physics
} // namespace game
