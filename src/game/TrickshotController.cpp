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
    zoomSource_ = ZoomTriggerSource::None;
    selectedTargetId_ = kInvalidEntityId;
    mildAirborneSlowActive_ = false;
    usedThisAirborne_ = false;
    showClickIcon_ = false;
    transitionT_ = 0.0f;
    lmbHoldSec_ = 0.0f;
    timeScale_ = 1.0f;
    aimYawOffset_ = 0.0f;
    aimPitchOffset_ = 0.0f;
    freeZoomBaseDir_ = glm::vec3{0.0f, 0.0f, -1.0f};
    camTrackInitialized_ = false;
    subCamInitialized_ = false;
    cameraRequest_ = CameraRequest{};
}

void TrickshotController::onTakeoff(bool enableMildSlow)
{
    usedThisAirborne_ = false;
    mildAirborneSlowActive_ = enableMildSlow;
    if (!isZoomActive())
        timeScale_ = mildAirborneSlowActive_ ? config_.airborneMildSlowScale : 1.0f;
}

void TrickshotController::toggleMildAirborneSlow()
{
    mildAirborneSlowActive_ = !mildAirborneSlowActive_;
    if (!isZoomActive())
        timeScale_ = mildAirborneSlowActive_ ? config_.airborneMildSlowScale : 1.0f;
}

float TrickshotController::nonFocusTimeScale(bool isAirborne) const
{
    return (isAirborne && mildAirborneSlowActive_) ? config_.airborneMildSlowScale : 1.0f;
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

    if (hit.hit && hit.entityId != kInvalidEntityId && hit.part != PartKind::Terrain && hit.part != PartKind::Prop)
    {
        selectedTargetId_ = hit.entityId;
        std::println("[CP02] Focus selected | Target ID={}", selectedTargetId_);
    }
    else
    {
        if (selectedTargetId_ != kInvalidEntityId)
            std::println("[CP02] Focus cleared (occluded or empty)");
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

void TrickshotController::beginZoom(ZoomTriggerSource source, const glm::vec3& initialAimDir)
{
    const float len = glm::length(initialAimDir);
    freeZoomBaseDir_ = (len > 1e-4f) ? (initialAimDir / len) : glm::vec3{0.0f, 0.0f, -1.0f};
    if (state_ == TrickshotState::Idle)
        subCamInitialized_ = false;
    zoomSource_ = source;
    state_ = TrickshotState::TransitionIn;
    timeScale_ = config_.slowTimeScale;
    showClickIcon_ = false;
    aimYawOffset_ = 0.0f;
    aimPitchOffset_ = 0.0f;

    std::println(
        "[CP02] Zoom ENTER ({}) | Target={} | TimeScale={:.2f}x",
        source == ZoomTriggerSource::RightMouse ? "RMB" : "LMB-Hold",
        selectedTargetId_,
        timeScale_);
}

void TrickshotController::endZoom(const char* reason, bool isAirborne)
{
    if (isZoomActive())
    {
        std::println(
            "[CP02] Zoom EXIT ({}) | TimeScale={:.2f}x",
            reason,
            nonFocusTimeScale(isAirborne));
    }

    zoomSource_ = ZoomTriggerSource::None;
    showClickIcon_ = false;
    timeScale_ = nonFocusTimeScale(isAirborne);
    if (transitionT_ > 0.0f)
        state_ = TrickshotState::TransitionOut;
    else
        state_ = TrickshotState::Idle;
}

void TrickshotController::update(
    float rawDt,
    const glm::vec2& rawMouseDelta,
    bool rmbHeld,
    bool lmbHeld,
    bool isAirborne,
    bool targetAlive,
    const glm::vec3& mainCamPos,
    const glm::vec3& mainCamLookAt,
    float mainCamFovDeg,
    const glm::vec3& playerPos,
    const glm::vec3& fallbackForward,
    const glm::vec3& targetPos)
{
    const float dt = std::clamp(rawDt, 0.0f, 0.1f);

    if (!isAirborne)
        mildAirborneSlowActive_ = false;

    if (selectedTargetId_ != kInvalidEntityId && !targetAlive)
    {
        selectedTargetId_ = kInvalidEntityId;
        if (isZoomActive())
            endZoom("Target Lost", isAirborne);
    }

    // LMB hold >= 1.2s triggers zoom + slow-mo; releasing LMB exits zoom & slow.
    if (lmbHeld && !isRightMouseZoom())
    {
        lmbHoldSec_ += dt;
        if (!isZoomActive() && lmbHoldSec_ >= config_.lmbZoomHoldThresholdSec)
            beginZoom(ZoomTriggerSource::LeftHold, fallbackForward);
    }
    else
    {
        if (isLeftHoldZoom() && !lmbHeld)
            endZoom("LMB Released", isAirborne);
        if (!lmbHeld)
            lmbHoldSec_ = 0.0f;
    }

    // Releasing RMB immediately exits RMB zoom & slow.
    if (isRightMouseZoom() && !rmbHeld)
        endZoom("RMB Released", isAirborne);

    if (isZoomActive())
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
        timeScale_ = nonFocusTimeScale(isAirborne);
        showClickIcon_ = false;
        transitionT_ = 0.0f;
        break;
    case TrickshotState::TransitionIn:
        timeScale_ = config_.slowTimeScale;
        transitionT_ = std::min(1.0f, transitionT_ + dt / std::max(config_.transitionInDurationSec, 1e-4f));
        if (transitionT_ >= 1.0f)
        {
            state_ = TrickshotState::AimReady;
            showClickIcon_ = true;
        }
        break;
    case TrickshotState::AimReady:
        timeScale_ = config_.slowTimeScale;
        showClickIcon_ = true;
        transitionT_ = 1.0f;
        break;
    case TrickshotState::TransitionOut:
        timeScale_ = nonFocusTimeScale(isAirborne);
        showClickIcon_ = false;
        transitionT_ = std::max(0.0f, transitionT_ - dt / std::max(config_.transitionOutDurationSec, 1e-4f));
        if (transitionT_ <= 0.0f)
            state_ = TrickshotState::Idle;
        break;
    }

    if (!camTrackInitialized_ || state_ == TrickshotState::Idle)
    {
        camTrackPos_ = playerPos;
        camTrackInitialized_ = true;
        if (state_ == TrickshotState::Idle)
            subCamInitialized_ = false;
    }
    else
    {
        const float horizBlend = 1.0f - std::exp(-3.0f * dt);
        const float vertBlend = 1.0f - std::exp(-2.6f * dt);
        camTrackPos_.x = glm::mix(camTrackPos_.x, playerPos.x, horizBlend);
        camTrackPos_.z = glm::mix(camTrackPos_.z, playerPos.z, horizBlend);
        camTrackPos_.y = glm::mix(camTrackPos_.y, playerPos.y, vertBlend);
    }

    if (isZoomActive())
    {
        const glm::vec3 aimOrigin = playerPos + glm::vec3{0.0f, 1.35f, 0.0f};
        const bool hasTarget = (selectedTargetId_ != kInvalidEntityId && targetAlive);
        const glm::vec3 targetCenter = targetPos + glm::vec3{0.0f, 0.9f, 0.0f};
        const glm::vec3 aimPoint = computeAimWorldTarget(aimOrigin, hasTarget, targetCenter);
        glm::vec3 aimDir = aimPoint - aimOrigin;
        const float aimLen = glm::length(aimDir);
        aimDir = (aimLen > 1e-4f) ? (aimDir / aimLen) : glm::vec3{0.0f, 0.0f, -1.0f};

        const glm::vec3 camAnchor = camTrackPos_ + glm::vec3{0.0f, 1.35f, 0.0f};
        const glm::vec3 desiredSubPos = camAnchor - aimDir * 5.6f + glm::vec3{0.0f, 1.65f, 0.0f};
        const glm::vec3 desiredSubLook = aimPoint;

        const float smoothCam = 1.0f - std::exp(-5.8f * dt);
        if (!subCamInitialized_)
        {
            lastSubCamPos_ = desiredSubPos;
            lastSubCamLookAt_ = desiredSubLook;
            subCamInitialized_ = true;
        }
        else
        {
            lastSubCamPos_ = glm::mix(lastSubCamPos_, desiredSubPos, smoothCam);
            lastSubCamLookAt_ = glm::mix(lastSubCamLookAt_, desiredSubLook, smoothCam);
        }
        lastSubCamPos_.y = std::max(lastSubCamPos_.y, 1.35f);
        lastZoomFocusPoint_ = hasTarget ? targetCenter : aimPoint;
    }

    const float t = std::clamp(transitionT_, 0.0f, 1.0f);
    const float smoothT = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
    cameraRequest_.blendAlpha = smoothT;
    cameraRequest_.active = (transitionT_ > 0.0f);
    cameraRequest_.position = glm::mix(mainCamPos, lastSubCamPos_, smoothT);
    cameraRequest_.lookAt = glm::mix(mainCamLookAt, lastSubCamLookAt_, smoothT);
    cameraRequest_.fovDeg = glm::mix(mainCamFovDeg, config_.subFovDeg, smoothT);
}

glm::vec3 TrickshotController::computeAimWorldTarget(
    const glm::vec3& originPos,
    bool hasTarget,
    const glm::vec3& targetCenter) const
{
    glm::vec3 baseDir = freeZoomBaseDir_;
    float dist = 24.0f;
    if (hasTarget)
    {
        const glm::vec3 diff = targetCenter - originPos;
        dist = std::max(glm::length(diff), 1.0f);
        baseDir = diff / dist;
    }

    const float baseYaw = std::atan2(-baseDir.x, -baseDir.z);
    const float horiz = std::sqrt(baseDir.x * baseDir.x + baseDir.z * baseDir.z);
    const float basePitch = std::atan2(baseDir.y, std::max(horiz, 1e-4f));

    const float yaw = baseYaw + aimYawOffset_;
    const float pitch = std::clamp(basePitch + aimPitchOffset_, -1.1f, 1.1f);
    const float cosPitch = std::cos(pitch);
    const glm::vec3 aimedDir{
        -std::sin(yaw) * cosPitch,
        std::sin(pitch),
        -std::cos(yaw) * cosPitch};

    return originPos + aimedDir * dist;
}

TrickshotHudState TrickshotController::hudState() const
{
    TrickshotHudState hud{};
    hud.state = state_;
    hud.selectedTargetId = selectedTargetId_;
    hud.showClickIcon = showClickIcon_;
    hud.timeScale = timeScale_;
    hud.aimYawOffsetRad = aimYawOffset_;
    hud.aimPitchOffsetRad = aimPitchOffset_;
    hud.maxAimYawRad = config_.maxAimYawRad;
    hud.maxAimPitchRad = config_.maxAimPitchRad;
    return hud;
}
} // namespace game
