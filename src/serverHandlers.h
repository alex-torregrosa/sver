#include "DiagnosticParser.h"
#include "LibLsp/JsonRpc/MessageIssue.h"
#include "LibLsp/JsonRpc/RemoteEndPoint.h"
#include "LibLsp/lsp/general/initialize.h"
#include "LibLsp/lsp/textDocument/completion.h"
#include "LibLsp/lsp/textDocument/did_change.h"
#include "LibLsp/lsp/textDocument/did_open.h"
#include "NodeVisitor.h"
#include "ProjectSources.h"
#include <array>
#include <memory>
#include <mutex>
#include <slang/compilation/Compilation.h>
#include <slang/diagnostics/DiagnosticEngine.h>
#include <slang/text/SourceManager.h>

#pragma once
class ServerHandlers {
public:
  ServerHandlers(lsp::Log &log, RemoteEndPoint &remote_end_point);
  td_initialize::response initializeHandler(const td_initialize::request &req);
  td_completion::response completionHandler(const td_completion::request &req);
  void didOpenHandler(Notify_TextDocumentDidOpen::notify &notify);
  void didModifyHandler(Notify_TextDocumentDidChange::notify &notify);
  void updateDiagnostics();

private:
  lsp::Log &logger;
  RemoteEndPoint &remote;
  slang::CompilationOptions coptions;
  slang::Bag options;
  std::shared_ptr<NodeVisitor> nv;
  ProjectSources sources;
  std::mutex visitor_mutex, compile_mutex;

  const std::array<std::string, 102> verilog_keywords = {
      "always",       "end",        "ifnone",   "or",        "rpmos",
      "tranif1",      "and",        "endcase",  "initial",   "output",
      "rtran",        "tri",        "assign",   "endmodule", "inout",
      "parameter",    "rtranif0",   "tri0",     "begin",     "endfunction",
      "input",        "pmos",       "rtranif1", "tri1",      "buf",
      "endprimitive", "integer",    "posedge",  "scalared",  "triand",
      "bufif0",       "endspecify", "join",     "primitive", "small",
      "trior",        "bufif1",     "endtable", "large",     "pull0",
      "specify",      "trireg",     "case",     "endtask",   "macromodule",
      "pull1",        "specparam",  "vectored", "casex",     "event",
      "medium",       "pullup",     "strong0",  "wait",      "casez",
      "for",          "module",     "pulldown", "strong1",   "wand",
      "cmos",         "force",      "nand",     "rcmos",     "supply0",
      "weak0",        "deassign",   "forever",  "negedge",   "real",
      "supply1",      "weak1",      "default",  "for",       "nmos",
      "realtime",     "table",      "while",    "defparam",  "function",
      "nor",          "reg",        "task",     "wire",      "disable",
      "highz0",       "not",        "release",  "time",      "wor",
      "edge",         "highz1",     "notif0",   "repeat",    "tran",
      "xnor",         "else",       "if",       "notif1",    "rnmos",
      "tranif0",      "xor"};

  const std::array<std::string, 96> systemverilog_keywords = {
      "alias",         "always_comb",  "always_ff",    "always_latch",
      "assert",        "assume",       "before",       "bind",
      "bins",          "binsof",       "bit",          "break",
      "byte",          "chandle",      "class",        "clocking",
      "const",         "constraint",   "context",      "continue",
      "cover",         "covergroup",   "coverpoint",   "cross",
      "dist",          "do",           "endclass",     "endclocking",
      "endgroup",      "endinterface", "endpackage",   "endprogram",
      "endproperty",   "endsequence",  "enum",         "expect",
      "export",        "extends",      "extern",       "final",
      "first_match",   "foreach",      "forkjoin",     "iff",
      "ignore_bins",   "illegal_bins", "import",       "inside",
      "int",           "interface",    "intersect",    "join_any",
      "join_none",     "local",        "logic",        "longint",
      "matches",       "modport",      "new",          "null",
      "package",       "packed",       "priority",     "program",
      "property",      "protected",    "pure",         "rand",
      "randc",         "randcase",     "randsequence", "ref",
      "return",        "sequence",     "shortint",     "shortreal",
      "solve",         "static",       "string",       "struct",
      "super",         "tagged",       "this",         "throughout",
      "timeprecision", "type",         "typedef",      "union",
      "unique",        "var",          "virtual",      "void",
      "wait_order",    "wildcard",     "with",         "within",
  };

  const std::array<std::string, 127> verilog_system_functions = {
      "$finish",
      "$stop",
      "$exit",
      "$realtime",
      "$stime",
      "$time",
      "$printtimescale",
      "$timeformat",
      "$bitstoreal",
      "$realtobits",
      "$bitstoshortreal",
      "$shortrealtobits",
      "$itor",
      "$rtoi",
      "$signed",
      "$unsigned",
      "$cast",
      "$bits",
      "$isunbounded",
      "$typename",
      "$unpacked_dimensions",
      "$dimensions",
      "$left",
      "$right",
      "$low",
      "$high",
      "$increment",
      "$size",
      "$clog2",
      "$asin",
      "$ln",
      "$acos",
      "$log10",
      "$atan",
      "$exp",
      "$atan2",
      "$sqrt",
      "$hypot",
      "$pow",
      "$sinh",
      "$floor",
      "$cosh",
      "$ceil",
      "$tanh",
      "$sin",
      "$asinh",
      "$cos",
      "$acosh",
      "$tan",
      "$atanh",
      "$countbits",
      "$countones",
      "$onehot",
      "$onehot0",
      "$isunknown",
      "$fatal",
      "$error",
      "$warning",
      "$info",
      "$fatal",
      "$error",
      "$warning",
      "$info",
      "$asserton",
      "$assertoff",
      "$assertkill",
      "$assertcontrol",
      "$assertpasson",
      "$assertpassoff",
      "$assertfailon",
      "$assertfailoff",
      "$assertnonvacuouson",
      "$assertvacuousoff",
      "$sampled",
      "$rose",
      "$fell",
      "$stable",
      "$changed",
      "$past",
      "$past_gclk",
      "$rose_gclk",
      "$fell_gclk",
      "$stable_gclk",
      "$changed_gclk",
      "$future_gclk",
      "$rising_gclk",
      "$falling_gclk",
      "$steady_gclk",
      "$changing_gclk",
      "$coverage_control",
      "$coverage_get_max",
      "$coverage_get",
      "$coverage_merge",
      "$coverage_save",
      "$get_coverage",
      "$set_coverage_db_name",
      "$load_coverage_db",
      "$random",
      "$dist_chi_square",
      "$dist_erlang",
      "$dist_exponential",
      "$dist_normal",
      "$dist_poisson",
      "$dist_t",
      "$dist_uniform",
      "$q_initialize",
      "$q_add",
      "$q_remove",
      "$q_full",
      "$q_exam",
      "$async$and$array",
      "$async$and$plane",
      "$async$nand$array",
      "$async$nand$plane",
      "$async$or$array",
      "$async$or$plane",
      "$async$nor$array",
      "$async$nor$plane",
      "$sync$and$array",
      "$sync$and$plane",
      "$sync$nand$array",
      "$sync$nand$plane",
      "$sync$or$array",
      "$sync$or$plane",
      "$sync$nor$array",
      "$sync$nor$plane",
      "$system",
  };
};
