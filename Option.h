#ifndef ARANGODB_PROGRAM_OPTIONS_OPTION_H
#define ARANGODB_PROGRAM_OPTIONS_OPTION_H 1

#include <string>
#include <iostream>
#include <memory>

#include "Parameters.h"

namespace arangodb {
namespace options {

// a single program option container
struct Option {
  // options are default copy-constructible and default movable

  // create an option, consisting of single string
  Option(std::string const& value, std::string const& description,
         Parameter* parameter, bool hidden, bool obsolete)
      : section(),
        name(),
        description(description),
        shorthand(),
        parameter(parameter),
        hidden(hidden),
        obsolete(obsolete) {
    auto parts = splitName(value);
    section = parts.first;
    name = parts.second;

    size_t const pos = name.find(',');
    if (pos != std::string::npos) {
      shorthand = stripShorthand(name.substr(pos + 1));
      name = name.substr(0, pos);
    }
  }

  // get display name for the option
  std::string displayName() const { return "--" + fullName(); }

  // get full name for the option
  std::string fullName() const {
    if (section.empty()) {
      return name;
    }
    return section + '.' + name;
  }

  // print help for an option
  void printHelp(size_t tw, size_t ow) const {
    if (!hidden) {
      std::cout << "  " << pad(nameWithType(), ow) << "   ";

      std::string value = description;
      if (parameter->requiresValue()) {
        value += " (default: " + parameter->valueString() + ")";
      }
      auto parts = wordwrap(value, tw - ow - 6);
      size_t const n = parts.size();
      for (size_t i = 0; i < n; ++i) {
        std::cout << trim(parts[i]) << std::endl;
        if (i < n - 1) {
          std::cout << "  " << pad("", ow) << "   ";
        }
      }
    }
  }

  std::string nameWithType() const {
    return displayName() + " " + parameter->typeDescription();
  }

  // determine the width of an option help string
  size_t optionsWidth() const {
    if (hidden) {
      return 0;
    }

    return nameWithType().size();
  }

  // strip the "--" from a string
  static std::string stripPrefix(std::string const& name) {
    size_t pos = name.find("--");
    if (pos == 0) {
      // strip initial "--"
      return name.substr(2);
    }
    return name;
  }

  // strip the "-" from a string
  static std::string stripShorthand(std::string const& name) {
    size_t pos = name.find("-");
    if (pos == 0) {
      // strip initial "-"
      return name.substr(1);
    }
    return name;
  }

  // split an option name at the ".", if it exists
  static std::pair<std::string, std::string> splitName(std::string name) {
    std::string section;
    name = stripPrefix(name);
    // split at "."
    size_t pos = name.find(".");
    if (pos == std::string::npos) {
      // global option
      section = "";
    } else {
      // section-specific option
      section = name.substr(0, pos);
      name = name.substr(pos + 1);
    }

    return std::make_pair(section, name);
  }

  static std::vector<std::string> wordwrap(std::string const& value,
                                           size_t size) {
    std::vector<std::string> result;
    std::string next = value;

    if (size > 0) {
      while (next.size() > size) {
        size_t m = next.find_last_of("., ", size - 1);

        if (m == std::string::npos || m < size / 2) {
          m = size;
        } else {
          m += 1;
        }

        result.emplace_back(next.substr(0, m));
        next = next.substr(m);
      }
    }

    result.emplace_back(next);

    return result;
  }

  // right-pad a string
  static std::string pad(std::string const& value, size_t length) {
    size_t const valueLength = value.size();
    if (valueLength > length) {
      return value.substr(0, length);
    }
    if (valueLength == length) {
      return value;
    }
    return value + std::string(length - valueLength, ' ');
  }

  static std::string trim(std::string const& value) {
    size_t const pos = value.find_first_not_of(" \t\n\r");
    if (pos == std::string::npos) {
      return "";
    }
    return value.substr(pos);
  }

  std::string section;
  std::string name;
  std::string description;
  std::string shorthand;
  std::shared_ptr<Parameter> parameter;
  bool hidden;
  bool obsolete;
};
}
}

#endif
