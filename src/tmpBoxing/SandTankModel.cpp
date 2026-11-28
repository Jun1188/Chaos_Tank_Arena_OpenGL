#include "tmpBoxing/SandTankModel.h"

#include <assimp/Importer.hpp>
#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <gl/glew.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <iterator>
#include <format>
#include <iostream>
#include <print>
#include <utility>

namespace tmpBoxing
{
namespace
{
glm::mat4 toGlm(const aiMatrix4x4& m)
{
    return glm::mat4{
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4};
}
unsigned int compileShader(GLenum type, const std::string& path)
{
    std::ifstream input{path, std::ios::binary};
    if (!input)
    {
        std::println(std::cerr, "[tmpBoxing] Tank shader missing: {}", path);
        return 0;
    }
    const std::string text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    // Files retain UTF-8 BOM. GLSL receives source after the encoding marker.
    const char* source = text.c_str() + (text.starts_with("\xEF\xBB\xBF") ? 3 : 0);
    const unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[1024]{};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::println(std::cerr, "[tmpBoxing] Tank shader failed: {} | {}", path, log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}
} // namespace

SandTankModel::~SandTankModel()
{
    if (program_ != 0)
        glDeleteProgram(program_);
}

bool SandTankModel::load(const std::string& path, const std::string& vertexPath, const std::string& fragmentPath)
{
    if (program_ != 0)
        glDeleteProgram(program_);
    program_ = 0;
    nodes_.clear();
    worlds_.clear();
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenNormals);
    if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE))
    {
        std::println(std::cerr, "[tmpBoxing] Tank GLB load failed: {} | {}", path, importer.GetErrorString());
        return false;
    }

    const aiNode* hull = scene->mRootNode->FindNode("Hull");
    const aiNode* turret = scene->mRootNode->FindNode("Turret");
    const aiNode* barrel = scene->mRootNode->FindNode("Barrel");
    if (!hull || !turret || !barrel || turret->mParent != hull || barrel->mParent != turret)
    {
        std::println(std::cerr, "[tmpBoxing] Tank GLB needs Hull -> Turret -> Barrel; using boxing fallback.");
        return false;
    }

    for (const char side : {'L', 'R'})
    {
        const std::string trackName = std::format("Track_{}", side);
        const aiNode* track = hull->FindNode(trackName.c_str());
        if (!track || track->mParent != hull)
        {
            std::println(std::cerr, "[tmpBoxing] Tank GLB missing fixed track: {}", trackName);
            return false;
        }
        for (int wheel = 1; wheel <= 6; ++wheel)
        {
            const std::string wheelName = std::format("RoadWheel_{}_{:02}", side, wheel);
            const aiNode* node = hull->FindNode(wheelName.c_str());
            if (!node || node->mParent != hull)
            {
                std::println(std::cerr, "[tmpBoxing] Tank GLB missing hull wheel: {}", wheelName);
                return false;
            }
        }
    }

    const unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexPath);
    const unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentPath);
    if (vertexShader != 0 && fragmentShader != 0)
    {
        program_ = glCreateProgram();
        glAttachShader(program_, vertexShader);
        glAttachShader(program_, fragmentShader);
        glLinkProgram(program_);
        int linked = 0;
        glGetProgramiv(program_, GL_LINK_STATUS, &linked);
        if (!linked)
        {
            char log[1024]{};
            glGetProgramInfoLog(program_, sizeof(log), nullptr, log);
            std::println(std::cerr, "[tmpBoxing] Tank shader link failed: {}", log);
            glDeleteProgram(program_);
            program_ = 0;
        }
    }
    if (vertexShader != 0)
        glDeleteShader(vertexShader);
    if (fragmentShader != 0)
        glDeleteShader(fragmentShader);
    if (program_ == 0)
        return false;

    // TankPose supplies Hull/Turret/Barrel transforms. Descendants retain GLB local pivots.
    appendNode(*hull, *scene, -1);
    worlds_.resize(nodes_.size(), glm::mat4{1.0f});
    std::println("[tmpBoxing] Tank GLB ready: {} | {} rigid nodes", path, nodes_.size());
    return true;
}

void SandTankModel::appendNode(const aiNode& source, const aiScene& scene, int parent)
{
    const int index = static_cast<int>(nodes_.size());
    Node node{};
    node.parent = parent;
    node.local = toGlm(source.mTransformation);
    node.name = source.mName.C_Str();

    for (unsigned int i = 0; i < source.mNumMeshes; ++i)
    {
        const aiMesh& sourceMesh = *scene.mMeshes[source.mMeshes[i]];
        std::vector<Vertex> vertices{};
        std::vector<unsigned int> indices{};
        vertices.reserve(sourceMesh.mNumVertices);
        indices.reserve(sourceMesh.mNumFaces * 3);
        for (unsigned int v = 0; v < sourceMesh.mNumVertices; ++v)
        {
            Vertex vertex{};
            const aiVector3D& p = sourceMesh.mVertices[v];
            vertex.pos = {p.x, p.y, p.z};
            vertex.color = glm::vec3{1.0f};
            if (sourceMesh.HasNormals())
            {
                const aiVector3D& n = sourceMesh.mNormals[v];
                vertex.normal = {n.x, n.y, n.z};
            }
            vertices.push_back(vertex);
        }
        for (unsigned int f = 0; f < sourceMesh.mNumFaces; ++f)
        {
            const aiFace& face = sourceMesh.mFaces[f];
            if (face.mNumIndices != 3)
                continue;
            indices.insert(indices.end(), face.mIndices, face.mIndices + face.mNumIndices);
        }

        aiColor4D color{1.0f, 1.0f, 1.0f, 1.0f};
        if (sourceMesh.mMaterialIndex < scene.mNumMaterials)
        {
            const aiMaterial& material = *scene.mMaterials[sourceMesh.mMaterialIndex];
            if (material.Get(AI_MATKEY_BASE_COLOR, color) != AI_SUCCESS)
                material.Get(AI_MATKEY_COLOR_DIFFUSE, color);
        }
        node.parts.push_back({
            Mesh{VertexTag::Normal, std::move(vertices), std::move(indices), std::vector<Texture>{}},
            glm::vec3{color.r, color.g, color.b}});
    }
    nodes_.push_back(std::move(node));
    for (unsigned int i = 0; i < source.mNumChildren; ++i)
        appendNode(*source.mChildren[i], scene, index);
}

void SandTankModel::draw(const game::TankPose& pose, const glm::vec3& tint,
    const glm::mat4& view, const glm::mat4& projection,
    float alpha, TankRenderFilter filter)
{
    if (!ready())
        return;
    glUseProgram(program_);
    glUniformMatrix4fv(glGetUniformLocation(program_, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(program_, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    const int modelLocation = glGetUniformLocation(program_, "model");
    const int colorLocation = glGetUniformLocation(program_, "objectColor");
    const int alphaLocation = glGetUniformLocation(program_, "objectAlpha");
    if (alphaLocation >= 0)
        glUniform1f(alphaLocation, alpha);

    for (std::size_t i = 0; i < nodes_.size(); ++i)
    {
        Node& node = nodes_[i];
        glm::mat4 world = node.parent < 0 ? pose.bodyWorld : worlds_[node.parent] * node.local;
        if (node.name == "Turret")
            world = pose.turretWorld;
        else if (node.name == "Barrel")
            world = pose.barrelWorld;
        else if (node.name.starts_with("RoadWheel_L_"))
            world *= glm::rotate(glm::mat4{1.0f}, pose.wheelSpinRad.x, glm::vec3{1.0f, 0.0f, 0.0f});
        else if (node.name.starts_with("RoadWheel_R_"))
            world *= glm::rotate(glm::mat4{1.0f}, pose.wheelSpinRad.y, glm::vec3{1.0f, 0.0f, 0.0f});
        worlds_[i] = world;

        const bool isBarrel = (node.name == "Barrel");
        if (filter == TankRenderFilter::BarrelOnly && !isBarrel)
            continue;
        if (filter == TankRenderFilter::NonBarrelOnly && isBarrel)
            continue;

        glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(world));
        for (Part& part : node.parts)
        {
            const glm::vec3 color = part.color * tint;
            glUniform3fv(colorLocation, 1, glm::value_ptr(color));
            part.mesh.Bind();
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(part.mesh.m_Indices.size()), GL_UNSIGNED_INT, nullptr);
        }
    }
}
} // namespace tmpBoxing
