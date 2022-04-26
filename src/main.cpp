#include <boost/program_options.hpp>
#include <iostream>

#include "StdIOServer.h"

using namespace std;
namespace po = boost::program_options;

int main(int argc, char *argv[]) {
  // Declare the supported options.
  po::options_description desc("Allowed options");
  desc.add_options()("help", "produce help message");

  po::variables_map vm;
  po::store(po::parse_command_line(argc, argv, desc), vm);
  po::notify(vm);

  if (vm.count("help")) {
    cout << desc << "\n";
    return 1;
  }

  // start the server
  StdIOServer server;
  server.esc_event.wait();

  return 0;
}
