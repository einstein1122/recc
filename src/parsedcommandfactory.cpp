// Copyright 2020 Bloomberg Finance L.P
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <parsedcommandfactory.h>

#include <compilerdefaults.h>
#include <env.h>
#include <fileutils.h>

#include <buildboxcommon_exception.h>
#include <buildboxcommon_fileutils.h>
#include <buildboxcommon_logging.h>

namespace recc {

/*
 * The maps below are used to construct the map used to find the coresponding
 * options depending on the compiler, returned from:
 * ParsedCommandFactory::getParsedCommandMap()
 */
static const ParsedCommandFactory::CompilerParseRulesMap GccRules = {
    // Interferes with dependencies
    {"-MD", ParseRule::parseInterfersWithDepsOption},
    {"-MMD", ParseRule::parseInterfersWithDepsOption},
    {"-MG", ParseRule::parseInterfersWithDepsOption},
    {"-MP", ParseRule::parseInterfersWithDepsOption},
    {"-MV", ParseRule::parseInterfersWithDepsOption},
    {"-Wmissing-include-dirs", ParseRule::parseInterfersWithDepsOption},
    {"-Werror=missing-include-dirs", ParseRule::parseInterfersWithDepsOption},
    // Compile options
    {"-c", ParseRule::parseIsCompileOption},
    // Macros
    {"-D", ParseRule::parseIsMacro},
    // Redirects output
    {"-o", ParseRule::parseOptionRedirectsOutput},
    {"-MF", ParseRule::parseOptionRedirectsDepsOutput},
    {"-MT", ParseRule::parseOptionDepsRuleTarget},
    {"-MQ", ParseRule::parseOptionDepsRuleTarget},
    // Coverage options
    {"--coverage", ParseRule::parseOptionCoverageOutput},
    {"-ftest-coverage", ParseRule::parseOptionCoverageOutput},
    {"-fprofile-note", ParseRule::parseOptionRedirectsCoverageOutput},
    // Input paths
    {"-include", ParseRule::parseIsInputPathOption},
    {"-imacros", ParseRule::parseIsInputPathOption},
    {"-I", ParseRule::parseIsInputPathOption},
    {"-iquote", ParseRule::parseIsInputPathOption},
    {"-isystem", ParseRule::parseIsInputPathOption},
    {"-idirafter", ParseRule::parseIsInputPathOption},
    {"-iprefix", ParseRule::parseIsInputPathOption},
    {"-isysroot", ParseRule::parseIsInputPathOption},
    {"--sysroot", ParseRule::parseIsEqualInputPathOption},
    // Preprocessor arguments
    {"-Wp,", ParseRule::parseIsPreprocessorArgOption},
    {"-Xpreprocessor", ParseRule::parseIsPreprocessorArgOption},
    // Sets language
    {"-x", ParseRule::parseOptionSetsGccLanguage},
    // Debug Options
    {"-gsplit-dwarf", ParseRule::parseOptionSplitDwarf},
    // Options not supported
    {"-fprofile-use", ParseRule::parseOptionIsUnsupported},
    {"-fauto-profile", ParseRule::parseOptionIsUnsupported},
    {"-fbranch-probabilities", ParseRule::parseOptionIsUnsupported},
    {"-specs", ParseRule::parseOptionIsUnsupported},
    {"-M", ParseRule::parseOptionIsUnsupported},
    {"-MM", ParseRule::parseOptionIsUnsupported},
    {"-E", ParseRule::parseOptionIsUnsupported},
    {"-S", ParseRule::parseOptionIsUnsupported},
    {"-save-temps", ParseRule::parseOptionIsUnsupported},
    {"-fdump", ParseRule::parseOptionIsUnsupported},
    {"-march", ParseRule::parseOptionNative},
    {"-mtune", ParseRule::parseOptionNative},
    {"-mcpu", ParseRule::parseOptionNative},
    {"--param", ParseRule::parseOptionParam},
    {"-z", ParseRule::parseOptionParam},
};

static const ParsedCommandFactory::CompilerParseRulesMap GccPreprocessorRules =
    {
        // Interferes with dependencies
        {"-MD", ParseRule::parseInterfersWithDepsOption},
        {"-MMD", ParseRule::parseInterfersWithDepsOption},
        {"-M", ParseRule::parseOptionIsUnsupported},
        {"-MM", ParseRule::parseOptionIsUnsupported},
        {"-MG", ParseRule::parseInterfersWithDepsOption},
        {"-MP", ParseRule::parseInterfersWithDepsOption},
        {"-MV", ParseRule::parseInterfersWithDepsOption},
        // Redirects output
        {"-o", ParseRule::parseOptionRedirectsOutput},
        {"-MF", ParseRule::parseOptionRedirectsDepsOutput},
        {"-MT", ParseRule::parseOptionDepsRuleTarget},
        {"-MQ", ParseRule::parseOptionDepsRuleTarget},
        // Input paths
        {"-include", ParseRule::parseIsInputPathOption},
        {"-imacros", ParseRule::parseIsInputPathOption},
        {"-I", ParseRule::parseIsInputPathOption},
        {"-iquote", ParseRule::parseIsInputPathOption},
        {"-isystem", ParseRule::parseIsInputPathOption},
        {"-idirafter", ParseRule::parseIsInputPathOption},
        {"-iprefix", ParseRule::parseIsInputPathOption},
        {"-isysroot", ParseRule::parseIsInputPathOption},
        {"--sysroot", ParseRule::parseIsEqualInputPathOption},
};

static const ParsedCommandFactory::CompilerParseRulesMap SunCPPRules = {
    // Phase rules
    {"-Qoption", ParseRule::parseOptionSolarisPhase},
    // Interferes with dependencies
    {"-xMD", ParseRule::parseInterfersWithDepsOption},
    {"-xMMD", ParseRule::parseInterfersWithDepsOption},
    // Macros
    {"-D", ParseRule::parseIsMacro},
    // Redirects output
    {"-o", ParseRule::parseOptionRedirectsOutput},
    {"-xMF", ParseRule::parseOptionRedirectsDepsOutput},
    // Input paths
    {"-I", ParseRule::parseIsInputPathOption},
    {"-include", ParseRule::parseIsInputPathOption},
    // Compile options
    {"-c", ParseRule::parseIsCompileOption},
    // Rule needed to avoid substring matching `-xar` rule
    {"-xarch", ParseRule::parseOptionSimple},
    // Options not supported
    {"-xar", ParseRule::parseOptionIsUnsupported},
    {"-xpch", ParseRule::parseOptionIsUnsupported},
    {"-xprofile", ParseRule::parseOptionIsUnsupported},
    {"-###", ParseRule::parseOptionIsUnsupported},
    {"-xM", ParseRule::parseOptionIsUnsupported},
    {"-xM1", ParseRule::parseOptionIsUnsupported},
    {"-E", ParseRule::parseOptionIsUnsupported},
    {"-S", ParseRule::parseOptionIsUnsupported},
};

static const ParsedCommandFactory::CompilerParseRulesMap AixRules = {
    // Interferes with dependencies
    {"-qsyntaxonly", ParseRule::parseInterfersWithDepsOption},
    {"-M", ParseRule::parseInterfersWithDepsOption},
    {"-qmakedep", ParseRule::parseInterfersWithDepsOption},
    {"-qmakedep=gcc", ParseRule::parseInterfersWithDepsOption},
    // Macros
    {"-D", ParseRule::parseIsMacro},
    // Redirects output
    {"-o", ParseRule::parseOptionRedirectsOutput},
    {"-MF", ParseRule::parseOptionRedirectsDepsOutput},
    {"-qexpfile", ParseRule::parseOptionRedirectsOutput},
    // Input paths
    {"-qinclude", ParseRule::parseIsInputPathOption},
    {"-I", ParseRule::parseIsInputPathOption},
    {"-qcinc", ParseRule::parseIsInputPathOption},
    // Compile options
    {"-c", ParseRule::parseIsCompileOption},
    // Options not supported
    {"-#", ParseRule::parseOptionIsUnsupported},
    {"-qshowpdf", ParseRule::parseOptionIsUnsupported},
    {"-qdump_class_hierachy", ParseRule::parseOptionIsUnsupported},
    {"-E", ParseRule::parseOptionIsUnsupported},
    {"-S", ParseRule::parseOptionIsUnsupported},
};

static const ParsedCommandFactory::CompilerParseRulesMap LdRules = {
    {"-o", ParseRule::parseOptionRedirectsOutput},
    {"-L", ParseRule::parseLdLibraryPath},
    {"--library-path", ParseRule::parseLdLibraryPath},
    {"-l", ParseRule::parseLdLibrary},
    {"--library", ParseRule::parseLdLibrary},
    {"-rpath-link", ParseRule::parseLdLibraryPath},
    {"--rpath-link", ParseRule::parseLdLibraryPath},
    {"-rpath", ParseRule::parseLdLibraryPath},
    {"--rpath", ParseRule::parseLdLibraryPath},
    {"-R", ParseRule::parseLdLibraryPath},
    {"-Bdynamic", ParseRule::parseLdOptionDynamic},
    {"-dy", ParseRule::parseLdOptionDynamic},
    {"-call_shared", ParseRule::parseLdOptionDynamic},
    {"-Bstatic", ParseRule::parseLdOptionStatic},
    {"-dn", ParseRule::parseLdOptionStatic},
    {"-non_shared", ParseRule::parseLdOptionStatic},
    {"-static", ParseRule::parseLdOptionStatic},
    {"--push-state", ParseRule::parseLdOptionState},
    {"--pop-state", ParseRule::parseLdOptionState},
    {"-m", ParseRule::parseLdOptionEmulation},
    {"-soname", ParseRule::parseOptionParam},
    {"--soname", ParseRule::parseOptionParam},
    {"-z", ParseRule::parseOptionParam},
    // Options not supported
    {"--dependency-file", ParseRule::parseOptionIsUnsupported},
    {"--just-symbols", ParseRule::parseOptionIsUnsupported},
    {"-T", ParseRule::parseOptionIsUnsupported},
    {"--script", ParseRule::parseOptionIsUnsupported},
    {"-dT", ParseRule::parseOptionIsUnsupported},
    {"--default-script", ParseRule::parseOptionIsUnsupported},
    {"-Y", ParseRule::parseOptionIsUnsupported},
    {"--dynamic-list", ParseRule::parseOptionIsUnsupported},
    {"-Map", ParseRule::parseOptionIsUnsupported},
    {"--error-handling-script", ParseRule::parseOptionIsUnsupported},
    {"--out-implib", ParseRule::parseOptionIsUnsupported},
    {"--retain-symbols-file", ParseRule::parseOptionIsUnsupported},
    {"--sysroot", ParseRule::parseOptionIsUnsupported},
    {"--version-script", ParseRule::parseOptionIsUnsupported},
    {"-a", ParseRule::parseOptionIsUnsupported},
};

static const ParsedCommandFactory::CompilerParseRulesMap SolarisLdRules = {
    {"-o", ParseRule::parseOptionRedirectsOutput},
    {"-L", ParseRule::parseLdLibraryPath},
    {"--library-path", ParseRule::parseLdLibraryPath},
    {"-l", ParseRule::parseLdLibrary},
    {"--library", ParseRule::parseLdLibrary},
    {"-rpath", ParseRule::parseLdLibraryPath},
    {"-R", ParseRule::parseLdLibraryPath},
    {"-B", ParseRule::parseSolarisLdOptionB},
    {"-d", ParseRule::parseSolarisLdOptionD},
    {"-Y", ParseRule::parseSolarisLdOptionY},
    {"-h", ParseRule::parseOptionParam},
    {"-soname", ParseRule::parseOptionParam},
    {"-z", ParseRule::parseOptionParam},
    {"-u", ParseRule::parseIsMacro},
    {"-M", ParseRule::parseSolarisLdMapfile},
};

ParsedCommand ParsedCommandFactory::createParsedCommand(
    const std::vector<std::string> &command,
    const std::string &workingDirectory)
{
    if (command.empty()) {
        return ParsedCommand();
    }

    // Pass the option to the ParsedCommand constructor which will do things
    // such as populate various bools depending on if the compiler is of a
    // certain type.
    ParsedCommand parsedCommand(command, workingDirectory);

    // Get the map that maps compilers to options maps.
    const auto &parsedCommandMap = getParsedCommandMap();

    // Map containing options corresponding to compiler passed in.
    CompilerParseRulesMap rulesToUse;
    bool compilerFound = false;

    // Find the options map that corresponds to the compiler.
    for (const auto &val : parsedCommandMap) {
        auto it = val.first.find(parsedCommand.d_compiler);
        if (it != val.first.end()) {
            rulesToUse = val.second;
            compilerFound = true;
            break;
        }
    }

    if (!compilerFound) {
        // Don't attempt to parse arguments of unsupported command
        parsedCommand.d_containsUnsupportedOptions = true;
        return parsedCommand;
    }

    // Parse and construct the command, and deps command vector.
    parseCommand(&parsedCommand, rulesToUse, workingDirectory);

    // If unsupported options or there are no input files, set compile command
    // to false, and return the constructed parsedCommand.
    if (parsedCommand.d_containsUnsupportedOptions ||
        parsedCommand.d_inputFiles.empty()) {
        parsedCommand.d_compilerCommand = false;
        return parsedCommand;
    }

    if (!parsedCommand.is_compiler_command()) {
        // Command without `-c` and without unsupported options
        parsedCommand.d_linkerCommand = true;
    }

    // Handle gccpreprocessor options which were populated during the original
    // parsing of the command.
    // These options require special flags, before each option.
    if (parsedCommand.d_preProcessorOptions.size() > 0) {
        ParsedCommand preprocessorCommand;
        // Set preprecessor command to that created from parsing original
        // command, so it can be parsed.
        preprocessorCommand.d_originalCommand.insert(
            preprocessorCommand.d_originalCommand.begin(),
            parsedCommand.d_preProcessorOptions.begin(),
            parsedCommand.d_preProcessorOptions.end());

        parseCommand(&preprocessorCommand, GccPreprocessorRules,
                     workingDirectory);

        for (const auto &preproArg : preprocessorCommand.d_command) {
            parsedCommand.d_command.push_back("-Xpreprocessor");
            parsedCommand.d_command.push_back(preproArg);
        }

        for (const auto &preproArg :
             preprocessorCommand.d_dependenciesCommand) {
            parsedCommand.d_dependenciesCommand.push_back("-Xpreprocessor");
            parsedCommand.d_dependenciesCommand.push_back(preproArg);
        }

        for (const auto &preproArg : preprocessorCommand.d_commandProducts) {
            parsedCommand.d_commandProducts.insert(preproArg);
        }

        for (const auto &preproArg :
             preprocessorCommand.d_commandDepsProducts) {
            parsedCommand.d_commandDepsProducts.insert(preproArg);
        }

        parsedCommand.d_md_option_set = preprocessorCommand.d_md_option_set ||
                                        parsedCommand.d_md_option_set;
    }

    // Insert default deps options into newly constructed parsedCommand deps
    // vector.
    // This vector is populated by the ParsedCommand constructor depending on
    // the compiler specified in the command.
    parsedCommand.d_dependenciesCommand.insert(
        parsedCommand.d_dependenciesCommand.end(),
        parsedCommand.d_defaultDepsCommand.begin(),
        parsedCommand.d_defaultDepsCommand.end());

    // d_originalCommand gets modified during the parsing of the
    // command-> Reset it.
    parsedCommand.d_originalCommand.insert(
        parsedCommand.d_originalCommand.begin(), command.begin(),
        command.end());

    return parsedCommand;
}

ParsedCommand ParsedCommandFactory::createParsedLinkerCommand(
    const std::vector<std::string> &command,
    const std::string &workingDirectory)
{
    ParsedCommand parsedCommand(command, workingDirectory);

    // Parse and construct the command
#ifdef __sun
    parseCommand(&parsedCommand, SolarisLdRules, workingDirectory);
#else
    parseCommand(&parsedCommand, LdRules, workingDirectory);
#endif

    if (parsedCommand.d_containsUnsupportedOptions ||
        parsedCommand.is_compiler_command()) {
        return parsedCommand;
    }

    parsedCommand.d_linkerCommand = true;

    // d_originalCommand gets modified during the parsing of the
    // command-> Reset it.
    parsedCommand.d_originalCommand.insert(
        parsedCommand.d_originalCommand.begin(), command.begin(),
        command.end());

    return parsedCommand;
}

ParsedCommand
ParsedCommandFactory::createParsedCommand(char **argv,
                                          const char *workingDirectory)
{
    return createParsedCommand(vectorFromArgv(argv), workingDirectory);
}

ParsedCommand ParsedCommandFactory::createParsedCommand(
    std::initializer_list<std::string> command)
{
    return createParsedCommand(std::vector<std::string>(command), "");
}

void ParsedCommandFactory::parseCommand(
    ParsedCommand *command, const CompilerParseRulesMap &parseRules,
    const std::string &workingDirectory)
{
    // Iterate through the command, searching for options to match to parse
    // rules. If there's a match, apply the parse rule to the option.
    while (!command->d_originalCommand.empty()) {
        const auto &currToken = command->d_originalCommand.front();

        const auto &optionModifier =
            ParseRuleHelper::matchCompilerOptions(currToken, parseRules);

        if (optionModifier.second) {
            optionModifier.second(command, workingDirectory,
                                  optionModifier.first);
        }
        else if (currToken == "-") {
            BUILDBOX_LOG_WARNING("recc does not support standard input");
            command->d_containsUnsupportedOptions = true;
            command->d_originalCommand.pop_front();
        }
        else if (!currToken.empty() && currToken.front() == '@') {
            BUILDBOX_LOG_WARNING("recc does not support reading command-line "
                                 "options from a file");
            command->d_containsUnsupportedOptions = true;
            command->d_originalCommand.pop_front();
        }
        else if (!currToken.empty() &&
                 (currToken.front() == '-' ||
                  (command->is_sun_studio() && currToken.front() == '+'))) {
            // Option without handler
            // Solaris Studio uses both `-` and `+` as option prefix
            ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                                   false, true);
        }
        else {
            std::string replacedPath;

            replacedPath =
                FileUtils::modifyPathForRemote(currToken, workingDirectory);

            command->d_command.push_back(replacedPath);
            command->d_dependenciesCommand.push_back(currToken);
            command->d_inputFiles.push_back(currToken);
            command->d_originalCommand.pop_front();
        }
    } // end while
}

std::vector<std::string>
ParsedCommandFactory::vectorFromArgv(const char *const *argv)
{
    std::vector<std::string> result;
    int i = 0;

    std::ostringstream arg_string;
    arg_string << "Parsing command:" << std::endl;
    while (argv[i] != nullptr) {
        std::string argstr = std::string(argv[i]);
        ++i;

        arg_string << "argv[" << i << "] = " << argstr << std::endl;
        result.push_back(argstr);
    }
    BUILDBOX_LOG_DEBUG(arg_string.str());

    return result;
}

ParsedCommandFactory::ParsedCommandMap
ParsedCommandFactory::getParsedCommandMap()
{
    return {
        {SupportedCompilers::Gcc, GccRules},
        {SupportedCompilers::GccPreprocessor, GccPreprocessorRules},
        {SupportedCompilers::SunCPP, SunCPPRules},
        {SupportedCompilers::AIX, AixRules},
    };
}

std::pair<std::string, std::function<void(ParsedCommand *, const std::string &,
                                          const std::string &)>>
ParseRuleHelper::matchCompilerOptions(
    const std::string &option,
    const ParsedCommandFactory::CompilerParseRulesMap &options)
{
    auto tempOption = option;

    if (!tempOption.empty() &&
        (tempOption.front() == '-' || tempOption.front() == '+')) {

        // Check for an equal sign, if any, return left side.
        tempOption = tempOption.substr(0, tempOption.find("="));

        // First try finding an exact match, removing and parsing until an
        // equal sign. Remove any spaces from the option.
        tempOption.erase(
            remove_if(tempOption.begin(), tempOption.end(), ::isspace),
            tempOption.end());

        if (options.count(tempOption) > 0) {
            return std::make_pair(tempOption, options.at(tempOption));
        }

        // Second, try a substring search, iterating through all the
        // options in the map.
        for (const auto &option_map_val : options) {
            const auto val = option.substr(0, option_map_val.first.length());
            if (val == option_map_val.first) {
                return std::make_pair(option_map_val.first,
                                      option_map_val.second);
            }
        }
    }

    return std::make_pair("", nullptr);
}

void ParseRule::parseOptionSimple(ParsedCommand *command,
                                  const std::string &workingDirectory,
                                  const std::string &)
{
    ParseRuleHelper::appendAndRemoveOption(command, workingDirectory, false,
                                           true);
}

void ParseRule::parseInterfersWithDepsOption(ParsedCommand *command,
                                             const std::string &,
                                             const std::string &option)
{
    if (command->d_originalCommand.front() == "-MMD" ||
        command->d_originalCommand.front() == "-MD" ||
        command->d_originalCommand.front() == "-xMMD" ||
        command->d_originalCommand.front() == "-xMD") {
        command->d_md_option_set = true;
    }
    else if (command->is_AIX() && (option == "-M" || option == "-qmakedep")) {
        command->d_qmakedep_option_set = true;
    }
    else if (command->d_originalCommand.front() == "-Wmissing-include-dirs" ||
             command->d_originalCommand.front() ==
                 "-Werror=missing-include-dirs") {
        command->d_upload_all_include_dirs = true;
    }

    // Only push back to command vector.
    command->d_command.push_back(command->d_originalCommand.front());
    command->d_originalCommand.pop_front();
}

void ParseRule::parseIsInputPathOption(ParsedCommand *command,
                                       const std::string &workingDirectory,
                                       const std::string &option)
{
    ParseRuleHelper::parseGccOption(command, workingDirectory, option);
}

void ParseRule::parseIsEqualInputPathOption(
    ParsedCommand *command, const std::string &workingDirectory,
    const std::string &option)
{
    ParseRuleHelper::parseGccOption(command, workingDirectory, option);
}

void ParseRule::parseIsCompileOption(ParsedCommand *command,
                                     const std::string &workingDirectory,
                                     const std::string &)
{
    command->d_compilerCommand = true;
    // Push back option (e.g "-c")
    ParseRuleHelper::appendAndRemoveOption(command, workingDirectory, false,
                                           true);
}

void ParseRule::parseOptionRedirectsOutput(ParsedCommand *command,
                                           const std::string &workingDirectory,
                                           const std::string &option)
{
    ParseRuleHelper::parseGccOption(command, workingDirectory, option, false,
                                    true);
}

void ParseRule::parseOptionRedirectsDepsOutput(
    ParsedCommand *command, const std::string &workingDirectory,
    const std::string &option)
{
    ParseRuleHelper::parseGccOption(command, workingDirectory, option, false,
                                    true, true);
}

void ParseRule::parseOptionDepsRuleTarget(ParsedCommand *command,
                                          const std::string &workingDirectory,
                                          const std::string &option)
{
    ParseRuleHelper::parseGccOption(command, workingDirectory, option, false);
}

void ParseRule::parseIsPreprocessorArgOption(ParsedCommand *command,
                                             const std::string &,
                                             const std::string &option)
{
    auto val = command->d_originalCommand.front();
    if (option == "-Wp,") {
        // parse comma separated list of args, and store in
        // commands preprocessor vector.
        auto optionList = val.substr(option.size());
        ParseRuleHelper::parseStageOptionList(optionList,
                                              &command->d_preProcessorOptions);
    }
    else if (option == "-Xpreprocessor") {
        // push back next arg
        command->d_originalCommand.pop_front();
        command->d_preProcessorOptions.push_back(
            command->d_originalCommand.front());
    }

    command->d_originalCommand.pop_front();
}

void ParseRule::parseIsMacro(ParsedCommand *command, const std::string &,
                             const std::string &option)
{
    /*
     * This can come in four forms:
     * 1. -Dname
     * 2. -Dname=definition
     * 3. -D name
     * 4. -D name=definition
     *
     * We just need to make sure we handle the cases where there's a
     * space between the -D flag and the rest of the arguments.
     */
    const std::string token = command->d_originalCommand.front();
    command->d_command.push_back(token);
    command->d_dependenciesCommand.push_back(token);
    if (token == option) {
        command->d_originalCommand.pop_front();
        const std::string arg = command->d_originalCommand.front();
        command->d_command.push_back(arg);
        command->d_dependenciesCommand.push_back(arg);
    }
    command->d_originalCommand.pop_front();
}

void ParseRule::parseOptionSetsGccLanguage(ParsedCommand *command,
                                           const std::string &workingDirectory,
                                           const std::string &option)
{
    const std::string originalCommandOption =
        command->d_originalCommand.front();
    command->d_originalCommand.pop_front();

    std::string language = "";

    if (originalCommandOption == option) {
        // Space between -x and argument, e.g. "-x assembler"
        if (command->d_originalCommand.empty()) {
            // The -x was at the end of the command with no argument
            BUILDBOX_LOG_WARNING("gcc's \"-x\" flag requires an argument");
            command->d_containsUnsupportedOptions = true;
            return;
        }
        language = command->d_originalCommand.front();
    }
    else {
        // No space, e.g. "-xassembler"
        // Note that gcc -x does not understand an equals sign. If "-x=c++"
        // is provided, the language is treated as "=c++"
        language = originalCommandOption.substr(option.length());
    }

    command->d_originalCommand.push_front(originalCommandOption);

    if (!SupportedCompilers::GccSupportedLanguages.count(language)) {
        BUILDBOX_LOG_WARNING("recc does not support the language [" << language
                                                                    << "].");
        command->d_containsUnsupportedOptions = true;
    }

    ParseRuleHelper::parseGccOption(command, workingDirectory, option);
}

void ParseRule::parseOptionIsUnsupported(ParsedCommand *command,
                                         const std::string &,
                                         const std::string &)
{
    command->d_containsUnsupportedOptions = true;

    // append the rest of the command and deps command vector.
    command->d_dependenciesCommand.insert(command->d_dependenciesCommand.end(),
                                          command->d_originalCommand.begin(),
                                          command->d_originalCommand.end());

    command->d_command.insert(command->d_command.end(),
                              command->d_originalCommand.begin(),
                              command->d_originalCommand.end());

    // clear the original command so parsing stops.
    command->d_originalCommand.clear();
}

void ParseRule::parseOptionCoverageOutput(ParsedCommand *command,
                                          const std::string &,
                                          const std::string &)
{
    // A gcno file will be created, add it to the commandProducts
    const std::string originalCommandOption =
        command->d_originalCommand.front();
    command->d_originalCommand.pop_front();

    command->d_coverage_option_set = true;
    command->d_command.push_back(originalCommandOption);
}

void ParseRule::parseOptionSplitDwarf(ParsedCommand *command,
                                      const std::string &workingDirectory,
                                      const std::string &)
{
    command->d_split_dwarf_option_set = true;
    ParseRuleHelper::appendAndRemoveOption(command, workingDirectory, false,
                                           true);
}

void ParseRule::parseOptionSolarisPhase(ParsedCommand *command,
                                        const std::string &workingDirectory,
                                        const std::string &option)
{
    // See https://docs.oracle.com/cd/E19205-01/819-5267/bkavy/index.html
    // Pop the -Qoption, phase, and option arguments.
    if (command->d_originalCommand.size() < 3) {
        ParseRule::parseOptionIsUnsupported(command, workingDirectory, option);
        return;
    }
    for (int i = 0; i < 3; i++) {
        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, true);
    }
}

void ParseRule::parseOptionRedirectsCoverageOutput(
    ParsedCommand *command, const std::string &workingDirectory,
    const std::string &option)
{
    // If a gcno file would be created, it will be created
    // at the specified location
    const std::string originalCommandOption =
        command->d_originalCommand.front();
    command->d_originalCommand.pop_front();

    auto currentOption = option;
    const auto equalPos = originalCommandOption.find('=');
    if (equalPos != std::string::npos) {
        auto optionPath = originalCommandOption.substr(equalPos + 1);
        const std::string replacedPath =
            FileUtils::modifyPathForRemote(optionPath, workingDirectory);

        command->d_commandCoverageProducts.insert(replacedPath);
        command->d_command.push_back(originalCommandOption);
    }
    else {
        BUILDBOX_LOG_WARNING(
            "gcc's \"-fprofile-note\" option requires an argument");
        command->d_containsUnsupportedOptions = true;
    }
}

void ParseRule::parseOptionNative(ParsedCommand *command,
                                  const std::string &workingDirectory,
                                  const std::string &option)
{
    const std::string originalCommandOption =
        command->d_originalCommand.front();
    const auto equalPos = originalCommandOption.find('=');
    if (equalPos != std::string::npos) {
        const auto optionMachine = originalCommandOption.substr(equalPos + 1);
        if (optionMachine == "native") {
            BUILDBOX_LOG_WARNING(
                "\"native\" machine type builds cannot be cached ["
                << originalCommandOption << "]");
            ParseRule::parseOptionIsUnsupported(command, workingDirectory,
                                                option);
            return;
        }
    }
    else {
        BUILDBOX_LOG_DEBUG("malformed machine type option ["
                           << originalCommandOption << "]");
    }

    ParseRuleHelper::appendAndRemoveOption(command, workingDirectory, false,
                                           true);
}

void ParseRule::parseOptionParam(ParsedCommand *command,
                                 const std::string &workingDirectory,
                                 const std::string &option)
{
    // https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html#index-param
    auto val = command->d_originalCommand.front();
    if (val == option) {
        if (command->d_originalCommand.size() < 2) {
            ParseRule::parseOptionIsUnsupported(command, workingDirectory,
                                                option);
            return;
        }
        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, true);
        // Push back corresponding (key=value)
        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, true);
    }
    else {
        // If "=" sign between option and path. (--param=ggc-min-expand=30)
        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, true);
    }
}

void ParseRule::parseLdLibrary(ParsedCommand *command,
                               const std::string &workingDirectory,
                               const std::string &option)
{
    std::string library;
    const auto val = command->d_originalCommand.front();

    // Space between option and library name (-l foo)
    if (val == option) {
        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, false);

        library = command->d_originalCommand.front();

        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, false);
    }
    // No space between option and library name (-lfoo)
    // Or if "=" sign between option and path. (--library=/usr/lib)
    else {
        const auto equalPos = val.find('=');
        library = val.substr(option.size());

        if (equalPos != std::string::npos) {
            library = val.substr(equalPos + 1);
        }

        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, false);
    }

    if (library.empty()) {
        ParseRule::parseOptionIsUnsupported(command, workingDirectory, option);
        return;
    }

    if (command->d_bstatic) {
        command->d_staticLibraries.insert(library);
    }
    else {
        command->d_libraries.insert(library);
    }
}

void ParseRule::parseLdLibraryPath(ParsedCommand *command,
                                   const std::string &workingDirectory,
                                   const std::string &option)
{
    std::string libraryPath;
    const auto val = command->d_originalCommand.front();
    command->d_originalCommand.pop_front();

    // Space between option and input path (-L /usr/lib)
    if (val == option) {
        libraryPath = command->d_originalCommand.front();
        command->d_originalCommand.pop_front();
    }
    // No space between option and path (-L/usr/lib)
    // Or if "=" sign between option and path. (--library-path=/usr/lib)
    else {
        libraryPath = val.substr(option.size());

        if (libraryPath.substr(0, 1) == "=") {
            libraryPath = libraryPath.substr(1);
        }
    }

    if (libraryPath.empty()) {
        ParseRule::parseOptionIsUnsupported(command, workingDirectory, option);
        return;
    }

    std::vector<std::string> *libraryDirs;
    if (option == "-rpath-link" || option == "--rpath-link") {
        libraryDirs = &command->d_rpathLinkDirs;
    }
    else if (option == "-rpath" || option == "--rpath" || option == "-R") {
        libraryDirs = &command->d_rpathDirs;
    }
    else {
        libraryDirs = &command->d_libraryDirs;
    }

    std::istringstream iss(libraryPath);
    char separator = ':';
    for (std::string token; std::getline(iss, token, separator);) {
        if (buildboxcommon::FileUtils::isDirectory(token.c_str())) {
            command->d_command.push_back(option);
            const std::string replacedPath =
                FileUtils::modifyPathForRemote(token, workingDirectory);
            command->d_command.push_back(replacedPath);

            libraryDirs->push_back(token);
        }
        else if (option == "-R" &&
                 buildboxcommon::FileUtils::isRegularFile(token.c_str())) {
            // `-R` is `--just-symbols` if the argument is a regular file
            ParseRule::parseOptionIsUnsupported(command, workingDirectory,
                                                option);
            return;
        }
    }
}

void ParseRule::parseLdOptionDynamic(ParsedCommand *command,
                                     const std::string &workingDirectory,
                                     const std::string &)
{
    command->d_bstatic = false;

    ParseRuleHelper::appendAndRemoveOption(command, workingDirectory, false,
                                           true);
}

void ParseRule::parseLdOptionStatic(ParsedCommand *command,
                                    const std::string &workingDirectory,
                                    const std::string &)
{
    command->d_bstatic = true;

    ParseRuleHelper::appendAndRemoveOption(command, workingDirectory, false,
                                           true);
}

void ParseRule::parseLdOptionState(ParsedCommand *command,
                                   const std::string &workingDirectory,
                                   const std::string &option)
{
    if (option == "--push-state") {
        command->d_bstaticStack.push_back(command->d_bstatic);
    }
    else if (option == "--pop-state" && !command->d_bstaticStack.empty()) {
        command->d_bstatic = command->d_bstaticStack.back();
        command->d_bstaticStack.pop_back();
    }
    else {
        ParseRule::parseOptionIsUnsupported(command, workingDirectory, option);
        return;
    }

    ParseRuleHelper::appendAndRemoveOption(command, workingDirectory, false,
                                           true);
}

void ParseRule::parseLdOptionEmulation(ParsedCommand *command,
                                       const std::string &,
                                       const std::string &option)
{
    // Support the `-m` flag with and without space
    const std::string token = command->d_originalCommand.front();
    command->d_command.push_back(token);
    command->d_dependenciesCommand.push_back(token);
    if (token == option) {
        command->d_originalCommand.pop_front();
        const std::string arg = command->d_originalCommand.front();
        command->d_command.push_back(arg);
        command->d_dependenciesCommand.push_back(arg);
    }
    command->d_originalCommand.pop_front();
}

void ParseRuleHelper::parseGccOption(ParsedCommand *command,
                                     const std::string &workingDirectory,
                                     const std::string &option, bool toDeps,
                                     bool isOutput, bool depsOutput)
{
    auto val = command->d_originalCommand.front();
    // Space between option and input path (-I /usr/bin/include)
    if (val == option) {
        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, toDeps);
        // Push back corresponding path, but not into deps command
        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory, true,
                                               toDeps, isOutput, depsOutput);
    }
    // No space between option and path (-I/usr/bin/include)
    // Or if "=" sign between option and path. (-I=/usr/bin/include)
    else {
        const auto equalPos = val.find('=');
        auto optionPath = val.substr(option.size());
        auto modifiedOption = option;

        if (equalPos != std::string::npos) {
            modifiedOption += "=";
            optionPath = val.substr(equalPos + 1);
        }

        const std::string replacedPath =
            FileUtils::modifyPathForRemote(optionPath, workingDirectory);

        // Include path in include directories
        const std::string local_normalized_path =
            buildboxcommon::FileUtils::normalizePath(optionPath.c_str());
        if (buildboxcommon::FileUtils::isDirectory(
                local_normalized_path.c_str())) {
            command->d_includeDirs.emplace(replacedPath);
        }

        command->d_command.push_back(modifiedOption + replacedPath);

        if (isOutput and !depsOutput) {
            command->d_commandProducts.insert(replacedPath);
        }
        else if (isOutput) {
            command->d_commandDepsProducts.insert(replacedPath);
        }
        else if (toDeps) {
            command->d_dependenciesCommand.push_back(modifiedOption +
                                                     optionPath);
        }

        command->d_originalCommand.pop_front();
    }
}

void ParseRule::parseSolarisLdOptionB(ParsedCommand *command,
                                      const std::string &workingDirectory,
                                      const std::string &option)
{
    std::string arg;
    auto val = command->d_originalCommand.front();
    if (val == option) {
        // Space between option and argument
        if (command->d_originalCommand.size() < 2) {
            ParseRule::parseOptionIsUnsupported(command, workingDirectory,
                                                option);
            return;
        }
        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, true);
        arg = command->d_originalCommand.front();
        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, true);
    }
    else {
        // No space between option and argument
        arg = val.substr(option.size());
        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, true);
    }

    if (arg == "dynamic") {
        command->d_bstatic = false;
    }
    else if (arg == "static") {
        command->d_bstatic = true;
    }
}

void ParseRule::parseSolarisLdOptionD(ParsedCommand *command,
                                      const std::string &workingDirectory,
                                      const std::string &option)
{
    std::string arg;
    auto val = command->d_originalCommand.front();
    if (val == option) {
        // Space between option and argument
        if (command->d_originalCommand.size() < 2) {
            ParseRule::parseOptionIsUnsupported(command, workingDirectory,
                                                option);
            return;
        }
        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, true);
        arg = command->d_originalCommand.front();
        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, true);
    }
    else {
        // No space between option and argument
        arg = val.substr(option.size());
        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, true);
    }

    if (arg == "y") {
        command->d_bstatic = false;
    }
    else if (arg == "n") {
        command->d_bstatic = true;
    }
}

void ParseRule::parseSolarisLdOptionY(ParsedCommand *command,
                                      const std::string &workingDirectory,
                                      const std::string &option)
{
    std::string arg;
    auto val = command->d_originalCommand.front();
    if (val == option) {
        // Space between option and argument
        if (command->d_originalCommand.size() < 2) {
            ParseRule::parseOptionIsUnsupported(command, workingDirectory,
                                                option);
            return;
        }
        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, true);
        arg = command->d_originalCommand.front();
        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, true);
    }
    else {
        // No space between option and argument
        arg = val.substr(option.size());
        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, true);
    }

    if (arg.substr(0, 2) == "P,") {
        command->d_defaultLibraryDirs.clear();

        std::istringstream iss(arg.substr(2));
        char separator = ':';
        for (std::string token; std::getline(iss, token, separator);) {
            if (buildboxcommon::FileUtils::isDirectory(token.c_str())) {
                command->d_defaultLibraryDirs.push_back(token);
            }
        }
    }
    else {
        ParseRule::parseOptionIsUnsupported(command, workingDirectory, option);
        return;
    }
}

void ParseRule::parseSolarisLdMapfile(ParsedCommand *command,
                                      const std::string &workingDirectory,
                                      const std::string &option)
{
    std::string mapfile;
    const auto val = command->d_originalCommand.front();

    // Space between option and filename (-M foo)
    if (val == option) {
        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, false);

        mapfile = command->d_originalCommand.front();

        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, false);
    }
    // No space between option and library name (-Mfoo)
    else {
        mapfile = val.substr(option.size());

        ParseRuleHelper::appendAndRemoveOption(command, workingDirectory,
                                               false, false);
    }

    if (mapfile.empty()) {
        ParseRule::parseOptionIsUnsupported(command, workingDirectory, option);
        return;
    }

    command->d_auxInputFiles.push_back(mapfile);
}

void ParseRuleHelper::appendAndRemoveOption(
    ParsedCommand *command, const std::string &workingDirectory, bool isPath,
    bool toDeps, bool isOutput, bool depsOutput)
{
    auto option = command->d_originalCommand.front();
    if (isPath) {

        const std::string replacedPath =
            FileUtils::modifyPathForRemote(option, workingDirectory);

        // Include path in include directories
        const std::string local_normalized_path =
            buildboxcommon::FileUtils::normalizePath(option.c_str());
        if (buildboxcommon::FileUtils::isDirectory(
                local_normalized_path.c_str())) {
            command->d_includeDirs.emplace(replacedPath);
        }

        // If pushing back to dependencies command, do not replace the
        // path since this will be run locally.
        if (toDeps) {
            command->d_dependenciesCommand.push_back(option);
        }
        command->d_command.push_back(replacedPath);

        if (isOutput and !depsOutput) {
            command->d_commandProducts.insert(replacedPath);
        }
        else if (isOutput) {
            command->d_commandDepsProducts.insert(replacedPath);
        }
    }
    else {
        // Append option to both vectors.
        command->d_command.push_back(option);
        if (toDeps) {
            command->d_dependenciesCommand.push_back(option);
        }
    }

    // Remove from original_command
    command->d_originalCommand.pop_front();
}

void ParseRuleHelper::parseStageOptionList(const std::string &option,
                                           std::vector<std::string> *result)
{
    bool quoted = false;
    std::string current;
    for (const char &character : option) {
        if (character == '\'') {
            quoted = !quoted;
        }
        else if (character == ',' && !quoted) {
            result->push_back(current);
            current = std::string();
        }
        else {
            current += character;
        }
    }
    result->push_back(current);
}

} // namespace recc
