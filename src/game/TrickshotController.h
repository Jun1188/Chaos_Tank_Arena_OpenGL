#pragma once

#include "game/Physics.h"

#include <glm/glm.hpp>
#include <span>

namespace game
{
struct PresentedView
{
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::ivec4 viewport{0, 0, 1280, 720};
    glm::vec2 windowSize{1280.0f, 720.0f};
    bool valid{false};
};

enum class TrickshotState : std::uint8_t
{
    Idle = 0,
    TransitionIn,
    AimReady,
    TransitionOut,
};

enum class ZoomTriggerSource : std::uint8_t
{
    None = 0,
    RightMouse,
    LeftHold,
};

struct CameraRequest
{
    bool active{false};
    glm::vec3 position{0.0f, 13.0f, 19.0f};
    glm::vec3 lookAt{0.0f, 0.8f, 0.0f};
    float fovDeg{45.0f};
    float blendAlpha{0.0f};
};

struct TrickshotHudState
{
    TrickshotState state{TrickshotState::Idle};
    EntityId selectedTargetId{kInvalidEntityId};
    bool showClickIcon{false};
    float timeScale{1.0f};
    float aimYawOffsetRad{0.0f};
    float aimPitchOffsetRad{0.0f};
    float maxAimYawRad{0.42f};
    float maxAimPitchRad{0.32f};
};

struct TrickshotConfig
{
    float minHeightH{1.6f};
    float maxRadiusR{12.5f};
    float airborneMildSlowScale{0.55f};
    float slowTimeScale{0.32f};
    float lmbZoomHoldThresholdSec{1.2f};
    float transitionInDurationSec{0.34f};
    float transitionOutDurationSec{0.32f};
    float subFovDeg{34.0f};
    float maxAimYawRad{0.42f};
    float maxAimPitchRad{0.32f};
    float aimSensitivityRadPerPx{0.0028f};
};

class TrickshotController
{
public:
    void reset(const TrickshotConfig& config = {});
    void onTakeoff(bool enableMildSlow = false);
    void toggleMildAirborneSlow();
    bool mildAirborneSlowActive() const { return mildAirborneSlowActive_; }

    EntityId selectTarget(
        const glm::vec2& cursorPos,
        const PresentedView& view,
        std::span<const TraceCollider> displayedColliders,
        EntityId playerId);

    void setSelectedTargetForTest(EntityId targetId) { selectedTargetId_ = targetId; }

    bool canEnter(
        bool isAirborne,
        float heightAboveGround,
        bool targetAlive,
        const glm::vec3& playerPos,
        const glm::vec3& targetPos) const;

    void beginZoom(ZoomTriggerSource source, const glm::vec3& initialAimDir);
    void endZoom(const char* reason, bool isAirborne);

    void update(
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
        const glm::vec3& targetPos);

    glm::vec3 computeAimWorldTarget(const glm::vec3& originPos, bool hasTarget, const glm::vec3& targetCenter) const;

    bool isZoomActive() const
    {
        return state_ == TrickshotState::TransitionIn || state_ == TrickshotState::AimReady;
    }
    bool isRightMouseZoom() const { return isZoomActive() && zoomSource_ == ZoomTriggerSource::RightMouse; }
    bool isLeftHoldZoom() const { return isZoomActive() && zoomSource_ == ZoomTriggerSource::LeftHold; }
    bool lmbHoldReachedThreshold() const { return lmbHoldSec_ >= config_.lmbZoomHoldThresholdSec; }

    EntityId selectedTargetId() const { return selectedTargetId_; }
    TrickshotState state() const { return state_; }
    float timeScale() const { return timeScale_; }
    float zoomAlphaFade() const { return cameraRequest_.blendAlpha; }
    const glm::vec3& zoomFocusPoint() const { return lastZoomFocusPoint_; }
    const CameraRequest& cameraRequest() const { return cameraRequest_; }
    const TrickshotConfig& config() const { return config_; }
    TrickshotHudState hudState() const;

private:
    float nonFocusTimeScale(bool isAirborne) const;

    TrickshotConfig config_{};
    TrickshotState state_{TrickshotState::Idle};
    ZoomTriggerSource zoomSource_{ZoomTriggerSource::None};
    EntityId selectedTargetId_{kInvalidEntityId};
    bool mildAirborneSlowActive_{false};
    bool usedThisAirborne_{false};
    bool showClickIcon_{false};

    float transitionT_{0.0f};
    float lmbHoldSec_{0.0f};
    float timeScale_{1.0f};
    float aimYawOffset_{0.0f};
    float aimPitchOffset_{0.0f};
    glm::vec3 freeZoomBaseDir_{0.0f, 0.0f, -1.0f};

    glm::vec3 camTrackPos_{0.0f, 0.0f, 10.0f};
    bool camTrackInitialized_{false};
    bool subCamInitialized_{false};
    glm::vec3 lastSubCamPos_{0.0f, 6.0f, 8.0f};
    glm::vec3 lastSubCamLookAt_{0.0f, 1.0f, -8.0f};
    glm::vec3 lastZoomFocusPoint_{0.0f, 1.0f, -10.0f};
    CameraRequest cameraRequest_{};
};
} // namespace game
