#include "game/ArenaScene.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <print>
#include <string_view>

namespace game
{
namespace
{
constexpr float kFloorHalfWidth = 25.0f;
constexpr float kFloorHalfThickness = 0.25f;

std::string_view partName(PartKind part)
{
    switch (part)
    {
    case PartKind::Terrain:
        return "Terrain";
    case PartKind::Body:
        return "Body";
    case PartKind::Turret:
        return "Turret";
    case PartKind::Barrel:
        return "Barrel";
    default:
        return "None";
    }
}

void appendAxesLines(
    std::vector<PreviewLineItem>& lines,
    const glm::vec3& origin,
    const glm::mat3& axes,
    float length)
{
    lines.push_back({origin, origin + axes[0] * length, glm::vec3{1.0f, 0.25f, 0.25f}});
    lines.push_back({origin, origin + axes[1] * length, glm::vec3{0.25f, 1.0f, 0.35f}});
    lines.push_back({origin, origin - axes[2] * length, glm::vec3{0.30f, 0.65f, 1.0f}});
}

void appendTankPreview(
    PreviewFrame& frame,
    const TankPose& pose,
    const glm::vec3& baseColor,
    bool drawDebug,
    bool drawAimRay,
    const glm::vec3& materialTint)
{
    // Keep boxing as a fallback when the GLB cannot be loaded.
    frame.tanks.push_back({pose, materialTint});
    frame.boxes.push_back({pose.bodyDraw, baseColor, false, true});
    frame.boxes.push_back({pose.turretDraw, baseColor * 1.18f, false, true});
    frame.boxes.push_back({pose.barrelDraw, baseColor * 0.82f, false, true});

    if (!drawDebug)
        return;

    const glm::vec3 obbWireColor{0.95f, 0.90f, 0.25f};
    frame.boxes.push_back({pose.bodyDraw, obbWireColor, true});
    frame.boxes.push_back({pose.turretDraw, obbWireColor, true});
    frame.boxes.push_back({pose.barrelDraw, obbWireColor, true});

    appendAxesLines(frame.lines, pose.bodyOBB.center, pose.bodyOBB.axes, 1.8f);
    appendAxesLines(frame.lines, pose.muzzleOrigin, pose.muzzleAxes, 0.75f);

    const float h = 0.22f;
    const glm::vec3 hinge = pose.barrelHingeWorld;
    const glm::vec3 hingeColor{1.0f, 0.85f, 0.15f};
    frame.lines.push_back({hinge - glm::vec3{h, 0.0f, 0.0f}, hinge + glm::vec3{h, 0.0f, 0.0f}, hingeColor});
    frame.lines.push_back({hinge - glm::vec3{0.0f, h, 0.0f}, hinge + glm::vec3{0.0f, h, 0.0f}, hingeColor});
    frame.lines.push_back({hinge - glm::vec3{0.0f, 0.0f, h}, hinge + glm::vec3{0.0f, 0.0f, h}, hingeColor});

    if (drawAimRay)
    {
        frame.lines.push_back({
            pose.muzzleOrigin,
            pose.muzzleOrigin + pose.muzzleForward * 24.0f,
            glm::vec3{0.20f, 0.95f, 0.95f}});
    }
}
} // namespace

void ArenaScene::reset()
{
    floorBox_ = OBB{};
    floorBox_.center = glm::vec3{0.0f, -kFloorHalfThickness, 0.0f};
    floorBox_.axes = glm::mat3{1.0f};
    floorBox_.halfExtents = glm::vec3{kFloorHalfWidth, kFloorHalfThickness, kFloorHalfWidth};

    nextEntityId_ = 1;
    const EntityId playerId = nextEntityId_++;
    const TankSpec tankSpec = makeSandTankSpec();
    player_.reset(playerId, glm::vec3{0.0f, 0.0f, 8.0f}, 0.0f, tankSpec);

    enemies_.clear();
    EnemyEntity target{};
    target.id = nextEntityId_++;
    target.tank.reset(target.id, glm::vec3{0.0f, 0.0f, -8.0f}, glm::pi<float>(), tankSpec);
    enemies_.push_back(target);

    projectiles_.clear();
    frameHits_.clear();
    totalTargetHits_ = 0;

    std::println(
        "[CP01] Reset | Player(ID={}) HP={}/{} | Target(ID={}) HP={}/{} | Projectiles=0 | Hits=0",
        player_.id(),
        player_.hp(),
        player_.maxHp(),
        enemies_.front().id,
        enemies_.front().tank.hp(),
        enemies_.front().tank.maxHp());
}

bool ArenaScene::fixedUpdate(float dt, const TickCommand& cmd, const PresentedView& view)
{
    if (cmd.restartRequested)
    {
        reset();
        return true;
    }

    if (cmd.toggleDebugRequested)
        debugOverlay_ = !debugOverlay_;

    player_.beginTick(dt);
    for (EnemyEntity& enemy : enemies_)
        enemy.tank.beginTick(dt);

    player_.applyDrive(cmd.moveForward, cmd.turnYaw, dt, kFloorHalfWidth - 2.0f);

    if (cmd.manualTurretTurn != 0.0f || cmd.manualBarrelPitch != 0.0f)
    {
        player_.adjustManualAim(cmd.manualTurretTurn, cmd.manualBarrelPitch, dt);
    }
    else if (cmd.hasMouseAim && view.valid)
    {
        const glm::vec3 aimPoint = computeMouseAimPoint(cmd.cursorPos, view);
        player_.aimAt(aimPoint);
    }

    if (cmd.fireRequested)
    {
        Projectile spawned{};
        if (player_.tryFire(spawned))
        {
            projectiles_.push_back(spawned);
            std::println(
                "[CP01] Fire | Owner={} Origin=({:.2f}, {:.2f}, {:.2f}) Vel=({:.2f}, {:.2f}, {:.2f})",
                spawned.ownerId,
                spawned.pos.x,
                spawned.pos.y,
                spawned.pos.z,
                spawned.velocity.x,
                spawned.velocity.y,
                spawned.velocity.z);
        }
    }

    stepProjectiles(dt);
    return false;
}

EnemyEntity* ArenaScene::findAliveEnemy(EntityId id)
{
    for (EnemyEntity& enemy : enemies_)
    {
        if (enemy.id == id && enemy.tank.alive())
            return &enemy;
    }
    return nullptr;
}

std::vector<TraceCollider> ArenaScene::gatherColliders() const
{
    std::vector<TraceCollider> colliders{};
    colliders.reserve(1 + (1 + enemies_.size()) * 3);

    colliders.push_back({kInvalidEntityId, PartKind::Terrain, floorBox_});

    if (player_.alive())
    {
        const TankPose p = player_.evaluatePose();
        colliders.push_back({player_.id(), PartKind::Body, p.bodyOBB});
        colliders.push_back({player_.id(), PartKind::Turret, p.turretOBB});
        colliders.push_back({player_.id(), PartKind::Barrel, p.barrelOBB});
    }

    for (const EnemyEntity& enemy : enemies_)
    {
        if (!enemy.tank.alive())
            continue;

        const TankPose e = enemy.tank.evaluatePose();
        colliders.push_back({enemy.id, PartKind::Body, e.bodyOBB});
        colliders.push_back({enemy.id, PartKind::Turret, e.turretOBB});
        colliders.push_back({enemy.id, PartKind::Barrel, e.barrelOBB});
    }

    return colliders;
}

glm::vec3 ArenaScene::computeMouseAimPoint(const glm::vec2& cursorPos, const PresentedView& view) const
{
    const float winY = view.windowSize.y - cursorPos.y;
    const glm::vec4 vp{
        static_cast<float>(view.viewport.x),
        static_cast<float>(view.viewport.y),
        static_cast<float>(view.viewport.z),
        static_cast<float>(view.viewport.w)};

    const glm::vec3 nearPt = glm::unProject(glm::vec3{cursorPos.x, winY, 0.0f}, view.view, view.projection, vp);
    const glm::vec3 farPt = glm::unProject(glm::vec3{cursorPos.x, winY, 1.0f}, view.view, view.projection, vp);
    const glm::vec3 diff = farPt - nearPt;
    const float lenSq = glm::dot(diff, diff);
    if (lenSq < 1e-8f)
        return player_.position() + glm::vec3{0.0f, 1.0f, -10.0f};

    const glm::vec3 rayDir = diff / std::sqrt(lenSq);
    const glm::vec3 rayEnd = nearPt + rayDir * 200.0f;

    const std::vector<TraceCollider> colliders = gatherColliders();
    const SegmentHit hit = Physics::traceSegment(nearPt, rayEnd, colliders, player_.id());

    glm::vec3 target = hit.hit ? hit.point : (nearPt + rayDir * 45.0f);

    const glm::vec3 fromTank = target - player_.position();
    const float horizDistSq = fromTank.x * fromTank.x + fromTank.z * fromTank.z;
    if (horizDistSq < 2.25f)
    {
        glm::vec2 xz{rayDir.x, rayDir.z};
        const float xzLen = glm::length(xz);
        xz = (xzLen > 1e-4f) ? (xz / xzLen) : glm::vec2{0.0f, -1.0f};
        target.x = player_.position().x + xz.x * 1.5f;
        target.z = player_.position().z + xz.y * 1.5f;
    }
    return target;
}

void ArenaScene::stepProjectiles(float dt)
{
    if (projectiles_.empty())
        return;

    const std::vector<TraceCollider> colliders = gatherColliders();

    for (Projectile& proj : projectiles_)
    {
        if (proj.pendingRemoval)
            continue;

        proj.prevPos = proj.pos;
        const float stepDt = std::min(dt, proj.remainingLife);
        const glm::vec3 nextPos = proj.pos + proj.velocity * stepDt;

        const SegmentHit contact = Physics::traceSegment(proj.pos, nextPos, colliders, proj.ownerId);
        if (contact.hit && contact.entityId != kInvalidEntityId && contact.part != PartKind::Terrain)
        {
            proj.pos = contact.point;
            proj.pendingRemoval = true;

            if (EnemyEntity* hitEnemy = findAliveEnemy(contact.entityId))
            {
                hitEnemy->tank.applyDamage(proj.damage);
                ++totalTargetHits_;
                frameHits_.push_back({
                    hitEnemy->id,
                    contact.part,
                    contact.point,
                    contact.normal,
                    proj.damage});

                std::println(
                    "[CP01] Hit! Target(ID={}) Part={} HP={}/{} Contact=({:.2f}, {:.2f}, {:.2f}) TotalHits={}",
                    hitEnemy->id,
                    partName(contact.part),
                    hitEnemy->tank.hp(),
                    hitEnemy->tank.maxHp(),
                    contact.point.x,
                    contact.point.y,
                    contact.point.z,
                    totalTargetHits_);
            }
        }
        else if (contact.hit || proj.remainingLife <= dt + 1e-6f)
        {
            proj.pos = contact.hit ? contact.point : nextPos;
            proj.pendingRemoval = true;
            std::println(
                "[CP01] Miss/Expire ({}) | EndPos=({:.2f}, {:.2f}, {:.2f}) | TotalHits={}",
                contact.hit ? "Terrain" : "Lifetime",
                proj.pos.x,
                proj.pos.y,
                proj.pos.z,
                totalTargetHits_);
        }
        else
        {
            proj.pos = nextPos;
            proj.remainingLife -= stepDt;
        }
    }

    std::erase_if(projectiles_, [](const Projectile& p) { return p.pendingRemoval; });
}

PreviewFrame ArenaScene::buildFrame(float alpha) const
{
    PreviewFrame frame{};
    frame.debugOverlay = debugOverlay_;
    frame.playerPos = player_.position();
    frame.playerHp = player_.hp();
    if (!enemies_.empty())
    {
        frame.targetHp = enemies_.front().tank.hp();
        frame.targetMaxHp = enemies_.front().tank.maxHp();
    }
    frame.totalTargetHits = totalTargetHits_;
    frame.activeProjectiles = projectiles_.size();

    const glm::mat4 floorDraw =
        glm::translate(glm::mat4{1.0f}, floorBox_.center) *
        glm::scale(glm::mat4{1.0f}, floorBox_.halfExtents * 2.0f);
    frame.boxes.push_back({floorDraw, glm::vec3{0.22f, 0.25f, 0.29f}, false});

    if (debugOverlay_)
    {
        for (int g = -20; g <= 20; g += 5)
        {
            const float c = static_cast<float>(g);
            const glm::vec3 gridColor = (g == 0)
                ? glm::vec3{0.38f, 0.42f, 0.48f}
                : glm::vec3{0.28f, 0.31f, 0.36f};
            frame.lines.push_back({glm::vec3{-25.0f, 0.01f, c}, glm::vec3{25.0f, 0.01f, c}, gridColor});
            frame.lines.push_back({glm::vec3{c, 0.01f, -25.0f}, glm::vec3{c, 0.01f, 25.0f}, gridColor});
        }
    }

    appendTankPreview(
        frame,
        player_.evaluatePose(alpha),
        glm::vec3{0.24f, 0.62f, 0.36f},
        debugOverlay_,
        true,
        glm::vec3{1.0f});

    for (const EnemyEntity& enemy : enemies_)
    {
        const float hpRatio = (enemy.tank.maxHp() > 0)
            ? static_cast<float>(enemy.tank.hp()) / static_cast<float>(enemy.tank.maxHp())
            : 0.0f;
        const glm::vec3 enemyColor = enemy.tank.alive()
            ? glm::mix(glm::vec3{0.95f, 0.55f, 0.18f}, glm::vec3{0.82f, 0.24f, 0.24f}, hpRatio)
            : glm::vec3{0.32f, 0.32f, 0.34f};

        appendTankPreview(
            frame,
            enemy.tank.evaluatePose(alpha),
            enemyColor,
            debugOverlay_ && enemy.tank.alive(),
            false,
            enemyColor);
    }

    const float t = std::clamp(alpha, 0.0f, 1.0f);
    for (const Projectile& proj : projectiles_)
    {
        const glm::vec3 renderPos = glm::mix(proj.prevPos, proj.pos, t);
        const glm::mat4 projDraw =
            glm::translate(glm::mat4{1.0f}, renderPos) *
            glm::scale(glm::mat4{1.0f}, glm::vec3{0.24f});
        frame.boxes.push_back({projDraw, glm::vec3{1.0f, 0.92f, 0.30f}, false});
    }

    return frame;
}

void runCp01UnitChecks()
{
    std::println("=== [CP01 QA Unit Checks] ===");

    // QA-02: 1/240s click -> 0 ticks (click retained), then 3/240s -> 2 ticks, total fire requests = 1
    {
        TimeManager tm{};
        InputBuffer input{};
        input.reset();
        tm.reset();

        input.onMouseButton(0, 1); // GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS
        tm.beginFrame(1.0f / 240.0f, 1.0f);
        int frame1Ticks = 0;
        int fireCount = 0;
        while (tm.consumeStep())
        {
            ++frame1Ticks;
            if (input.peekCommand().fireRequested)
                ++fireCount;
            input.consumeOneShot();
        }
        const bool retainedAfterZeroTick = input.peekCommand().fireRequested;

        tm.beginFrame(3.0f / 240.0f, 1.0f);
        int frame2Ticks = 0;
        while (tm.consumeStep())
        {
            ++frame2Ticks;
            if (input.peekCommand().fireRequested)
                ++fireCount;
            input.consumeOneShot();
        }

        const bool pass = (frame1Ticks == 0) && retainedAfterZeroTick && (frame2Ticks == 2) && (fireCount == 1);
        std::println(
            "[QA-02] {} | frame1Ticks={} retained={} frame2Ticks={} totalFireRequests={}",
            pass ? "PASS" : "FAIL",
            frame1Ticks,
            retainedAfterZeroTick,
            frame2Ticks,
            fireCount);
    }

    // QA-03: 1.0s split into 30 / 60 / 120 frames at scale 1.0 (120 ticks) and 0.25 (30 ticks)
    {
        const auto countTicks = [](int frames, float scale)
        {
            TimeManager tm{};
            tm.reset();
            int ticks = 0;
            const float dt = 1.0f / static_cast<float>(frames);
            for (int i = 0; i < frames; ++i)
            {
                tm.beginFrame(dt, scale);
                while (tm.consumeStep())
                    ++ticks;
            }
            return ticks;
        };

        const int n30 = countTicks(30, 1.0f);
        const int n60 = countTicks(60, 1.0f);
        const int n120 = countTicks(120, 1.0f);
        const int s30 = countTicks(30, 0.25f);
        const int s60 = countTicks(60, 0.25f);
        const int s120 = countTicks(120, 0.25f);

        const auto nearTick = [](int actual, int expected) { return std::abs(actual - expected) <= 1; };
        const bool pass = nearTick(n30, 120) && nearTick(n60, 120) && nearTick(n120, 120) &&
                          nearTick(s30, 30) && nearTick(s60, 30) && nearTick(s120, 30);
        std::println(
            "[QA-03] {} | 1.0x=({}, {}, {}) [exp 120] | 0.25x=({}, {}, {}) [exp 30]",
            pass ? "PASS" : "FAIL",
            n30,
            n60,
            n120,
            s30,
            s60,
            s120);
    }

    // QA-06: Single segment crossing two collinear thin target OBBs -> front hit once, back hit zero
    {
        TraceCollider front{};
        front.ownerId = 10;
        front.part = PartKind::Body;
        front.box = OBB{glm::vec3{0.0f, 1.0f, -3.0f}, glm::mat3{1.0f}, glm::vec3{0.6f, 0.6f, 0.1f}};

        TraceCollider back{};
        back.ownerId = 11;
        back.part = PartKind::Body;
        back.box = OBB{glm::vec3{0.0f, 1.0f, -6.0f}, glm::mat3{1.0f}, glm::vec3{0.6f, 0.6f, 0.1f}};

        const TraceCollider targets[2]{front, back};
        int frontHits = 0;
        int backHits = 0;

        const SegmentHit hit = Physics::traceSegment(
            glm::vec3{0.0f, 1.0f, 0.0f},
            glm::vec3{0.0f, 1.0f, -10.0f},
            targets,
            1);
        if (hit.hit && hit.entityId == front.ownerId)
            ++frontHits;
        if (hit.hit && hit.entityId == back.ownerId)
            ++backHits;

        const bool pass = (frontHits == 1) && (backHits == 0);
        std::println(
            "[QA-06] {} | hitId={} frontHits={} backHits={}",
            pass ? "PASS" : "FAIL",
            hit.entityId,
            frontHits,
            backHits);
    }
}
} // namespace game
