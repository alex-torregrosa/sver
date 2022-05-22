#pragma once
#include <flat_hash_map.hpp>
#include <slang/symbols/ASTVisitor.h>
#include <slang/symbols/ValueSymbol.h>
#include <slang/text/SourceManager.h>
#include "LibLsp/lsp/lsp_completion.h"
#include <string_view>

class NodeVisitor : public slang::ASTVisitor<NodeVisitor, true, true> {
public:
  typedef struct {
    const slang::ValueSymbol *sym;
    std::string parent_name, type_name;
    lsCompletionItemKind kind;
  } syminfo;

  NodeVisitor(std::shared_ptr<slang::SourceManager> sm);

  template <typename T> void handle(const T &t) {
    if constexpr (std::is_base_of_v<slang::ValueSymbol, T>) {
      handle_value(t);
    } else if constexpr (std::is_base_of_v<slang::PackageSymbol, T>) {
      handle_pkg(t);
    } else if constexpr (std::is_base_of_v<slang::InstanceSymbolBase, T>) {
      handle_instance(t);
    }
    visitDefault(t);
  }
  const std::map<string_view, syminfo> *getFileSymbols(const std::string &file);
  const std::vector<string_view> &getPackageList();

private:
  void handle_value(const slang::ValueSymbol &sym);
  void handle_pkg(const slang::PackageSymbol &sym);
  void handle_instance(const slang::InstanceSymbolBase &unit);
  std::string cleanupDecl(const std::string& decl);

  std::shared_ptr<slang::SourceManager> sm;
  flat_hash_map<std::string, std::map<string_view, syminfo>> known_symbols;
  std::vector<string_view> known_packages;
  std::string last_toplevel;
};
