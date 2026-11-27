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

    if (!frame.lines.empty() && lineVao_ != 0)
    {
        shader_->SetUniform("model", glm::mat4{1.0f});
        glBindVertexArray(lineVao_);
        glBindBuffer(GL_ARRAY_BUFFER, lineVbo_);

        for (const game::PreviewLineItem& line : frame.lines)
        {
            const glm::vec3 pts[2]{line.a, line.b};
            shader_->SetUniform("objectColor", line.color);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(pts), pts);
            glDrawArrays(GL_LINES, 0, 2);
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
