#ifndef COMMAND_LINE_PARSER
#define COMMAND_LINE_PARSER

#include <string>
#include <vector>

class CommandLineParser {
public:
	CommandLineParser(int argc, char** argv);
	std::string find_val_for_flag(const std::string& flag) const;
	bool has_flag(const std::string& flag) const;

private:
	int _argc;
	char** _argv;
};

#endif // COMMAND_LINE_PARSER