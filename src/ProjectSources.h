# pragma once
#include "LibLsp/lsp/AbsolutePath.h"
#include "slang/text/SourceManager.h"
class ProjectSources {
    struct file_info {
        std::string content;
        bool modified;
        bool userLoaded;
    };

public:
    void addFile(AbsolutePath& file_path, bool user_loaded=true);
    void addFile(AbsolutePath& file_path, std::string_view contents, bool user_loaded=true);
    void modifyFile(AbsolutePath& file_path, std::string_view contents);

    std::shared_ptr<slang::SourceManager> buildSourceManager();
    const std::vector<slang::SourceBuffer>& getBuffers() const;
    const std::vector<std::string> getUserFiles() const;

    private:
    bool dirty;
    std::shared_ptr<slang::SourceManager> sm;
    std::map<AbsolutePath, file_info> files_map;
    std::vector<slang::SourceBuffer> loadedBuffers;
};
