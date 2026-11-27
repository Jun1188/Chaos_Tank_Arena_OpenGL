#include <format>
#include <limits>
#include <print>
#include <string>
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
    cam.PlaceAt(glm::vec3{0.0f, 13.0f, 19.0f});
    cam.Zoom(45.0f);
    cam.FocusAt(glm::vec3{0.0f, 0.8f, 0.0f});
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
    AppendCamera(glm::vec3{0.0f, 13.0f, 19.0f}, glm::vec3{0.0f, 1.0f, 0.0f}, 45.0f, -90.0f, -35.0f);
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
        "[CP01 Controls] WASD: Hull | Mouse: Aim | LMB: Fire | Arrows: Manual Turret/Barrel | MMB Hold: Camera Look | KP 8/2/4/6/9/3: Camera Move | F1: Debug | F2: Unit QA | R: Reset | ESC: Quit");
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

    g_Runtime.timeManager.beginFrame(deltaTime, 1.0f);
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

    g_Runtime.latestFrame = g_Runtime.scene.buildFrame(g_Runtime.timeManager.alpha());
    g_Runtime.scene.clearFrameHits();

    std::string nextTitle = std::format(
        "Chaos Tank Arena [CP01] | Target HP: {}/{} | Hits: {} | Debug(F1): {} | UnitQA: F2 | Reset: R",
        g_Runtime.latestFrame.targetHp,
        g_Runtime.latestFrame.targetMaxHp,
        g_Runtime.latestFrame.totalTargetHits,
        g_Runtime.latestFrame.debugOverlay ? "ON" : "OFF");
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
