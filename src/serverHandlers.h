#include "DiagnosticParser.h"
#include "LibLsp/JsonRpc/MessageIssue.h"
#include "LibLsp/JsonRpc/RemoteEndPoint.h"
#include "LibLsp/lsp/general/initialize.h"
#include "LibLsp/lsp/lsAny.h"
#include "LibLsp/lsp/textDocument/completion.h"
#include "LibLsp/lsp/textDocument/did_change.h"
#include "LibLsp/lsp/textDocument/did_open.h"
#include "LibLsp/lsp/workspace/did_change_configuration.h"
#include "NodeVisitor.h"
#include "ProjectSources.h"
#include <array>
#include <memory>
#include <mutex>
#include <slang/compilation/Compilation.h>
#include <slang/diagnostics/DiagnosticEngine.h>
#include <slang/text/SourceManager.h>
#include "ServerConfig.h"

#pragma once
class ServerHandlers {
  
public:
  ServerHandlers(lsp::Log &log, RemoteEndPoint &remote_end_point);
  td_initialize::response initializeHandler(const td_initialize::request &req);
  td_completion::response completionHandler(const td_completion::request &req);
  void didOpenHandler(Notify_TextDocumentDidOpen::notify &notify);
  void didModifyHandler(Notify_TextDocumentDidChange::notify &notify);
  void configChange(Notify_WorkspaceDidChangeConfiguration::notify &notify);
  void updateDiagnostics();

private:
  lsp::Log &logger;
  RemoteEndPoint &remote;
  slang::CompilationOptions coptions;
  slang::Bag options;
  std::shared_ptr<NodeVisitor> nv;
  ProjectSources sources;
  std::mutex visitor_mutex, compile_mutex;
};
