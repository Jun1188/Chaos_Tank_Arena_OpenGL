#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace tmpBoxing
{
std::filesystem::path resolveRepoPath(std::string_view repoRelativePath);
std::string resolveRepoPathString(std::string_view repoRelativePath);
} // namespace tmpBoxing
