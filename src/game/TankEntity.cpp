#include "game/TankEntity.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace game
{
namespace
{
float wrapAnglePi(float angle)
{
    while (angle > glm::pi<float>())
        angle -= glm::two_pi<float>();
    while (angle < -glm::pi<float>())
        angle += glm::two_pi<float>();
    return angle;
}

float lerpAngle(float a, float b, float t)
{
    return a + wrapAnglePi(b - a) * t;
}

OBB makePartOBB(const glm::mat4& partWorld, const glm::vec3& localOffset, const glm::vec3& halfExtents)
{
    OBB obb{};
    obb.center = glm::vec3(partWorld * glm::vec4(localOffset, 1.0f));
    obb.axes = glm::mat3(partWorld);
    obb.halfExtents = halfExtents;
    return obb;
}

glm::mat4 makePartDrawMatrix(const glm::mat4& partWorld, const glm::vec3& localOffset, const glm::vec3& halfExtents)
{
    const glm::mat4 offset = glm::translate(glm::mat4{1.0f}, localOffset);
    const glm::mat4 scale = glm::scale(glm::mat4{1.0f}, halfExtents * 2.0f);
    return partWorld * offset * scale;
}
} // namespace

TankSpec makeSandTankSpec()
{
    // sand-tank.glb: Y-up, -Z forward. Bounds include fixed tracks; pivots are node-local.
    TankSpec spec{};
    spec.bodyHalfExtents = {1.13f, 0.435f, 1.51f};
    spec.bodyLocalOffset = {0.0f, 0.435f, -0.03f};
    spec.turretMount = {0.0f, 0.79f, 0.29f};
    spec.turretHalfExtents = {0.70f, 0.291f, 0.8265f};
    spec.turretLocalOffset = {0.0f, 0.256f, -0.0515f};
    spec.barrelHinge = {0.0f, 0.32f, -0.86f};
    spec.barrelHalfExtents = {0.115f, 0.115f, 1.026f};
    spec.barrelLocalOffset = {0.0f, 0.0f, -0.974f};
    spec.muzzleOffset = {0.0f, 0.0f, -2.01f};
    return spec;
}

void TankEntity::reset(EntityId id, const glm::vec3& groundPos, float bodyYawRad, const TankSpec& spec)
{
    id_ = id;
    spec_ = spec;
    curr_ = ControlState{};
    curr_.bodyPos = groundPos;
    curr_.bodyYaw = wrapAnglePi(bodyYawRad);
    curr_.bodyRot = glm::angleAxis(curr_.bodyYaw, glm::vec3{0.0f, 1.0f, 0.0f});
    curr_.turretYaw = 0.0f;
    curr_.barrelPitch = 0.0f;
    prev_ = curr_;
    hp_ = spec_.maxHp;
    cooldownRemaining_ = 0.0f;
}

void TankEntity::beginTick(float dt)
{
    prev_ = curr_;
    cooldownRemaining_ = std::max(0.0f, cooldownRemaining_ - dt);
}

void TankEntity::applyDrive(float moveInput, float turnInput, float dt, float arenaHalfExtent)
{
    if (!alive())
        return;

    const glm::vec3 previousPos = curr_.bodyPos;
    const float previousYaw = curr_.bodyYaw;
    curr_.bodyYaw = wrapAnglePi(curr_.bodyYaw + turnInput * spec_.turnSpeedRad * dt);
    curr_.bodyRot = glm::normalize(glm::angleAxis(curr_.bodyYaw, glm::vec3{0.0f, 1.0f, 0.0f}));

    const glm::vec3 forward = curr_.bodyRot * glm::vec3{0.0f, 0.0f, -1.0f};
    curr_.bodyPos += forward * (moveInput * spec_.moveSpeed * dt);
    curr_.bodyPos.x = std::clamp(curr_.bodyPos.x, -arenaHalfExtent, arenaHalfExtent);
    curr_.bodyPos.z = std::clamp(curr_.bodyPos.z, -arenaHalfExtent, arenaHalfExtent);
    curr_.bodyPos.y = 0.0f;

    const float travel = glm::dot(curr_.bodyPos - previousPos, forward);
    const float turnTravel = wrapAnglePi(curr_.bodyYaw - previousYaw) * spec_.trackHalfSpacing;
    if (spec_.roadWheelRadius > 0.0f)
        curr_.wheelSpinRad -= glm::vec2{travel - turnTravel, travel + turnTravel} / spec_.roadWheelRadius;
}

void TankEntity::adjustManualAim(float turretTurnInput, float barrelPitchInput, float dt)
{
    if (!alive())
        return;

    curr_.turretYaw = wrapAnglePi(curr_.turretYaw + turretTurnInput * spec_.manualAimSpeedRad * dt);
    curr_.barrelPitch = std::clamp(
        curr_.barrelPitch + barrelPitchInput * spec_.manualAimSpeedRad * dt,
        spec_.minPitchRad,
        spec_.maxPitchRad);
}

void TankEntity::aimAt(const glm::vec3& worldTarget)
{
    if (!alive())
        return;

    const glm::mat4 bodyWorld =
        glm::translate(glm::mat4{1.0f}, curr_.bodyPos) * glm::mat4_cast(curr_.bodyRot);
    const glm::vec3 localAtMount =
        glm::vec3(glm::inverse(bodyWorld) * glm::vec4(worldTarget, 1.0f)) - spec_.turretMount;

    const float horizSq = localAtMount.x * localAtMount.x + localAtMount.z * localAtMount.z;
    if (horizSq > 1e-6f)
        curr_.turretYaw = std::atan2(-localAtMount.x, -localAtMount.z);

    const glm::mat4 turretWorld =
        bodyWorld *
        glm::translate(glm::mat4{1.0f}, spec_.turretMount) *
        glm::rotate(glm::mat4{1.0f}, curr_.turretYaw, glm::vec3{0.0f, 1.0f, 0.0f});

    const glm::vec3 localAtHinge =
        glm::vec3(glm::inverse(turretWorld) * glm::vec4(worldTarget, 1.0f)) - spec_.barrelHinge;
    const float hingeHoriz =
        std::sqrt(localAtHinge.x * localAtHinge.x + localAtHinge.z * localAtHinge.z);

    if (hingeHoriz > 1e-5f || std::abs(localAtHinge.y) > 1e-5f)
    {
        curr_.barrelPitch = std::clamp(
            std::atan2(localAtHinge.y, std::max(hingeHoriz, 1e-4f)),
            spec_.minPitchRad,
            spec_.maxPitchRad);
    }
}

TankPose TankEntity::evaluatePose() const
{
    return evaluateFromState(curr_);
}

TankPose TankEntity::evaluatePose(float alpha) const
{
    const float t = std::clamp(alpha, 0.0f, 1.0f);
    ControlState blended{};
    blended.bodyPos = glm::mix(prev_.bodyPos, curr_.bodyPos, t);
    blended.bodyRot = glm::normalize(glm::slerp(prev_.bodyRot, curr_.bodyRot, t));
    blended.bodyYaw = lerpAngle(prev_.bodyYaw, curr_.bodyYaw, t);
    blended.turretYaw = lerpAngle(prev_.turretYaw, curr_.turretYaw, t);
    blended.barrelPitch = glm::mix(prev_.barrelPitch, curr_.barrelPitch, t);
    blended.wheelSpinRad = glm::mix(prev_.wheelSpinRad, curr_.wheelSpinRad, t);
    return evaluateFromState(blended);
}

TankPose TankEntity::evaluateFromState(const ControlState& state) const
{
    TankPose pose{};
    pose.wheelSpinRad = state.wheelSpinRad;
    pose.bodyWorld =
        glm::translate(glm::mat4{1.0f}, state.bodyPos) * glm::mat4_cast(state.bodyRot);

    pose.turretWorld =
        pose.bodyWorld *
        glm::translate(glm::mat4{1.0f}, spec_.turretMount) *
        glm::rotate(glm::mat4{1.0f}, state.turretYaw, glm::vec3{0.0f, 1.0f, 0.0f});

    pose.barrelHingeWorld =
        glm::vec3(pose.turretWorld * glm::vec4(spec_.barrelHinge, 1.0f));

    pose.barrelWorld =
        pose.turretWorld *
        glm::translate(glm::mat4{1.0f}, spec_.barrelHinge) *
        glm::rotate(glm::mat4{1.0f}, state.barrelPitch, glm::vec3{1.0f, 0.0f, 0.0f});

    const glm::mat4 muzzleWorld =
        pose.barrelWorld * glm::translate(glm::mat4{1.0f}, spec_.muzzleOffset);

    pose.muzzleOrigin = glm::vec3(muzzleWorld[3]);
    pose.muzzleAxes = glm::mat3(muzzleWorld);
    pose.muzzleForward = glm::normalize(-pose.muzzleAxes[2]);

    pose.bodyOBB = makePartOBB(pose.bodyWorld, spec_.bodyLocalOffset, spec_.bodyHalfExtents);
    pose.turretOBB = makePartOBB(pose.turretWorld, spec_.turretLocalOffset, spec_.turretHalfExtents);
    pose.barrelOBB = makePartOBB(pose.barrelWorld, spec_.barrelLocalOffset, spec_.barrelHalfExtents);

    pose.bodyDraw = makePartDrawMatrix(pose.bodyWorld, spec_.bodyLocalOffset, spec_.bodyHalfExtents);
    pose.turretDraw = makePartDrawMatrix(pose.turretWorld, spec_.turretLocalOffset, spec_.turretHalfExtents);
    pose.barrelDraw = makePartDrawMatrix(pose.barrelWorld, spec_.barrelLocalOffset, spec_.barrelHalfExtents);

    return pose;
}

bool TankEntity::tryFire(Projectile& outProjectile)
{
    if (!alive() || cooldownRemaining_ > 0.0f)
        return false;

    const TankPose pose = evaluatePose();
    outProjectile = Projectile{};
    outProjectile.ownerId = id_;
    outProjectile.prevPos = pose.muzzleOrigin;
    outProjectile.pos = pose.muzzleOrigin;
    outProjectile.velocity = glm::normalize(pose.muzzleForward) * spec_.projectileSpeed;
    outProjectile.remainingLife = spec_.projectileLifetimeSec;
    outProjectile.damage = spec_.projectileDamage;
    outProjectile.pendingRemoval = false;

    cooldownRemaining_ = spec_.fireCooldownSec;
    return true;
}

void TankEntity::applyDamage(int amount)
{
    if (amount <= 0)
        return;
    hp_ = std::max(0, hp_ - amount);
}
} // namespace game
