#pragma once
#include <flat_hash_map.hpp>
#include <slang/symbols/ASTVisitor.h>
#include <slang/symbols/ValueSymbol.h>
#include <slang/text/SourceManager.h>

class NodeVisitor : public slang::ASTVisitor<NodeVisitor, true, true> {
public:
  typedef struct {
    const slang::ValueSymbol *sym;
    std::string parent_name, type_name;
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

private:
  void handle_value(const slang::ValueSymbol &sym);
  void handle_pkg(const slang::PackageSymbol &sym);
  void handle_instance(const slang::InstanceSymbolBase &unit);

  std::shared_ptr<slang::SourceManager> sm;
  flat_hash_map<std::string_view, std::map<string_view, syminfo>> known_symbols;
  std::vector<string_view> known_packages;
  string_view last_toplevel;
};
