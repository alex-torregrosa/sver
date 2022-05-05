#include "LibLsp/JsonRpc/MessageIssue.h"
#include "LibLsp/lsp/lsp_diagnostic.h"
#include "slang/diagnostics/DiagnosticEngine.h"
#include <slang/diagnostics/DiagnosticClient.h>
#include <string_view>

#pragma once

class DiagnosticParser : public slang::DiagnosticClient {
public:
  DiagnosticParser(lsp::Log &log);
  ~DiagnosticParser() = default;
  void report(const slang::ReportedDiagnostic &diagnostic);

  void clearDiagnostics();
  const std::map<std::string_view, std::vector<lsDiagnostic>> &getDiagnostics();

private:
  lsp::Log &logger;
  std::map<std::string_view, std::vector<lsDiagnostic>> diagnostics;
};
