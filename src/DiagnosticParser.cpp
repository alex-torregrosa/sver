#include "DiagnosticParser.h"
#include "LibLsp/lsp/lsp_diagnostic.h"
#include <fmt/core.h>
#include <slang/text/SourceManager.h>
#include <slang/util/SmallVector.h>
#include <sstream>

DiagnosticParser::DiagnosticParser(lsp::Log &log) : logger(log) {}

void DiagnosticParser::clearDiagnostics() { diagnostics.clear(); }
const std::vector<lsDiagnostic> &DiagnosticParser::getDiagnostics() {
  return diagnostics;
}

void DiagnosticParser::report(const slang::ReportedDiagnostic &diagnostic) {
  std::stringstream msg;
  auto sm = this->sourceManager;
  int line = sm->getLineNumber(diagnostic.location);
  int col = sm->getColumnNumber(diagnostic.location);

  // Get all highlight ranges mapped into the reported location of the
  // diagnostic.
  slang::SmallVectorSized<slang::SourceRange, 8> mappedRanges;
  engine->mapSourceRanges(diagnostic.location, diagnostic.ranges, mappedRanges);

  msg << getSeverityString(diagnostic.severity) << ": ";
  msg << diagnostic.formattedMessage;
  msg << " @ L" << line << ", C" << col;
  logger.info(msg.str());

  lsDiagnostic lsp_diagnostic;
  lsp_diagnostic.message = diagnostic.formattedMessage;
  lsp_diagnostic.range.start.line = line - 1;
  lsp_diagnostic.range.start.character = col - 1;
  lsp_diagnostic.range.end = lsp_diagnostic.range.start;

  // Map the severity enum
  switch (diagnostic.severity) {
  case slang::DiagnosticSeverity::Note:
    lsp_diagnostic.severity = lsDiagnosticSeverity::Hint;
    break;
  case slang::DiagnosticSeverity::Warning:
    lsp_diagnostic.severity = lsDiagnosticSeverity::Warning;
    break;
  case slang::DiagnosticSeverity::Error:
    lsp_diagnostic.severity = lsDiagnosticSeverity::Error;
    break;
  case slang::DiagnosticSeverity::Fatal:
    lsp_diagnostic.severity = lsDiagnosticSeverity::Error;
    break;
  default:
    lsp_diagnostic.severity = lsDiagnosticSeverity::Information;
  }

  for (auto &range : mappedRanges) {
    int s_line = sm->getLineNumber(range.start());
    int e_line = sm->getLineNumber(range.end());
    int s_col = sm->getColumnNumber(range.start());
    int e_col = sm->getColumnNumber(range.end());

    logger.info(fmt::format("  {}:{} -> {}{}", s_line, s_col, e_line, e_col));
    // Overwrite last range because idk
    lsp_diagnostic.range.start.character = s_col - 1;
    lsp_diagnostic.range.start.line = s_line - 1;
    lsp_diagnostic.range.end.character = e_col - 1;
    lsp_diagnostic.range.end.line = e_line - 1;
  }

  // Write diag to the array
  diagnostics.push_back(lsp_diagnostic);
}
