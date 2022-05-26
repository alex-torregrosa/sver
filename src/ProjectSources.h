# pragma once
#include "LibLsp/lsp/AbsolutePath.h"
#include "slang/text/SourceManager.h"
#include <mutex>
#include <slang/compilation/Compilation.h>
#include <filesystem>
#include <string>

class ProjectSources {
    struct file_info {
        std::string content;
        bool modified;
        bool userLoaded;
    };

    struct init_config {
        bool loaded;
        std::vector<fs::path> library_directories;
        std::vector<fs::path> include_directories;
    };

public:
    ProjectSources();
    void addFile(AbsolutePath& file_path, bool user_loaded=true);
    void addFile(fs::path& file_path, bool user_loaded=true);
    void addFile(AbsolutePath& file_path, std::string_view contents, bool user_loaded=true);
    void modifyFile(AbsolutePath& file_path, std::string_view contents);
    std::shared_ptr<slang::Compilation> compile();
    std::shared_ptr<slang::SourceManager> getSourceManager();

    const std::vector<std::string> getUserFiles() const;

    const std::string_view getFileContents(const std::string& fpath);

    private:
    void locateInitConfig(fs::path base);
    bool dirty;
    init_config config;
    std::shared_ptr<slang::SourceManager> sm;
    std::map<std::string, file_info> files_map;
    std::map<std::string, slang::SourceBuffer> loadedBuffers;
    std::mutex compilation_mutex, filelist_mutex, config_mutex;
};
