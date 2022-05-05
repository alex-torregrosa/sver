#include "serverHandlers.h"
#include "DiagnosticParser.h"
#include "LibLsp/lsp/AbsolutePath.h"
#include "LibLsp/lsp/lsDocumentUri.h"
#include "LibLsp/lsp/textDocument/publishDiagnostics.h"
#include "slang/text/SourceLocation.h"
#include <fmt/core.h>
#include <memory>
#include <slang/syntax/SyntaxTree.h>

ServerHandlers::ServerHandlers(lsp::Log &log, RemoteEndPoint &remote_end_point)
    : logger(log), remote(remote_end_point) {
  coptions.lintMode = true;

  options.set(coptions);
  log.info("Initializing ServerHandlers\n");
  compilation = nullptr;

  diagEngine = new slang::DiagnosticEngine(sm);
  diagEngine->setDefaultWarnings();
  parser = std::make_shared<DiagnosticParser>(log);
  diagEngine->addClient(parser);
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

  auto stored_syn = parsed_files.find(path.path);
  if (stored_syn != parsed_files.end()) {
    // File already parsed??
    // Release the pointer to the SyntaxTree
    stored_syn->second.reset();
  }

  // Create a SourceBuffer from the original file
  slang::SourceBuffer buffer = sm.readSource(path.path);
  if (!buffer) {
    logger.error(fmt::format(" File '{}' not found", path.path));
    return;
  }

  // Parse just that file and add it to the compilation
  parsed_files[path.path] = slang::SyntaxTree::fromBuffer(buffer, sm, options);

  updateDiagnostics();
}

void ServerHandlers::updateDiagnostics() {
  // Rebuild the whole compilation unit
  if (compilation)
    delete compilation;
  compilation = new slang::Compilation(options);

  // Clear the previous diagnostics list
  parser->clearDiagnostics();

  // Add all the parsed files
  for (auto &&[pathname, tree] : parsed_files)
    compilation->addSyntaxTree(tree);

  // Get diagnostics and feed them to the parser
  auto &diags = compilation->getAllDiagnostics();
  for (auto &diag : diags) {
    diagEngine->issue(diag);
  }

  // Create the PublishDiagnostics message
  Notify_TextDocumentPublishDiagnostics::notify pub;
  auto &pub_params = pub.params;
  for (auto &&[filename, diagnostics] : parser->getDiagnostics()) {
    logger.info(fmt::format("Sending diagnostics for file: {}", filename));
    pub_params.uri.SetPath(AbsolutePath(std::string(filename)));
    pub_params.diagnostics = diagnostics;
  }

  // Send the diagnostics to the client
  remote.send(pub);
}
