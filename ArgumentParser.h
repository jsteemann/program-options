#ifndef ARANGODB_PROGRAM_OPTIONS_ARGUMENT_PARSER_H
#define ARANGODB_PROGRAM_OPTIONS_ARGUMENT_PARSER_H 1

#include <string>

#include "ProgramOptions.h"

namespace arangodb {
namespace options {

class ArgumentParser {
 public:
  explicit ArgumentParser(ProgramOptions* options) : _options(options) {}

  // get the name of the section for which help was requested, and "*" if only
  // --help was specified
  std::string helpSection(int argc, char* argv[]) {
    for (int i = 1; i < argc; ++i) {
      std::string const current(argv[i]);
      if (current.size() >= 6 && current.substr(0, 6) == "--help") {
        if (current.size() <= 7) {
          return "*";
        }
        return current.substr(7);
      }
    }
    return "";
  }

  // parse options from argc/argv. returns true if all is well, false otherwise
  // errors that occur during parse are reported to _options
  bool parse(int argc, char* argv[]) {
    // set context for parsing (used in error messages)
    _options->setContext("command-line options");

    std::string lastOption;

    for (int i = 1; i < argc; ++i) {
      std::string option;
      std::string value;
      std::string const current(argv[i]);

      if (!lastOption.empty()) {
        option = lastOption;
      }

      if (!option.empty()) {
        value = current;
      } else {
        option = current;

        size_t dashes = 0;
        if (option.substr(0, 2) == "--") {
          dashes = 2;
        } else if (option.substr(0, 1) == "-") {
          dashes = 1;
        }

        if (dashes == 0) {
          _options->addPositional(option);
          continue;
        }

        option = option.substr(dashes);

        size_t const pos = option.find('=');

        if (pos == std::string::npos) {
          // only option
          if (dashes == 1) {
            option = _options->translateShorthand(option);
          }
          if (!_options->require(option)) {
            return false;
          }

          if (!_options->requiresValue(option)) {
            // option does not require a parameter
            if (!_options->setValue(option, "")) {
              return false;
            }
          } else {
            // option requires a parameter
            lastOption = option;
          }
          continue;
        } else {
          // option = value
          value = option.substr(pos + 1);
          option = option.substr(0, pos);
          if (dashes == 1) {
            option = _options->translateShorthand(option);
          }
        }
      }

      if (!_options->setValue(option, value)) {
        return false;
      }
      lastOption = "";
    }

    // we got some previous option, but no value was specified for it
    if (!lastOption.empty()) {
      return _options->fail("no value specified for option '" + lastOption +
                            "'");
    }

    // all is well
    return true;
  }

 private:
  ProgramOptions* _options;
};
}
}

#endif
