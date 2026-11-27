#include "game/TimeManager.h"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace game
{
namespace
{
constexpr float kMaxFrameDeltaSeconds = 0.25f;
constexpr float kStepEpsilon = 1e-6f;
} // namespace

void InputBuffer::onKey(int key, int action)
{
    if (action != GLFW_PRESS && action != GLFW_RELEASE)
        return;

    const bool pressed = (action == GLFW_PRESS);
    switch (key)
    {
    case GLFW_KEY_W:
        w_ = pressed;
        break;
    case GLFW_KEY_S:
        s_ = pressed;
        break;
    case GLFW_KEY_A:
        a_ = pressed;
        break;
    case GLFW_KEY_D:
        d_ = pressed;
        break;
    case GLFW_KEY_LEFT:
        arrowLeft_ = pressed;
        break;
    case GLFW_KEY_RIGHT:
        arrowRight_ = pressed;
        break;
    case GLFW_KEY_UP:
        arrowUp_ = pressed;
        break;
    case GLFW_KEY_DOWN:
        arrowDown_ = pressed;
        break;
    case GLFW_KEY_R:
        rotateTurretHeld_ = pressed;
        break;
    case GLFW_KEY_F5:
        if (pressed)
            restartQueued_ = true;
        break;
    case GLFW_KEY_F1:
        if (pressed)
            toggleDebugQueued_ = true;
        break;
    case GLFW_KEY_F3:
        if (pressed)
            despawnSelectedTargetQueued_ = true;
        break;
    default:
        break;
    }
}

void InputBuffer::onMouseButton(int button, int action)
{
    if (action != GLFW_PRESS)
        return;

    if (button == GLFW_MOUSE_BUTTON_LEFT)
        fireQueued_ = true;
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
        selectTargetQueued_ = true;
}

void InputBuffer::onCursorPos(float x, float y)
{
    const glm::vec2 next{x, y};
    if (!cursorInitialized_)
    {
        cursorPos_ = next;
        cursorInitialized_ = true;
        return;
    }

    const glm::vec2 delta = next - cursorPos_;
    cursorPos_ = next;
    rawMouseDelta_ += delta;
}

TickCommand InputBuffer::peekCommand() const
{
    TickCommand cmd{};
    cmd.moveForward = (w_ ? 1.0f : 0.0f) - (s_ ? 1.0f : 0.0f);
    cmd.moveRight = (d_ ? 1.0f : 0.0f) - (a_ ? 1.0f : 0.0f);
    cmd.manualTurretTurn = (arrowLeft_ ? 1.0f : 0.0f) - (arrowRight_ ? 1.0f : 0.0f);
    cmd.manualBarrelPitch = (arrowUp_ ? 1.0f : 0.0f) - (arrowDown_ ? 1.0f : 0.0f);
    cmd.cursorPos = cursorPos_;
    cmd.turretAimActive = rotateTurretHeld_;
    cmd.fireRequested = fireQueued_;
    cmd.selectTargetRequested = selectTargetQueued_;
    cmd.restartRequested = restartQueued_;
    cmd.toggleDebugRequested = toggleDebugQueued_;
    cmd.despawnSelectedTargetRequested = despawnSelectedTargetQueued_;
    return cmd;
}

glm::vec2 InputBuffer::consumeRawMouseDelta()
{
    const glm::vec2 delta = rawMouseDelta_;
    rawMouseDelta_ = glm::vec2{0.0f, 0.0f};
    return delta;
}

void InputBuffer::consumeOneShot()
{
    fireQueued_ = false;
    selectTargetQueued_ = false;
    restartQueued_ = false;
    toggleDebugQueued_ = false;
    despawnSelectedTargetQueued_ = false;
}

void InputBuffer::reset()
{
    w_ = false;
    s_ = false;
    a_ = false;
    d_ = false;
    rotateTurretHeld_ = false;
    arrowLeft_ = false;
    arrowRight_ = false;
    arrowUp_ = false;
    arrowDown_ = false;
    rawMouseDelta_ = glm::vec2{0.0f, 0.0f};
    fireQueued_ = false;
    selectTargetQueued_ = false;
    restartQueued_ = false;
    toggleDebugQueued_ = false;
    despawnSelectedTargetQueued_ = false;
}

void TimeManager::reset()
{
    accumulator_ = 0.0f;
    scale_ = 1.0f;
}

void TimeManager::beginFrame(float rawDeltaSeconds, float timeScale)
{
    const float clampedDt = std::clamp(rawDeltaSeconds, 0.0f, kMaxFrameDeltaSeconds);
    scale_ = std::max(0.0f, timeScale);
    accumulator_ += clampedDt * scale_;
}

bool TimeManager::consumeStep()
{
    if (accumulator_ + kStepEpsilon < kFixedDt)
        return false;

    accumulator_ = std::max(0.0f, accumulator_ - kFixedDt);
    return true;
}

float TimeManager::alpha() const
{
    return std::clamp(accumulator_ / kFixedDt, 0.0f, 1.0f);
}
} // namespace game
