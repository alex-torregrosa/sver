#pragma once
#include "LibLsp/JsonRpc/serializer.h"
#include "LibLsp/lsp/lsAny.h"
#include <string>
#include <vector>

struct ServerConfig {
  std::vector<std::string> includePaths;
  std::vector<std::string> libraryPaths;
  std::vector<std::string> filelists;
  std::vector<std::string> compileFiles;
};

MAKE_REFLECT_STRUCT(ServerConfig, includePaths, libraryPaths, filelists,
                    compileFiles);
REFLECT_MAP_TO_STRUCT(ServerConfig, includePaths, libraryPaths, filelists,
                      compileFiles);
struct ServerConfigTop {
  ServerConfig verilog;
};

MAKE_REFLECT_STRUCT(ServerConfigTop, verilog);
REFLECT_MAP_TO_STRUCT(ServerConfigTop, verilog);
