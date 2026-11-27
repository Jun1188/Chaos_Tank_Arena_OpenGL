#include "game/TrickshotController.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <print>

namespace game
{
void TrickshotController::reset(const TrickshotConfig& config)
{
    config_ = config;
    state_ = TrickshotState::Idle;
    selectedTargetId_ = kInvalidEntityId;
    usedThisAirborne_ = false;
    shotFiredThisAirborne_ = false;
    showClickIcon_ = false;
    transitionT_ = 0.0f;
    timeScale_ = 1.0f;
    aimYawOffset_ = 0.0f;
    aimPitchOffset_ = 0.0f;
    cameraRequest_ = CameraRequest{};
}

void TrickshotController::onTakeoff()
{
    usedThisAirborne_ = false;
    shotFiredThisAirborne_ = false;
}

EntityId TrickshotController::selectTarget(
    const glm::vec2& cursorPos,
    const PresentedView& view,
    std::span<const TraceCollider> displayedColliders,
    EntityId playerId)
{
    if (!view.valid)
    {
        selectedTargetId_ = kInvalidEntityId;
        return selectedTargetId_;
    }

    const float scaleX = (view.windowSize.x > 1.0f) ? (static_cast<float>(view.viewport.z) / view.windowSize.x) : 1.0f;
    const float scaleY = (view.windowSize.y > 1.0f) ? (static_cast<float>(view.viewport.w) / view.windowSize.y) : 1.0f;
    const float fbX = static_cast<float>(view.viewport.x) + cursorPos.x * scaleX;
    const float fbY = static_cast<float>(view.viewport.y) + (view.windowSize.y - cursorPos.y) * scaleY;
    const glm::vec4 vp{
        static_cast<float>(view.viewport.x),
        static_cast<float>(view.viewport.y),
        static_cast<float>(view.viewport.z),
        static_cast<float>(view.viewport.w)};

    const glm::vec3 nearPt = glm::unProject(glm::vec3{fbX, fbY, 0.0f}, view.view, view.projection, vp);
    const glm::vec3 farPt = glm::unProject(glm::vec3{fbX, fbY, 1.0f}, view.view, view.projection, vp);
    const glm::vec3 diff = farPt - nearPt;
    const float lenSq = glm::dot(diff, diff);
    if (lenSq < 1e-8f)
    {
        selectedTargetId_ = kInvalidEntityId;
        return selectedTargetId_;
    }

    const glm::vec3 rayDir = diff / std::sqrt(lenSq);
    const SegmentHit hit = Physics::traceSegment(nearPt, nearPt + rayDir * 250.0f, displayedColliders, playerId);

    if (hit.hit && hit.entityId != kInvalidEntityId && hit.part != PartKind::Terrain)
    {
        selectedTargetId_ = hit.entityId;
        std::println("[CP02] Focus selected | Target ID={}", selectedTargetId_);
    }
    else
    {
        if (selectedTargetId_ != kInvalidEntityId)
            std::println("[CP02] Focus cleared (occluded or empty)");
        else
            std::println("[CP02] Focus none (occluded or empty)");
        selectedTargetId_ = kInvalidEntityId;
    }
    return selectedTargetId_;
}

bool TrickshotController::canEnter(
    bool isAirborne,
    float heightAboveGround,
    bool targetAlive,
    const glm::vec3& playerPos,
    const glm::vec3& targetPos) const
{
    if (!isAirborne || usedThisAirborne_ || selectedTargetId_ == kInvalidEntityId || !targetAlive)
        return false;

    if (heightAboveGround < config_.minHeightH)
        return false;

    const float dx = targetPos.x - playerPos.x;
    const float dz = targetPos.z - playerPos.z;
    return (dx * dx + dz * dz) <= (config_.maxRadiusR * config_.maxRadiusR);
}

bool TrickshotController::tryEnter(
    bool isAirborne,
    float heightAboveGround,
    bool targetAlive,
    const glm::vec3& playerPos,
    const glm::vec3& targetPos)
{
    if (isInFocusControl())
        return false;

    if (!canEnter(isAirborne, heightAboveGround, targetAlive, playerPos, targetPos))
        return false;

    state_ = TrickshotState::TransitionIn;
    usedThisAirborne_ = true;
    shotFiredThisAirborne_ = false;
    showClickIcon_ = false;
    transitionT_ = 0.0f;
    timeScale_ = config_.slowTimeScale;
    aimYawOffset_ = 0.0f;
    aimPitchOffset_ = 0.0f;

    std::println(
        "[CP02] Trickshot ENTER | Target ID={} | Height={:.2f}m | TimeScale={:.2f}x",
        selectedTargetId_,
        heightAboveGround,
        timeScale_);
    return true;
}

bool TrickshotController::enterImmediate(bool targetAlive)
{
    if (selectedTargetId_ == kInvalidEntityId || !targetAlive)
        return false;

    state_ = TrickshotState::TransitionIn;
    usedThisAirborne_ = true;
    shotFiredThisAirborne_ = false;
    showClickIcon_ = false;
    transitionT_ = 0.0f;
    timeScale_ = config_.slowTimeScale;
    aimYawOffset_ = 0.0f;
    aimPitchOffset_ = 0.0f;

    std::println(
        "[CP02] Explosion Trickshot ENTER (B) | Target ID={} | TimeScale={:.2f}x",
        selectedTargetId_,
        timeScale_);
    return true;
}

void TrickshotController::cancelImmediate(const char* reason)
{
    exitToMain(reason);
}

void TrickshotController::exitToMain(const char* reason)
{
    if (isInFocusControl())
        std::println("[CP02] Trickshot EXIT ({}) | TimeScale=1.00x | ClickIcon=OFF", reason);

    showClickIcon_ = false;
    timeScale_ = 1.0f;
    if (transitionT_ > 0.0f)
        state_ = TrickshotState::TransitionOut;
    else
        state_ = TrickshotState::Idle;
}

void TrickshotController::update(
    float rawDt,
    const glm::vec2& rawMouseDelta,
    bool isAirborne,
    bool targetAlive,
    const glm::vec3& mainCamPos,
    const glm::vec3& mainCamLookAt,
    float mainCamFovDeg,
    const glm::vec3& playerPos,
    const glm::vec3& targetPos)
{
    const float dt = std::clamp(rawDt, 0.0f, 0.1f);

    if (selectedTargetId_ != kInvalidEntityId && !targetAlive)
    {
        selectedTargetId_ = kInvalidEntityId;
        if (isInFocusControl())
            exitToMain("Target Lost");
    }

    if (isInFocusControl() && !isAirborne)
        exitToMain("Landed");

    if (state_ == TrickshotState::TransitionIn || state_ == TrickshotState::AimReady)
    {
        aimYawOffset_ = std::clamp(
            aimYawOffset_ - rawMouseDelta.x * config_.aimSensitivityRadPerPx,
            -config_.maxAimYawRad,
            config_.maxAimYawRad);
        aimPitchOffset_ = std::clamp(
            aimPitchOffset_ - rawMouseDelta.y * config_.aimSensitivityRadPerPx,
            -config_.maxAimPitchRad,
            config_.maxAimPitchRad);
    }

    switch (state_)
    {
    case TrickshotState::Idle:
        timeScale_ = 1.0f;
        showClickIcon_ = false;
        transitionT_ = 0.0f;
        break;
    case TrickshotState::TransitionIn:
        timeScale_ = config_.slowTimeScale;
        showClickIcon_ = false;
        transitionT_ = std::min(1.0f, transitionT_ + dt / std::max(config_.transitionInDurationSec, 1e-4f));
        if (transitionT_ >= 1.0f)
        {
            state_ = TrickshotState::AimReady;
            showClickIcon_ = true;
            std::println("[CP02] Trickshot AIM_READY | ClickIcon=ON");
        }
        break;
    case TrickshotState::AimReady:
        timeScale_ = config_.slowTimeScale;
        showClickIcon_ = true;
        transitionT_ = 1.0f;
        break;
    case TrickshotState::ShotFired:
        timeScale_ = config_.slowTimeScale;
        showClickIcon_ = false;
        transitionT_ = 1.0f;
        break;
    case TrickshotState::TransitionOut:
        timeScale_ = 1.0f;
        showClickIcon_ = false;
        transitionT_ = std::max(0.0f, transitionT_ - dt / std::max(config_.transitionOutDurationSec, 1e-4f));
        if (transitionT_ <= 0.0f)
            state_ = TrickshotState::Idle;
        break;
    }

    if (isInFocusControl() && targetAlive)
    {
        const glm::vec3 aimOrigin = playerPos + glm::vec3{0.0f, 1.45f, 0.0f};
        const glm::vec3 targetCenter = targetPos + glm::vec3{0.0f, 0.9f, 0.0f};
        const glm::vec3 aimPoint = computeAimWorldTarget(aimOrigin, targetCenter);
        glm::vec3 aimDir = aimPoint - aimOrigin;
        const float aimLen = glm::length(aimDir);
        aimDir = (aimLen > 1e-4f) ? (aimDir / aimLen) : glm::vec3{0.0f, 0.0f, -1.0f};

        // Stabilized world-horizon near-1st-person back-view synced with barrel aim; ignores hull spin.
        lastSubCamPos_ = aimOrigin - aimDir * 3.0f + glm::vec3{0.0f, 0.65f, 0.0f};
        lastSubCamPos_.y = std::max(lastSubCamPos_.y, 1.15f);
        lastSubCamLookAt_ = aimPoint;
    }

    const float smoothT = transitionT_ * transitionT_ * (3.0f - 2.0f * transitionT_);
    cameraRequest_.blendAlpha = smoothT;
    cameraRequest_.active = (transitionT_ > 0.0f);
    cameraRequest_.position = glm::mix(mainCamPos, lastSubCamPos_, smoothT);
    cameraRequest_.lookAt = glm::mix(mainCamLookAt, lastSubCamLookAt_, smoothT);
    cameraRequest_.fovDeg = glm::mix(mainCamFovDeg, config_.subFovDeg, smoothT);
}

glm::vec3 TrickshotController::computeAimWorldTarget(const glm::vec3& originPos, const glm::vec3& targetCenter) const
{
    const glm::vec3 diff = targetCenter - originPos;
    const float dist = std::max(glm::length(diff), 1.0f);
    const glm::vec3 dir = diff / dist;

    const float baseYaw = std::atan2(-dir.x, -dir.z);
    const float horiz = std::sqrt(dir.x * dir.x + dir.z * dir.z);
    const float basePitch = std::atan2(dir.y, std::max(horiz, 1e-4f));

    const float yaw = baseYaw + aimYawOffset_;
    const float pitch = std::clamp(basePitch + aimPitchOffset_, -1.2f, 1.2f);
    const float cosPitch = std::cos(pitch);
    const glm::vec3 aimedDir{
        -std::sin(yaw) * cosPitch,
        std::sin(pitch),
        -std::cos(yaw) * cosPitch};

    return originPos + aimedDir * dist;
}

bool TrickshotController::consumeShotOpportunity()
{
    if (state_ != TrickshotState::AimReady || shotFiredThisAirborne_)
        return false;

    shotFiredThisAirborne_ = true;
    showClickIcon_ = false;
    state_ = TrickshotState::ShotFired;
    std::println("[CP02] Trickshot SHOT FIRED | ClickIcon=OFF | Further trickshot firing locked");
    return true;
}

TrickshotHudState TrickshotController::hudState() const
{
    TrickshotHudState hud{};
    hud.state = state_;
    hud.selectedTargetId = selectedTargetId_;
    hud.showClickIcon = showClickIcon_;
    hud.shotFiredThisAirborne = shotFiredThisAirborne_;
    hud.timeScale = timeScale_;
    hud.aimYawOffsetRad = aimYawOffset_;
    hud.aimPitchOffsetRad = aimPitchOffset_;
    hud.maxAimYawRad = config_.maxAimYawRad;
    hud.maxAimPitchRad = config_.maxAimPitchRad;
    return hud;
}
} // namespace game
