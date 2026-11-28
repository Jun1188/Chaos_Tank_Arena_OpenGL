#pragma once

#include "game/Physics.h"
#include "game/TankEntity.h"
#include "game/TimeManager.h"
#include "game/TrickshotController.h"

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>

namespace game
{
struct HitRecord
{
    EntityId targetId{kInvalidEntityId};
    PartKind part{PartKind::None};
    glm::vec3 point{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    int damage{0};
};

struct EnemyEntity
{
    EntityId id{kInvalidEntityId};
    TankEntity tank{};
};

struct PropBody
{
    EntityId id{kInvalidEntityId};
    glm::vec3 prevPos{0.0f};
    glm::vec3 pos{0.0f};
    glm::quat prevRot{1.0f, 0.0f, 0.0f, 0.0f};
    glm::quat rot{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 velocity{0.0f};
    glm::vec3 angularVel{0.0f};
    glm::vec3 halfExtents{1.0f};

    OBB toOBB(float alpha = 1.0f) const;
    void reset(EntityId propId, const glm::vec3& center, const glm::vec3& extents);
    void applyHitImpulse(const glm::vec3& hitPoint, const glm::vec3& impulseDir);
    void step(
        float dt,
        const RampSurface* ramp = nullptr,
        std::span<const glm::vec3> tankPositions = {},
        float arenaHalfExtent = 23.0f);
};

struct PreviewBoxItem
{
    glm::mat4 worldMatrix{1.0f};
    glm::vec3 color{1.0f};
    bool wireframe{false};
    bool tankFallback{false};
    bool keepOpaqueInZoom{false};
    glm::vec3 worldCenter{0.0f};
    float boundingRadius{0.5f};
};

struct PreviewLineItem
{
    glm::vec3 a{0.0f};
    glm::vec3 b{0.0f};
    glm::vec3 color{1.0f};
};

struct PreviewTankItem
{
    TankPose pose{};
    glm::vec3 tint{1.0f};
    bool isPlayer{false};
    bool isZoomTarget{false};
    glm::vec3 worldCenter{0.0f};
    float boundingRadius{1.8f};
};

struct PreviewFrame
{
    std::vector<PreviewTankItem> tanks{};
    std::vector<PreviewBoxItem> boxes{};
    std::vector<PreviewLineItem> lines{};
    glm::vec3 playerPos{0.0f};
    bool playerAirborne{false};
    float playerHeight{0.0f};
    int playerHp{100};
    int targetHp{100};
    int targetMaxHp{100};
    std::uint32_t totalTargetHits{0};
    std::size_t activeProjectiles{0};
    bool debugOverlay{true};
    float zoomAlphaFade{0.0f};
    glm::vec3 zoomTargetPos{0.0f, 1.0f, -10.0f};
    CameraRequest cameraRequest{};
    TrickshotHudState trickshot{};
};

struct AimCameraContext
{
    glm::vec3 playerPos{0.0f};
    glm::vec3 aimPoint{0.0f, 0.9f, -10.0f};
    float planarSpeed{0.0f};
    bool hasTargetFocus{false};
    bool airborne{false};
    float heightAboveGround{0.0f};
};

class ArenaScene
{
public:
    void reset();
    void updateRealTime(
        float rawDt,
        float renderAlpha,
        const glm::vec2& rawMouseDelta,
        bool rmbHeld,
        bool lmbHeld,
        const glm::vec3& mainCamPos,
        const glm::vec3& mainCamLookAt,
        float mainCamFovDeg);
    bool fixedUpdate(float dt, const TickCommand& cmd, const PresentedView& view);
    PreviewFrame buildFrame(float alpha);

    const TankEntity& player() const { return player_; }
    glm::vec3 playerInterpolatedPosition(float alpha) const { return player_.interpolatedPosition(alpha); }
    glm::vec3 playerInterpolatedForwardXZ(float alpha) const { return player_.interpolatedForwardXZ(alpha); }
    AimCameraContext cameraAimContext(float alpha) const;
    const std::vector<EnemyEntity>& enemies() const { return enemies_; }
    const std::vector<Projectile>& projectiles() const { return projectiles_; }
    const std::vector<HitRecord>& frameHits() const { return frameHits_; }
    const TrickshotController& trickshot() const { return trickshot_; }
    float timeScale() const { return trickshot_.timeScale(); }
    std::uint32_t totalTargetHits() const { return totalTargetHits_; }
    bool debugOverlay() const { return debugOverlay_; }

    void clearFrameHits() { frameHits_.clear(); }

private:
    EnemyEntity* findAliveEnemy(EntityId id);
    const EnemyEntity* findAliveEnemy(EntityId id) const;
    std::vector<TraceCollider> gatherColliders(bool interpolated, float alpha) const;
    glm::vec3 computeMouseAimPoint(
        const glm::vec2& cursorPos,
        const PresentedView& view,
        EntityId* outHoveredEnemyId = nullptr) const;
    void firePlayerCannon(bool ignoreCooldown);
    void stepProjectiles(float dt);

    OBB floorBox_{};
    RampSurface ramp_{};
    OBB rampDeckBox_{};
    OBB rampSupportBox_{};
    PropBody barricade_{};

    TankEntity player_{};
    std::vector<EnemyEntity> enemies_{};
    std::vector<Projectile> projectiles_{};
    std::vector<HitRecord> frameHits_{};
    std::vector<TraceCollider> displayedColliders_{};
    TrickshotController trickshot_{};

    struct ExplosionBurst
    {
        glm::vec3 origin{0.0f};
        float elapsed{0.0f};
        float duration{0.45f};
    };

    std::vector<ExplosionBurst> explosionBursts_{};
    glm::vec3 worldAimPoint_{0.0f, 0.9f, -10.0f};
    EntityId hoveredEnemyId_{kInvalidEntityId};
    EntityId nextEntityId_{1};
    std::uint32_t totalTargetHits_{0};
    bool debugOverlay_{true};
};

void runCp01UnitChecks();
void runCp02UnitChecks();
} // namespace game
