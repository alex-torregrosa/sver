#include "CompletionHandler.h"
#include "LibLsp/lsp/lsp_completion.h"

CompletionHandler::CompletionHandler(std::shared_ptr<NodeVisitor> node_visitor)
    : nv(node_visitor) {}

void CompletionHandler::complete(const std::string &line,
                                 std::string_view fname,
                                 td_completion::response &resp) {
  if (line[0] == '$') {
    // We are autocompleting a system function
    // Give the full list
    add_sysfuncs(resp.result.items);
    return;
  }

  // Try to complete the struct, if it succeeds, we are done
  if (complete_struct(line, fname, resp.result.items))
    return;

  /***************************
   *   REGULAR COMPLETION    *
   ***************************/
  // Add the Verilog and SystemVerilog Keywords
  add_keywords(resp.result.items);

  //  No compilation yet, return a basic response
  if (nv == nullptr)
    return;

  add_file_symbols(fname, resp.result.items);
  add_package_symbols(resp.result.items);
}

void CompletionHandler::add_file_symbols(std::string_view fname,
                                         std::vector<lsCompletionItem> &items) {
  // Get symbols from the current file
  const auto symbols = nv->getFileSymbols(fname);
  if (symbols != nullptr) {
    for (auto &&[key, item] : *symbols) {
      lsCompletionItem it;
      it.label = key;
      it.detail = item.type_name;
      it.documentation = std::make_pair(item.parent_name, boost::none);
      it.kind = item.kind;
      items.push_back(it);
    }
  }
}

void CompletionHandler::add_package_symbols(
    std::vector<lsCompletionItem> &items) {
  // Get symbols from all the loaded packages
  const auto &pkgs = nv->getPackageList();
  for (auto &pkg : pkgs) {
    const auto pkg_syms = nv->getFileSymbols(pkg);
    if (pkg_syms != nullptr) {
      for (auto &&[key, item] : *pkg_syms) {
        lsCompletionItem it;
        it.label = key;
        it.detail = item.type_name;
        it.documentation = std::make_pair(item.parent_name, boost::none);
        it.kind = item.kind;
        items.push_back(it);
      }
    }
  }
}

bool CompletionHandler::complete_struct(const std::string &line,
                                        std::string_view fname,
                                        std::vector<lsCompletionItem> &items) {
  /***************************
   *   STRUCT COMPLETION    *
   ***************************/
  //  No compilation yet, return a basic response
  if (nv == nullptr)
    return false;

  // If the line does not have a dot, we are not completing a struct
  auto dotpos = line.find_last_of('.');
  if (dotpos == std::string::npos)
    return false;

  // Decompose the hierarchical reference
  // Line is cropped until the last dot
  std::vector<std::string> struct_path;
  std::string part;
  std::stringstream line_stream(line.substr(0, dotpos));
  while (std::getline(line_stream, part, '.')) {
    struct_path.push_back(part);
  }
  if (struct_path.size() == 0) {
    // This should not happen, since we have at least one dot
    return false;
  }

  // Try to obtain the symbols visible from the file
  const auto fsymbols = nv->getFileSymbols(fname);
  if (fsymbols == nullptr)
    return false;

  // Search the struct base symbol
  std::string_view base = struct_path[0];
  auto res = fsymbols->find(base);
  // Symbol not found
  if (res == fsymbols->end())
    return false;
  const auto &symtype = res->second.type_name;

  // Iterate the struct chain to get the last structinfo
  auto struct_i = nv->getStructInfo(symtype);
  if(struct_i == nullptr) struct_i = nv->getStructInfo(std::string(base));

  for (int i = 1; i < struct_path.size(); ++i) {
    auto act = struct_path[i];
    if (struct_i == nullptr)
      return false;
    for (auto &member : *struct_i) {
      if (member.name == act) {
        // recurse to this sub-struct
        struct_i = nv->getStructInfo(member.type_name);
      }
    }
  }
  // Final field was not a struct, exit
  if (struct_i == nullptr)
    return false;

  // Insert members into the completion
  for (auto &member : *struct_i) {
    lsCompletionItem it;
    it.label = member.name;
    it.kind = member.kind;
    it.detail = member.type_name;
    items.emplace_back(it);
  }

  return true;
}

void CompletionHandler::add_sysfuncs(std::vector<lsCompletionItem> &items) {
  for (const auto &key : verilog_system_functions) {
    lsCompletionItem it;
    it.label = key;
    it.insertText = key.substr(1);
    it.kind = lsCompletionItemKind::Function;
    items.push_back(it);
  }
}

void CompletionHandler::add_keywords(std::vector<lsCompletionItem> &items) {
  // Fill the completion with all the verilog keywords
  for (const auto &key : verilog_keywords) {
    lsCompletionItem it;
    it.label = key;
    it.kind = lsCompletionItemKind::Keyword;
    items.push_back(it);
  }
  for (const auto &key : systemverilog_keywords) {
    lsCompletionItem it;
    it.label = key;
    it.kind = lsCompletionItemKind::Keyword;
    items.push_back(it);
  }
}
