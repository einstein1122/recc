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

#include <compilerdefaults.h>
#include <env.h>
#include <gtest/gtest.h>
#include <parsedcommand.h>
#include <parsedcommandfactory.h>

using namespace recc;

TEST(VectorFromArgvTest, EmptyArgv)
{
    const char *argv[] = {nullptr};
    EXPECT_EQ(ParsedCommandFactory::vectorFromArgv(argv),
              std::vector<std::string>());
}

TEST(VectorFromArgvTest, OneItemArgv)
{
    const char *argv[] = {"gcc", nullptr};
    std::vector<std::string> expected = {"gcc"};

    EXPECT_EQ(ParsedCommandFactory::vectorFromArgv(argv), expected);
}

TEST(VectorFromArgvTest, MultiItemArgv)
{
    const char *argv[] = {"test", "", "of long", "argv", nullptr};
    std::vector<std::string> expected = {"test", "", "of long", "argv"};

    EXPECT_EQ(ParsedCommandFactory::vectorFromArgv(argv), expected);
}

TEST(TestParsedCommandFactory, emptyCommand)
{
    const std::vector<std::string> command = {};

    auto parsedCommand =
        ParsedCommandFactory::createParsedCommand(command, "/home/nobody/");

    const std::vector<std::string> expectedCommand = {};

    // Change once deps are handled.
    const std::vector<std::string> expectedDepsCommand = {};

    EXPECT_EQ(expectedCommand, parsedCommand.get_command());
    EXPECT_EQ(expectedDepsCommand, parsedCommand.get_dependencies_command());
    EXPECT_EQ("", parsedCommand.get_aix_dependency_file_name());
    EXPECT_EQ(false, parsedCommand.is_AIX());
    EXPECT_EQ(false, parsedCommand.is_clang());
    EXPECT_EQ(false, parsedCommand.is_compiler_command());
}

TEST(TestParsedCommandFactory, testInputPathsAndGCC)
{
    RECC_PROJECT_ROOT = "/home/nobody/";
    const std::vector<std::string> command = {
        "gcc", "-c", "/home/nobody/test/hello.c",
        "-I/home/nobody/test/include/user.h"};

    auto parsedCommand =
        ParsedCommandFactory::createParsedCommand(command, "/home/nobody/");

    const std::vector<std::string> expectedCommand = {
        "gcc", "-c", "test/hello.c", "-Itest/include/user.h"};

    // Change once deps are handled.
    const std::vector<std::string> expectedDepsCommand = {
        "gcc", "-c", "/home/nobody/test/hello.c",
        "-I/home/nobody/test/include/user.h", "-M"};

    EXPECT_EQ(expectedCommand, parsedCommand.get_command());
    EXPECT_EQ(expectedDepsCommand, parsedCommand.get_dependencies_command());
    EXPECT_EQ(false, parsedCommand.is_AIX());
    EXPECT_EQ("", parsedCommand.get_aix_dependency_file_name());
    EXPECT_EQ(false, parsedCommand.is_clang());
    EXPECT_EQ(true, parsedCommand.is_compiler_command());
}

TEST(TestParsedCommandFactory, testUnsupportedLanguage)
{
    RECC_PROJECT_ROOT = "/home/nobody/";
    const std::vector<std::string> command = {
        "gcc",
        "-x",
        "assembler",
        "-c",
        "/home/nobody/test/hello.c",
        "-I/home/nobody/test/include/user.h"};

    auto parsedCommand =
        ParsedCommandFactory::createParsedCommand(command, "/home/nobody/");

    const std::vector<std::string> expectedCommand = {
        "gcc", "-x",           "assembler",
        "-c",  "test/hello.c", "-Itest/include/user.h"};

    // parseCommand() sees that the option was invalid, so the deps command
    // isn't calculated.
    const std::vector<std::string> expectedDepsCommand = {
        "gcc",
        "-x",
        "assembler",
        "-c",
        "/home/nobody/test/hello.c",
        "-I/home/nobody/test/include/user.h"};

    EXPECT_EQ(expectedCommand, parsedCommand.get_command());
    EXPECT_EQ(expectedDepsCommand, parsedCommand.get_dependencies_command());
    EXPECT_EQ("", parsedCommand.get_aix_dependency_file_name());
    EXPECT_EQ(false, parsedCommand.is_AIX());
    EXPECT_EQ(false, parsedCommand.is_clang());
    EXPECT_EQ(false, parsedCommand.is_compiler_command());
}

TEST(TestParsedCommandFactory, testUnsupportedLanguageNoSpace)
{
    RECC_PROJECT_ROOT = "/home/nobody/";
    const std::vector<std::string> command = {
        "gcc", "-xassembler", "-c", "/home/nobody/test/hello.c",
        "-I/home/nobody/test/include/user.h"};

    auto parsedCommand =
        ParsedCommandFactory::createParsedCommand(command, "/home/nobody/");

    const std::vector<std::string> expectedCommand = {
        "gcc", "-xassembler", "-c", "test/hello.c", "-Itest/include/user.h"};

    // parseCommand() sees that the option was invalid, so the deps command
    // isn't calculated.
    const std::vector<std::string> expectedDepsCommand = {
        "gcc", "-xassembler", "-c", "/home/nobody/test/hello.c",
        "-I/home/nobody/test/include/user.h"};

    EXPECT_EQ(expectedCommand, parsedCommand.get_command());
    EXPECT_EQ(expectedDepsCommand, parsedCommand.get_dependencies_command());
    EXPECT_EQ("", parsedCommand.get_aix_dependency_file_name());
    EXPECT_EQ(false, parsedCommand.is_AIX());
    EXPECT_EQ(false, parsedCommand.is_clang());
    EXPECT_EQ(false, parsedCommand.is_compiler_command());
}

TEST(TestParsedCommandFactory, testSupportedLanguage)
{
    RECC_PROJECT_ROOT = "/home/nobody/";
    const std::vector<std::string> command = {
        "gcc",
        "-x",
        "c",
        "-c",
        "/home/nobody/test/hello.c",
        "-I/home/nobody/test/include/user.h"};

    auto parsedCommand =
        ParsedCommandFactory::createParsedCommand(command, "/home/nobody/");

    const std::vector<std::string> expectedCommand = {
        "gcc", "-x", "c", "-c", "test/hello.c", "-Itest/include/user.h"};

    // The language is valid, so the deps command is computed as expected
    const std::vector<std::string> expectedDepsCommand = {
        "gcc",
        "-x",
        "c",
        "-c",
        "/home/nobody/test/hello.c",
        "-I/home/nobody/test/include/user.h",
        "-M"};

    EXPECT_EQ(expectedCommand, parsedCommand.get_command());
    EXPECT_EQ(expectedDepsCommand, parsedCommand.get_dependencies_command());
    EXPECT_EQ("", parsedCommand.get_aix_dependency_file_name());
    EXPECT_EQ(false, parsedCommand.is_AIX());
    EXPECT_EQ(false, parsedCommand.is_clang());
    EXPECT_EQ(true, parsedCommand.is_compiler_command());
}

TEST(TestParsedCommandFactory, testSupportedLanguageNoSpace)
{
    RECC_PROJECT_ROOT = "/home/nobody/";
    const std::vector<std::string> command = {
        "gcc", "-xc", "-c", "/home/nobody/test/hello.c",
        "-I/home/nobody/test/include/user.h"};

    auto parsedCommand =
        ParsedCommandFactory::createParsedCommand(command, "/home/nobody/");

    const std::vector<std::string> expectedCommand = {
        "gcc", "-xc", "-c", "test/hello.c", "-Itest/include/user.h"};

    // The language is valid, so the deps command is computed as expected
    const std::vector<std::string> expectedDepsCommand = {
        "gcc",
        "-xc",
        "-c",
        "/home/nobody/test/hello.c",
        "-I/home/nobody/test/include/user.h",
        "-M"};

    EXPECT_EQ(expectedCommand, parsedCommand.get_command());
    EXPECT_EQ(expectedDepsCommand, parsedCommand.get_dependencies_command());
    EXPECT_EQ("", parsedCommand.get_aix_dependency_file_name());
    EXPECT_EQ(false, parsedCommand.is_AIX());
    EXPECT_EQ(false, parsedCommand.is_clang());
    EXPECT_EQ(true, parsedCommand.is_compiler_command());
}

TEST(TestParsedCommandFactory, testEqualInputPaths)
{
    RECC_PROJECT_ROOT = "/home/nobody/";
    const std::vector<std::string> command = {
        "gcc", "-c", "/home/nobody/test/hello.c",
        "-I/home/nobody/test/include/user.h", "--sysroot=/home/nobody/test"};

    auto parsedCommand =
        ParsedCommandFactory::createParsedCommand(command, "/home/nobody/");

    const std::vector<std::string> expectedCommand = {
        "gcc", "-c", "test/hello.c", "-Itest/include/user.h",
        "--sysroot=test"};

    // Change once deps are handled.
    const std::vector<std::string> expectedDepsCommand = {
        "gcc",
        "-c",
        "/home/nobody/test/hello.c",
        "-I/home/nobody/test/include/user.h",
        "--sysroot=/home/nobody/test",
        "-M"};

    EXPECT_EQ(expectedCommand, parsedCommand.get_command());
    EXPECT_EQ(expectedDepsCommand, parsedCommand.get_dependencies_command());
}

TEST(TestParsedCommandFactory, testOptionParam)
{
    RECC_PROJECT_ROOT = "/home/nobody/";
    const std::vector<std::string> command = {
        "gcc", "-c", "hello.c", "--param", "max-inline-insns-single=1000"};

    auto parsedCommand =
        ParsedCommandFactory::createParsedCommand(command, "/home/nobody/");

    const std::vector<std::string> expectedCommand = {
        "gcc", "-c", "hello.c", "--param", "max-inline-insns-single=1000"};

    // Ensure that the param is not in the input files
    const std::vector<std::string> expectedInputs = {"hello.c"};

    const std::vector<std::string> expectedDepsCommand = {
        "gcc", "-c", "hello.c", "--param", "max-inline-insns-single=1000",
        "-M"};
    EXPECT_EQ(expectedInputs, parsedCommand.d_inputFiles);
    EXPECT_EQ(expectedCommand, parsedCommand.get_command());
    EXPECT_EQ(expectedDepsCommand, parsedCommand.get_dependencies_command());
}

TEST(TestParsedCommandFactory, testOptionParamNoSpace)
{
    RECC_PROJECT_ROOT = "/home/nobody/";
    const std::vector<std::string> command = {
        "gcc", "-c", "hello.c", "--param=max-inline-insns-single=1000"};

    auto parsedCommand =
        ParsedCommandFactory::createParsedCommand(command, "/home/nobody/");

    const std::vector<std::string> expectedCommand = {
        "gcc", "-c", "hello.c", "--param=max-inline-insns-single=1000"};

    // Ensure that the param is not in the input files
    const std::vector<std::string> expectedInputs = {"hello.c"};

    const std::vector<std::string> expectedDepsCommand = {
        "gcc", "-c", "hello.c", "--param=max-inline-insns-single=1000", "-M"};
    EXPECT_EQ(expectedInputs, parsedCommand.d_inputFiles);
    EXPECT_EQ(expectedCommand, parsedCommand.get_command());
    EXPECT_EQ(expectedDepsCommand, parsedCommand.get_dependencies_command());
}

TEST(TestParsedCommandFactory, testOptionParamInvalid)
{
    // Place --param at the end of the parser with a missing option value.
    // Should report is_compiler_command false rather than crash.
    RECC_PROJECT_ROOT = "/home/nobody/";
    const std::vector<std::string> command = {"gcc", "-c", "hello.c",
                                              "--param"};

    auto parsedCommand =
        ParsedCommandFactory::createParsedCommand(command, "/home/nobody/");

    ASSERT_FALSE(parsedCommand.is_compiler_command());
}

TEST(TestParsedCommandFactory, testPreprocessor)
{
    RECC_PROJECT_ROOT = "/home/nobody/";

    const std::vector<std::string> command = {
        "gcc",
        "-c",
        "/home/nobody/test/hello.c",
        "-o",
        "/home/nobody/test/hello.o",
        "-I/home/nobody/headers",
        "-I",
        "/home/nobody/test/moreheaders/",
        "-Wp,-I,/home/nobody/evenmoreheaders",
        "-Xpreprocessor",
        "-I",
        "-Xpreprocessor",
        "/usr/include/something"};

    auto parsedCommand = ParsedCommandFactory::createParsedCommand(
        command, "/home/nobody/test");

    const std::vector<std::string> expectedCommand = {
        "gcc",
        "-c",
        "hello.c",
        "-o",
        "hello.o",
        "-I../headers",
        "-I",
        "moreheaders",
        "-Xpreprocessor",
        "-I",
        "-Xpreprocessor",
        "../evenmoreheaders",
        "-Xpreprocessor",
        "-I",
        "-Xpreprocessor",
        "/usr/include/something"};

    const std::vector<std::string> expectedDepsCommand = {
        "gcc",
        "-c",
        "/home/nobody/test/hello.c",
        "-I/home/nobody/headers",
        "-I",
        "/home/nobody/test/moreheaders/",
        "-Xpreprocessor",
        "-I",
        "-Xpreprocessor",
        "/home/nobody/evenmoreheaders",
        "-Xpreprocessor",
        "-I",
        "-Xpreprocessor",
        "/usr/include/something",
        "-M"};
    const std::set<std::string> expectedProducts = {"hello.o"};

    ASSERT_TRUE(parsedCommand.is_compiler_command());
    EXPECT_EQ(parsedCommand.get_command(), expectedCommand);
    EXPECT_EQ(parsedCommand.get_dependencies_command(), expectedDepsCommand);
    EXPECT_EQ(parsedCommand.get_products(), expectedProducts);
    EXPECT_EQ(false, parsedCommand.is_AIX());
    EXPECT_EQ("", parsedCommand.get_aix_dependency_file_name());
    EXPECT_EQ(false, parsedCommand.is_clang());
    EXPECT_EQ(true, parsedCommand.is_compiler_command());
}

TEST(TestParsedCommandFactory, TestDashDArgs)
{
    const std::vector<std::vector<std::string>> dashDArgs = {
        {"-DFOO"},
        {"-DSRCDIR=\"../../foo/src\""},
        {"-D", "FOO"},
        {"-D", "SRCDIR=\"../../foo/src\""},
    };

    for (const std::vector<std::string> &dashDArg : dashDArgs) {
        RECC_PROJECT_ROOT = "/home/nobody/";

        std::vector<std::string> command = {"gcc", "-c",
                                            "/home/nobody/test/hello.c", "-o",
                                            "/home/nobody/test/hello.o"};

        command.insert(command.begin() + 1, dashDArg.begin(), dashDArg.end());

        auto parsedCommand = ParsedCommandFactory::createParsedCommand(
            command, "/home/nobody/test");

        std::vector<std::string> expectedCommand = {"gcc", "-c", "hello.c",
                                                    "-o", "hello.o"};

        expectedCommand.insert(expectedCommand.begin() + 1, dashDArg.begin(),
                               dashDArg.end());

        std::vector<std::string> expectedDepsCommand = {
            "gcc", "-c", "/home/nobody/test/hello.c", "-M"};

        expectedDepsCommand.insert(expectedDepsCommand.begin() + 1,
                                   dashDArg.begin(), dashDArg.end());
        const std::set<std::string> expectedProducts = {"hello.o"};

        ASSERT_TRUE(parsedCommand.is_compiler_command());
        EXPECT_EQ(parsedCommand.get_command(), expectedCommand);
        EXPECT_EQ(parsedCommand.get_dependencies_command(),
                  expectedDepsCommand);
        EXPECT_EQ(parsedCommand.get_products(), expectedProducts);
        EXPECT_EQ(false, parsedCommand.is_AIX());
        EXPECT_EQ("", parsedCommand.get_aix_dependency_file_name());
        EXPECT_EQ(false, parsedCommand.is_clang());
        EXPECT_EQ(true, parsedCommand.is_compiler_command());
    }
}

TEST(TestParsedCommandFactory, testStandardInput)
{
    auto parsedCommand = ParsedCommandFactory::createParsedCommand(
        {"gcc", "-c", "-x", "c", "-", "-o", "hello.o"});

    // Standard input is not supported
    EXPECT_FALSE(parsedCommand.is_compiler_command());
}

TEST(TestParsedCommandFactory, testAtFile)
{
    auto parsedCommand = ParsedCommandFactory::createParsedCommand(
        {"gcc", "@hello.rsp", "-c", "hello.c", "-o", "hello.o"});

    // Reading command-line options from a file is not supported
    EXPECT_FALSE(parsedCommand.is_compiler_command());
}

TEST(TestParsedCommandFactory, testProfile)
{
    auto parsedCommand1 = ParsedCommandFactory::createParsedCommand(
        {"gcc", "-c", "-fprofile-use", "hello.c", "-o", "hello.o"});
    auto parsedCommand2 = ParsedCommandFactory::createParsedCommand(
        {"gcc", "-c", "-fprofile-use=path", "hello.c", "-o", "hello.o"});
    auto parsedCommand3 = ParsedCommandFactory::createParsedCommand(
        {"gcc", "-c", "-fauto-profile", "hello.c", "-o", "hello.o"});
    auto parsedCommand4 = ParsedCommandFactory::createParsedCommand(
        {"gcc", "-c", "-fauto-profile=path", "hello.c", "-o", "hello.o"});
    auto parsedCommand5 = ParsedCommandFactory::createParsedCommand(
        {"gcc", "-c", "-fbranch-probabilities", "hello.c", "-o", "hello.o"});

    // Use of profiling information is not supported
    EXPECT_FALSE(parsedCommand1.is_compiler_command());
    EXPECT_FALSE(parsedCommand2.is_compiler_command());
    EXPECT_FALSE(parsedCommand3.is_compiler_command());
    EXPECT_FALSE(parsedCommand4.is_compiler_command());
    EXPECT_FALSE(parsedCommand5.is_compiler_command());
}

TEST(TestParsedCommandFactory, testCoverage)
{
    std::vector<std::vector<std::string>> coverageDefaultPathCommands = {
        {"gcc", "-c", "--coverage", "hello.c", "-o", "hello.o"},
        {"gcc", "-c", "-ftest-coverage", "hello.c", "-o", "hello.o"}};
    std::vector<std::vector<std::string>> coverageCustomPathCommands = {
        {"gcc", "-c", "-fprofile-note=custom.gcno", "--coverage", "hello.c",
         "-o", "hello.o"},
        {"gcc", "-c", "-fprofile-note=custom.gcno", "-ftest-coverage",
         "hello.c", "-o", "hello.o"}};

    std::set<std::string> expectedProducts = {};
    for (auto commandArgs : coverageDefaultPathCommands) {
        auto command =
            ParsedCommandFactory::createParsedCommand(commandArgs, "");
        EXPECT_TRUE(command.is_compiler_command());
        EXPECT_TRUE(command.d_coverage_option_set);
        EXPECT_EQ(expectedProducts, command.get_coverage_products());
    }

    expectedProducts = {"custom.gcno"};
    for (auto commandArgs : coverageCustomPathCommands) {
        auto command =
            ParsedCommandFactory::createParsedCommand(commandArgs, "");
        EXPECT_TRUE(command.is_compiler_command());
        EXPECT_TRUE(command.d_coverage_option_set);
        EXPECT_EQ(expectedProducts, command.get_coverage_products());
    }

    // Specifying -fprofile-note doesn't result in a coverage file being
    // produced
    std::vector<std::vector<std::string>> coveragePathWithNoCoverageOption = {
        {"gcc", "-c", "-fprofile-note=custom.gcno", "hello.c", "-o",
         "hello.o"}};
    expectedProducts = {"custom.gcno"};
    for (auto commandArgs : coveragePathWithNoCoverageOption) {
        auto command =
            ParsedCommandFactory::createParsedCommand(commandArgs, "");
        EXPECT_TRUE(command.is_compiler_command());
        EXPECT_FALSE(command.d_coverage_option_set);
        EXPECT_EQ(expectedProducts, command.get_coverage_products());
    }
}

TEST(TestParsedCommandFactory, testSpecs)
{
    auto parsedCommand = ParsedCommandFactory::createParsedCommand(
        {"gcc", "-c", "-specs=foo.specs", "hello.c", "-o", "hello.o"});

    // Spec files are not supported
    EXPECT_FALSE(parsedCommand.is_compiler_command());
}

/*
The next section of helpers/variables is used explicitly for the
CompilerOptionMatch tests.
*/
static const ParsedCommandFactory::CompilerParseRulesMap testRules = {
    {"-BBB", ParseRule::parseIsInputPathOption},
    {"-B", ParseRule::parseOptionRedirectsOutput},
    {"-BT", ParseRule::parseInterfersWithDepsOption},
};

// Helper method to get the underlying address of the function target of a
// std::function.
// Source:
// https://stackoverflow.com/questions/18039723/c-trying-to-get-function-address-from-a-stdfunction
template <typename T, typename... U>
size_t getAddress(std::function<T(U...)> f)
{
    typedef T(fnType)(U...);
    fnType **fnPointer = f.template target<fnType *>();
    return (size_t)*fnPointer;
}

TEST(CompilerOptionMatch, simpleMatches)
{
    auto flag = "-B";
    auto funcPtr = ParseRuleHelper::matchCompilerOptions(flag, testRules);

    ASSERT_NE(funcPtr.second, nullptr);
    EXPECT_EQ(getAddress(funcPtr.second), getAddress(testRules.at(flag)));

    auto equalFlag = "-B=";
    funcPtr = ParseRuleHelper::matchCompilerOptions(equalFlag, testRules);
    ASSERT_NE(funcPtr.second, nullptr);
    EXPECT_EQ(getAddress(funcPtr.second), getAddress(testRules.at(flag)));

    // Make sure the function pointer is unique, and doesn't match the other
    // flags.
    EXPECT_NE(getAddress(funcPtr.second), getAddress(testRules.at("-BBB")));
}

TEST(CompilerOptionMatch, moreComplexMatches)
{
    auto flag = "-B hello -C";
    auto funcPtr = ParseRuleHelper::matchCompilerOptions(flag, testRules);

    ASSERT_NE(funcPtr.second, nullptr);
    EXPECT_EQ(getAddress(funcPtr.second), getAddress(testRules.at("-B")));

    flag = "-B.../usr/bin";
    funcPtr = ParseRuleHelper::matchCompilerOptions(flag, testRules);

    ASSERT_NE(funcPtr.second, nullptr);
    EXPECT_EQ(getAddress(funcPtr.second), getAddress(testRules.at("-B")));

    funcPtr = ParseRuleHelper::matchCompilerOptions("B", testRules);
    EXPECT_EQ(funcPtr.second, nullptr);

    flag = "-B = hi ";
    funcPtr = ParseRuleHelper::matchCompilerOptions(flag, testRules);
    EXPECT_EQ(getAddress(funcPtr.second), getAddress(testRules.at("-B")));
    EXPECT_EQ(funcPtr.first, "-B");
}

TEST(TestParsedCommandFactory, testSolarisPhase)
{
    auto parsedCommand = ParsedCommandFactory::createParsedCommand(
        {"CC", "-features=rtti", "-m32", "-mt", "-xtarget=generic", "-Qoption",
         "ccfe", "-xglobalstatic", "-v", "-o", "hello.c.o", "-c", "hello.c"});

    const std::vector<std::string> expectedCommand = {
        "CC",     "-features=rtti",   "-m32",
        "-mt",    "-xtarget=generic", "-Qoption",
        "ccfe",   "-xglobalstatic",   "-v",
        "-o",     "hello.c.o",        "-c",
        "hello.c"};
    const std::vector<std::string> expectedDependenciesCommand = {
        "CC",   "-features=rtti",   "-m32",
        "-mt",  "-xtarget=generic", "-Qoption",
        "ccfe", "-xglobalstatic",   "-v",
        "-c",   "hello.c",          "-xM"};
    const std::set<std::string> expectedProducts = {"hello.c.o"};
    const std::vector<std::string> expectedInputs = {"hello.c"};

    ASSERT_TRUE(parsedCommand.is_compiler_command());
    EXPECT_EQ(parsedCommand.get_command(), expectedCommand);
    EXPECT_EQ(parsedCommand.get_dependencies_command(),
              expectedDependenciesCommand);
    EXPECT_EQ(parsedCommand.get_products(), expectedProducts);
    EXPECT_EQ(parsedCommand.d_inputFiles, expectedInputs);
    EXPECT_EQ(false, parsedCommand.is_AIX());
    EXPECT_EQ(true, parsedCommand.is_sun_studio());
    EXPECT_EQ("", parsedCommand.get_aix_dependency_file_name());
    EXPECT_EQ(false, parsedCommand.is_clang());
}

TEST(TestParsedCommandFactory, testSolarisPhaseInvalid)
{
    // Place -Qoption at the end of the parser with a missing option value.
    // Should report is_compiler_command false rather than crash.
    auto parsedCommand = ParsedCommandFactory::createParsedCommand(
        {"CC", "-features=rtti", "-m32", "-mt", "-xtarget=generic", "-v", "-o",
         "hello.c.o", "-c", "hello.c", "-Qoption", "ccfe"});

    ASSERT_FALSE(parsedCommand.is_compiler_command());
    EXPECT_EQ(false, parsedCommand.is_AIX());
    EXPECT_EQ(true, parsedCommand.is_sun_studio());
}

TEST(TestParsedCommandFactory, testSupportedMarch)
{
    auto parsedCommand = ParsedCommandFactory::createParsedCommand(
        {"gcc", "-march=haswell", "-o", "hello.c.o", "-c", "hello.c"});
    std::vector<std::string> expectedCommand = {
        "gcc", "-march=haswell", "-o", "hello.c.o", "-c", "hello.c"};
    std::vector<std::string> expectedDependenciesCommand = {
        "gcc", "-march=haswell", "-c", "hello.c", "-M"};

    ASSERT_TRUE(parsedCommand.is_compiler_command());
    EXPECT_EQ(parsedCommand.get_command(), expectedCommand);
    EXPECT_EQ(parsedCommand.get_dependencies_command(),
              expectedDependenciesCommand);
}

TEST(TestParsedCommandFactory, testUnsupportedMarch)
{
    auto parsedCommand = ParsedCommandFactory::createParsedCommand(
        {"gcc", "-march=native", "-o", "hello.c.o", "-c", "hello.c"});
    std::vector<std::string> expectedCommand = {
        "gcc", "-march=native", "-o", "hello.c.o", "-c", "hello.c"};

    ASSERT_FALSE(parsedCommand.is_compiler_command());
    EXPECT_EQ(parsedCommand.get_command(), expectedCommand);
    EXPECT_EQ(parsedCommand.get_dependencies_command(), expectedCommand);
}

TEST(TestParsedCommandFactory, testSupportedMtune)
{
    auto parsedCommand = ParsedCommandFactory::createParsedCommand(
        {"gcc", "-mtune=haswell", "-o", "hello.c.o", "-c", "hello.c"});
    std::vector<std::string> expectedCommand = {
        "gcc", "-mtune=haswell", "-o", "hello.c.o", "-c", "hello.c"};
    std::vector<std::string> expectedDependenciesCommand = {
        "gcc", "-mtune=haswell", "-c", "hello.c", "-M"};

    ASSERT_TRUE(parsedCommand.is_compiler_command());
    EXPECT_EQ(parsedCommand.get_command(), expectedCommand);
    EXPECT_EQ(parsedCommand.get_dependencies_command(),
              expectedDependenciesCommand);
}

TEST(TestParsedCommandFactory, testUnsupportedMtune)
{
    auto parsedCommand = ParsedCommandFactory::createParsedCommand(
        {"gcc", "-mtune=native", "-o", "hello.c.o", "-c", "hello.c"});
    std::vector<std::string> expectedCommand = {
        "gcc", "-mtune=native", "-o", "hello.c.o", "-c", "hello.c"};

    ASSERT_FALSE(parsedCommand.is_compiler_command());
    EXPECT_EQ(parsedCommand.get_command(), expectedCommand);
    EXPECT_EQ(parsedCommand.get_dependencies_command(), expectedCommand);
}

TEST(TestParsedCommandFactory, testSupportedMcpu)
{
    auto parsedCommand = ParsedCommandFactory::createParsedCommand(
        {"gcc", "-mcpu=haswell", "-o", "hello.c.o", "-c", "hello.c"});
    std::vector<std::string> expectedCommand = {
        "gcc", "-mcpu=haswell", "-o", "hello.c.o", "-c", "hello.c"};
    std::vector<std::string> expectedDependenciesCommand = {
        "gcc", "-mcpu=haswell", "-c", "hello.c", "-M"};

    ASSERT_TRUE(parsedCommand.is_compiler_command());
    EXPECT_EQ(parsedCommand.get_command(), expectedCommand);
    EXPECT_EQ(parsedCommand.get_dependencies_command(),
              expectedDependenciesCommand);
}

TEST(TestParsedCommandFactory, testUnsupportedMcpu)
{
    auto parsedCommand = ParsedCommandFactory::createParsedCommand(
        {"gcc", "-mcpu=native", "-o", "hello.c.o", "-c", "hello.c"});
    std::vector<std::string> expectedCommand = {
        "gcc", "-mcpu=native", "-o", "hello.c.o", "-c", "hello.c"};

    ASSERT_FALSE(parsedCommand.is_compiler_command());
    EXPECT_EQ(parsedCommand.get_command(), expectedCommand);
    EXPECT_EQ(parsedCommand.get_dependencies_command(), expectedCommand);
}
