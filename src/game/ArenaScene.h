#pragma once

#include "game/Physics.h"
#include "game/TankEntity.h"
#include "game/TimeManager.h"
#include "game/TrickshotController.h"

#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>
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

struct PreviewBoxItem
{
    glm::mat4 worldMatrix{1.0f};
    glm::vec3 color{1.0f};
    bool wireframe{false};
    bool tankFallback{false};
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
    CameraRequest cameraRequest{};
    TrickshotHudState trickshot{};
};

class ArenaScene
{
public:
    void reset();
    void updateRealTime(
        float rawDt,
        const glm::vec2& rawMouseDelta,
        const glm::vec3& mainCamPos,
        const glm::vec3& mainCamLookAt,
        float mainCamFovDeg);
    bool fixedUpdate(float dt, const TickCommand& cmd, const PresentedView& view);
    PreviewFrame buildFrame(float alpha);

    const TankEntity& player() const { return player_; }
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
    glm::vec3 computeMouseAimPoint(const glm::vec2& cursorPos, const PresentedView& view) const;
    void stepProjectiles(float dt);

    OBB floorBox_{};
    RampSurface ramp_{};
    OBB rampDeckBox_{};
    OBB rampSupportBox_{};
    OBB barricadeBox_{};

    TankEntity player_{};
    std::vector<EnemyEntity> enemies_{};
    std::vector<Projectile> projectiles_{};
    std::vector<HitRecord> frameHits_{};
    std::vector<TraceCollider> displayedColliders_{};
    TrickshotController trickshot_{};

    EntityId nextEntityId_{1};
    std::uint32_t totalTargetHits_{0};
    bool debugOverlay_{true};
};

void runCp01UnitChecks();
void runCp02UnitChecks();
} // namespace game
