#include "LibLsp/JsonRpc/MessageIssue.h"
#include "LibLsp/lsp/general/initialize.h"
#include "LibLsp/lsp/textDocument/did_open.h"
#include <slang/text/SourceManager.h>

# pragma once
class ServerHandlers {
    public:
    ServerHandlers(lsp::Log& log);
    td_initialize::response initializeHandler (const td_initialize::request& req);
    void didOpenHandler(Notify_TextDocumentDidOpen::notify& notify);

private:
    lsp::Log& logger;
    slang::SourceManager sm;
};
