#include "DiagnosticParser.h"
#include "LibLsp/JsonRpc/MessageIssue.h"
#include "LibLsp/JsonRpc/RemoteEndPoint.h"
#include "LibLsp/lsp/general/initialize.h"
#include "LibLsp/lsp/textDocument/did_open.h"
#include <slang/compilation/Compilation.h>
#include <slang/diagnostics/DiagnosticEngine.h>
#include <slang/text/SourceManager.h>

#pragma once
class ServerHandlers {
public:
  ServerHandlers(lsp::Log &log, RemoteEndPoint &remote_end_point);
  td_initialize::response initializeHandler(const td_initialize::request &req);
  void didOpenHandler(Notify_TextDocumentDidOpen::notify &notify);
  void updateDiagnostics();

private:
  lsp::Log &logger;
  RemoteEndPoint &remote;
  slang::SourceManager sm;
  slang::CompilationOptions coptions;
  slang::Bag options;
  slang::Compilation *compilation;
  slang::DiagnosticEngine *diagEngine;
  std::shared_ptr<DiagnosticParser> parser;
  std::vector<slang::SourceBuffer> buffers;
  std::map<std::string, std::shared_ptr<slang::SyntaxTree>> parsed_files;
};
