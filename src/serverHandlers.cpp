#include "serverHandlers.h"
#include "DiagnosticParser.h"
#include "LibLsp/lsp/AbsolutePath.h"
#include "LibLsp/lsp/lsDocumentUri.h"
#include "LibLsp/lsp/lsp_completion.h"
#include "LibLsp/lsp/lsp_diagnostic.h"
#include "LibLsp/lsp/textDocument/publishDiagnostics.h"
#include "NodeVisitor.h"
#include "slang/text/SourceLocation.h"
#include <fmt/core.h>
#include <memory>
#include <slang/syntax/SyntaxTree.h>

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
  completion_options.triggerCharacters = std::vector<std::string>({"."});

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
  rsp.result.capabilities.workspace = workspace_options;
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

  // parsed_files[path.path] = slang::SyntaxTree::fromBuffer(buffer, sm,
  // options);
  updateDiagnostics();
}

void ServerHandlers::updateDiagnostics() {
  // Recompile the design
  compile_mutex.lock();
  std::shared_ptr<slang::Compilation> compilation = sources.compile();
  std::shared_ptr<slang::SourceManager> sm = sources.getSourceManager();
  compile_mutex.unlock();

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

  // Lock the visitor mutex and swap our pointer
  // this allows compilation and autocomplete sequences to gracefully overlap
  visitor_mutex.lock();
  if (nv == nullptr)
    nv = new_visitor;
  else
    nv.swap(new_visitor);
  visitor_mutex.unlock();
}

td_completion::response
ServerHandlers::completionHandler(const td_completion::request &req) {
  td_completion::response resp;

  auto fname = req.params.textDocument.uri.GetAbsolutePath().path;
  // logger.info("Getting completions FOR " + fname);
  //  No compulation yet, return an empty response
  if (nv == nullptr)
    return resp;

  // Lock the NodeVisitor while we are using it
  visitor_mutex.lock();

  // Get symbols from the current file
  const auto symbols = nv->getFileSymbols(fname);
  if (symbols != nullptr) {
    for (auto &&[key, item] : *symbols) {
      lsCompletionItem it;
      it.label = key;
      it.detail = "\n" + item.type_name;
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

  // Unlock it!
  visitor_mutex.unlock();

  return resp;
}
