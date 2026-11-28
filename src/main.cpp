#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#define CG_APPLICATION_CUSTOM_CALLBACK_KEY
#define CG_APPLICATION_CUSTOM_CALLBACK_MOUSE_BUTTON
#define CG_APPLICATION_CUSTOM_CALLBACK_CURSOR_POS
#include <Application.inl>

#include "game/ArenaScene.h"
#include "game/TimeManager.h"
#include "tmpBoxing/PreviewRenderer.h"

namespace
{
struct RuntimeState
{
    game::InputBuffer input{};
    game::TimeManager timeManager{};
    game::ArenaScene scene{};
    tmpBoxing::PreviewRenderer previewRenderer{};
    game::PreviewFrame latestFrame{};
    game::PresentedView presentedView{};

    glm::vec3 quarterAnchorPos{0.0f, 0.0f, 10.0f};
    float midQuarterYaw{0.0f};
    float currentQuarterYaw{0.0f};
    float smoothedAirHeight{0.0f};
    float smoothedMotionSpread{0.0f};
    glm::vec3 mainCamPos{1.6f, 10.6f, 23.5f};
    glm::vec3 mainCamLookAt{0.0f, 0.9f, 6.2f};
    float mainCamFovDeg{45.0f};

    bool middleMouseHeld{false};
    std::string lastWindowTitle{};

    void shutdown()
    {
        previewRenderer.shutdown();
    }
};

RuntimeState g_Runtime{};

float wrapAnglePiMain(float angle)
{
    constexpr float kPi = 3.14159265358979323846f;
    while (angle > kPi)
        angle -= 2.0f * kPi;
    while (angle < -kPi)
        angle += 2.0f * kPi;
    return angle;
}

void computeAimQuarterViewTarget(
    const glm::vec3& anchorPos,
    const game::AimCameraContext& ctx,
    float camYaw,
    float smoothedAirHeight,
    float smoothedSpread,
    glm::vec3& outPos,
    glm::vec3& outLookAt)
{
    const glm::vec3 fwd{-std::sin(camYaw), 0.0f, -std::cos(camYaw)};
    const glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3{0.0f, 1.0f, 0.0f}));

    const glm::vec3 toAim = ctx.aimPoint - anchorPos;
    const float aimDistXZ = std::clamp(glm::length(glm::vec2{toAim.x, toAim.z}), 4.0f, 24.0f);
    const float leadDist = std::clamp(aimDistXZ * 0.20f, 1.4f, 3.8f);

    const float pullback = 13.5f + smoothedAirHeight * 0.42f + smoothedSpread;
    const float camHeight = 10.6f + smoothedAirHeight * 0.32f + smoothedSpread * 0.35f;

    outPos = anchorPos - fwd * pullback + right * 1.6f + glm::vec3{0.0f, camHeight, 0.0f};
    outPos.y = std::max(outPos.y, 3.0f);

    const glm::vec3 subtleTankOffset = (ctx.playerPos - anchorPos) * 0.18f;
    const float baseFocusY = glm::mix(anchorPos.y, ctx.playerPos.y, 0.25f) + 0.90f;
    const float airAimWeight = std::clamp(smoothedAirHeight / 7.0f, 0.0f, 0.38f);
    const float lookY = glm::mix(baseFocusY, ctx.aimPoint.y, airAimWeight);

    outLookAt = glm::vec3{
        anchorPos.x + subtleTankOffset.x + fwd.x * leadDist,
        lookY,
        anchorPos.z + subtleTankOffset.z + fwd.z * leadDist};
}

void resetArenaCamera(Camera& cam)
{
    const game::AimCameraContext ctx = g_Runtime.scene.cameraAimContext(0.0f);
    g_Runtime.quarterAnchorPos = ctx.playerPos;
    const glm::vec2 toAimXZ{ctx.aimPoint.x - ctx.playerPos.x, ctx.aimPoint.z - ctx.playerPos.z};
    const float initYaw = (glm::length(toAimXZ) > 1e-3f) ? std::atan2(-toAimXZ.x, -toAimXZ.y) : 0.0f;
    g_Runtime.midQuarterYaw = initYaw;
    g_Runtime.currentQuarterYaw = initYaw;
    g_Runtime.smoothedAirHeight = 0.0f;
    g_Runtime.smoothedMotionSpread = 0.0f;
    g_Runtime.mainCamFovDeg = 45.0f;
    computeAimQuarterViewTarget(
        g_Runtime.quarterAnchorPos,
        ctx,
        g_Runtime.currentQuarterYaw,
        0.0f,
        0.0f,
        g_Runtime.mainCamPos,
        g_Runtime.mainCamLookAt);
    cam.PlaceAt(g_Runtime.mainCamPos);
    cam.Zoom(g_Runtime.mainCamFovDeg);
    cam.FocusAt(g_Runtime.mainCamLookAt);
}

std::string_view trickshotStateLabel(game::TrickshotState state)
{
    switch (state)
    {
    case game::TrickshotState::TransitionIn:
        return "ZOOM_IN";
    case game::TrickshotState::AimReady:
        return "ZOOM_AIM";
    case game::TrickshotState::TransitionOut:
        return "RETURN";
    default:
        return "IDLE";
    }
}
} // namespace

int main()
{
    Application app{1280, 720};
    const int result = app.Init("Chaos Tank Arena");
    if (result != 0)
        return result;

    app.Run();
    g_Runtime.shutdown();
    return 0;
}

void Application::OnInit()
{
    AppendCamera(g_Runtime.mainCamPos, glm::vec3{0.0f, 1.0f, 0.0f}, g_Runtime.mainCamFovDeg, -90.0f, -35.0f);
    mouseActiveCamera = 0;
    glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetInputMode(m_Window, GLFW_LOCK_KEY_MODS, GLFW_TRUE);

    double cx = 0.0;
    double cy = 0.0;
    glfwGetCursorPos(m_Window, &cx, &cy);
    m_MousePos = glm::vec2{static_cast<float>(cx), static_cast<float>(cy)};
    g_Runtime.input.onCursorPos(m_MousePos.x, m_MousePos.y);

    g_Runtime.previewRenderer.init();
    g_Runtime.input.reset();
    g_Runtime.timeManager.reset();
    g_Runtime.scene.reset();
    resetArenaCamera(m_Cameras[0]);
    g_Runtime.latestFrame = g_Runtime.scene.buildFrame(0.0f);

    std::println(
        "[CP02 Controls] WASD: 4-Way Move | r: Align Hull Pose & Forward Basis to Current Aim | R (Shift+R/F5): Scene Reset | Shift: Boost | Hold RMB: Zoom+Slow -> LMB: Fire | Hold LMB >=1.2s: Charge Zoom | B: Explosion Jump | F1: Debug | F2: QA | F3: QA-12 | ESC: Quit");
}

void Application::OnKeyEvent(int key, int action, int mods)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(m_Window, GLFW_TRUE);
        return;
    }

    if (key == GLFW_KEY_F2 && action == GLFW_PRESS)
    {
        game::runCp01UnitChecks();
        game::runCp02UnitChecks();
        return;
    }

    g_Runtime.input.onKey(key, action, mods);
}

void Application::OnMouseButtonEvent(int button, int action, int /*mods*/)
{
    if (button == GLFW_MOUSE_BUTTON_MIDDLE)
    {
        g_Runtime.middleMouseHeld = (action != GLFW_RELEASE);
        if (!g_Runtime.middleMouseHeld && m_Window != nullptr)
            glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        return;
    }

    g_Runtime.input.onMouseButton(button, action);
}

void Application::OnCursorMoveEvent(float xPos, float yPos)
{
    if (!g_Runtime.middleMouseHeld && m_Window != nullptr &&
        glfwGetInputMode(m_Window, GLFW_CURSOR) != GLFW_CURSOR_NORMAL)
    {
        glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        double restoredX = 0.0;
        double restoredY = 0.0;
        glfwGetCursorPos(m_Window, &restoredX, &restoredY);
        m_MousePos = glm::vec2{static_cast<float>(restoredX), static_cast<float>(restoredY)};
        g_Runtime.input.onCursorPos(m_MousePos.x, m_MousePos.y);
        return;
    }

    if (!g_Runtime.middleMouseHeld)
        g_Runtime.input.onCursorPos(xPos, yPos);
}

void Application::Update()
{
    if (!g_Runtime.middleMouseHeld && m_Window != nullptr &&
        glfwGetInputMode(m_Window, GLFW_CURSOR) != GLFW_CURSOR_NORMAL)
    {
        glfwSetInputMode(m_Window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    const glm::vec2 rawMouseDelta = g_Runtime.input.consumeRawMouseDelta();

    g_Runtime.timeManager.beginFrame(deltaTime, g_Runtime.scene.timeScale());
    while (g_Runtime.timeManager.consumeStep())
    {
        const game::TickCommand cmd = g_Runtime.input.peekCommand();
        const bool restarted = g_Runtime.scene.fixedUpdate(
            g_Runtime.timeManager.fixedDt(),
            cmd,
            g_Runtime.presentedView);
        g_Runtime.input.consumeOneShot();

        if (restarted)
        {
            g_Runtime.input.reset();
            g_Runtime.timeManager.reset();
            if (!m_Cameras.empty())
                resetArenaCamera(m_Cameras[0]);
            break;
        }
    }

    const float renderAlpha = g_Runtime.timeManager.alpha();
    const float clampedRawDt = std::clamp(deltaTime, 0.0f, 0.1f);

    // Quarter-view camera that smoothly orbits along the (Player -> World Aim / Enemy) sightline during WASD movement,
    // while filtering subtle recoil hops and accommodating airborne elevation.
    const game::AimCameraContext camCtx = g_Runtime.scene.cameraAimContext(renderAlpha);

    const glm::vec2 deltaXZ{
        camCtx.playerPos.x - g_Runtime.quarterAnchorPos.x,
        camCtx.playerPos.z - g_Runtime.quarterAnchorPos.z};
    const float distXZ = glm::length(deltaXZ);
    const float deadzoneT = std::clamp((distXZ - 1.1f) / 3.6f, 0.0f, 1.0f);
    const float smoothDeadzone = deadzoneT * deadzoneT * (3.0f - 2.0f * deadzoneT);
    const float outerLeashT = std::clamp((distXZ - 5.0f) / 3.2f, 0.0f, 1.0f);
    const float anchorHorizRate = glm::mix(0.65f, 2.75f, smoothDeadzone) + outerLeashT * 2.2f;
    const float anchorHorizBlend = 1.0f - std::exp(-anchorHorizRate * clampedRawDt);
    g_Runtime.quarterAnchorPos.x = glm::mix(g_Runtime.quarterAnchorPos.x, camCtx.playerPos.x, anchorHorizBlend);
    g_Runtime.quarterAnchorPos.z = glm::mix(g_Runtime.quarterAnchorPos.z, camCtx.playerPos.z, anchorHorizBlend);

    // Filter out compact ground-recoil hops (< 0.85m above floor) from vertical camera bounce while tracking high B/ramp jumps.
    const bool highAirborne = camCtx.airborne && (camCtx.heightAboveGround > 0.85f);
    const float rawDeltaY = camCtx.playerPos.y - g_Runtime.quarterAnchorPos.y;
    const float verticalTargetY = (!highAirborne && std::abs(rawDeltaY) < 0.85f)
        ? g_Runtime.quarterAnchorPos.y
        : (highAirborne ? (camCtx.playerPos.y * 0.62f) : camCtx.playerPos.y);
    const float anchorVertRate = highAirborne ? 1.65f : 2.15f;
    g_Runtime.quarterAnchorPos.y = glm::mix(
        g_Runtime.quarterAnchorPos.y,
        verticalTargetY,
        1.0f - std::exp(-anchorVertRate * clampedRawDt));

    const float targetAirH = highAirborne ? camCtx.heightAboveGround : 0.0f;
    const float airSmoothRate = (targetAirH > g_Runtime.smoothedAirHeight) ? 1.75f : 2.35f;
    g_Runtime.smoothedAirHeight = glm::mix(
        g_Runtime.smoothedAirHeight,
        targetAirH,
        1.0f - std::exp(-airSmoothRate * clampedRawDt));

    const float targetSpread = std::clamp((distXZ - 1.2f) * 0.45f, 0.0f, 2.2f);
    g_Runtime.smoothedMotionSpread = glm::mix(
        g_Runtime.smoothedMotionSpread,
        targetSpread,
        1.0f - std::exp(-1.90f * clampedRawDt));

    // Blend 78% live player XZ so WASD strafing (e.g. WA while aiming at enemy) immediately & progressively rotates the sightline.
    const glm::vec2 sightOriginXZ = glm::mix(
        glm::vec2{g_Runtime.quarterAnchorPos.x, g_Runtime.quarterAnchorPos.z},
        glm::vec2{camCtx.playerPos.x, camCtx.playerPos.z},
        0.78f);
    const glm::vec2 aimVecXZ{
        camCtx.aimPoint.x - sightOriginXZ.x,
        camCtx.aimPoint.z - sightOriginXZ.y};
    if (glm::length(aimVecXZ) > 1.5f)
    {
        const float rawAimYaw = std::atan2(-aimVecXZ.x, -aimVecXZ.y);
        const float diff = wrapAnglePiMain(rawAimYaw - g_Runtime.currentQuarterYaw);
        const bool activeOrbit = camCtx.hasTargetFocus || (camCtx.planarSpeed > 0.45f);
        const float microDeadzone = activeOrbit ? 0.0f : 0.06f;

        float yawDrive = 0.0f;
        if (std::abs(diff) <= microDeadzone)
        {
            yawDrive = diff * 0.25f;
        }
        else
        {
            const float excess = diff - std::copysign(microDeadzone, diff);
            yawDrive = std::copysign(microDeadzone * 0.25f, diff) + excess * (activeOrbit ? 0.94f : 0.80f);
        }

        const float desiredYaw = wrapAnglePiMain(g_Runtime.currentQuarterYaw + yawDrive);
        const float midRate = activeOrbit ? 2.15f : 1.25f;
        const float midDiff = wrapAnglePiMain(desiredYaw - g_Runtime.midQuarterYaw);
        g_Runtime.midQuarterYaw = wrapAnglePiMain(
            g_Runtime.midQuarterYaw + midDiff * (1.0f - std::exp(-midRate * clampedRawDt)));
    }

    const float finalRate = (camCtx.hasTargetFocus || camCtx.planarSpeed > 0.45f) ? 2.55f : 1.65f;
    const float finalYawDiff = wrapAnglePiMain(g_Runtime.midQuarterYaw - g_Runtime.currentQuarterYaw);
    g_Runtime.currentQuarterYaw = wrapAnglePiMain(
        g_Runtime.currentQuarterYaw + finalYawDiff * (1.0f - std::exp(-finalRate * clampedRawDt)));

    glm::vec3 targetMainPos{};
    glm::vec3 targetMainLookAt{};
    computeAimQuarterViewTarget(
        g_Runtime.quarterAnchorPos,
        camCtx,
        g_Runtime.currentQuarterYaw,
        g_Runtime.smoothedAirHeight,
        g_Runtime.smoothedMotionSpread,
        targetMainPos,
        targetMainLookAt);

    const float posBlend = 1.0f - std::exp(-2.65f * clampedRawDt);
    const float lookBlend = 1.0f - std::exp(-3.10f * clampedRawDt);
    g_Runtime.mainCamPos = glm::mix(g_Runtime.mainCamPos, targetMainPos, posBlend);
    g_Runtime.mainCamLookAt = glm::mix(g_Runtime.mainCamLookAt, targetMainLookAt, lookBlend);

    g_Runtime.scene.updateRealTime(
        clampedRawDt,
        renderAlpha,
        rawMouseDelta,
        g_Runtime.input.isRmbHeld(),
        g_Runtime.input.isLmbHeld(),
        g_Runtime.mainCamPos,
        g_Runtime.mainCamLookAt,
        g_Runtime.mainCamFovDeg);

    g_Runtime.latestFrame = g_Runtime.scene.buildFrame(renderAlpha);
    g_Runtime.scene.clearFrameHits();

    if (!m_Cameras.empty() && !g_Runtime.middleMouseHeld)
    {
        const game::CameraRequest& camReq = g_Runtime.latestFrame.cameraRequest;
        if (camReq.active)
        {
            m_Cameras[0].PlaceAt(camReq.position);
            m_Cameras[0].Zoom(camReq.fovDeg);
            m_Cameras[0].FocusAt(camReq.lookAt);
        }
        else
        {
            m_Cameras[0].PlaceAt(g_Runtime.mainCamPos);
            m_Cameras[0].Zoom(g_Runtime.mainCamFovDeg);
            m_Cameras[0].FocusAt(g_Runtime.mainCamLookAt);
        }
    }

    const auto& ts = g_Runtime.latestFrame.trickshot;
    std::string nextTitle = std::format(
        "Chaos Tank Arena [CP02] | Focus: {} | State: {} ({:.2f}x) | FOV: {:.0f} | Target HP: {}/{} | B:Jump RMB:Zoom LMB:Fire r:Hull R:Reset",
        ts.selectedTargetId == game::kInvalidEntityId ? std::string{"None"} : std::format("ID={}", ts.selectedTargetId),
        trickshotStateLabel(ts.state),
        ts.timeScale,
        g_Runtime.latestFrame.cameraRequest.active ? g_Runtime.latestFrame.cameraRequest.fovDeg : g_Runtime.mainCamFovDeg,
        g_Runtime.latestFrame.targetHp,
        g_Runtime.latestFrame.targetMaxHp);
    if (nextTitle != g_Runtime.lastWindowTitle)
    {
        g_Runtime.lastWindowTitle = std::move(nextTitle);
        glfwSetWindowTitle(m_Window, g_Runtime.lastWindowTitle.c_str());
    }
}

void Application::Clear()
{
    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Application::Render(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix, int cameraIdx)
{
    if (cameraIdx == 0 && !m_CameraViewports.empty())
    {
        const glm::ivec4 rawVp = m_CameraViewports[0];
        const int vpW = rawVp.z - rawVp.x;
        const int vpH = rawVp.w - rawVp.y;
        const int vpBottomY = static_cast<int>(m_ScreenSize.y) - rawVp.w;

        int winW = static_cast<int>(m_ScreenSize.x);
        int winH = static_cast<int>(m_ScreenSize.y);
        if (m_Window != nullptr)
            glfwGetWindowSize(m_Window, &winW, &winH);

        g_Runtime.presentedView.view = viewMatrix;
        g_Runtime.presentedView.projection = projectionMatrix;
        g_Runtime.presentedView.viewport = glm::ivec4{rawVp.x, vpBottomY, vpW, vpH};
        g_Runtime.presentedView.windowSize = glm::vec2{static_cast<float>(winW), static_cast<float>(winH)};
        g_Runtime.presentedView.valid = (vpW > 0 && vpH > 0 && winW > 0 && winH > 0);
    }

    g_Runtime.previewRenderer.draw(g_Runtime.latestFrame, viewMatrix, projectionMatrix);
}
