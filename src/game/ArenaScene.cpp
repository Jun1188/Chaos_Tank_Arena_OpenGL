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

glm::mat4 makeOBBDrawMatrix(const OBB& obb)
{
    glm::mat4 world{1.0f};
    world[0] = glm::vec4{obb.axes[0], 0.0f};
    world[1] = glm::vec4{obb.axes[1], 0.0f};
    world[2] = glm::vec4{obb.axes[2], 0.0f};
    world[3] = glm::vec4{obb.center, 1.0f};
    return world * glm::scale(glm::mat4{1.0f}, obb.halfExtents * 2.0f);
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

void appendGroundCircle(
    std::vector<PreviewLineItem>& lines,
    const glm::vec3& center,
    float radius,
    const glm::vec3& color,
    int segments = 36)
{
    for (int i = 0; i < segments; ++i)
    {
        const float a0 = glm::two_pi<float>() * (static_cast<float>(i) / static_cast<float>(segments));
        const float a1 = glm::two_pi<float>() * (static_cast<float>(i + 1) / static_cast<float>(segments));
        const glm::vec3 p0{center.x + std::cos(a0) * radius, 0.04f, center.z + std::sin(a0) * radius};
        const glm::vec3 p1{center.x + std::cos(a1) * radius, 0.04f, center.z + std::sin(a1) * radius};
        lines.push_back({p0, p1, color});
    }
}

void appendTankPreview(
    PreviewFrame& frame,
    const TankPose& pose,
    const glm::vec3& baseColor,
    bool drawDebug,
    bool drawAimRay,
    bool selectedHighlight,
    const glm::vec3& materialTint)
{
    // Keep boxing as a fallback when the GLB cannot be loaded.
    frame.tanks.push_back({pose, materialTint});
    frame.boxes.push_back({pose.bodyDraw, baseColor, false, true});
    frame.boxes.push_back({pose.turretDraw, baseColor * 1.18f, false, true});
    frame.boxes.push_back({pose.barrelDraw, baseColor * 0.82f, false, true});

    if (selectedHighlight)
    {
        const glm::vec3 focusColor{0.18f, 0.95f, 1.0f};
        frame.boxes.push_back({pose.bodyDraw, focusColor, true});
        frame.boxes.push_back({pose.turretDraw, focusColor, true});
        const glm::vec3 top = pose.turretOBB.center + glm::vec3{0.0f, 0.55f, 0.0f};
        frame.lines.push_back({top, top + glm::vec3{0.0f, 2.2f, 0.0f}, focusColor});
        frame.lines.push_back({top + glm::vec3{-0.45f, 1.8f, 0.0f}, top + glm::vec3{0.45f, 1.8f, 0.0f}, focusColor});
        frame.lines.push_back({top + glm::vec3{0.0f, 1.8f, -0.45f}, top + glm::vec3{0.0f, 1.8f, 0.45f}, focusColor});
    }

    if (!drawDebug)
        return;

    if (!selectedHighlight)
    {
        const glm::vec3 obbWireColor{0.95f, 0.90f, 0.25f};
        frame.boxes.push_back({pose.bodyDraw, obbWireColor, true});
        frame.boxes.push_back({pose.turretDraw, obbWireColor, true});
        frame.boxes.push_back({pose.barrelDraw, obbWireColor, true});
    }

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

    // Jump ramp aligned toward -Z on the right lane.
    ramp_ = RampSurface{};
    {
        const float run = ramp_.startZ - ramp_.endZ;
        const float rise = ramp_.topHeight;
        const float deckLength = std::sqrt(run * run + rise * rise);
        const float slope = ramp_.slopePitchRad();
        const glm::mat4 deckRot = glm::rotate(glm::mat4{1.0f}, slope, glm::vec3{1.0f, 0.0f, 0.0f});
        const glm::mat3 deckAxes = glm::mat3(deckRot);
        const float halfThick = 0.14f;
        const glm::vec3 topMid{
            (ramp_.minX + ramp_.maxX) * 0.5f,
            rise * 0.5f,
            (ramp_.startZ + ramp_.endZ) * 0.5f};

        rampDeckBox_.center = topMid - deckAxes[1] * halfThick;
        rampDeckBox_.axes = deckAxes;
        rampDeckBox_.halfExtents = glm::vec3{(ramp_.maxX - ramp_.minX) * 0.5f, halfThick, deckLength * 0.5f};

        rampSupportBox_.center = glm::vec3{(ramp_.minX + ramp_.maxX) * 0.5f, rise * 0.42f, ramp_.endZ + 0.85f};
        rampSupportBox_.axes = glm::mat3{1.0f};
        rampSupportBox_.halfExtents = glm::vec3{(ramp_.maxX - ramp_.minX) * 0.48f, rise * 0.42f, 0.85f};
    }

    // Opaque barricade partially occluding the enemy from spawn for QA-08 focus testing.
    barricadeBox_ = OBB{};
    barricadeBox_.center = glm::vec3{-1.25f, 0.95f, -6.2f};
    barricadeBox_.axes = glm::mat3{1.0f};
    barricadeBox_.halfExtents = glm::vec3{1.25f, 0.95f, 0.35f};

    nextEntityId_ = 1;
    const EntityId playerId = nextEntityId_++;
    const TankSpec tankSpec = makeSandTankSpec();
    player_.reset(playerId, glm::vec3{0.0f, 0.0f, 10.0f}, 0.0f, tankSpec);

    enemies_.clear();
    EnemyEntity target{};
    target.id = nextEntityId_++;
    target.tank.reset(target.id, glm::vec3{0.0f, 0.0f, -10.0f}, glm::pi<float>(), tankSpec);
    enemies_.push_back(target);

    projectiles_.clear();
    frameHits_.clear();
    trickshot_.reset();
    totalTargetHits_ = 0;
    displayedColliders_ = gatherColliders(false, 0.0f);

    std::println(
        "[CP02] Reset | Player(ID={}) HP={}/{} | Target(ID={}) HP={}/{} | Focus=None | Projectiles=0",
        player_.id(),
        player_.hp(),
        player_.maxHp(),
        enemies_.front().id,
        enemies_.front().tank.hp(),
        enemies_.front().tank.maxHp());
}

void ArenaScene::updateRealTime(
    float rawDt,
    const glm::vec2& rawMouseDelta,
    const glm::vec3& mainCamPos,
    const glm::vec3& mainCamLookAt,
    float mainCamFovDeg)
{
    const EnemyEntity* selected = findAliveEnemy(trickshot_.selectedTargetId());
    const bool targetAlive = (selected != nullptr);
    const glm::vec3 targetPos = targetAlive ? selected->tank.position() : glm::vec3{0.0f};

    trickshot_.update(
        rawDt,
        rawMouseDelta,
        player_.isAirborne(),
        targetAlive,
        mainCamPos,
        mainCamLookAt,
        mainCamFovDeg,
        player_.position(),
        targetPos);
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

    if (cmd.despawnSelectedTargetRequested)
    {
        if (EnemyEntity* selected = findAliveEnemy(trickshot_.selectedTargetId()))
        {
            selected->tank.applyDamage(selected->tank.maxHp());
            std::println("[CP02] QA-12 test | Selected Target(ID={}) removed/destroyed", selected->id);
        }
    }

    if (cmd.selectTargetRequested)
        trickshot_.selectTarget(cmd.cursorPos, view, displayedColliders_, player_.id());

    player_.beginTick(dt);
    for (EnemyEntity& enemy : enemies_)
        enemy.tank.beginTick(dt);

    const OBB solidObstacles[1]{barricadeBox_};
    player_.applyDrive(cmd.moveForward, cmd.moveRight, dt, &ramp_, solidObstacles, kFloorHalfWidth - 2.0f);

    if (player_.tookOffThisTick())
        trickshot_.onTakeoff();

    const EnemyEntity* selectedTarget = findAliveEnemy(trickshot_.selectedTargetId());
    if (player_.isAirborne() && selectedTarget != nullptr)
    {
        trickshot_.tryEnter(
            true,
            player_.heightAboveGround(&ramp_),
            true,
            player_.position(),
            selectedTarget->tank.position());
    }

    if (trickshot_.isInFocusControl() && selectedTarget != nullptr)
    {
        const glm::vec3 aimOrigin = player_.position() + glm::vec3{0.0f, 1.1f, 0.0f};
        const glm::vec3 targetCenter = selectedTarget->tank.position() + glm::vec3{0.0f, 0.9f, 0.0f};
        player_.aimAt(trickshot_.computeAimWorldTarget(aimOrigin, targetCenter));
    }
    else if (cmd.manualTurretTurn != 0.0f || cmd.manualBarrelPitch != 0.0f)
    {
        player_.adjustManualAim(cmd.manualTurretTurn, cmd.manualBarrelPitch, dt);
    }
    else if (cmd.turretAimActive && view.valid)
    {
        const glm::vec3 aimPoint = computeMouseAimPoint(cmd.cursorPos, view);
        player_.aimAt(aimPoint);
    }

    if (cmd.fireRequested)
    {
        if (trickshot_.isInFocusControl())
        {
            if (trickshot_.consumeShotOpportunity())
            {
                Projectile spawned{};
                if (player_.tryFire(spawned, true))
                {
                    projectiles_.push_back(spawned);
                    std::println(
                        "[CP02] Trickshot Fire | Owner={} Origin=({:.2f}, {:.2f}, {:.2f}) Vel=({:.2f}, {:.2f}, {:.2f})",
                        spawned.ownerId,
                        spawned.pos.x,
                        spawned.pos.y,
                        spawned.pos.z,
                        spawned.velocity.x,
                        spawned.velocity.y,
                        spawned.velocity.z);
                }
            }
        }
        else if (trickshot_.canNormalFire())
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
    }

    stepProjectiles(dt);
    return false;
}

EnemyEntity* ArenaScene::findAliveEnemy(EntityId id)
{
    if (id == kInvalidEntityId)
        return nullptr;
    for (EnemyEntity& enemy : enemies_)
    {
        if (enemy.id == id && enemy.tank.alive())
            return &enemy;
    }
    return nullptr;
}

const EnemyEntity* ArenaScene::findAliveEnemy(EntityId id) const
{
    if (id == kInvalidEntityId)
        return nullptr;
    for (const EnemyEntity& enemy : enemies_)
    {
        if (enemy.id == id && enemy.tank.alive())
            return &enemy;
    }
    return nullptr;
}

std::vector<TraceCollider> ArenaScene::gatherColliders(bool interpolated, float alpha) const
{
    std::vector<TraceCollider> colliders{};
    colliders.reserve(4 + (1 + enemies_.size()) * 3);

    colliders.push_back({kInvalidEntityId, PartKind::Terrain, floorBox_});
    colliders.push_back({kInvalidEntityId, PartKind::Terrain, rampDeckBox_});
    colliders.push_back({kInvalidEntityId, PartKind::Terrain, rampSupportBox_});
    colliders.push_back({kInvalidEntityId, PartKind::Terrain, barricadeBox_});

    if (player_.alive())
    {
        const TankPose p = interpolated ? player_.evaluatePose(alpha) : player_.evaluatePose();
        colliders.push_back({player_.id(), PartKind::Body, p.bodyOBB});
        colliders.push_back({player_.id(), PartKind::Turret, p.turretOBB});
        colliders.push_back({player_.id(), PartKind::Barrel, p.barrelOBB});
    }

    for (const EnemyEntity& enemy : enemies_)
    {
        if (!enemy.tank.alive())
            continue;

        const TankPose e = interpolated ? enemy.tank.evaluatePose(alpha) : enemy.tank.evaluatePose();
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

    const std::vector<TraceCollider> colliders = gatherColliders(false, 0.0f);
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

    const std::vector<TraceCollider> colliders = gatherColliders(false, 0.0f);

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

PreviewFrame ArenaScene::buildFrame(float alpha)
{
    // Cache displayed colliders matching the rendered poses for CP02-FOCUS right-click selection.
    displayedColliders_ = gatherColliders(true, alpha);

    PreviewFrame frame{};
    frame.debugOverlay = debugOverlay_;
    frame.playerPos = player_.position();
    frame.playerAirborne = player_.isAirborne();
    frame.playerHeight = player_.heightAboveGround(&ramp_);
    frame.playerHp = player_.hp();
    if (!enemies_.empty())
    {
        frame.targetHp = enemies_.front().tank.hp();
        frame.targetMaxHp = enemies_.front().tank.maxHp();
    }
    frame.totalTargetHits = totalTargetHits_;
    frame.activeProjectiles = projectiles_.size();
    frame.cameraRequest = trickshot_.cameraRequest();
    frame.trickshot = trickshot_.hudState();

    frame.boxes.push_back({makeOBBDrawMatrix(floorBox_), glm::vec3{0.22f, 0.25f, 0.29f}, false});
    frame.boxes.push_back({makeOBBDrawMatrix(rampDeckBox_), glm::vec3{0.76f, 0.56f, 0.22f}, false});
    frame.boxes.push_back({makeOBBDrawMatrix(rampSupportBox_), glm::vec3{0.50f, 0.37f, 0.16f}, false});
    frame.boxes.push_back({makeOBBDrawMatrix(barricadeBox_), glm::vec3{0.52f, 0.55f, 0.60f}, false});

    if (debugOverlay_)
    {
        frame.boxes.push_back({makeOBBDrawMatrix(rampDeckBox_), glm::vec3{1.0f, 0.85f, 0.35f}, true});
        frame.boxes.push_back({makeOBBDrawMatrix(barricadeBox_), glm::vec3{0.85f, 0.88f, 0.92f}, true});

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
        false,
        glm::vec3{1.0f});

    for (const EnemyEntity& enemy : enemies_)
    {
        const float hpRatio = (enemy.tank.maxHp() > 0)
            ? static_cast<float>(enemy.tank.hp()) / static_cast<float>(enemy.tank.maxHp())
            : 0.0f;
        const glm::vec3 enemyColor = enemy.tank.alive()
            ? glm::mix(glm::vec3{0.95f, 0.55f, 0.18f}, glm::vec3{0.82f, 0.24f, 0.24f}, hpRatio)
            : glm::vec3{0.32f, 0.32f, 0.34f};
        const bool isSelected = enemy.tank.alive() && (enemy.id == trickshot_.selectedTargetId());

        appendTankPreview(
            frame,
            enemy.tank.evaluatePose(alpha),
            enemyColor,
            debugOverlay_ && enemy.tank.alive(),
            false,
            isSelected,
            enemyColor);

        if (isSelected && debugOverlay_)
        {
            appendGroundCircle(
                frame.lines,
                enemy.tank.position(),
                trickshot_.config().maxRadiusR,
                glm::vec3{0.18f, 0.90f, 0.95f});
        }
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

void runCp02UnitChecks()
{
    std::println("=== [CP02 QA Unit Checks] ===");

    // QA-09: Height H and Radius R boundary checks for TrickshotController::canEnter
    {
        TrickshotController ctrl{};
        TrickshotConfig cfg{};
        cfg.minHeightH = 2.0f;
        cfg.maxRadiusR = 10.0f;
        ctrl.reset(cfg);
        ctrl.setSelectedTargetForTest(2);

        const glm::vec3 targetPos{0.0f, 0.0f, 0.0f};
        const bool atBoundary = ctrl.canEnter(true, cfg.minHeightH, true, glm::vec3{cfg.maxRadiusR, cfg.minHeightH, 0.0f}, targetPos);
        const bool outsideRadius = ctrl.canEnter(true, cfg.minHeightH, true, glm::vec3{cfg.maxRadiusR + 0.01f, cfg.minHeightH, 0.0f}, targetPos);
        const bool belowHeight = ctrl.canEnter(true, cfg.minHeightH - 0.01f, true, glm::vec3{cfg.maxRadiusR, cfg.minHeightH - 0.01f, 0.0f}, targetPos);

        const bool pass = atBoundary && !outsideRadius && !belowHeight;
        std::println(
            "[QA-09] {} | (H,R)={} (H,R+eps)={} (H-eps,R)={}",
            pass ? "PASS" : "FAIL",
            atBoundary,
            outsideRadius,
            belowHeight);
    }
}
} // namespace game
