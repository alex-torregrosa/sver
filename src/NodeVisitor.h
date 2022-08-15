#pragma once
#include <flat_hash_map.hpp>
#include <memory>
#include <slang/symbols/ASTVisitor.h>
#include <slang/symbols/ValueSymbol.h>
#include <slang/text/SourceManager.h>
#include "LibLsp/lsp/lsp_completion.h"
#include <string_view>
#include <vector>

class NodeVisitor : public slang::ASTVisitor<NodeVisitor, true, true> {
public:
  typedef struct {
    std::string parent_name, type_name;
    lsCompletionItemKind kind;
  } syminfo;

  typedef struct {
    std::string name;
    lsCompletionItemKind kind;
  } member_info;

  typedef std::vector<member_info> struct_info;

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
  const struct_info *getStructInfo(const std::string& name);

  const std::vector<string_view> &getPackageList();

private:
  lsCompletionItemKind getKind(const slang::Type& type, const std::string& basename);
  void handle_value(const slang::ValueSymbol &sym);
  void handle_pkg(const slang::PackageSymbol &sym);
  void handle_instance(const slang::InstanceSymbolBase &unit);
  std::string cleanupDecl(const std::string& decl);

  std::shared_ptr<slang::SourceManager> sm;
  slang::flat_hash_map<std::string, std::map<string_view, syminfo>> known_symbols;
  slang::flat_hash_map<std::string, struct_info> known_structs;
  std::vector<string_view> known_packages;
  std::string last_toplevel;
};
