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

    glm::vec3 mainCamPos{0.0f, 13.0f, 21.0f};
    glm::vec3 mainCamLookAt{0.0f, 0.8f, 0.0f};
    float mainCamFovDeg{45.0f};
    bool cameraWasDrivenByTrickshot{false};

    bool middleMouseHeld{false};
    std::string lastWindowTitle{};

    void shutdown()
    {
        previewRenderer.shutdown();
    }
};

RuntimeState g_Runtime{};

void resetArenaCamera(Camera& cam)
{
    g_Runtime.mainCamPos = glm::vec3{0.0f, 13.0f, 21.0f};
    g_Runtime.mainCamLookAt = glm::vec3{0.0f, 0.8f, 0.0f};
    g_Runtime.mainCamFovDeg = 45.0f;
    g_Runtime.cameraWasDrivenByTrickshot = false;
    cam.PlaceAt(g_Runtime.mainCamPos);
    cam.Zoom(g_Runtime.mainCamFovDeg);
    cam.FocusAt(g_Runtime.mainCamLookAt);
}

glm::vec3 cameraForwardFromAngles(const Camera& cam)
{
    const float yawRad = glm::radians(cam.QueryYaw());
    const float pitchRad = glm::radians(cam.QueryPitch());
    return glm::normalize(glm::vec3{
        std::cos(yawRad) * std::cos(pitchRad),
        std::sin(pitchRad),
        std::sin(yawRad) * std::cos(pitchRad)});
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
    resetArenaCamera(m_Cameras[0]);
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
    g_Runtime.latestFrame = g_Runtime.scene.buildFrame(0.0f);

    std::println(
        "[CP02 Controls] WASD: 4-Way Body Move | Hold R + Mouse: Aim Turret/Barrel | LMB: Fire | RMB: Focus Target | Ramp(Right): Jump Trickshot | Arrows: Manual Turret/Barrel | MMB Hold: Camera Look | F1: Debug | F2: Unit QA | F3: QA-12 Despawn Focus | F5: Reset | ESC: Quit");
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

    if (!m_Cameras.empty() && !g_Runtime.cameraWasDrivenByTrickshot)
    {
        g_Runtime.mainCamPos = m_Cameras[0].QueryPos();
        g_Runtime.mainCamLookAt = g_Runtime.mainCamPos + cameraForwardFromAngles(m_Cameras[0]) * 20.0f;
    }

    const glm::vec2 rawMouseDelta = g_Runtime.input.consumeRawMouseDelta();
    g_Runtime.scene.updateRealTime(
        deltaTime,
        rawMouseDelta,
        g_Runtime.mainCamPos,
        g_Runtime.mainCamLookAt,
        g_Runtime.mainCamFovDeg);

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

    // Keep real-time trickshot camera/slow exit responsive on the same frame as takeoff/landing/target loss.
    g_Runtime.scene.updateRealTime(
        0.0f,
        glm::vec2{0.0f, 0.0f},
        g_Runtime.mainCamPos,
        g_Runtime.mainCamLookAt,
        g_Runtime.mainCamFovDeg);

    g_Runtime.latestFrame = g_Runtime.scene.buildFrame(g_Runtime.timeManager.alpha());
    g_Runtime.scene.clearFrameHits();

    if (!m_Cameras.empty())
    {
        const game::CameraRequest& camReq = g_Runtime.latestFrame.cameraRequest;
        if (camReq.active)
        {
            m_Cameras[0].PlaceAt(camReq.position);
            m_Cameras[0].Zoom(camReq.fovDeg);
            m_Cameras[0].FocusAt(camReq.lookAt);
            g_Runtime.cameraWasDrivenByTrickshot = true;
        }
        else if (g_Runtime.cameraWasDrivenByTrickshot)
        {
            m_Cameras[0].PlaceAt(g_Runtime.mainCamPos);
            m_Cameras[0].Zoom(g_Runtime.mainCamFovDeg);
            m_Cameras[0].FocusAt(g_Runtime.mainCamLookAt);
            g_Runtime.cameraWasDrivenByTrickshot = false;
        }
    }

    const auto& ts = g_Runtime.latestFrame.trickshot;
    std::string nextTitle = std::format(
        "Chaos Tank Arena [CP02] | Focus: {} | Trickshot: {} ({:.2f}x) | Target HP: {}/{} | Hits: {} | R+Mouse:Turret F1:Debug F2:QA F5:Reset",
        ts.selectedTargetId == game::kInvalidEntityId ? std::string{"None"} : std::format("ID={}", ts.selectedTargetId),
        trickshotStateLabel(ts.state),
        ts.timeScale,
        g_Runtime.latestFrame.targetHp,
        g_Runtime.latestFrame.targetMaxHp,
        g_Runtime.latestFrame.totalTargetHits);
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

        g_Runtime.presentedView.view = viewMatrix;
        g_Runtime.presentedView.projection = projectionMatrix;
        g_Runtime.presentedView.viewport = glm::ivec4{rawVp.x, vpBottomY, vpW, vpH};
        g_Runtime.presentedView.windowSize = m_ScreenSize;
        g_Runtime.presentedView.valid = (vpW > 0 && vpH > 0);
    }

    g_Runtime.previewRenderer.draw(g_Runtime.latestFrame, viewMatrix, projectionMatrix);
}
