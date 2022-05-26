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

lsCompletionItemKind NodeVisitor::getKind(const slang::Type &type, const std::string& basename) {
  if (type.isEnum()) {
    return lsCompletionItemKind::Enum;
  } else if (type.isClass()) {
    return lsCompletionItemKind::Class;
  } else if (type.isStruct()) {
    // Parse struct
    auto &str = type.getCanonicalType().as<slang::Scope>();
    auto &memberlist = known_structs[basename];
    memberlist.clear();
    for (auto &member : str.members()) {
      member_info m_info;
      m_info.name = member.name;
      // std::cerr << "MEMBER:" << member.name <<" "<<member.kind << std::endl;
      auto &member_sym = member.as<slang::FieldSymbol>();
      m_info.kind = getKind(member_sym.getType(), std::string(member.name));
      m_info.kind = lsCompletionItemKind::Variable;
      memberlist.emplace_back(m_info);
    }
    return lsCompletionItemKind::Struct;
  } else if (type.isVirtualInterface()) {
    return lsCompletionItemKind::Interface;
  }
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

  if (type.name.empty()) {
    // Got no type name? Get the full declaration from the SyntaxTree
    slang::SyntaxPrinter printer =
        slang::SyntaxPrinter().setIncludeComments(false).print(
            *sym.getSyntax()->parent);
    info.type_name = printer.str();

    // Cleanup the string
    info.type_name = cleanupDecl(info.type_name);
  } else {
    info.type_name = std::string(type.name) + " " + std::string(sym.name);
  }

  // Handle Completion type
  info.kind = getKind(type, std::string(sym.name));

  known_symbols[fpath].emplace(std::make_pair(sym.name, info));
}

const std::map<string_view, NodeVisitor::syminfo> *
NodeVisitor::getFileSymbols(const std::string &file) {
  auto res = known_symbols.find(file);

  if (res != known_symbols.end()) {
    return &res->second;
  }
  return nullptr;
}

const std::vector<string_view> &NodeVisitor::getPackageList() {
  return known_packages;
}

const NodeVisitor::struct_info *NodeVisitor::getStructInfo(const std::string& name) {
  auto res = known_structs.find(name);
  if(res == known_structs.end()) return nullptr;
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
