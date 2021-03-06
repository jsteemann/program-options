#ifndef ARANGODB_PROGRAM_OPTIONS_PROGRAM_OPTIONS_H
#define ARANGODB_PROGRAM_OPTIONS_PROGRAM_OPTIONS_H 1

#include <string>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <cstring>
#include <functional>

#include "Option.h"
#include "Section.h"

#define ARANGODB_PROGRAM_OPTIONS_PROGNAME "#progname#"

namespace arangodb {
namespace options {

// program options data structure
// typically an application will have a single instance of this
class ProgramOptions {
 public:
  // struct containing the option processing result
  class ProcessingResult {
   public:
    ProcessingResult() : _positionals(), _touched(), _failed(false) {}
    ~ProcessingResult() = default;

    // mark an option as being touched during options processing
    void touch(std::string const& name) { _touched.emplace(name); }
    // whether or not an option was touched during options processing
    bool touched(std::string const& name) const {
      return _touched.find(Option::stripPrefix(name)) != _touched.end();
    }
    // mark options processing as failed
    void failed(bool value) { _failed = value; }
    // whether or not options processing has failed
    bool failed() const { return _failed; }

    // values of all positional arguments found
    std::vector<std::string> _positionals;
    // which options were touched during option processing
    std::unordered_set<std::string> _touched;
    // whether or not options processing failed
    bool _failed;
  };

  // function type for determining terminal width
  typedef std::function<size_t()> TerminalWidthFuncType;
  // function type for determining the similarity between two strings
  typedef std::function<int(std::string const&, std::string const&)>
      SimilarityFuncType;

  // no need to copy this
  ProgramOptions(ProgramOptions const&) = delete;
  ProgramOptions& operator=(ProgramOptions const&) = delete;

  ProgramOptions(char const* progname, std::string const& usage,
                 std::string const& more,
                 TerminalWidthFuncType const& terminalWidth,
                 SimilarityFuncType const& similarity)
      : _progname(progname),
        _usage(usage),
        _more(more),
        _terminalWidth(terminalWidth),
        _similarity(similarity),
        _processingResult(),
        _sealed(false) {
    // find progname wildcard in string
    size_t const pos = _usage.find(ARANGODB_PROGRAM_OPTIONS_PROGNAME);

    if (pos != std::string::npos) {
      // and replace it with actual program name
      _usage = usage.substr(0, pos) + _progname +
               _usage.substr(pos + strlen(ARANGODB_PROGRAM_OPTIONS_PROGNAME));
    }
  }

  // return a const reference to the processing result
  ProcessingResult const& processingResult() const { return _processingResult; }

  // return a reference to the processing result
  ProcessingResult& processingResult() { return _processingResult; }

  // seal the options
  // tryin to add an option or a section after sealing will throw an error
  void seal() { _sealed = true; }

  // set context for error reporting
  void setContext(std::string const& value) { _context = value; }

  // adds a section to the options
  void addSection(Section const& section) {
    checkIfSealed();
    _sections.emplace(section.name, section);
  }

  // adds a (regular) section to the program options
  void addSection(std::string const& name, std::string const& description) {
    addSection(Section(name, description, "", false, false));
  }

  // adds a hidden section to the program options
  void addHiddenSection(std::string const& name,
                        std::string const& description) {
    addSection(Section(name, description, "", true, false));
  }

  // adds a hidden and obsolete section to the program options
  void addObsoleteSection(std::string const& name) {
    addSection(Section(name, "", "", true, true));
  }

  // adds an option to the program options
  void addOption(std::string const& name, std::string const& description,
                 Parameter* parameter) {
    addOption(Option(name, description, parameter, false, false));
  }

  // adds a hidden option to the program options
  void addHiddenOption(std::string const& name, std::string const& description,
                       Parameter* parameter) {
    addOption(Option(name, description, parameter, true, false));
  }

  // adds an obsolete and hidden option to the program options
  void addObsoleteOption(std::string const& name,
                         std::string const& description) {
    addOption(Option(name, description, new ObsoleteParameter(), true, true));
  }

  // prints usage information
  void printUsage() const { std::cout << _usage << std::endl << std::endl; }

  // prints a help for all options
  void printHelp(std::string const& section) const {
    printUsage();

    size_t const tw = _terminalWidth();
    size_t const ow = optionsWidth();

    for (auto const& it : _sections) {
      if (section == "*" || section == it.second.name) {
        it.second.printHelp(tw, ow);
      }
    }

    printSectionsHelp();
  }

  // prints the names for all section help options
  void printSectionsHelp() const {
    // print names of sections
    std::cout << _more;
    for (auto const& it : _sections) {
      if (!it.second.name.empty() && it.second.hasOptions()) {
        std::cout << " --help-" << it.second.name;
      }
    }
    std::cout << std::endl;
  }

  // translate a shorthand option
  std::string translateShorthand(std::string const& name) const {
    auto it = _shorthands.find(name);

    if (it == _shorthands.end()) {
      return name;
    }
    return (*it).second;
  }

  void walk(std::function<void(Section const&, Option const&)> const& callback,
            bool onlyTouched) {
    for (auto const& it : _sections) {
      if (it.second.obsolete) {
        // obsolete section. ignore it
        continue;
      }
      for (auto const& it2 : it.second.options) {
        if (it2.second.obsolete) {
          // obsolete option. ignore it
          continue;
        }
        if (onlyTouched && !_processingResult.touched(it2.second.fullName())) {
          // option not touched. skip over it
          continue;
        }
        callback(it.second, it2.second);
      }
    }
  }

  // checks whether a specific option exists
  // if the option does not exist, this will flag an error
  bool require(std::string const& name) {
    auto parts = Option::splitName(name);
    auto it = _sections.find(parts.first);

    if (it == _sections.end()) {
      return unknownOption(name);
    }

    auto it2 = (*it).second.options.find(parts.second);

    if (it2 == (*it).second.options.end()) {
      return unknownOption(name);
    }

    return true;
  }

  // sets a value for an option
  bool setValue(std::string const& name, std::string const& value) {
    auto parts = Option::splitName(name);
    auto it = _sections.find(parts.first);

    if (it == _sections.end()) {
      return unknownOption(name);
    }

    if ((*it).second.obsolete) {
      // section is obsolete. ignore it
      return true;
    }

    auto it2 = (*it).second.options.find(parts.second);

    if (it2 == (*it).second.options.end()) {
      return unknownOption(name);
    }

    auto& option = (*it2).second;
    if (option.obsolete) {
      // option is obsolete. ignore it
      _processingResult.touch(name);
      return true;
    }

    std::string result = option.parameter->set(value);

    if (!result.empty()) {
      // parameter validation failed
      return fail("error setting value for option '" + name + "': " + result);
    }

    _processingResult.touch(name);

    return true;
  }

  // check whether or not an option requires a value
  bool requiresValue(std::string const& name) const {
    auto parts = Option::splitName(name);
    auto it = _sections.find(parts.first);

    if (it == _sections.end()) {
      return false;
    }

    auto it2 = (*it).second.options.find(parts.second);

    if (it2 == (*it).second.options.end()) {
      return false;
    }

    return (*it2).second.parameter->requiresValue();
  }

  // returns a pointer to an option, specified by option name
  // returns a nullptr if the option is unknown
  template <typename T>
  T* get(std::string const& name) {
    auto parts = Option::splitName(name);
    auto it = _sections.find(parts.first);

    if (it == _sections.end()) {
      return nullptr;
    }

    auto it2 = (*it).second.options.find(parts.second);

    if (it2 == (*it).second.options.end()) {
      return nullptr;
    }

    Option& option = (*it2).second;

    return dynamic_cast<T>(option.parameter.get());
  }

  // handle an unknown option
  bool unknownOption(std::string const& name) {
    fail("unknown option '" + name + "'");

    auto similarOptions = similar(name, 8, 4);
    if (!similarOptions.empty()) {
      std::cerr << "Did you mean one of these?" << std::endl;
      for (auto const& it : similarOptions) {
        std::cerr << "  " << it << std::endl;
      }
      std::cerr << std::endl;
    }
    return false;
  }

  // report an error (callback from parser)
  bool fail(std::string const& message) {
    std::cerr << "Error while processing " << _context << ":" << std::endl;
    std::cerr << "  " << message << std::endl << std::endl;
    _processingResult.failed(true);
    return false;
  }

  // add a positional argument (callback from parser)
  void addPositional(std::string const& value) {
    _processingResult._positionals.emplace_back(value);
  }

 private:
  // adds an option to the list of options
  void addOption(Option const& option) {
    checkIfSealed();
    auto it = _sections.find(option.section);

    if (it == _sections.end()) {
      throw std::logic_error(
          std::string("no section defined for program option ") +
          option.displayName());
    }

    if (!option.shorthand.empty()) {
      if (!_shorthands.emplace(option.shorthand, option.fullName()).second) {
        throw std::logic_error(
            std::string("shorthand option already defined for option ") +
            option.displayName());
      }
    }

    (*it).second.options.emplace(option.name, option);
  }

  // determine maximum width of all options labels
  size_t optionsWidth() const {
    size_t width = 0;
    for (auto const& it : _sections) {
      width = (std::max)(width, it.second.optionsWidth());
    }
    return width;
  }

  // check if the options are already sealed and throw if yes
  void checkIfSealed() const {
    if (_sealed) {
      throw std::logic_error("program options are already sealed");
    }
  }

  // get a list of similar options
  std::vector<std::string> similar(std::string const& value, int cutOff,
                                   size_t max) {
    std::vector<std::string> result;

    if (_similarity != nullptr) {
      // build a sorted map of similar values first
      std::multimap<int, std::string> distances;
      // walk over all options
      walk([this, &value, &distances](Section const&, Option const& option) {
        if (option.fullName() != value) {
          distances.emplace(_similarity(value, option.fullName()),
                            option.displayName());
        }
      }, false);

      // now return the ones that have an edit distance not higher than the
      // cutOff value
      int last = 0;
      for (auto const& it : distances) {
        if (last > 1 && it.first > 2 * last) {
          break;
        }
        if (it.first > cutOff) {
          continue;
        }
        result.emplace_back(it.second);
        if (result.size() >= max) {
          break;
        }
        last = it.first;
      }
    }

    return result;
  }

 private:
  // name of binary (i.e. argv[0])
  std::string _progname;
  // usage hint, e.g. "usage: #progname# [<options>] ..."
  std::string _usage;
  // help text for section help, e.g. "for more information use"
  std::string _more;
  // context string that's shown when errors are printed
  std::string _context;
  // all sections
  std::map<std::string, Section> _sections;
  // shorthands for options, translating from short options to long option names
  // e.g. "-c" to "--configuration"
  std::unordered_map<std::string, std::string> _shorthands;
  // callback function for determining the terminal width
  TerminalWidthFuncType _terminalWidth;
  // callback function for determining the similarity between two option names
  SimilarityFuncType _similarity;
  // option processing result
  ProcessingResult _processingResult;
  // whether or not the program options setup is still mutable
  bool _sealed;
};
}
}

#endif
