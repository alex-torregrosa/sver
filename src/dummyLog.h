#include "LibLsp/JsonRpc/MessageIssue.h"
#include <iostream>

#pragma once

class DummyLog : public lsp::Log {
public:
  void log(Level level, std::wstring &&msg) { std::wcerr << msg << std::endl; };
  void log(Level level, const std::wstring &msg) {
    std::wcerr << msg << std::endl;
  };
  void log(Level level, std::string &&msg) { std::cerr << msg << std::endl; };
  void log(Level level, const std::string &msg) {
    std::cerr << msg << std::endl;
  };
};
