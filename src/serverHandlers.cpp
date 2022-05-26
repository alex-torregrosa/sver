#include "serverHandlers.h"
#include "DiagnosticParser.h"
#include "LibLsp/lsp/AbsolutePath.h"
#include "LibLsp/lsp/lsDocumentUri.h"
#include "LibLsp/lsp/lsp_completion.h"
#include "LibLsp/lsp/lsp_diagnostic.h"
#include "LibLsp/lsp/textDocument/completion.h"
#include "LibLsp/lsp/textDocument/publishDiagnostics.h"
#include "NodeVisitor.h"
#include "slang/text/SourceLocation.h"
#include "slang/types/AllTypes.h"
#include <boost/none.hpp>
#include <fmt/core.h>
#include <memory>
#include <slang/syntax/SyntaxTree.h>
#include <sstream>
#include <string>

ServerHandlers::ServerHandlers(lsp::Log &log, RemoteEndPoint &remote_end_point)
    : logger(log), remote(remote_end_point) {
  coptions.lintMode = true;

  options.set(coptions);
  log.info("Initializing ServerHandlers\n");
}

td_initialize::response
ServerHandlers::initializeHandler(const td_initialize::request &req) {
  td_initialize::response rsp;
  rsp.id = req.id;

  lsCompletionOptions completion_options;
  completion_options.resolveProvider = true;
  // Autocomplete on .
  completion_options.triggerCharacters = std::vector<std::string>({"$", "."});

  CodeLensOptions code_lens_options;
  code_lens_options.resolveProvider = true;

  WorkspaceServerCapabilities workspace_options;
  WorkspaceFoldersOptions workspace_folder_options;
  workspace_folder_options.supported = true;
  workspace_options.workspaceFolders = workspace_folder_options;

  // TODO: Configure  TextDocumentSyncOptions

  // TODO: Add more capabilities!
  rsp.result.capabilities.codeLensProvider = code_lens_options;
  rsp.result.capabilities.completionProvider = completion_options;
  rsp.result.capabilities.renameProvider = std::make_pair(true, boost::none);
  rsp.result.capabilities.definitionProvider =
      std::make_pair(true, boost::none);
  rsp.result.capabilities.typeDefinitionProvider =
      std::make_pair(true, boost::none);
  // rsp.result.capabilities.workspace = workspace_options;
  return rsp;
}

void ServerHandlers::didOpenHandler(
    Notify_TextDocumentDidOpen::notify &notify) {
  auto &params = notify.params;
  // Get full path of the opened file
  AbsolutePath path = params.textDocument.uri.GetAbsolutePath();
  logger.info("Opened file in: " + path.path);

  // Create a SourceBuffer from the original file
  sources.addFile(path);

  updateDiagnostics();
}

void ServerHandlers::didModifyHandler(
    Notify_TextDocumentDidChange::notify &notify) {
  auto &params = notify.params;
  AbsolutePath path = params.textDocument.uri.GetAbsolutePath();
  logger.info("Modified file: " + path.path);

  // Create a buffer from the new full content
  int latestChange = params.contentChanges.size() - 1;
  auto &latestContent = params.contentChanges[latestChange].text;
  sources.modifyFile(path, latestContent);

  updateDiagnostics();
}

void ServerHandlers::updateDiagnostics() {
  // Recompile the design
  std::shared_ptr<slang::Compilation> compilation = sources.compile();
  std::shared_ptr<slang::SourceManager> sm = sources.getSourceManager();

  // Recreate diagnostic tree
  slang::DiagnosticEngine engine(*sm);
  engine.setDefaultWarnings();
  auto parser = std::make_shared<DiagnosticParser>(logger);
  engine.addClient(parser);

  // Clear the previous diagnostics list
  parser->clearDiagnostics();

  // Get diagnostics and feed them to the parser
  auto &diags = compilation->getAllDiagnostics();
  for (auto &diag : diags) {
    engine.issue(diag);
  }

  // Create the PublishDiagnostics message
  Notify_TextDocumentPublishDiagnostics::notify pub;
  auto &pub_params = pub.params;

  std::vector<lsDiagnostic> empty_list;
  auto &diagnostics = parser->getDiagnostics();

  // Iterate all the known open files
  for (auto &filename : sources.getUserFiles()) {
    pub_params.uri.SetPath(AbsolutePath(filename));
    auto res = diagnostics.find(filename);

    // Does this file have diagnostics? Add them to the message
    if (res != diagnostics.end()) {
      pub_params.diagnostics = res->second;
    } else {
      // Otherwise, clear the diagnostics for the file
      pub_params.diagnostics = empty_list;
    }
    logger.info(fmt::format("Sending diagnostics for file: {}", filename));
    // Send the diagnostics to the client
    remote.send(pub);
  }

  std::shared_ptr<NodeVisitor> new_visitor = std::make_shared<NodeVisitor>(sm);
  // Load the symbols from the compiled tree
  compilation->getRoot().visit(*new_visitor);

  if (nv == nullptr)
    nv = new_visitor;
  else
    nv.swap(new_visitor);
}

td_completion::response
ServerHandlers::completionHandler(const td_completion::request &req) {
  td_completion::response resp;
  bool is_struct = false;

  auto fname = req.params.textDocument.uri.GetAbsolutePath().path;
  auto lineno = req.params.position.line;
  auto colno = req.params.position.character;

  std::string line;
  std::string contents(sources.getFileContents(fname));
  std::istringstream in(contents);

  // Try to pre-detect structs
  auto ctx = req.params.context;
  if (ctx != boost::none) {
    if (ctx->triggerKind == lsCompletionTriggerKind::TriggerCharacter) {
      if (ctx->triggerCharacter != boost::none &&
          *ctx->triggerCharacter == ".") {
        is_struct = true;
      }
    }
  }

  if (!contents.empty()) {
    // We have the file's contents, lets get the line we want
    line.reserve(256);
    bool valid_line = true;
    int i = 0;
    while (i <= lineno && std::getline(in, line)) {
      i++;
    }
    if (i - 1 == lineno) {
      if (colno <= line.length()) {
        line = line.substr(0, colno);
      }
      auto start = line.find_last_of(" \t\f\v+-*/&|^()[]?:@!~");
      if (start != std::string::npos) {
        line = line.substr(start + 1);
      }
      /***************************
       *   SYSFUNC COMPLETION    *
       ***************************/

      if (line[0] == '$') {
        // We are autocompleting a system function
        // Give the full list
        for (const auto &key : verilog_system_functions) {
          lsCompletionItem it;
          it.label = key;
          it.insertText = key.substr(1);
          it.kind = lsCompletionItemKind::Function;
          resp.result.items.push_back(it);
        }
        return resp;
      }
      /***************************
       *   STRUCT COMPLETION    *
       ***************************/

      const auto fsymbols = nv->getFileSymbols(fname);

      auto dotpos = line.find('.');
      if (fsymbols != nullptr) {
        if (dotpos != std::string::npos || is_struct) {
          // Separate the base
          auto base = line.substr(0, dotpos);
          line = line.substr(dotpos + 1);

          auto struct_i = nv->getStructInfo(base);
          if(struct_i != nullptr) {
            for(auto& member : *struct_i) {
                logger.info(std::string(member.name));
                lsCompletionItem it;
                it.label = member.name;
                it.kind = member.kind;
                resp.result.items.push_back(it);
            }
            return resp;
          }
        }
      }
    }
  }

  /***************************
   *   REGULAR COMPLETION    *
   ***************************/

  // Fill the completion with all the verilog keywords
  for (const auto &key : verilog_keywords) {
    lsCompletionItem it;
    it.label = key;
    it.kind = lsCompletionItemKind::Keyword;
    resp.result.items.push_back(it);
  }
  for (const auto &key : systemverilog_keywords) {
    lsCompletionItem it;
    it.label = key;
    it.kind = lsCompletionItemKind::Keyword;
    resp.result.items.push_back(it);
  }

  //  No compilation yet, return a basic response
  if (nv == nullptr)
    return resp;


  // Get symbols from the current file
  const auto symbols = nv->getFileSymbols(fname);
  if (symbols != nullptr) {
    for (auto &&[key, item] : *symbols) {
      lsCompletionItem it;
      it.label = key;
      it.detail = item.type_name;
      it.documentation = std::make_pair(item.parent_name, boost::none);
      it.kind = item.kind;
      resp.result.items.push_back(it);
    }
  }

  // Get symbols from all the loaded packages
  const auto &pkgs = nv->getPackageList();
  for (auto &pkg : pkgs) {
    const auto pkg_syms = nv->getFileSymbols(std::string(pkg));
    if (pkg_syms != nullptr) {
      for (auto &&[key, item] : *pkg_syms) {
        lsCompletionItem it;
        it.label = key;
        it.detail = item.type_name;
        it.documentation = std::make_pair(item.parent_name, boost::none);
        it.kind = item.kind;
        resp.result.items.push_back(it);
      }
    }
  }


  return resp;
}
