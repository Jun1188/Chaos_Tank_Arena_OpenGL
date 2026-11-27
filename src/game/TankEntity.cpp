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

void resolveObstaclePenetrationXZ(float& x, float& z, float bodyRadius, std::span<const OBB> obstacles)
{
    for (const OBB& box : obstacles)
    {
        const float dx = x - box.center.x;
        const float dz = z - box.center.z;
        const float penX = (box.halfExtents.x + bodyRadius) - std::abs(dx);
        const float penZ = (box.halfExtents.z + bodyRadius) - std::abs(dz);
        if (penX > 0.0f && penZ > 0.0f)
        {
            if (penX < penZ)
                x += (dx >= 0.0f ? penX : -penX);
            else
                z += (dz >= 0.0f ? penZ : -penZ);
        }
    }
}
} // namespace

float RampSurface::heightAt(float x, float z) const
{
    if (!containsXZ(x, z))
        return 0.0f;

    const float span = std::max(startZ - endZ, 1e-4f);
    const float t = std::clamp((startZ - z) / span, 0.0f, 1.0f);
    return t * topHeight;
}

float RampSurface::slopePitchRad() const
{
    return std::atan2(topHeight, std::max(startZ - endZ, 1e-4f));
}

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

glm::quat TankEntity::buildBodyRotation(float yaw, float pitch) const
{
    const glm::quat qYaw = glm::angleAxis(yaw, glm::vec3{0.0f, 1.0f, 0.0f});
    const glm::quat qPitch = glm::angleAxis(pitch, glm::vec3{1.0f, 0.0f, 0.0f});
    return glm::normalize(qYaw * qPitch);
}

void TankEntity::reset(EntityId id, const glm::vec3& groundPos, float bodyYawRad, const TankSpec& spec)
{
    id_ = id;
    spec_ = spec;
    curr_ = ControlState{};
    curr_.bodyPos = groundPos;
    curr_.bodyYaw = wrapAnglePi(bodyYawRad);
    curr_.bodyPitch = 0.0f;
    curr_.bodyRot = buildBodyRotation(curr_.bodyYaw, curr_.bodyPitch);
    curr_.velocity = glm::vec3{0.0f};
    curr_.angularVel = glm::vec3{0.0f};
    curr_.turretYaw = 0.0f;
    curr_.barrelPitch = 0.0f;
    curr_.airborne = false;
    curr_.onRamp = false;
    prev_ = curr_;
    hp_ = spec_.maxHp;
    cooldownRemaining_ = 0.0f;
    tookOffThisTick_ = false;
}

void TankEntity::beginTick(float dt)
{
    prev_ = curr_;
    cooldownRemaining_ = std::max(0.0f, cooldownRemaining_ - dt);
    tookOffThisTick_ = false;
}

float TankEntity::heightAboveGround(const RampSurface* ramp) const
{
    const float surfaceY = ramp ? ramp->heightAt(curr_.bodyPos.x, curr_.bodyPos.z) : 0.0f;
    return std::max(0.0f, curr_.bodyPos.y - surfaceY);
}

glm::vec3 TankEntity::interpolatedPosition(float alpha) const
{
    return glm::mix(prev_.bodyPos, curr_.bodyPos, std::clamp(alpha, 0.0f, 1.0f));
}

glm::vec3 TankEntity::interpolatedForwardXZ(float alpha) const
{
    const float yaw = lerpAngle(prev_.bodyYaw, curr_.bodyYaw, std::clamp(alpha, 0.0f, 1.0f));
    return glm::vec3{-std::sin(yaw), 0.0f, -std::cos(yaw)};
}

void TankEntity::triggerExplosionJump(const glm::vec3& horizontalDirXZ)
{
    if (!alive())
        return;

    glm::vec3 baseDir{horizontalDirXZ.x, 0.0f, horizontalDirXZ.z};
    const float len = glm::length(baseDir);
    if (len > 1e-4f)
        baseDir /= len;
    else
        baseDir = glm::vec3{-std::sin(curr_.bodyYaw), 0.0f, -std::cos(curr_.bodyYaw)};

    std::uniform_real_distribution<float> angleJitterDist(-0.95f, 0.95f);
    std::uniform_real_distribution<float> horizSpeedDist(4.5f, 10.5f);
    std::uniform_real_distribution<float> upSpeedDist(13.5f, 18.5f);
    std::uniform_real_distribution<float> pitchSpinDist(5.5f, 11.5f);
    std::uniform_real_distribution<float> yawSpinDist(-7.5f, 7.5f);
    std::uniform_real_distribution<float> rollSpinDist(5.0f, 10.5f);
    std::uniform_int_distribution<int> signDist(0, 1);

    const float jitter = angleJitterDist(rng_);
    const float c = std::cos(jitter);
    const float s = std::sin(jitter);
    const glm::vec3 launchDir{
        baseDir.x * c - baseDir.z * s,
        0.0f,
        baseDir.x * s + baseDir.z * c};

    const float pitchSign = signDist(rng_) == 0 ? 1.0f : -1.0f;
    const float rollSign = signDist(rng_) == 0 ? 1.0f : -1.0f;

    // TABS-style chaotic, exaggerated explosion launch and multi-axis tumble.
    curr_.velocity = launchDir * horizSpeedDist(rng_) + glm::vec3{0.0f, upSpeedDist(rng_), 0.0f};
    curr_.angularVel = glm::vec3{
        pitchSign * pitchSpinDist(rng_),
        yawSpinDist(rng_),
        rollSign * rollSpinDist(rng_)};
    curr_.airborne = true;
    curr_.onRamp = false;
    tookOffThisTick_ = true;
}

void TankEntity::applyFireRecoil(const glm::vec3& fireForward)
{
    if (!alive())
        return;

    glm::vec3 recoilDir = -fireForward;
    const float len = glm::length(recoilDir);
    recoilDir = (len > 1e-4f) ? (recoilDir / len) : glm::vec3{0.0f, 0.0f, 1.0f};

    std::uniform_real_distribution<float> kickJitterDist(0.85f, 1.25f);
    std::uniform_real_distribution<float> wobbleDist(-2.4f, 2.4f);
    const float mult = kickJitterDist(rng_);

    const glm::vec3 recoilHoriz{recoilDir.x, 0.0f, recoilDir.z};
    const float horizLen = glm::length(recoilHoriz);
    const glm::vec3 recoilXZ = (horizLen > 1e-4f) ? (recoilHoriz / horizLen) : glm::vec3{0.0f, 0.0f, 1.0f};

    // World torque axis that pitches the nose upward / rocks the hull opposite to fire direction.
    const glm::vec3 worldTorqueAxis = glm::cross(glm::vec3{0.0f, 1.0f, 0.0f}, recoilXZ);
    const glm::vec3 localTorqueAxis = glm::inverse(curr_.bodyRot) * worldTorqueAxis;

    if (curr_.airborne)
    {
        // Mid-air TABS trickshot cannon recoil: strong backward blast + comedic tumble spin.
        curr_.velocity += recoilDir * (9.5f * mult) + glm::vec3{0.0f, 3.2f * mult, 0.0f};
        curr_.angularVel += localTorqueAxis * (7.5f * mult) +
                            glm::vec3{wobbleDist(rng_), wobbleDist(rng_) * 1.4f, wobbleDist(rng_)};
    }
    else
    {
        // Grounded TABS comic recoil: exaggerated backward kick + hop + nose-up rock.
        curr_.velocity = recoilXZ * (7.2f * mult) + glm::vec3{0.0f, 3.8f * mult, 0.0f};
        curr_.angularVel = localTorqueAxis * (5.2f * mult) +
                           glm::vec3{wobbleDist(rng_) * 0.45f, wobbleDist(rng_) * 0.6f, wobbleDist(rng_) * 0.7f};
        curr_.airborne = true;
        curr_.onRamp = false;
    }
}

void TankEntity::applyDrive(
    float moveForwardInput,
    float moveRightInput,
    float dt,
    const RampSurface* ramp,
    std::span<const OBB> solidObstacles,
    float arenaHalfExtent)
{
    if (!alive())
        return;

    const glm::vec3 previousPos = curr_.bodyPos;
    const float previousYaw = curr_.bodyYaw;

    // WASD 4-directional movement in world XZ (W: -Z, S: +Z, A: -X, D: +X).
    glm::vec3 moveDirXZ{moveRightInput, 0.0f, -moveForwardInput};
    const float moveInputLen = glm::length(moveDirXZ);
    if (moveInputLen > 1e-4f)
        moveDirXZ /= moveInputLen;
    else
        moveDirXZ = glm::vec3{0.0f};

    if (!curr_.airborne)
    {
        if (moveInputLen > 1e-4f)
        {
            const float desiredYaw = std::atan2(-moveDirXZ.x, -moveDirXZ.z);
            const float yawDelta = wrapAnglePi(desiredYaw - curr_.bodyYaw);
            const float maxStep = spec_.turnSpeedRad * dt;
            curr_.bodyYaw = wrapAnglePi(curr_.bodyYaw + std::clamp(yawDelta, -maxStep, maxStep));
        }

        const glm::vec3 forwardXZ{-std::sin(curr_.bodyYaw), 0.0f, -std::cos(curr_.bodyYaw)};
        float nextX = std::clamp(curr_.bodyPos.x + moveDirXZ.x * (spec_.moveSpeed * dt), -arenaHalfExtent, arenaHalfExtent);
        float nextZ = std::clamp(curr_.bodyPos.z + moveDirXZ.z * (spec_.moveSpeed * dt), -arenaHalfExtent, arenaHalfExtent);

        const float bodyRadius = std::min(spec_.bodyHalfExtents.x, spec_.bodyHalfExtents.z);
        resolveObstaclePenetrationXZ(nextX, nextZ, bodyRadius, solidObstacles);
        nextX = std::clamp(nextX, -arenaHalfExtent, arenaHalfExtent);
        nextZ = std::clamp(nextZ, -arenaHalfExtent, arenaHalfExtent);

        const bool wasOnRamp = curr_.onRamp;
        const float prevSurfaceY = curr_.bodyPos.y;
        if (!wasOnRamp && ramp)
        {
            const float tryXSurface = ramp->heightAt(nextX, curr_.bodyPos.z);
            if (ramp->containsXZ(nextX, curr_.bodyPos.z) && (tryXSurface - prevSurfaceY) > 0.65f)
                nextX = curr_.bodyPos.x;

            const float tryZSurface = ramp->heightAt(nextX, nextZ);
            if (ramp->containsXZ(nextX, nextZ) && (tryZSurface - prevSurfaceY) > 0.65f)
                nextZ = curr_.bodyPos.z;
        }

        const bool nextOnRamp = ramp && ramp->containsXZ(nextX, nextZ);
        const bool crossedLaunchLip =
            ramp && wasOnRamp &&
            ((nextZ < ramp->endZ && prevSurfaceY >= ramp->topHeight * 0.72f) ||
             (!nextOnRamp && prevSurfaceY > 0.85f));

        if (crossedLaunchLip)
        {
            const glm::vec3 horizVel = (dt > 1e-6f)
                ? glm::vec3{(nextX - previousPos.x) / dt, 0.0f, (nextZ - previousPos.z) / dt}
                : moveDirXZ * spec_.moveSpeed;
            const float horizSpeed = glm::length(horizVel);
            const float launchUp = std::max(horizSpeed * std::tan(ramp->slopePitchRad()) * 1.65f, 6.4f);

            curr_.bodyPos = glm::vec3{nextX, prevSurfaceY + 0.02f, nextZ};
            curr_.velocity = glm::vec3{horizVel.x, launchUp, horizVel.z};
            curr_.angularVel = glm::vec3{0.8f, 0.0f, 0.0f};
            curr_.airborne = true;
            curr_.onRamp = false;
            tookOffThisTick_ = true;
        }
        else
        {
            const bool finalOnRamp = ramp && ramp->containsXZ(nextX, nextZ);
            const float finalY = ramp ? ramp->heightAt(nextX, nextZ) : 0.0f;
            curr_.bodyPos = glm::vec3{nextX, finalY, nextZ};
            curr_.onRamp = finalOnRamp;
            curr_.velocity = (dt > 1e-6f) ? (curr_.bodyPos - previousPos) / dt : glm::vec3{0.0f};
            curr_.angularVel = glm::vec3{0.0f};

            const float targetPitch = finalOnRamp
                ? (ramp->slopePitchRad() * glm::dot(forwardXZ, glm::vec3{0.0f, 0.0f, -1.0f}))
                : 0.0f;
            curr_.bodyPitch += (targetPitch - curr_.bodyPitch) * std::clamp(dt * 14.0f, 0.0f, 1.0f);

            const glm::quat targetRot = buildBodyRotation(curr_.bodyYaw, curr_.bodyPitch);
            curr_.bodyRot = glm::normalize(glm::slerp(curr_.bodyRot, targetRot, std::clamp(dt * 14.0f, 0.0f, 1.0f)));
        }

        const float travel = glm::dot(curr_.bodyPos - previousPos, forwardXZ);
        const float turnTravel = wrapAnglePi(curr_.bodyYaw - previousYaw) * spec_.trackHalfSpacing;
        if (spec_.roadWheelRadius > 0.0f)
            curr_.wheelSpinRad -= glm::vec2{travel - turnTravel, travel + turnTravel} / spec_.roadWheelRadius;
        return;
    }

    // Airborne: smooth physics integration with chaotic comedic tumble + weak 4-way air drift.
    curr_.velocity += moveDirXZ * (spec_.airControlAccel * dt);
    curr_.velocity.y -= spec_.gravity * dt;

    curr_.bodyPos += curr_.velocity * dt;
    curr_.bodyPos.x = std::clamp(curr_.bodyPos.x, -arenaHalfExtent, arenaHalfExtent);
    curr_.bodyPos.z = std::clamp(curr_.bodyPos.z, -arenaHalfExtent, arenaHalfExtent);

    const float angSpeed = glm::length(curr_.angularVel);
    if (angSpeed > 1e-4f)
    {
        const glm::quat stepRot = glm::angleAxis(angSpeed * dt, curr_.angularVel / angSpeed);
        curr_.bodyRot = glm::normalize(curr_.bodyRot * stepRot);
        curr_.angularVel *= std::exp(-0.55f * dt);
    }

    const float surfaceBelow = ramp ? ramp->heightAt(curr_.bodyPos.x, curr_.bodyPos.z) : 0.0f;
    const float heightNow = std::max(0.0f, curr_.bodyPos.y - surfaceBelow);

    // Smoothly align tank bottom face downward during descent so tracks land flat on the floor.
    if (curr_.velocity.y < 0.0f && heightNow < 5.5f)
    {
        const glm::vec3 currFwd = curr_.bodyRot * glm::vec3{0.0f, 0.0f, -1.0f};
        if (currFwd.x * currFwd.x + currFwd.z * currFwd.z > 0.04f)
            curr_.bodyYaw = std::atan2(-currFwd.x, -currFwd.z);

        const float proximity = 1.0f - std::clamp(heightNow / 5.5f, 0.0f, 1.0f);
        curr_.angularVel *= std::exp(-(4.5f + proximity * 8.0f) * dt);

        const glm::quat uprightRot = buildBodyRotation(curr_.bodyYaw, 0.0f);
        const float alignRate = 4.0f + proximity * 14.0f;
        curr_.bodyRot = glm::normalize(glm::slerp(curr_.bodyRot, uprightRot, std::clamp(dt * alignRate, 0.0f, 1.0f)));
    }

    if (curr_.bodyPos.y <= surfaceBelow && curr_.velocity.y <= 0.0f)
    {
        const float bodyRadius = std::min(spec_.bodyHalfExtents.x, spec_.bodyHalfExtents.z);
        resolveObstaclePenetrationXZ(curr_.bodyPos.x, curr_.bodyPos.z, bodyRadius, solidObstacles);
        curr_.bodyPos.x = std::clamp(curr_.bodyPos.x, -arenaHalfExtent, arenaHalfExtent);
        curr_.bodyPos.z = std::clamp(curr_.bodyPos.z, -arenaHalfExtent, arenaHalfExtent);

        const float resolvedSurface = ramp ? ramp->heightAt(curr_.bodyPos.x, curr_.bodyPos.z) : 0.0f;
        curr_.bodyPos.y = resolvedSurface;
        curr_.velocity = glm::vec3{0.0f};
        curr_.angularVel = glm::vec3{0.0f};
        curr_.airborne = false;
        curr_.onRamp = ramp && ramp->containsXZ(curr_.bodyPos.x, curr_.bodyPos.z) && resolvedSurface > 0.01f;
        curr_.bodyPitch = 0.0f;
        const glm::quat uprightRot = buildBodyRotation(curr_.bodyYaw, 0.0f);
        curr_.bodyRot = glm::normalize(glm::slerp(curr_.bodyRot, uprightRot, 0.45f));
    }
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

void TankEntity::aimAt(const glm::vec3& worldTarget, bool syncPrev)
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

    if (syncPrev)
    {
        prev_.turretYaw = curr_.turretYaw;
        prev_.barrelPitch = curr_.barrelPitch;
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
    blended.bodyPitch = glm::mix(prev_.bodyPitch, curr_.bodyPitch, t);
    blended.turretYaw = lerpAngle(prev_.turretYaw, curr_.turretYaw, t);
    blended.barrelPitch = glm::mix(prev_.barrelPitch, curr_.barrelPitch, t);
    blended.wheelSpinRad = glm::mix(prev_.wheelSpinRad, curr_.wheelSpinRad, t);
    blended.airborne = curr_.airborne;
    blended.onRamp = curr_.onRamp;
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

bool TankEntity::tryFire(Projectile& outProjectile, bool ignoreCooldown)
{
    if (!alive() || (!ignoreCooldown && cooldownRemaining_ > 0.0f))
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
