#include "ProjectSources.h"
#include "LibLsp/lsp/AbsolutePath.h"
#include "slang/compilation/Compilation.h"
#include "slang/syntax/SyntaxTree.h"
#include "slang/text/SourceLocation.h"
#include <filesystem>
#include <iostream>
#include <memory>

ProjectSources::ProjectSources() {
  config.loaded = false;
  dirty = false;
}
void ProjectSources::addFile(fs::path &file_path, bool userLoaded) {
  AbsolutePath p(file_path.c_str());
  addFile(p, userLoaded);
}

void ProjectSources::addFile(AbsolutePath &file_path, bool userLoaded) {
  auto res = files_map.find(file_path.path);
  if (res == files_map.end()) {
    file_info info;
    info.modified = false;
    info.userLoaded = userLoaded;
    files_map[file_path.path] = info;
  } else {
    // We already have this file and it was user-loaded,
    // no further processing is needed
    if(res->second.userLoaded) return;
    // We already have this file, do the minimal modifications
    res->second.userLoaded = userLoaded;
  }

  dirty |= userLoaded; // If this is auto-loaded, the compilation already has it

  // Try to locate a config if needed
  if (!config.loaded)
    locateInitConfig(fs::path(file_path.path));
}

void ProjectSources::addFile(AbsolutePath &file_path, std::string_view contents,
                             bool userLoaded) {
  auto res = files_map.find(file_path.path);
  if (res == files_map.end()) {
    file_info info;
    info.content = contents;
    info.modified = true;
    info.userLoaded = userLoaded;
    files_map[file_path.path] = info;
  } else {
    // We already have this file, do the minimal modifications
    res->second.userLoaded |= userLoaded;
    res->second.modified = true;
    res->second.content = contents;
  }


  // Loading file contents always dirties the compilation
  dirty = true;
  if (!config.loaded)
    locateInitConfig(fs::path(file_path.path));
}

void ProjectSources::modifyFile(AbsolutePath &file_path,
                                std::string_view contents) {
  auto res = files_map.find(file_path.path);
  if (res == files_map.end()) {

  } else {
    // We do have it
    res->second.content = contents;
    res->second.modified = true;
    dirty = true;
  }
}

std::shared_ptr<slang::SourceManager> ProjectSources::getSourceManager() {
  return sm;
}

const std::string_view ProjectSources::getFileContents(const std::string& fpath) {
    // Try to find it in the locally modified files
    auto res_f = files_map.find(fpath);
    if(res_f != files_map.end()) {
        if(res_f->second.modified) {
            auto& content = res_f->second.content;
            return content;
        }
    }
    // If not, search for it in the compilation
    auto res = loadedBuffers.find(fpath);
    if(res != loadedBuffers.end()) {
        auto mview = res->second.data;
        return mview;
    }
    return "";
}

std::shared_ptr<slang::Compilation> ProjectSources::compile() {
  slang::CompilationOptions coptions;
  slang::Bag options;
  coptions.lintMode = false;
  options.set(coptions);

  std::shared_ptr<slang::Compilation> compilation(
      new slang::Compilation(options));

  // Recreate SourceManager
  // Since it does not acept modifying files, we have to do this for every
  // compilation
  if (sm != nullptr) {
    sm.reset();
    loadedBuffers.clear();
  }
  sm = std::make_shared<slang::SourceManager>();

  // Add the user include directories to the SM
  for (auto &dpath : config.include_directories) {
    sm->addUserDirectory(dpath.string());
  }

  // Parse and compile all the known files
  // Lock the filelist while we are using it
  for (auto &&[filepath, info] : files_map) {
    slang::SourceBuffer buff;
    if (info.modified)
      buff = sm->assignText(filepath, info.content);
    else
      buff = sm->readSource(filepath);
    loadedBuffers[filepath] = buff;
    auto tree = slang::SyntaxTree::fromBuffer(buff, *sm, options);
    if (!info.userLoaded)
      tree->isLibrary = true;
    compilation->addSyntaxTree(tree);
  }

  /* ********************************************************************
   * Code from slang/tools/driver/driver.cpp to find the names of missing
   * packages
   * ********************************************************************/
  slang::flat_hash_set<string_view> knownNames;
  auto addKnownNames = [&](const std::shared_ptr<slang::SyntaxTree> &tree) {
    auto &meta = tree->getMetadata();
    for (auto &[n, _] : meta.nodeMap) {
      auto decl = &n->as<slang::ModuleDeclarationSyntax>();
      string_view name = decl->header->name.valueText();
      if (!name.empty())
        knownNames.emplace(name);
    }

    for (auto classDecl : meta.classDecls) {
      string_view name = classDecl->name.valueText();
      if (!name.empty())
        knownNames.emplace(name);
    }
  };

  auto findMissingNames = [&](const std::shared_ptr<slang::SyntaxTree> &tree,
                              slang::flat_hash_set<string_view> &missing) {
    auto &meta = tree->getMetadata();
    for (auto name : meta.globalInstances) {
      if (knownNames.find(name) == knownNames.end())
        missing.emplace(name);
    }

    for (auto idName : meta.classPackageNames) {
      string_view name = idName->identifier.valueText();
      if (!name.empty() && knownNames.find(name) == knownNames.end())
        missing.emplace(name);
    }

    for (auto importDecl : meta.packageImports) {
      for (auto importItem : importDecl->items) {
        string_view name = importItem->package.valueText();
        if (!name.empty() && knownNames.find(name) == knownNames.end())
          missing.emplace(name);
      }
    }
  };

  for (auto &tree : compilation->getSyntaxTrees())
    addKnownNames(tree);

  slang::flat_hash_set<string_view> missingNames;
  for (auto &tree : compilation->getSyntaxTrees())
    findMissingNames(tree, missingNames);
  /************* END OF SLANG CODE***************/

  std::vector<std::string> extensions = {"v", "sv"};

  // Loop modified from original slang code
  // Keep loading new files as long as we are making forward progress.
  slang::flat_hash_set<string_view> nextMissingNames;
  while (true) {
    for (auto name : missingNames) {
      slang::SourceBuffer buffer;
      for (auto &dir : config.library_directories) {
        fs::path path(dir);
        path /= name;

        for (auto &ext : extensions) {
          path.replace_extension(ext);
          if (!sm->isCached(path)) {
            buffer = sm->readSource(path);
            if (buffer) {
              // Add to the local filelist to ease future loading
              addFile(path, false);
              break;
            }
          }
        }

        if (buffer)
          break;
      }

      if (buffer) {
        auto tree = slang::SyntaxTree::fromBuffer(buffer, *sm, options);
        tree->isLibrary = true;
        compilation->addSyntaxTree(tree);

        // Re-calculate the missing names
        addKnownNames(tree);
        findMissingNames(tree, nextMissingNames);
      }
    }

    if (nextMissingNames.empty())
      break;

    missingNames = std::move(nextMissingNames);
    nextMissingNames.clear();
  }

  return compilation;
}

const std::vector<std::string> ProjectSources::getUserFiles() const {
  std::vector<std::string> result;
  for (auto &&[filepath, info] : files_map) {
    if (info.userLoaded)
      result.push_back(filepath);
  }
  return result;
}

void ProjectSources::locateInitConfig(fs::path base) {
  // If given a file, get the directory
  if (!fs::is_directory(base)) {
    base = base.parent_path();
  }
  // Ensure it is absolute
  if (!base.is_absolute())
    base = fs::absolute(base);

  bool found = false;
  while (!found) {
    auto conf_file = base / ".sver_config";
    auto git_folder = base / ".git";
    if (fs::is_regular_file(conf_file)) {
      // Found the file!
      found = true;
      // TODO: Load it
      config.loaded = true;
    } else if (fs::is_directory(git_folder)) {
      // We reached a repo top level with no config file
      found = true;
      // Let's try to guess some parameters
      auto rtl_dir = base / "rtl";
      if (fs::is_directory(rtl_dir)) {
        config.library_directories.push_back(rtl_dir);
        auto include_dir = rtl_dir / "include";
        if (fs::is_directory(include_dir)) {
          config.library_directories.push_back(include_dir);
          config.include_directories.push_back(include_dir);
        }
      }

      config.loaded = true;
    }

    // Go up!!!!!
    if (base.has_parent_path())
      base = base.parent_path();
    else {
      return; // We reached top or something
    }
  }

}
