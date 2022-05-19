#include "slang/syntax/SyntaxPrinter.h"
#include "NodeVisitor.h"

const std::string WHITESPACE = " \n\r\t\f\v";

NodeVisitor::NodeVisitor(std::shared_ptr<slang::SourceManager> sm) : sm(sm) {}

void NodeVisitor::handle_pkg(const slang::PackageSymbol &sym) {
  last_toplevel = sym.name;
  known_packages.push_back(sym.name);
}

void NodeVisitor::handle_instance(const slang::InstanceSymbolBase &unit) {
  last_toplevel = unit.name;
}

void NodeVisitor::handle_value(const slang::ValueSymbol &sym) {
  // We found a symbol!! q
  auto fname = sm->getFileName(sym.location);
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

  known_symbols[last_toplevel][sym.name] = info;
}
