#include "Tree.h"

#include <filesystem>

using std::filesystem::exists;
using std::filesystem::is_directory;
using std::filesystem::path;
using std::filesystem::directory_iterator;


FileNode GetTree(const std::string& path, bool dirs_only) {
  FileNode answer;
  if (!exists(path)) {
    throw std::invalid_argument("Path not exist");
  }

  if (!is_directory(path)) {
    throw std::invalid_argument("Path is not directory");
  }
  answer.name = std::filesystem::path(path).filename().string();
  answer.is_dir = true;
  for (auto& child: directory_iterator(path)) {
    if (is_directory(child)){
      answer.children.push_back(
          GetTree(std::filesystem::path(child).string(), dirs_only)
      );
    } else {
      if (dirs_only) {
        continue;
      }
      answer.children.push_back({
        std::filesystem::path(child).filename().string(), false, {}
      });
    }
  }
  return answer;
}


void FilterEmptyNodes(const FileNode& node, const path& current_path) {
    if (!node.is_dir) {
        return;
    }

    if (node.children.empty()) {
      if (current_path == ".") {
        throw std::runtime_error("You are permitted to remove current folder!");
      }
      std::filesystem::remove(current_path);
    }
    for (auto& child: node.children) {
      FilterEmptyNodes(child, current_path / child.name); 
    }
}

bool operator == (const FileNode& f1, const FileNode& f2) {
  return f1.name == f2.name && f1.is_dir == f2.is_dir && f1.children == f2.children;
}

