#include "serverHandlers.h"
#include "CompletionHandler.h"
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
}

td_initialize::response
ServerHandlers::initializeHandler(const td_initialize::request &req) {
  td_initialize::response rsp;
  rsp.id = req.id;

  lsCompletionOptions completion_options;
  completion_options.resolveProvider = false;
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

  // Create a SourceBuffer from the original file
  sources.addFile(path);

  updateDiagnostics();
}

void ServerHandlers::didModifyHandler(
    Notify_TextDocumentDidChange::notify &notify) {
  auto &params = notify.params;
  AbsolutePath path = params.textDocument.uri.GetAbsolutePath();

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

  auto fname = req.params.textDocument.uri.GetAbsolutePath().path;
  auto lineno = req.params.position.line;
  auto colno = req.params.position.character;
  CompletionHandler completer(nv);

  std::string line;
  std::string contents(sources.getFileContents(fname));
  std::istringstream in(contents);

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
      // Run the completion
      completer.complete(line, fname, resp);
    }
  }
  return resp;
}
