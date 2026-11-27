#include "tmpBoxing/AssetPath.h"

#include <filesystem>

namespace tmpBoxing
{
std::filesystem::path resolveRepoPath(std::string_view repoRelativePath)
{
    std::filesystem::path rel = std::filesystem::path{repoRelativePath}.lexically_normal();

    std::error_code ec{};
    if (std::filesystem::exists(rel, ec) && !ec)
        return rel.make_preferred();

    std::filesystem::path fromBuildDir = (std::filesystem::path{"../../.."} / rel).lexically_normal();
    ec.clear();
    if (std::filesystem::exists(fromBuildDir, ec) && !ec)
        return fromBuildDir.make_preferred();

    return rel.make_preferred();
}

std::string resolveRepoPathString(std::string_view repoRelativePath)
{
    return resolveRepoPath(repoRelativePath).string();
}
} // namespace tmpBoxing
