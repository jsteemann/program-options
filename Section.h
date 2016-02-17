#ifndef ARANGODB_PROGRAM_OPTIONS_SECTION_H
#define ARANGODB_PROGRAM_OPTIONS_SECTION_H 1

#include <string>
#include <map>
#include <iostream>

#include "Option.h"

namespace arangodb {
namespace options {

// a single program options section
struct Section {
 // sections are default copy-constructible and default movable

 Section(std::string const& name, std::string const& description, std::string const& alias, bool hidden, bool obsolete)
   : name(name), description(description), alias(alias), hidden(hidden), obsolete(obsolete) {}

 // adds a program option to the section
 void addOption(Option const& option) {
   options.emplace(option.name, option);
 }
  
 // get display name for the section
 std::string displayName() const {
   return alias.empty() ? name : alias;
 }

 // whether or not the section has (displayable) options
 bool hasOptions() const {
   if (!hidden) {
     for (auto const& it : options) {
       if (!it.second.hidden) {
         return true;
       }
     }  
   }
   return false;
 }

 // print help for a section
 void printHelp(size_t tw, size_t ow) const {
   if (hidden || !hasOptions()) {
     return;
   }

   std::cout << "Section '" << displayName() << "' (" << description << ")" << std::endl;

   // propagate print command to options 
   for (auto const& it : options) { 
     it.second.printHelp(tw, ow);
   }
     
   std::cout << std::endl; 
 }

 // determine display width for a section
 size_t optionsWidth() const {
   size_t width = 0;

   if (!hidden) {
     for (auto const& it : options) {
       width = (std::max)(width, it.second.optionsWidth());
     }
   }
    
   return width; 
 }

 std::string name;
 std::string description;
 std::string alias;
 bool hidden;
 bool obsolete;
  
 // program options of the section
 std::map<std::string, Option> options;
};

}
}

#endif
