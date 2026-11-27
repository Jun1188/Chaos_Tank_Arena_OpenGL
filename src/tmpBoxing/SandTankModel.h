#pragma once

#include "game/TankEntity.h"

#include <Mesh.h>
#include <string>
#include <vector>

struct aiNode;
struct aiScene;

namespace tmpBoxing
{
// Rigid GLB preview adapter. GPU resources stay outside the game module.
class SandTankModel
{
public:
    SandTankModel() = default;
    ~SandTankModel();
    SandTankModel(const SandTankModel&) = delete;
    SandTankModel& operator=(const SandTankModel&) = delete;
    bool load(const std::string& path, const std::string& vertexPath, const std::string& fragmentPath);
    bool ready() const { return program_ != 0; }
    void draw(const game::TankPose& pose, const glm::vec3& tint,
        const glm::mat4& view, const glm::mat4& projection);

private:
    struct Part
    {
        Mesh mesh;
        glm::vec3 color{1.0f};
    };

    struct Node
    {
        int parent{-1};
        glm::mat4 local{1.0f};
        std::string name{};
        std::vector<Part> parts{};
    };

    void appendNode(const aiNode& node, const aiScene& scene, int parent);
    unsigned int program_{0};
    std::vector<Node> nodes_{};
    std::vector<glm::mat4> worlds_{};
};
} // namespace tmpBoxing
