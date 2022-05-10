#include "ProjectSources.h"
#include "slang/text/SourceLocation.h"

void ProjectSources::addFile(AbsolutePath &file_path, bool userLoaded) {
  file_info info;
  info.modified = false;
  info.userLoaded = userLoaded;
  files_map[file_path] = info;
  dirty = true;
}

void ProjectSources::addFile(AbsolutePath &file_path, std::string_view contents,
                             bool userLoaded) {
  file_info info;
  info.content = contents;
  info.modified = true;
  info.userLoaded = userLoaded;
  files_map[file_path] = info;
  dirty = true;
}

void ProjectSources::modifyFile(AbsolutePath &file_path,
                                std::string_view contents) {
  auto res = files_map.find(file_path);
  if (res == files_map.end()) {

  } else {
    // We do have it
    res->second.content = contents;
    res->second.modified = true;
    dirty = true;
  }
}

std::shared_ptr<slang::SourceManager> ProjectSources::buildSourceManager() {
  if (dirty || sm == nullptr) {
    // Create the tree
    if (sm != nullptr) {
      sm.reset();
      loadedBuffers.clear();
    }
    sm = std::make_shared<slang::SourceManager>();
    // Add all known files to the SourceManager
    for (auto &&[filepath, info] : files_map) {
      slang::SourceBuffer buff;
      if (info.modified)
        buff = sm->assignText(filepath.path, info.content);
      else
        buff = sm->readSource(filepath.path);
      loadedBuffers.push_back(buff);
    }
  }
  // return the shared_ptr
  return sm;
}

const std::vector<slang::SourceBuffer> &ProjectSources::getBuffers() const {
    return loadedBuffers;
}

const std::vector<std::string> ProjectSources::getUserFiles() const {
  std::vector<std::string> result;
  for (auto &&[filepath, info] : files_map) {
    if (info.userLoaded)
      result.push_back(filepath.path);
  }
  return result;
}
