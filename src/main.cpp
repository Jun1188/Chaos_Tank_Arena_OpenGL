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
    glm::vec3 quarterHeadingXZ{0.0f, 0.0f, -1.0f};
    glm::vec3 mainCamPos{5.0f, 11.5f, 22.0f};
    glm::vec3 mainCamLookAt{0.0f, 1.0f, 6.5f};
    float mainCamFovDeg{45.0f};

    bool middleMouseHeld{false};
    std::string lastWindowTitle{};

    void shutdown()
    {
        previewRenderer.shutdown();
    }
};

RuntimeState g_Runtime{};

void computeQuarterViewTarget(
    const glm::vec3& playerPos,
    const glm::vec3& headingXZ,
    glm::vec3& outPos,
    glm::vec3& outLookAt)
{
    const glm::vec3 fwd = (glm::length(headingXZ) > 1e-4f) ? glm::normalize(headingXZ) : glm::vec3{0.0f, 0.0f, -1.0f};
    const glm::vec3 right = glm::normalize(glm::cross(fwd, glm::vec3{0.0f, 1.0f, 0.0f}));
    outPos = playerPos - fwd * 11.5f + right * 5.2f + glm::vec3{0.0f, 11.2f, 0.0f};
    outLookAt = playerPos + fwd * 3.5f + glm::vec3{0.0f, 1.0f, 0.0f};
}

void resetArenaCamera(Camera& cam)
{
    g_Runtime.quarterAnchorPos = g_Runtime.scene.player().position();
    g_Runtime.quarterHeadingXZ = glm::vec3{0.0f, 0.0f, -1.0f};
    g_Runtime.mainCamFovDeg = 45.0f;
    computeQuarterViewTarget(
        g_Runtime.quarterAnchorPos,
        g_Runtime.quarterHeadingXZ,
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
        return "CLICK_NOW";
    case game::TrickshotState::ShotFired:
        return "FIRED";
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
        "[CP02 Controls] WASD: 4-Way Body Move | Mouse: Aim Turret/Barrel | LMB: Fire | RMB: Focus Target | B: Explosion Jump & Back-View Slow Toggle | Arrows: Manual Turret/Barrel | F1: Debug | F2: Unit QA | F3: QA-12 Despawn Focus | R: Reset | ESC: Quit");
}

void Application::OnKeyEvent(int key, int action, int /*mods*/)
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

    g_Runtime.input.onKey(key, action);
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

    // Calm, low-sensitivity quarter-view main camera with position dead-zone and lazy heading drift.
    const glm::vec3 playerRenderPos = g_Runtime.scene.playerInterpolatedPosition(renderAlpha);
    const glm::vec3 playerForwardXZ = g_Runtime.scene.playerInterpolatedForwardXZ(renderAlpha);

    const glm::vec3 anchorDelta{
        playerRenderPos.x - g_Runtime.quarterAnchorPos.x,
        0.0f,
        playerRenderPos.z - g_Runtime.quarterAnchorPos.z};
    constexpr float kDeadZoneRadius = 2.6f;
    const float anchorDist = glm::length(anchorDelta);
    if (anchorDist > kDeadZoneRadius)
    {
        const glm::vec3 excessTarget = playerRenderPos - (anchorDelta / anchorDist) * kDeadZoneRadius;
        const float anchorBlend = 1.0f - std::exp(-2.2f * clampedRawDt);
        g_Runtime.quarterAnchorPos.x = glm::mix(g_Runtime.quarterAnchorPos.x, excessTarget.x, anchorBlend);
        g_Runtime.quarterAnchorPos.z = glm::mix(g_Runtime.quarterAnchorPos.z, excessTarget.z, anchorBlend);
    }
    g_Runtime.quarterAnchorPos.y = glm::mix(
        g_Runtime.quarterAnchorPos.y,
        playerRenderPos.y * 0.25f,
        1.0f - std::exp(-1.5f * clampedRawDt));

    const float headingDot = glm::dot(g_Runtime.quarterHeadingXZ, playerForwardXZ);
    if (headingDot < 0.70f)
    {
        const float headingBlend = 1.0f - std::exp(-0.65f * clampedRawDt);
        const glm::vec3 mixedHeading = glm::mix(g_Runtime.quarterHeadingXZ, playerForwardXZ, headingBlend);
        if (glm::length(mixedHeading) > 1e-4f)
            g_Runtime.quarterHeadingXZ = glm::normalize(mixedHeading);
    }

    glm::vec3 targetMainPos{};
    glm::vec3 targetMainLookAt{};
    computeQuarterViewTarget(g_Runtime.quarterAnchorPos, g_Runtime.quarterHeadingXZ, targetMainPos, targetMainLookAt);
    const float camFollowBlend = 1.0f - std::exp(-2.4f * clampedRawDt);
    g_Runtime.mainCamPos = glm::mix(g_Runtime.mainCamPos, targetMainPos, camFollowBlend);
    g_Runtime.mainCamLookAt = glm::mix(g_Runtime.mainCamLookAt, targetMainLookAt, camFollowBlend);

    g_Runtime.scene.updateRealTime(
        clampedRawDt,
        renderAlpha,
        rawMouseDelta,
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
        "Chaos Tank Arena [CP02] | Focus: {} | Trickshot: {} ({:.2f}x) | FOV: {:.0f} | Target HP: {}/{} | B:BoomBackView R:Reset",
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
