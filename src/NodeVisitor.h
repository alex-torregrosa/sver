#pragma once
#include "LibLsp/lsp/lsp_completion.h"
#include <flat_hash_map.hpp>
#include <memory>
#include <slang/symbols/ASTVisitor.h>
#include <slang/symbols/ValueSymbol.h>
#include <slang/text/SourceManager.h>
#include <string_view>
#include <vector>
#include <set>

class NodeVisitor : public slang::ASTVisitor<NodeVisitor, false, false> {
public:
  typedef struct {
    std::string parent_name, type_name, struct_name;
    int arrayLevels;
    lsCompletionItemKind kind;
  } syminfo;

  typedef struct {
    std::string name, type_name;
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
    } else if constexpr (std::is_base_of_v<slang::Type, T>) {
      handle_type(t);
    }

    visitDefault(t);
  }
  const std::map<string_view, syminfo> *getFileSymbols(std::string_view file);
  const struct_info *getStructInfo(const std::string &name);

  const std::vector<string_view> &getPackageList();

  const std::set<std::string>& getFileScopes(const fs::path& file);
  const std::set<std::string>& getScopeTypes(std::string_view scope);

private:
  lsCompletionItemKind getKind(const slang::Type &type, bool isMember = false);
  std::string getTypeName(const slang::Type &type);
  void handleScope(const slang::Type &type, std::string_view sym_name);

  void handle_value(const slang::ValueSymbol &sym);
  void handle_type(const slang::Type &sym);
  void handle_pkg(const slang::PackageSymbol &sym);
  void handle_instance(const slang::InstanceSymbolBase &unit);
  std::string cleanupDecl(const std::string &decl);

  std::shared_ptr<slang::SourceManager> sm;
  slang::flat_hash_map<std::string, std::map<string_view, syminfo>>
      known_symbols;
  slang::flat_hash_map<std::string, struct_info> known_structs;
  slang::flat_hash_map<std::string_view, std::set<std::string>> known_types;
  slang::flat_hash_map<fs::path, std::set<std::string>> file2scopes;
  std::vector<string_view> known_packages;
  std::string last_toplevel;
};
