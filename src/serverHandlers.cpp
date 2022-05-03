#include "serverHandlers.h"
#include "DiagnosticParser.h"
#include "LibLsp/lsp/AbsolutePath.h"
#include "LibLsp/lsp/textDocument/publishDiagnostics.h"
#include "slang/text/SourceLocation.h"
#include <memory>
#include <slang/syntax/SyntaxTree.h>

ServerHandlers::ServerHandlers(lsp::Log &log, RemoteEndPoint &remote_end_point)
    : logger(log), remote(remote_end_point) {
  slang::CompilationOptions coptions;
  coptions.lintMode = true;

  options.set(coptions);
  compilation = new slang::Compilation(options);
  log.info("Initializing ServerHandlers\n");

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

  slang::SourceBuffer buffer = sm.readSource(path.path);
  if (!buffer) {
    logger.error(" File not found");
    return;
  }

  auto tree = slang::SyntaxTree::fromBuffer(buffer, sm, options);
  compilation->addSyntaxTree(tree);

  // Get diagnostics and feed them to the parser
  auto &diags = compilation->getAllDiagnostics();
  parser->clearDiagnostics();
  for (auto &diag : diags) {
    diagEngine->issue(diag);
  }

  // Create the PublishDiagnostics message
  Notify_TextDocumentPublishDiagnostics::notify pub;
  auto& pub_params = pub.params;
  pub_params.uri = params.textDocument.uri;
  pub_params.diagnostics = parser->getDiagnostics();

  // Send the diagnostics to the client
  remote.send(pub);
}
