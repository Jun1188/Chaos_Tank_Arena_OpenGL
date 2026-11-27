#pragma once

#include <glm/glm.hpp>

namespace game
{
inline constexpr float kFixedDt = 1.0f / 120.0f;

struct TickCommand
{
    float moveForward{0.0f};
    float moveRight{0.0f};
    float manualTurretTurn{0.0f};
    float manualBarrelPitch{0.0f};
    glm::vec2 cursorPos{0.0f, 0.0f};
    bool turretAimActive{false};
    bool fireRequested{false};
    bool selectTargetRequested{false};
    bool restartRequested{false};
    bool toggleDebugRequested{false};
    bool despawnSelectedTargetRequested{false};
};

class InputBuffer
{
public:
    void onKey(int key, int action);
    void onMouseButton(int button, int action);
    void onCursorPos(float x, float y);

    TickCommand peekCommand() const;
    glm::vec2 consumeRawMouseDelta();
    void consumeOneShot();
    void reset();

private:
    bool w_{false};
    bool s_{false};
    bool a_{false};
    bool d_{false};
    bool rotateTurretHeld_{false};
    bool arrowLeft_{false};
    bool arrowRight_{false};
    bool arrowUp_{false};
    bool arrowDown_{false};

    glm::vec2 cursorPos_{0.0f, 0.0f};
    glm::vec2 rawMouseDelta_{0.0f, 0.0f};
    bool cursorInitialized_{false};

    bool fireQueued_{false};
    bool selectTargetQueued_{false};
    bool restartQueued_{false};
    bool toggleDebugQueued_{false};
    bool despawnSelectedTargetQueued_{false};
};

class TimeManager
{
public:
    void reset();
    void beginFrame(float rawDeltaSeconds, float timeScale = 1.0f);
    bool consumeStep();

    float alpha() const;
    float fixedDt() const { return kFixedDt; }
    float timeScale() const { return scale_; }

private:
    float accumulator_{0.0f};
    float scale_{1.0f};
};
} // namespace game
