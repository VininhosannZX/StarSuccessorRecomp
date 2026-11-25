#pragma once
#include <filesystem>
#include <functional>
#include <string>
namespace StarDisc {
using Progress=std::function<void(size_t,size_t,const std::string&)>;
bool ValidateTree(const std::filesystem::path& directory);
std::filesystem::path ImportImage(const std::filesystem::path& image,const std::filesystem::path& destination,Progress progress={});
}
