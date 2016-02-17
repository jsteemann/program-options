#include <string>
#include <vector>
#include <iostream>
#include <cstdint>
#include <functional>
#include <numeric>

#include "ArgumentParser.h"
#include "IniFileParser.h"
#include "Option.h"
#include "Parameters.h"
#include "ProgramOptions.h"
#include "Section.h"

// only used for terminalWidth below
#ifndef _WIN32
#include <sys/ioctl.h>
#endif

using namespace arangodb::options;

// a custom parameter type for port numbers
struct PortParameter : public Parameter {
  typedef uint32_t ValueType;

  explicit PortParameter(ValueType* ptr) : ptr(ptr) {}

  std::string name() const override { return "port number"; }

  std::string valueString() const override { return std::to_string(*ptr); }

  std::string set(std::string const& value) override {
    try {
      uint32_t v = static_cast<uint32_t>(std::stoull(value));
      if (v >= 1024 && v <= 65535) {
        *ptr = std::stoull(value);
        return "";
      }
    } catch (...) {
      return "invalid numeric value";
    }
    return "number out of range (port number must be between 1024 and 65535)";
  }

  ValueType* ptr;
};

// callback function for calculating the similarity of two string values
static int similarityFunc(std::string const& lhs, std::string const& rhs) {
  int const lhsLength = static_cast<int>(lhs.size());
  int const rhsLength = static_cast<int>(rhs.size());

  int* col = new int[lhsLength + 1];
  int start = 1;
  // fill with initial values
  std::iota(col + start, col + lhsLength + 1, start);

  for (int x = start; x <= rhsLength; ++x) {
    col[0] = x;
    int last = x - start;
    for (int y = start; y <= lhsLength; ++y) {
      int const save = col[y];
      col[y] = (std::min)({
          col[y] + 1,                                // deletion
          col[y - 1] + 1,                            // insertion
          last + (lhs[y - 1] == rhs[x - 1] ? 0 : 1)  // substitution
      });
      last = save;
    }
  }

  // fetch final value
  int result = col[lhsLength];
  // free memory
  delete[] col;

  return result;
}

// callback function for determining the output width of the terminal
static int terminalWidthFunc() {
  static size_t const DefaultColumns = 80;
#ifndef _WIN32
  unsigned short values[4];

  int ret = ::ioctl(0, TIOCGWINSZ, &values);

  return ret == -1 ? DefaultColumns : static_cast<size_t>(values[1]);
#else
  return DefaultColumns;
#endif
}

int main(int argc, char* argv[]) {
  // destination variables for option values
  std::string configFile;
  std::vector<std::string> endpoints = {"tcp://127.0.0.1:80",
                                        "ssl://192.168.0.1:443"};
  std::vector<uint32_t> ports = {8529, 16384};
  uint32_t journalSize = 16 * 1024 * 1024;
  bool quiet = false;
  bool noServer = false;
  bool waitForSync = false;
  bool crashMe = false;
  int32_t int32 = 1;
  uint32_t uint32 = 0;
  uint32_t bounded = 99;

  // set up program options
  ProgramOptions options(argv[0], "Usage: " ARANGODB_PROGRAM_OPTIONS_PROGNAME
                                  " [<options>] <database-directory>",
                         "For more information use:", terminalWidthFunc,
                         similarityFunc);

  // set up some basic sections

  // global (unnamed section)
  options.addSection(Section("", "Global options description goes here",
                             "global options", false, false));
  options.addOption("--quiet,-q", "tell the server to be quiet",
                    new BooleanParameter(&quiet, false));
  options.addOption("--no-server", "don't start server at all",
                    new BooleanParameter(&noServer, false));
  options.addOption("--configuration,-c", "parse configuration file",
                    new StringParameter(&configFile));
  options.addOption("--version", "prints version information",
                    new ObsoleteParameter());

  // "server" options section
  options.addSection("server", "Server options description goes here");
  options.addOption("--server.endpoints,-e", "server endpoints",
                    new VectorParameter<StringParameter>(&endpoints));
  options.addOption("--server.ports", "the server ports",
                    new VectorParameter<PortParameter>(&ports));
  options.addOption("--server.int32-value", "an int32 value",
                    new Int32Parameter(&int32));
  options.addOption("--server.uint32-value", "a uint32 value",
                    new UInt32Parameter(&uint32));
  options.addOption("--server.bounded-value", "a bounded uint32 value",
                    new BoundedParameter<UInt32Parameter>(&bounded, 42, 8193));

  // "database" options section
  options.addSection("database", "Database options description goes here");
  options.addOption("--database.journal-size", "maximal journal size",
                    new UInt32Parameter(&journalSize));
  options.addOption("--database.wait-for-sync", "wait for sync description",
                    new BooleanParameter(&waitForSync));

  // hidden section
  options.addHiddenSection("debugging",
                           "Debugging options description goes here");
  options.addOption("--debugging.crash-me",
                    "whatever (option can still be used but it is not shown)",
                    new BooleanParameter(&crashMe));
  options.addObsoleteOption("--debugging.not-used-anymore",
                            "whatever (obsolete)");

  // obsolete section (all options in this section do nothing)
  options.addObsoleteSection("y2kbug");

  // parse initial command-line options from argv
  {
    ArgumentParser parser(&options);

    std::string helpSection = parser.helpSection(argc, argv);
    if (!helpSection.empty()) {
      // user asked for "--help"
      options.printHelp(helpSection);
      return 0;
    }

    std::cout << "Parsing command-line options..." << std::endl << std::endl;
    if (!parser.parse(argc, argv)) {
      // command-line option parsing failed. an error was already printed
      // by now, so we can exit
      return 0;
    }

    if (options.processingResult().touched("version")) {
      // user asked for --version, now show it
      std::cout << "Version: 0.01" << std::endl << std::endl;
      return 0;
    }
  }

  if (!configFile.empty()) {
    // config file specified, now parse it
    std::cout << "Parsing config file '" << configFile << "'..." << std::endl
              << std::endl;

    IniFileParser parser(&options);
    if (!parser.parse(configFile)) {
      // config file parsing failed. an error was already printed
      // by now, so we can exit
      return 0;
    }
  }

  // all setup is done. now print some options
  std::cout << "Options parsed successfully" << std::endl << std::endl;

  // print positional argument value
  auto const& positionals = options.processingResult()._positionals;

  std::cout << "Positional arguments (" << positionals.size()
            << "):" << std::endl;
  for (auto const& it : positionals) {
    std::cout << "- positional: '" << it << "'" << std::endl;
  }
  std::cout << std::endl;

  // print all (touched) option values
  std::cout << "Touched options:" << std::endl;
  options.walk([](Section const& section, Option const& option) {
    std::cout << "- section: '" << section.name << "', option: '" << option.name
              << "', full name: '" << option.displayName() << "', type: '"
              << option.parameter->typeDescription() << "', value: '"
              << option.parameter->valueString() << "'" << std::endl;
  }, true);
  std::cout << std::endl;
}
