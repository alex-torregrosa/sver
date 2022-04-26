#include "serverHandlers.h"
#include "LibLsp/lsp/AbsolutePath.h"

ServerHandlers::ServerHandlers(lsp::Log &log) : logger(log) {}

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
}
