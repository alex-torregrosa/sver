#include "NodeVisitor.h"
#include "LibLsp/lsp/lsp_completion.h"
#include "slang/symbols/ValueSymbol.h"
#include "slang/syntax/SyntaxPrinter.h"
#include <filesystem>
#include <memory>

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
  if (type.isEnum()) {
    info.kind = lsCompletionItemKind::Enum;
  } else if (type.isClass()) {
    info.kind = lsCompletionItemKind::Class;
  } else if (type.isStruct()) {
    info.kind = lsCompletionItemKind::Struct;
  } else if (type.isVirtualInterface()) {
    info.kind = lsCompletionItemKind::Interface;
  } else {
    info.kind = lsCompletionItemKind::Variable;
  }

  info.sym = &sym;
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
