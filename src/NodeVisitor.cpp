#include "NodeVisitor.h"
#include "LibLsp/lsp/lsp_completion.h"
#include "slang/syntax/SyntaxPrinter.h"
#include <filesystem>

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
  auto fpath =fs::canonical(fname);
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
    size_t start = info.type_name.find_first_not_of(WHITESPACE);
    if (start != std::string::npos)
      info.type_name = info.type_name.substr(start);
  } else {
    info.type_name = std::string(type.name) + " " + std::string(sym.name);
  }

  // Handle Completion type
  if(type.isEnum()) {
    info.kind = lsCompletionItemKind::Enum;
  }
  else if(type.isClass()) {
    info.kind = lsCompletionItemKind::Class;
  }
  else if(type.isStruct()) {
    info.kind = lsCompletionItemKind::Struct;
  }
  else if(type.isVirtualInterface()) {
    info.kind = lsCompletionItemKind::Interface;
  }
  else {
    info.kind = lsCompletionItemKind::Variable;
  }

  known_symbols[fpath][sym.name] = info;
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

