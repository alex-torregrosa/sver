#include "NodeVisitor.h"
#include "LibLsp/lsp/lsp_completion.h"
#include "slang/symbols/ValueSymbol.h"
#include "slang/symbols/VariableSymbols.h"
#include "slang/syntax/SyntaxPrinter.h"
#include <filesystem>
#include <memory>
#include <string>

const std::string WHITESPACE = " \n\r\t\f\v";

NodeVisitor::NodeVisitor(std::shared_ptr<slang::SourceManager> sm) : sm(sm) {}

void NodeVisitor::handle_pkg(const slang::PackageSymbol &sym) {
  auto fname = sm->getFileName(sym.location);
  fs::path fpath(fname);
  last_toplevel = fs::canonical(fpath).string();

  known_packages.push_back(last_toplevel);
}

void NodeVisitor::handle_instance(const slang::InstanceSymbolBase &unit) {
  auto fname = sm->getFileName(unit.location);
  fs::path fpath(fname);

  last_toplevel = fs::canonical(fpath).string();
}

std::string NodeVisitor::getTypeName(const slang::Type &type) {
  // Types with name get directly the name
  if (!type.name.empty())
    return std::string(type.name);

  // No name, we should build it manually
  if (type.isArray()) {
    const slang::Type *baseType = type.getArrayElementType();
    // All arrays should have an element type
    if (baseType == nullptr)
      return "";
    std::string baseName = getTypeName(*baseType);
    return baseName + "[]";
  } else {
    // Got no type name? Get the full declaration from the SyntaxTree
    /*slang::SyntaxPrinter printer =
        slang::SyntaxPrinter().setIncludeComments(false).print(
            *sym.getSyntax()->parent);
    */
    return "unknown";
  }
}

void NodeVisitor::handleScope(const slang::Type &type,
                              std::string_view sym_name) {
  // Parse struct
  auto &m_scope = type.getCanonicalType().as<slang::Scope>();
  // Set the name: Structs with no type get the symbol name,
  // typedefed ones get the typename
  std::string scopename(type.name.empty() ? sym_name : type.name);

  auto &memberlist = known_structs[scopename];
  if (memberlist.size() == 0) {
    for (auto &member : m_scope.members()) {
      // Get the type
      auto &member_type = member.as<slang::VariableSymbol>().getType();
      // Fill the info struct
      member_info m_info;
      m_info.name = member.name;
      m_info.kind = getKind(member_type, true);
      m_info.type_name = getTypeName(member_type);
      // Push it to the list
      memberlist.emplace_back(m_info);
      // Recurse structs
      if (member_type.isStruct()) {
        handleScope(type, member.name);
      }
    }
  }
}

lsCompletionItemKind NodeVisitor::getKind(const slang::Type &type, bool isMember) {
  if (type.isEnum()) {
    return lsCompletionItemKind::Enum;
  } else if (type.isClass()) {
    return lsCompletionItemKind::Class;
  } else if (type.isStruct() || type.isPackedUnion() || type.isUnpackedUnion()) {
    return lsCompletionItemKind::Struct;
  } else if (type.isVirtualInterface()) {
    return lsCompletionItemKind::Interface;
  }
  if(isMember) return lsCompletionItemKind::Field;
  return lsCompletionItemKind::Variable;
}

void NodeVisitor::handle_value(const slang::ValueSymbol &sym) {
  // We found a symbol!! q
  auto fname = sm->getFileName(sym.location);
  auto fpath = fs::canonical(fname);
  auto def = sym.getDeclaringDefinition();
  auto &type = sym.getType();

  syminfo info;

  if (def != nullptr) {
    info.parent_name = def->name;
  }

  if (type.isStruct() || type.isClass() || type.isPackedUnion() ||
      type.isUnpackedUnion())
    handleScope(type, sym.name);

  info.type_name = getTypeName(type);
  info.kind = getKind(type);

  known_symbols[fpath].emplace(std::make_pair(sym.name, info));
}

const std::map<string_view, NodeVisitor::syminfo> *
NodeVisitor::getFileSymbols(std::string_view file) {
  std::string fname(file);
  auto res = known_symbols.find(fname);

  if (res != known_symbols.end()) {
    return &res->second;
  }
  return nullptr;
}

const std::vector<string_view> &NodeVisitor::getPackageList() {
  return known_packages;
}

const NodeVisitor::struct_info *
NodeVisitor::getStructInfo(const std::string &name) {
  auto res = known_structs.find(name);
  if (res == known_structs.end())
    return nullptr;
  return &res->second;
}

std::string NodeVisitor::cleanupDecl(const std::string &decl) {
  std::string res = "";

  // Trim repeated whitespace
  bool last_was_space = false;
  for (const auto c : decl) {
    if (c == '[') {
      // Trick to remove whitespace after width decls
      last_was_space = true;
      res += c;
    } else if (c == ' ') {
      if (!last_was_space)
        res += c;
      last_was_space = true;
    } else {
      last_was_space = false;
      res += c;
    }
  }

  // remove initial tabs/whitespace
  size_t start = res.find_first_not_of(WHITESPACE);
  if (start != std::string::npos)
    res = res.substr(start);

  return res;
}
