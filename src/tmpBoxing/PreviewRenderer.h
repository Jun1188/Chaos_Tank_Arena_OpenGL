#pragma once

#include "game/ArenaScene.h"

#include <glm/glm.hpp>
#include <memory>

class Model;
class Shader;

namespace tmpBoxing
{
class SandTankModel;

class PreviewRenderer
{
public:
    PreviewRenderer();
    ~PreviewRenderer();

    PreviewRenderer(const PreviewRenderer&) = delete;
    PreviewRenderer& operator=(const PreviewRenderer&) = delete;

    bool init();
    void draw(const game::PreviewFrame& frame, const glm::mat4& view, const glm::mat4& projection);
    void shutdown();

private:
    std::unique_ptr<Shader> shader_{};
    std::unique_ptr<Shader> transparentShader_{};
    std::unique_ptr<Model> cubeModel_{};
    std::unique_ptr<SandTankModel> tankModel_{};
    unsigned int lineVao_{0};
    unsigned int lineVbo_{0};
    bool initialized_{false};
};
} // namespace tmpBoxing
