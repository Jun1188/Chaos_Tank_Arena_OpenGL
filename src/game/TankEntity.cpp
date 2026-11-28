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

void resolveRampSideAndBackWalls(
    glm::vec3& pos,
    glm::vec3& vel,
    const glm::vec3& prevPos,
    bool wasOnRamp,
    float radiusXZ,
    const RampSurface* ramp)
{
    if (ramp == nullptr || wasOnRamp)
        return;

    const float minX = ramp->minX - radiusXZ;
    const float maxX = ramp->maxX + radiusXZ;
    const float minZ = ramp->endZ - radiusXZ;
    const float maxZ = ramp->startZ;

    if (pos.x <= minX || pos.x >= maxX || pos.z <= minZ || pos.z >= maxZ)
        return;

    const float sampleX = std::clamp(pos.x, ramp->minX, ramp->maxX);
    const float sampleZ = std::clamp(pos.z, ramp->endZ, ramp->startZ);
    const float deckY = ramp->heightAt(sampleX, sampleZ);

    // Allow smooth entry from the low front lip or landing onto the deck from above.
    if (deckY <= 0.45f || prevPos.y + 0.35f >= deckY)
        return;

    const float penLeft = pos.x - minX;
    const float penRight = maxX - pos.x;
    const float penBack = pos.z - minZ;

    if (penLeft <= penRight && penLeft <= penBack)
    {
        pos.x = minX;
        vel.x = std::min(vel.x, 0.0f);
    }
    else if (penRight <= penLeft && penRight <= penBack)
    {
        pos.x = maxX;
        vel.x = std::max(vel.x, 0.0f);
    }
    else
    {
        pos.z = minZ;
        vel.z = std::min(vel.z, 0.0f);
    }
}

void resolveSolidObstacles(
    glm::vec3& bodyPos,
    glm::vec3& velocity,
    const glm::quat& bodyRot,
    std::span<const OBB> obstacles)
{
    constexpr glm::vec3 kHullSphereOffsets[2]{
        glm::vec3{0.0f, 0.52f, -0.52f},
        glm::vec3{0.0f, 0.52f, 0.52f}};
    constexpr float kSphereRadius = 0.98f;

    for (int iter = 0; iter < 2; ++iter)
    {
        for (const OBB& box : obstacles)
        {
            const glm::mat3 invAxes = glm::transpose(box.axes);
            for (const glm::vec3& localOff : kHullSphereOffsets)
            {
                const glm::vec3 sphereCenter = bodyPos + (bodyRot * localOff);
                const glm::vec3 localP = invAxes * (sphereCenter - box.center);
                const glm::vec3 localQ = glm::clamp(localP, -box.halfExtents, box.halfExtents);
                const glm::vec3 localDiff = localP - localQ;
                const float distSq = glm::dot(localDiff, localDiff);
                if (distSq >= kSphereRadius * kSphereRadius)
                    continue;

                if (distSq > 1e-6f)
                {
                    const float dist = std::sqrt(distSq);
                    const glm::vec3 worldNormal = (box.axes * localDiff) / dist;
                    const float penetration = kSphereRadius - dist;

                    if (worldNormal.y > 0.65f)
                    {
                        bodyPos.y += worldNormal.y * penetration;
                        glm::vec2 awayXZ{bodyPos.x - box.center.x, bodyPos.z - box.center.z};
                        const float awayLen = glm::length(awayXZ);
                        awayXZ = (awayLen > 1e-4f) ? (awayXZ / awayLen) : glm::vec2{1.0f, 0.0f};
                        bodyPos.x += awayXZ.x * (penetration * 0.85f + 0.03f);
                        bodyPos.z += awayXZ.y * (penetration * 0.85f + 0.03f);
                        velocity.x += awayXZ.x * 2.2f;
                        velocity.z += awayXZ.y * 2.2f;
                    }
                    else
                    {
                        glm::vec3 horizN{worldNormal.x, 0.0f, worldNormal.z};
                        const float horizLen = glm::length(horizN);
                        horizN = (horizLen > 1e-4f) ? (horizN / horizLen) : glm::vec3{0.0f, 0.0f, 1.0f};
                        bodyPos += horizN * penetration;
                        const float vn = glm::dot(velocity, horizN);
                        if (vn < 0.0f)
                            velocity -= horizN * vn;
                    }
                }
                else
                {
                    const float dx = box.halfExtents.x - std::abs(localP.x);
                    const float dz = box.halfExtents.z - std::abs(localP.z);
                    const glm::vec3 localAxis = (dx < dz)
                        ? glm::vec3{(localP.x >= 0.0f) ? 1.0f : -1.0f, 0.0f, 0.0f}
                        : glm::vec3{0.0f, 0.0f, (localP.z >= 0.0f) ? 1.0f : -1.0f};
                    glm::vec3 worldPush = box.axes * localAxis;
                    worldPush.y = 0.0f;
                    const float pushLen = glm::length(worldPush);
                    worldPush = (pushLen > 1e-4f) ? (worldPush / pushLen) : glm::vec3{0.0f, 0.0f, 1.0f};
                    bodyPos += worldPush * (std::min(dx, dz) + kSphereRadius);
                    const float vn = glm::dot(velocity, worldPush);
                    if (vn < 0.0f)
                        velocity -= worldPush * vn;
                }
            }
        }
    }
}

float minBodyYAboveSurface(const glm::quat& rot, const TankSpec& spec)
{
    const glm::mat3 axes = glm::mat3_cast(rot);
    const float centerOffsetY = (axes * spec.bodyLocalOffset).y;
    const float supportExtentY =
        std::abs(axes[0].y) * spec.bodyHalfExtents.x +
        std::abs(axes[1].y) * spec.bodyHalfExtents.y +
        std::abs(axes[2].y) * spec.bodyHalfExtents.z;
    return std::max(0.0f, supportExtentY - centerOffsetY);
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
    airRecoilCount_ = 0;
    recoilHopActive_ = false;
    groundRollActive_ = false;
    recoilSettleActive_ = false;
    recoilSettleElapsed_ = 0.0f;
    recoilStartRot_ = curr_.bodyRot;
    hullAlignActive_ = false;
    hullAlignElapsed_ = 0.0f;
    hullAlignStartYaw_ = curr_.bodyYaw;
    hullAlignTargetYaw_ = curr_.bodyYaw;
    hullAlignStartRot_ = curr_.bodyRot;
    driveBasisYaw_ = curr_.bodyYaw;
    recoilSlideVelXZ_ = glm::vec3{0.0f};
    boostBlend_ = 0.0f;
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

void TankEntity::resetHullToAim(const glm::vec3& aimForwardXZ)
{
    if (!alive())
        return;

    glm::vec3 flatAim{aimForwardXZ.x, 0.0f, aimForwardXZ.z};
    const float len = glm::length(flatAim);
    const float desiredYaw = (len > 1e-4f)
        ? std::atan2(-flatAim.x / len, -flatAim.z / len)
        : curr_.bodyYaw;

    curr_.angularVel = glm::vec3{0.0f};
    recoilSlideVelXZ_ = glm::vec3{0.0f};
    recoilHopActive_ = false;
    groundRollActive_ = false;
    recoilSettleActive_ = false;

    hullAlignActive_ = true;
    hullAlignElapsed_ = 0.0f;
    hullAlignStartYaw_ = curr_.bodyYaw;
    hullAlignTargetYaw_ = wrapAnglePi(desiredYaw);
    hullAlignStartRot_ = curr_.bodyRot;
    driveBasisYaw_ = hullAlignTargetYaw_;
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

    std::uniform_real_distribution<float> angleJitterDist(-0.90f, 0.90f);
    std::uniform_real_distribution<float> horizSpeedDist(4.5f, 9.5f);
    std::uniform_real_distribution<float> upSpeedDist(11.5f, 14.5f);
    std::uniform_real_distribution<float> pitchSpinDist(5.0f, 10.0f);
    std::uniform_real_distribution<float> yawSpinDist(-6.5f, 6.5f);
    std::uniform_real_distribution<float> rollSpinDist(4.5f, 9.5f);
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

    curr_.velocity = launchDir * horizSpeedDist(rng_) + glm::vec3{0.0f, upSpeedDist(rng_), 0.0f};
    curr_.angularVel = glm::vec3{
        pitchSign * pitchSpinDist(rng_),
        yawSpinDist(rng_),
        rollSign * rollSpinDist(rng_)};
    curr_.airborne = true;
    curr_.onRamp = false;
    recoilHopActive_ = false;
    groundRollActive_ = false;
    recoilSettleActive_ = false;
    hullAlignActive_ = false;
    airRecoilCount_ = 0;
    tookOffThisTick_ = true;
}

void TankEntity::applyFireRecoil(const glm::vec3& fireForward)
{
    if (!alive())
        return;

    glm::vec3 recoilDir = -fireForward;
    const float len = glm::length(recoilDir);
    recoilDir = (len > 1e-4f) ? (recoilDir / len) : glm::vec3{0.0f, 0.0f, 1.0f};

    std::uniform_real_distribution<float> kickJitterDist(0.90f, 1.18f);
    std::uniform_real_distribution<float> wobbleDist(-1.8f, 1.8f);
    const float mult = kickJitterDist(rng_);

    const glm::vec3 recoilHoriz{recoilDir.x, 0.0f, recoilDir.z};
    const float horizLen = glm::length(recoilHoriz);
    const glm::vec3 recoilXZ = (horizLen > 1e-4f) ? (recoilHoriz / horizLen) : glm::vec3{0.0f, 0.0f, 1.0f};

    const glm::vec3 worldTorqueAxis = glm::cross(glm::vec3{0.0f, 1.0f, 0.0f}, recoilXZ);
    const glm::vec3 localTorqueAxis = glm::inverse(curr_.bodyRot) * worldTorqueAxis;

    if (curr_.airborne)
    {
        // Airborne recoil diminishes exponentially on repeated mid-air shots so the tank cannot fly forever.
        const float decay = std::pow(0.55f, static_cast<float>(airRecoilCount_));
        ++airRecoilCount_;

        const float upAllowance = (curr_.bodyPos.y < 5.5f) ? (2.6f * decay * mult) : 0.0f;
        curr_.velocity += recoilXZ * (10.5f * decay * mult) + glm::vec3{0.0f, upAllowance, 0.0f};
        curr_.velocity.y = std::min(curr_.velocity.y, 12.0f);
        curr_.angularVel += localTorqueAxis * (7.5f * decay * mult) +
                            glm::vec3{wobbleDist(rng_), wobbleDist(rng_), wobbleDist(rng_)} * decay;
        recoilSettleActive_ = false;
        hullAlignActive_ = false;
    }
    else
    {
        // Grounded recoil: hop slightly into mid-air + backward with nose-up/roll kick, then smoothly land back down.
        curr_.velocity = glm::vec3{curr_.velocity.x * 0.35f, 0.0f, curr_.velocity.z * 0.35f} +
                         recoilXZ * (5.4f * mult) +
                         glm::vec3{0.0f, 4.1f * mult, 0.0f};
        curr_.angularVel = localTorqueAxis * (3.2f * mult) +
                           glm::vec3{wobbleDist(rng_) * 0.22f, wobbleDist(rng_) * 0.25f, wobbleDist(rng_) * 0.35f};
        curr_.bodyPos.y += 0.03f;
        curr_.airborne = true;
        curr_.onRamp = false;
        recoilHopActive_ = true;
        airRecoilCount_ = 1;
        groundRollActive_ = false;
        recoilSettleActive_ = false;
        hullAlignActive_ = false;
    }
}

void TankEntity::applyHitImpulse(const glm::vec3& hitPoint, const glm::vec3& impulseDir)
{
    glm::vec3 dir = impulseDir;
    const float len = glm::length(dir);
    dir = (len > 1e-4f) ? (dir / len) : glm::vec3{0.0f, 0.0f, -1.0f};

    std::uniform_real_distribution<float> jitterDist(0.85f, 1.25f);
    std::uniform_real_distribution<float> spinJitter(-2.2f, 2.2f);
    const float mult = jitterDist(rng_);

    const glm::vec3 lever = hitPoint - (curr_.bodyPos + glm::vec3{0.0f, spec_.bodyHalfExtents.y, 0.0f});
    const glm::vec3 worldTorque = glm::cross(lever, dir);
    const glm::vec3 localTorque = glm::inverse(curr_.bodyRot) * worldTorque;

    const glm::vec3 dirXZ{dir.x, 0.0f, dir.z};
    const float xzLen = glm::length(dirXZ);
    const glm::vec3 flatDir = (xzLen > 1e-4f) ? (dirXZ / xzLen) : glm::vec3{0.0f, 0.0f, -1.0f};

    curr_.velocity += flatDir * (9.5f * mult) + glm::vec3{0.0f, 6.8f * mult, 0.0f};
    curr_.velocity.y = std::min(curr_.velocity.y, 12.5f);
    curr_.angularVel += localTorque * (7.5f * mult) +
                        glm::vec3{spinJitter(rng_), spinJitter(rng_), spinJitter(rng_)};
    curr_.airborne = true;
    curr_.onRamp = false;
    recoilHopActive_ = false;
    groundRollActive_ = false;
    recoilSettleActive_ = false;
    hullAlignActive_ = false;
}

void TankEntity::applyDrive(
    float moveForwardInput,
    float moveRightInput,
    bool boostActive,
    float dt,
    const RampSurface* ramp,
    std::span<const OBB> solidObstacles,
    float arenaHalfExtent)
{
    if (!alive())
        return;

    const glm::vec3 previousPos = curr_.bodyPos;
    const float previousYaw = curr_.bodyYaw;

    const float boostTarget = boostActive ? 1.0f : 0.0f;
    boostBlend_ += (boostTarget - boostBlend_) * std::clamp(dt * (boostActive ? 10.0f : 7.0f), 0.0f, 1.0f);
    const float effectiveMoveSpeed = spec_.moveSpeed * glm::mix(1.0f, spec_.boostSpeedMultiplier, boostBlend_);

    // WASD uses the hull home forward basis set at spawn or when 'r' resets hull pose to aim point.
    // Pressing W itself does NOT chase the live mouse aim point unless 'r' was pressed.
    const glm::vec3 basisFwd{-std::sin(driveBasisYaw_), 0.0f, -std::cos(driveBasisYaw_)};
    const glm::vec3 basisRight = glm::normalize(glm::cross(basisFwd, glm::vec3{0.0f, 1.0f, 0.0f}));

    glm::vec3 moveDirXZ = basisFwd * moveForwardInput + basisRight * moveRightInput;
    float moveInputLen = glm::length(moveDirXZ);
    if (moveInputLen > 1e-4f)
    {
        moveDirXZ /= moveInputLen;
    }
    else if (boostActive)
    {
        moveDirXZ = glm::vec3{-std::sin(curr_.bodyYaw), 0.0f, -std::cos(curr_.bodyYaw)};
        moveInputLen = 1.0f;
    }
    else
    {
        moveDirXZ = glm::vec3{0.0f};
    }

    if (hullAlignActive_ && moveInputLen > 1e-4f &&
        (moveForwardInput <= 0.0f || std::abs(moveRightInput) > 1e-4f))
    {
        hullAlignActive_ = false;
    }

    if (!curr_.airborne)
    {
        if (hullAlignActive_)
        {
            hullAlignElapsed_ += dt;
            const float u = std::clamp(hullAlignElapsed_ / std::max(hullAlignDuration_, 1e-4f), 0.0f, 1.0f);
            const float inv = 1.0f - u;
            const float ease = 1.0f - inv * inv * inv;
            curr_.bodyYaw = lerpAngle(hullAlignStartYaw_, hullAlignTargetYaw_, ease);
        }
        else if (moveInputLen > 1e-4f)
        {
            const float desiredYaw = std::atan2(-moveDirXZ.x, -moveDirXZ.z);
            const float yawDelta = wrapAnglePi(desiredYaw - curr_.bodyYaw);
            const float maxStep = spec_.turnSpeedRad * dt;
            curr_.bodyYaw = wrapAnglePi(curr_.bodyYaw + std::clamp(yawDelta, -maxStep, maxStep));
        }

        const glm::vec3 forwardXZ{-std::sin(curr_.bodyYaw), 0.0f, -std::cos(curr_.bodyYaw)};
        recoilSlideVelXZ_ *= std::exp(-3.8f * dt);
        glm::vec3 groundVel = moveDirXZ * effectiveMoveSpeed + recoilSlideVelXZ_;
        glm::vec3 candidatePos{
            std::clamp(curr_.bodyPos.x + groundVel.x * dt, -arenaHalfExtent, arenaHalfExtent),
            curr_.bodyPos.y,
            std::clamp(curr_.bodyPos.z + groundVel.z * dt, -arenaHalfExtent, arenaHalfExtent)};

        const bool wasOnRamp = curr_.onRamp;
        const float bodyRadius = std::min(spec_.bodyHalfExtents.x, spec_.bodyHalfExtents.z);
        resolveRampSideAndBackWalls(candidatePos, groundVel, previousPos, wasOnRamp, bodyRadius, ramp);
        resolveSolidObstacles(candidatePos, groundVel, curr_.bodyRot, solidObstacles);
        candidatePos.x = std::clamp(candidatePos.x, -arenaHalfExtent, arenaHalfExtent);
        candidatePos.z = std::clamp(candidatePos.z, -arenaHalfExtent, arenaHalfExtent);

        const float nextX = candidatePos.x;
        const float nextZ = candidatePos.z;
        const float prevSurfaceY = ramp ? ramp->heightAt(previousPos.x, previousPos.z) : 0.0f;

        const bool nextOnRamp = ramp && ramp->containsXZ(nextX, nextZ);
        const bool crossedLaunchLip =
            ramp && wasOnRamp &&
            ((nextZ < ramp->endZ && prevSurfaceY >= ramp->topHeight * 0.72f) ||
             (!nextOnRamp && prevSurfaceY > 0.85f));

        if (crossedLaunchLip)
        {
            const glm::vec3 horizVel = (dt > 1e-6f)
                ? glm::vec3{(nextX - previousPos.x) / dt, 0.0f, (nextZ - previousPos.z) / dt}
                : moveDirXZ * effectiveMoveSpeed;
            const float horizSpeed = glm::length(horizVel);
            const float launchUp = std::clamp(horizSpeed * std::tan(ramp->slopePitchRad()) * 1.55f, 6.4f, 11.5f);

            curr_.bodyPos = glm::vec3{nextX, prevSurfaceY + 0.03f, nextZ};
            curr_.velocity = glm::vec3{horizVel.x, launchUp, horizVel.z};
            curr_.angularVel = glm::vec3{0.8f, 0.0f, 0.0f};
            curr_.airborne = true;
            curr_.onRamp = false;
            recoilHopActive_ = false;
            groundRollActive_ = false;
            recoilSettleActive_ = false;
            hullAlignActive_ = false;
            airRecoilCount_ = 0;
            tookOffThisTick_ = true;
        }
        else
        {
            const bool finalOnRamp = ramp && ramp->containsXZ(nextX, nextZ);
            const float surfaceY = ramp ? ramp->heightAt(nextX, nextZ) : 0.0f;

            const float dashLean = -0.07f * boostBlend_;
            const float targetPitch = finalOnRamp
                ? (ramp->slopePitchRad() * glm::dot(forwardXZ, glm::vec3{0.0f, 0.0f, -1.0f}))
                : dashLean;
            curr_.bodyPitch += (targetPitch - curr_.bodyPitch) * std::clamp(dt * 12.0f, 0.0f, 1.0f);
            const glm::quat targetRot = buildBodyRotation(curr_.bodyYaw, curr_.bodyPitch);

            if (hullAlignActive_)
            {
                const float u = std::clamp(hullAlignElapsed_ / std::max(hullAlignDuration_, 1e-4f), 0.0f, 1.0f);
                const float inv = 1.0f - u;
                const float ease = 1.0f - inv * inv * inv;
                curr_.angularVel = glm::vec3{0.0f};
                curr_.bodyRot = glm::normalize(glm::slerp(hullAlignStartRot_, targetRot, ease));
                if (u >= 1.0f)
                {
                    curr_.bodyRot = targetRot;
                    hullAlignActive_ = false;
                }
            }
            else if (groundRollActive_)
            {
                const float angSpeed = glm::length(curr_.angularVel);
                if (angSpeed > 0.35f)
                {
                    const glm::quat stepRot = glm::angleAxis(angSpeed * dt, curr_.angularVel / angSpeed);
                    curr_.bodyRot = glm::normalize(curr_.bodyRot * stepRot);
                    curr_.angularVel *= std::exp(-3.6f * dt);
                }
                else
                {
                    curr_.angularVel = glm::vec3{0.0f};
                    groundRollActive_ = false;
                    recoilStartRot_ = curr_.bodyRot;
                    recoilSettleElapsed_ = 0.0f;
                    recoilSettleActive_ = true;
                }
            }
            else if (recoilSettleActive_)
            {
                // Smooth logarithmic ease back to flat upright neutral pose with zero floor penetration.
                recoilSettleElapsed_ += dt;
                const float u = std::clamp(recoilSettleElapsed_ / std::max(recoilSettleDuration_, 1e-4f), 0.0f, 1.0f);
                const float smoothU = u * u * (3.0f - 2.0f * u);
                constexpr float kLogK = 4.0f;
                const float logAlpha = std::log1p(kLogK * smoothU) / std::log1p(kLogK);
                curr_.bodyRot = glm::normalize(glm::slerp(recoilStartRot_, targetRot, logAlpha));
                if (u >= 1.0f)
                {
                    curr_.bodyRot = targetRot;
                    recoilSettleActive_ = false;
                }
            }
            else
            {
                curr_.angularVel = glm::vec3{0.0f};
                curr_.bodyRot = glm::normalize(glm::slerp(curr_.bodyRot, targetRot, std::clamp(dt * 12.0f, 0.0f, 1.0f)));
            }

            const float minClearance = minBodyYAboveSurface(curr_.bodyRot, spec_);
            curr_.bodyPos = glm::vec3{nextX, surfaceY + minClearance, nextZ};
            curr_.onRamp = finalOnRamp;
            curr_.velocity = (dt > 1e-6f) ? (curr_.bodyPos - previousPos) / dt : glm::vec3{0.0f};
        }

        const float travel = glm::dot(curr_.bodyPos - previousPos, forwardXZ);
        const float turnTravel = wrapAnglePi(curr_.bodyYaw - previousYaw) * spec_.trackHalfSpacing;
        if (spec_.roadWheelRadius > 0.0f)
            curr_.wheelSpinRad -= glm::vec2{travel - turnTravel, travel + turnTravel} / spec_.roadWheelRadius;
        return;
    }

    // Airborne: altitude-enhanced gravity + brisk descent so high jumps land quickly without dragging.
    const float airAccel = spec_.airControlAccel * glm::mix(1.0f, 1.45f, boostBlend_);
    curr_.velocity += moveDirXZ * (airAccel * dt);

    const float surfaceNow = ramp ? ramp->heightAt(curr_.bodyPos.x, curr_.bodyPos.z) : 0.0f;
    const float altitude = std::max(0.0f, curr_.bodyPos.y - surfaceNow);
    float effectiveGravity = spec_.gravity;
    if (altitude > 4.5f)
        effectiveGravity += (altitude - 4.5f) * 4.2f;
    if (curr_.velocity.y < 0.0f)
        effectiveGravity *= 1.35f;

    curr_.velocity.y -= effectiveGravity * dt;
    curr_.velocity.y = std::min(curr_.velocity.y, 13.5f);

    curr_.bodyPos += curr_.velocity * dt;
    const float bodyRadius = std::min(spec_.bodyHalfExtents.x, spec_.bodyHalfExtents.z);
    resolveRampSideAndBackWalls(curr_.bodyPos, curr_.velocity, previousPos, false, bodyRadius, ramp);
    resolveSolidObstacles(curr_.bodyPos, curr_.velocity, curr_.bodyRot, solidObstacles);
    curr_.bodyPos.x = std::clamp(curr_.bodyPos.x, -arenaHalfExtent, arenaHalfExtent);
    curr_.bodyPos.z = std::clamp(curr_.bodyPos.z, -arenaHalfExtent, arenaHalfExtent);

    if (hullAlignActive_)
    {
        hullAlignElapsed_ += dt;
        const float u = std::clamp(hullAlignElapsed_ / std::max(hullAlignDuration_, 1e-4f), 0.0f, 1.0f);
        const float inv = 1.0f - u;
        const float ease = 1.0f - inv * inv * inv;
        curr_.angularVel = glm::vec3{0.0f};
        curr_.bodyYaw = lerpAngle(hullAlignStartYaw_, hullAlignTargetYaw_, ease);
        const glm::quat airTargetRot = buildBodyRotation(curr_.bodyYaw, 0.0f);
        curr_.bodyRot = glm::normalize(glm::slerp(hullAlignStartRot_, airTargetRot, ease));
        if (u >= 1.0f)
        {
            curr_.bodyRot = airTargetRot;
            hullAlignActive_ = false;
        }
    }
    else
    {
        const float angSpeed = glm::length(curr_.angularVel);
        if (angSpeed > 1e-4f)
        {
            const glm::quat stepRot = glm::angleAxis(angSpeed * dt, curr_.angularVel / angSpeed);
            curr_.bodyRot = glm::normalize(curr_.bodyRot * stepRot);
            curr_.angularVel *= std::exp((recoilHopActive_ ? -2.8f : -0.85f) * dt);
        }
    }

    const float surfaceBelow = ramp ? ramp->heightAt(curr_.bodyPos.x, curr_.bodyPos.z) : 0.0f;
    const float minClearanceNow = minBodyYAboveSurface(curr_.bodyRot, spec_);
    const float heightNow = std::max(0.0f, curr_.bodyPos.y - (surfaceBelow + minClearanceNow));

    // Smoothly align tank bottom face downward during descent (including brief recoil hops) so tracks touch down cleanly.
    const bool descendingNearGround = (curr_.velocity.y < 0.0f && heightNow < 5.0f);
    const bool settlingRecoilHop = (recoilHopActive_ && curr_.velocity.y < 1.5f);
    if (descendingNearGround || settlingRecoilHop)
    {
        const glm::vec3 currFwd = curr_.bodyRot * glm::vec3{0.0f, 0.0f, -1.0f};
        if (currFwd.x * currFwd.x + currFwd.z * currFwd.z > 0.04f)
            curr_.bodyYaw = std::atan2(-currFwd.x, -currFwd.z);

        const float proximity = 1.0f - std::clamp(heightNow / 5.0f, 0.0f, 1.0f);
        curr_.angularVel *= std::exp(-(5.5f + proximity * 8.0f) * dt);

        const glm::vec3 yawFwd{-std::sin(curr_.bodyYaw), 0.0f, -std::cos(curr_.bodyYaw)};
        const float landingSlopePitch = (ramp && ramp->containsXZ(curr_.bodyPos.x, curr_.bodyPos.z) && surfaceBelow > 0.01f)
            ? (ramp->slopePitchRad() * glm::dot(yawFwd, glm::vec3{0.0f, 0.0f, -1.0f}))
            : 0.0f;
        const glm::quat uprightRot = buildBodyRotation(curr_.bodyYaw, landingSlopePitch);
        const float alignRate = recoilHopActive_ ? (7.0f + proximity * 7.0f) : (3.8f + proximity * 10.0f);
        curr_.bodyRot = glm::normalize(glm::slerp(curr_.bodyRot, uprightRot, std::clamp(dt * alignRate, 0.0f, 1.0f)));
    }

    const float landingY = surfaceBelow + minBodyYAboveSurface(curr_.bodyRot, spec_);
    if (curr_.bodyPos.y < landingY)
    {
        // Strict floor/ramp contact clamp: prevents any hull/track corner from ever dipping underground.
        curr_.bodyPos.y = landingY;
    }

    if (curr_.bodyPos.y <= landingY + 1e-4f && curr_.velocity.y <= 0.0f)
    {
        resolveRampSideAndBackWalls(curr_.bodyPos, curr_.velocity, previousPos, false, bodyRadius, ramp);
        resolveSolidObstacles(curr_.bodyPos, curr_.velocity, curr_.bodyRot, solidObstacles);
        curr_.bodyPos.x = std::clamp(curr_.bodyPos.x, -arenaHalfExtent, arenaHalfExtent);
        curr_.bodyPos.z = std::clamp(curr_.bodyPos.z, -arenaHalfExtent, arenaHalfExtent);

        const float resolvedSurface = ramp ? ramp->heightAt(curr_.bodyPos.x, curr_.bodyPos.z) : 0.0f;
        curr_.bodyPos.y = resolvedSurface + minBodyYAboveSurface(curr_.bodyRot, spec_);
        recoilSlideVelXZ_ = glm::vec3{curr_.velocity.x * 0.28f, 0.0f, curr_.velocity.z * 0.28f};
        curr_.velocity = glm::vec3{0.0f};
        curr_.angularVel = glm::vec3{0.0f};
        curr_.airborne = false;
        recoilHopActive_ = false;
        airRecoilCount_ = 0;
        curr_.onRamp = ramp && ramp->containsXZ(curr_.bodyPos.x, curr_.bodyPos.z) && resolvedSurface > 0.01f;
        curr_.bodyPitch = 0.0f;

        // Continuous, zero-snap smooth settle from landing orientation to flat neutral pose.
        recoilStartRot_ = curr_.bodyRot;
        recoilSettleElapsed_ = 0.0f;
        recoilSettleActive_ = true;
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
    blended.bodyPos.y = std::max(blended.bodyPos.y, minBodyYAboveSurface(blended.bodyRot, spec_));
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
