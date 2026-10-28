#include "command_line_parser.h"


CommandLineParser::CommandLineParser(int argc, char** argv)
    : _argc(argc), _argv(argv) {}

std::string CommandLineParser::find_val_for_flag(const std::string& flag) const {
   for (int i = 0; i < _argc; ++i) {
      if (flag == _argv[i]) {
         if (i + 1 < _argc) {
            return std::string(_argv[i + 1]);
         }
      }
   }
   return "";
}

bool CommandLineParser::has_flag(const std::string& flag) const {
   for (int i = 0; i < _argc; ++i) {
      if (flag == _argv[i]) {
         return true;
      }
   }
   return false;
}