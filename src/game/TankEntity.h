#pragma once

#include "game/Physics.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <span>

namespace game
{
struct RampSurface
{
    float minX{3.6f};
    float maxX{8.4f};
    float startZ{6.5f};
    float endZ{0.5f};
    float topHeight{2.5f};

    bool containsXZ(float x, float z) const
    {
        return x >= minX && x <= maxX && z <= startZ && z >= endZ;
    }

    float heightAt(float x, float z) const;
    float slopePitchRad() const;
};

struct TankSpec
{
    glm::vec3 bodyHalfExtents{1.10f, 0.45f, 1.50f};
    glm::vec3 bodyLocalOffset{0.0f, 0.45f, 0.0f};

    glm::vec3 turretMount{0.0f, 0.90f, 0.0f};
    glm::vec3 turretHalfExtents{0.70f, 0.30f, 0.80f};
    glm::vec3 turretLocalOffset{0.0f, 0.30f, 0.0f};

    glm::vec3 barrelHinge{0.0f, 0.30f, -0.75f};
    glm::vec3 barrelHalfExtents{0.12f, 0.12f, 0.85f};
    glm::vec3 barrelLocalOffset{0.0f, 0.0f, -0.85f};
    glm::vec3 muzzleOffset{0.0f, 0.0f, -1.75f};

    float roadWheelRadius{0.176f};
    float trackHalfSpacing{0.813f};

    float moveSpeed{8.5f};
    float turnSpeedRad{10.0f};
    float airControlAccel{5.5f};
    float gravity{12.0f};
    float manualAimSpeedRad{1.8f};
    float minPitchRad{-0.45f};
    float maxPitchRad{0.70f};

    float fireCooldownSec{0.35f};
    float projectileSpeed{32.0f};
    float projectileLifetimeSec{2.5f};
    int projectileDamage{25};
    int maxHp{100};
};

TankSpec makeSandTankSpec();

struct TankPose
{
    glm::vec2 wheelSpinRad{0.0f};
    glm::mat4 bodyWorld{1.0f};
    glm::mat4 turretWorld{1.0f};
    glm::mat4 barrelWorld{1.0f};

    glm::mat4 bodyDraw{1.0f};
    glm::mat4 turretDraw{1.0f};
    glm::mat4 barrelDraw{1.0f};

    OBB bodyOBB{};
    OBB turretOBB{};
    OBB barrelOBB{};

    glm::vec3 barrelHingeWorld{0.0f};
    glm::vec3 muzzleOrigin{0.0f};
    glm::vec3 muzzleForward{0.0f, 0.0f, -1.0f};
    glm::mat3 muzzleAxes{1.0f};
};

struct Projectile
{
    EntityId ownerId{kInvalidEntityId};
    glm::vec3 prevPos{0.0f};
    glm::vec3 pos{0.0f};
    glm::vec3 velocity{0.0f};
    float remainingLife{0.0f};
    int damage{25};
    bool pendingRemoval{false};
};

class TankEntity
{
public:
    void reset(EntityId id, const glm::vec3& groundPos, float bodyYawRad, const TankSpec& spec = {});
    void beginTick(float dt);

    void applyDrive(
        float moveForwardInput,
        float moveRightInput,
        float dt,
        const RampSurface* ramp = nullptr,
        std::span<const OBB> solidObstacles = {},
        float arenaHalfExtent = 23.0f);
    void adjustManualAim(float turretTurnInput, float barrelPitchInput, float dt);
    void aimAt(const glm::vec3& worldTarget);

    TankPose evaluatePose() const;
    TankPose evaluatePose(float alpha) const;

    bool tryFire(Projectile& outProjectile, bool ignoreCooldown = false);
    void applyDamage(int amount);

    EntityId id() const { return id_; }
    int hp() const { return hp_; }
    int maxHp() const { return spec_.maxHp; }
    bool alive() const { return hp_ > 0; }
    bool isAirborne() const { return curr_.airborne; }
    bool tookOffThisTick() const { return tookOffThisTick_; }
    float heightAboveGround(const RampSurface* ramp = nullptr) const;
    float cooldownRemaining() const { return cooldownRemaining_; }
    const glm::vec3& position() const { return curr_.bodyPos; }
    const glm::vec3& velocity() const { return curr_.velocity; }
    const TankSpec& spec() const { return spec_; }

private:
    struct ControlState
    {
        glm::vec3 bodyPos{0.0f};
        glm::quat bodyRot{1.0f, 0.0f, 0.0f, 0.0f};
        glm::vec3 velocity{0.0f};
        float bodyYaw{0.0f};
        float bodyPitch{0.0f};
        float turretYaw{0.0f};
        float barrelPitch{0.0f};
        glm::vec2 wheelSpinRad{0.0f};
        bool airborne{false};
        bool onRamp{false};
    };

    TankPose evaluateFromState(const ControlState& state) const;
    glm::quat buildBodyRotation(float yaw, float pitch) const;

    EntityId id_{kInvalidEntityId};
    TankSpec spec_{};
    ControlState prev_{};
    ControlState curr_{};
    int hp_{100};
    float cooldownRemaining_{0.0f};
    bool tookOffThisTick_{false};
};
} // namespace game
