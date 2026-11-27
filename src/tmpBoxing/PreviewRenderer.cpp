#include "tmpBoxing/PreviewRenderer.h"

#include "tmpBoxing/AssetPath.h"
#include "tmpBoxing/SandTankModel.h"

#include <Model.h>
#include <Shader.h>
#include <gl/glew.h>
#include <iostream>
#include <print>
#include <string>

namespace tmpBoxing
{
namespace
{
void drawLine2D(Shader& shader, const glm::vec2& a, const glm::vec2& b, const glm::vec3& color)
{
    const glm::vec3 pts[2]{{a.x, a.y, 0.0f}, {b.x, b.y, 0.0f}};
    shader.SetUniform("objectColor", color);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(pts), pts);
    glDrawArrays(GL_LINES, 0, 2);
}

void drawRect2D(Shader& shader, const glm::vec2& center, const glm::vec2& halfSize, const glm::vec3& color)
{
    const glm::vec2 bl{center.x - halfSize.x, center.y - halfSize.y};
    const glm::vec2 br{center.x + halfSize.x, center.y - halfSize.y};
    const glm::vec2 tr{center.x + halfSize.x, center.y + halfSize.y};
    const glm::vec2 tl{center.x - halfSize.x, center.y + halfSize.y};
    drawLine2D(shader, bl, br, color);
    drawLine2D(shader, br, tr, color);
    drawLine2D(shader, tr, tl, color);
    drawLine2D(shader, tl, bl, color);
}
} // namespace

PreviewRenderer::PreviewRenderer() = default;
PreviewRenderer::~PreviewRenderer() = default;

bool PreviewRenderer::init()
{
    if (initialized_)
        return true;

    glEnable(GL_DEPTH_TEST);

    const std::string vsPath = resolveRepoPathString(
        "framework/local/ComputerGraphics/ComputerGraphics/shaders/1_opengl_basics/projectionView.vs");
    const std::string fsPath = resolveRepoPathString(
        "framework/local/ComputerGraphics/ComputerGraphics/shaders/1_opengl_basics/uniformColor.fs");
    const std::string cubePath = resolveRepoPathString("assets/boxing/cube.obj");

    shader_ = std::make_unique<Shader>(vsPath, fsPath);
    cubeModel_ = std::make_unique<Model>(cubePath);

    tankModel_ = std::make_unique<SandTankModel>();
    tankModel_->load(
        resolveRepoPathString("assets/tanks/sand-tank.glb"),
        resolveRepoPathString("assets/tanks/tank.vs"),
        resolveRepoPathString("assets/tanks/tank.fs"));

    glGenVertexArrays(1, &lineVao_);
    glGenBuffers(1, &lineVbo_);
    glBindVertexArray(lineVao_);
    glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * 2, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glDisableVertexAttribArray(1);
    glVertexAttrib3f(1, 1.0f, 1.0f, 1.0f);
    glBindVertexArray(0);

    initialized_ = !cubeModel_->Meshes().empty();
    if (initialized_)
    {
        std::println("[tmpBoxing] PreviewRenderer ready | mesh={} ({} submesh)", cubePath, cubeModel_->Meshes().size());
    }
    else
    {
        std::println(std::cerr, "[tmpBoxing] PreviewRenderer failed to load boxing mesh: {}", cubePath);
    }
    return initialized_;
}

void PreviewRenderer::draw(const game::PreviewFrame& frame, const glm::mat4& view, const glm::mat4& projection)
{
    if (!initialized_ || !shader_ || !cubeModel_)
        return;

    if (tankModel_ && tankModel_->ready())
    {
        for (const game::PreviewTankItem& tank : frame.tanks)
            tankModel_->draw(tank.pose, tank.tint, view, projection);
    }

    shader_->Bind();
    shader_->SetUniform("view", view);
    shader_->SetUniform("projection", projection);

    auto& meshes = cubeModel_->Meshes();
    bool wireActive = false;

    for (const game::PreviewBoxItem& box : frame.boxes)
    {
        if (box.tankFallback && tankModel_ && tankModel_->ready())
            continue;

        if (box.wireframe != wireActive)
        {
            glPolygonMode(GL_FRONT_AND_BACK, box.wireframe ? GL_LINE : GL_FILL);
            wireActive = box.wireframe;
        }

        shader_->SetUniform("model", box.worldMatrix);
        shader_->SetUniform("objectColor", box.color);

        for (Mesh& mesh : meshes)
        {
            mesh.Bind();
            glDrawElements(
                GL_TRIANGLES,
                static_cast<GLsizei>(mesh.m_Indices.size()),
                GL_UNSIGNED_INT,
                nullptr);
        }
    }

    if (wireActive)
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    if (lineVao_ != 0)
    {
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);

        if (!frame.lines.empty())
        {
            shader_->SetUniform("model", glm::mat4{1.0f});
            for (const game::PreviewLineItem& line : frame.lines)
            {
                const glm::vec3 pts[2]{line.a, line.b};
                shader_->SetUniform("objectColor", line.color);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(pts), pts);
                glDrawArrays(GL_LINES, 0, 2);
            }
        }

        // Temporary 2D NDC click-prompt & reticle overlay for CP02 trickshot AimReady state.
        if (frame.trickshot.showClickIcon)
        {
            const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
            glDisable(GL_DEPTH_TEST);

            const glm::mat4 identity{1.0f};
            shader_->SetUniform("view", identity);
            shader_->SetUniform("projection", identity);
            shader_->SetUniform("model", identity);

            const glm::vec3 reticleColor{0.20f, 1.0f, 0.90f};
            const glm::vec3 iconColor{1.0f, 0.92f, 0.25f};

            // Center crosshair and aim frame.
            drawLine2D(*shader_, {-0.045f, 0.0f}, {-0.015f, 0.0f}, reticleColor);
            drawLine2D(*shader_, {0.015f, 0.0f}, {0.045f, 0.0f}, reticleColor);
            drawLine2D(*shader_, {0.0f, -0.075f}, {0.0f, -0.025f}, reticleColor);
            drawLine2D(*shader_, {0.0f, 0.025f}, {0.0f, 0.075f}, reticleColor);
            drawRect2D(*shader_, {0.0f, 0.0f}, {0.14f, 0.20f}, reticleColor);

            // Stylized LMB click icon below center.
            const glm::vec2 iconCenter{0.0f, -0.45f};
            drawRect2D(*shader_, iconCenter, {0.045f, 0.085f}, iconColor);
            drawLine2D(*shader_, {iconCenter.x - 0.045f, iconCenter.y + 0.01f}, {iconCenter.x + 0.045f, iconCenter.y + 0.01f}, iconColor);
            drawLine2D(*shader_, {iconCenter.x, iconCenter.y + 0.01f}, {iconCenter.x, iconCenter.y + 0.085f}, iconColor);
            // Highlight left button inside mouse outline.
            drawRect2D(*shader_, {iconCenter.x - 0.022f, iconCenter.y + 0.047f}, {0.016f, 0.028f}, glm::vec3{1.0f, 0.35f, 0.20f});

            if (depthWasEnabled)
                glEnable(GL_DEPTH_TEST);
        }

        glBindVertexArray(0);
    }
}

void PreviewRenderer::shutdown()
{
    if (lineVbo_ != 0)
    {
        glDeleteBuffers(1, &lineVbo_);
        lineVbo_ = 0;
    }
    if (lineVao_ != 0)
    {
        glDeleteVertexArrays(1, &lineVao_);
        lineVao_ = 0;
    }
    tankModel_.reset();
    cubeModel_.reset();
    shader_.reset();
    initialized_ = false;
}
} // namespace tmpBoxing
