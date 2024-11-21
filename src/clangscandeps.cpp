// Copyright 2022-2023 Bloomberg Finance L.P
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

#include <clangscandeps.h>

#include <compilerdefaults.h>
#include <deps.h>
#include <digestgenerator.h>
#include <env.h>
#include <shellutils.h>
#include <subprocess.h>

#include <buildboxcommon_fileutils.h>
#include <buildboxcommon_logging.h>
#include <buildboxcommon_stringutils.h>
#include <buildboxcommon_systemutils.h>
#include <buildboxcommon_temporarydirectory.h>
#include <buildboxcommonmetrics_countingmetricutil.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

#ifdef RECC_CLANG_SCAN_DEPS
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

static const std::string COUNTER_NAME_CLANG_SCAN_DEPS_INVOCATION_SUCCESS =
    "recc.clang_scan_deps_invocation_success";
static const std::string COUNTER_NAME_CLANG_SCAN_DEPS_INVOCATION_FAILURE =
    "recc.clang_scan_deps_invocation_failure";
static const std::string COUNTER_NAME_CLANG_SCAN_DEPS_TARGET_SUCCESS =
    "recc.clang_scan_deps_target_success";
static const std::string COUNTER_NAME_CLANG_SCAN_DEPS_TARGET_FAILURE =
    "recc.clang_scan_deps_target_failure";

namespace recc {

#ifdef RECC_CLANG_SCAN_DEPS

namespace {

/**
 * Split the clang-scan-deps output into one file per target.
 *
 * The clang-scan-deps output consists of `make`-compatible rules describing
 * the dependencies of each target. For projects with many targets, this can
 * get fairly large. To avoid requiring each `recc` invocation in a project
 * build to parse all rules, this function splits the dependencies into one
 * file per rule, using the hash of the target as filename.
 *
 * Example rule as produced by clang-scan-deps:
 *     target.o: \
 *       /src/foo/target.cpp \
 *       /usr/include/header.h
 */
void split_scan_deps_rules(const std::string &rules,
                           const std::string &depsdir)
{
    std::set<std::string> targets;

    size_t rulestart = 0;
    while (rulestart < rules.size()) {
        size_t pos = rulestart;
        size_t rulesize = 0;

        // Scan for an unescaped newline to find the end of the rule
        while (true) {
            pos = rules.find('\n', pos);
            if (pos == std::string::npos) {
                // End of file
                rulesize = rules.size() - rulestart;
                break;
            }
            else if (pos > 0 && rules[pos - 1] == '\\') {
                // Escaped newline, rule continues
                pos++;
            }
            else {
                // End of rule
                rulesize = pos + 1 - rulestart;
                break;
            }
        }

        const std::string rule = rules.substr(rulestart, rulesize);
        size_t colon = rule.find(':');
        if (colon != std::string::npos) {
            const std::string target =
                buildboxcommon::StringUtils::trim(rule.substr(0, colon));
            const auto targetDigest = DigestGenerator::make_digest(target);
            const std::string path = depsdir + "/" + targetDigest.hash();

            if (targets.find(target) == targets.end()) {
                targets.insert(target);
                buildboxcommon::FileUtils::writeFileAtomically(path, rule,
                                                               0644);
            }
            else {
                // Duplicate target name, remove dependency file to trigger
                // fallback to dependencies command.
                if (unlink(path.c_str()) < 0 && errno != ENOENT) {
                    BUILDBOXCOMMON_THROW_SYSTEM_EXCEPTION(
                        std::system_error, errno, std::system_category,
                        "Failed to remove file \"" << path << "\"");
                }
            }
        }
        else if (!buildboxcommon::StringUtils::trim(rule).empty()) {
            BUILDBOXCOMMON_THROW_EXCEPTION(
                std::runtime_error,
                "Failed to parse clang-scan-deps rule: " << rule);
        }
        // Start of next rule
        rulestart += rulesize;
    }
}

/**
 * Determine predefined macros and system include directories of the compiler.
 *
 * If the compiler is not part of the same toolchain as `clang-scan-deps`,
 * there may be differences in predefined macros and system include
 * directories. This function generates a header file for predefined macros
 * and a list of extra command-line arguments for `clang-scan-deps` to use
 * the predefined macros and system include directories from the compiler
 * instead of the defaults from the `clang-scan-deps` toolchain.
 *
 * To avoid redundantly retrieving this information for every entry in the
 * compilation database, the results are cached using the compiler name and
 * relevant command-line options as cache key.
 */
std::vector<std::string> get_extra_args_for_scan_deps(
    std::map<std::string, std::vector<std::string>> *cache,
    const std::string &basedir, const std::vector<std::string> &arguments)
{
    std::vector<std::string> compilerPrintCommand;
    std::string compilerKey;
    compilerPrintCommand.push_back(arguments[0]);
    compilerKey.append(arguments[0]);

    for (const auto &argument : arguments) {
        if (argument.substr(0, 5) == "-std=" ||
            argument.substr(0, 2) == "-O" || argument.substr(0, 2) == "-f" ||
            argument.substr(0, 2) == "-m" || argument == "-undef" ||
            argument == "-nostdinc") {
            // These flags may affect predefined macros or include directories
            compilerPrintCommand.push_back(argument);
            compilerKey.append(" " + argument);
        }
    }

    const auto cachedExtraArgs = cache->find(compilerKey);
    if (cachedExtraArgs != cache->end())
        return cachedExtraArgs->second;

    const std::string emptyHeader = basedir + "/recc-empty.h";
    std::ofstream emptyHeaderStream(emptyHeader);
    emptyHeaderStream.close();

    compilerPrintCommand.push_back("-E");
    compilerPrintCommand.push_back("-dM");
    compilerPrintCommand.push_back("-Wp,-v");
    compilerPrintCommand.push_back(emptyHeader);

    const auto subprocessResult =
        Subprocess::execute(compilerPrintCommand, true, true, RECC_DEPS_ENV);

    if (subprocessResult.d_exitCode != 0) {
        std::string errorMsg = "Failed to execute: ";
        for (const auto &token : compilerPrintCommand) {
            errorMsg += (token + " ");
        }
        BUILDBOX_LOG_ERROR(errorMsg);
        BUILDBOX_LOG_ERROR("Exit status: " << subprocessResult.d_exitCode);
        BUILDBOX_LOG_DEBUG("stdout: " << subprocessResult.d_stdOut);
        BUILDBOX_LOG_DEBUG("stderr: " << subprocessResult.d_stdErr);
        throw subprocess_failed_error(subprocessResult.d_exitCode);
    }

    std::vector<std::string> extraArgs;
    extraArgs.push_back("-undef");
    extraArgs.push_back("-nostdinc");

    const auto compilerKeyDigest = DigestGenerator::make_digest(compilerKey);

    const std::string predefinedHeader =
        basedir + "/" + compilerKeyDigest.hash() + "-recc-scan-deps.h";
    buildboxcommon::FileUtils::writeFileAtomically(
        predefinedHeader, subprocessResult.d_stdOut, 0644);

    extraArgs.push_back("-include");
    extraArgs.push_back(predefinedHeader);

    // Get the system include directories
    std::istringstream preprocessorStream(subprocessResult.d_stdErr);
    std::string line;
    bool inSearchList = false;
    while (std::getline(preprocessorStream, line)) {
        if (line == "#include <...> search starts here:") {
            inSearchList = true;
        }
        else if (line == "End of search list.") {
            break;
        }
        else if (inSearchList) {
            buildboxcommon::StringUtils::ltrim(&line);
            extraArgs.push_back("-idirafter");
            extraArgs.push_back(line);
        }
    }

    (*cache)[compilerKey] = extraArgs;

    return extraArgs;
}

/**
 * Invoke clang-scan-deps to determine dependencies.
 */
void populate_dependencies_directory(const std::string &topbuilddir,
                                     const std::string &scanDepsPath,
                                     const std::string &depsdir)
{
    // Use a temporary directory for generated header files to ensure that
    // they aren't picked up by `*.h` glob patterns in build systems.
    buildboxcommon::TemporaryDirectory tempDir("recc");

    std::ifstream compilationDatabaseStream(topbuilddir + "/" +
                                            RECC_COMPILATION_DATABASE);
    json compilationDatabase = json::parse(compilationDatabaseStream);
    compilationDatabaseStream.close();

    json modifiedCompilationDatabase = json::array();
    std::map<std::string, std::vector<std::string>> extraArgsCache;

    for (auto &commandObject : compilationDatabase) {
        if (!commandObject.contains("file")) {
            throw std::runtime_error(
                "Command object in compilation database without file");
        }

        const std::string file = commandObject["file"];

        if (!Deps::is_source_file(file) ||
            !buildboxcommon::FileUtils::isRegularFile(file.c_str())) {
            /* Only C/C++ source files are supported by clang-scan-deps. Also
             * ignore files that don't exist as the compilation database may
             * contain files that are generated as part of the build process.
             */
            continue;
        }

        std::vector<std::string> arguments;

        if (commandObject.contains("command")) {
            arguments = ShellUtils::splitCommand(commandObject["command"]);
            commandObject.erase("command");
        }
        else if (commandObject.contains("arguments")) {
            arguments =
                commandObject["arguments"].get<std::vector<std::string>>();
        }
        else {
            BUILDBOXCOMMON_THROW_EXCEPTION(
                std::runtime_error, "Command object in compilation database "
                                    "without arguments or command");
        }

        if (arguments.empty()) {
            BUILDBOXCOMMON_THROW_EXCEPTION(
                std::runtime_error, "Command object in compilation database "
                                    "with empty argument list");
        }

        const auto extraArgs = get_extra_args_for_scan_deps(
            &extraArgsCache, tempDir.name(), arguments);
        arguments.insert(arguments.begin() + 1, extraArgs.begin(),
                         extraArgs.end());
        commandObject["arguments"] = arguments;

        modifiedCompilationDatabase.push_back(commandObject);
    }

    buildboxcommon::TemporaryFile modifiedCompilationDatabaseFile;
    std::ofstream modifiedCompilationDatabaseStream(
        modifiedCompilationDatabaseFile.strname());
    modifiedCompilationDatabaseStream << modifiedCompilationDatabase;
    modifiedCompilationDatabaseStream.close();

    std::vector<std::string> scanDepsCommand;
    scanDepsCommand.push_back(scanDepsPath);
    scanDepsCommand.push_back("--compilation-database=" +
                              modifiedCompilationDatabaseFile.strname());

    std::ostringstream depCommand;
    for (auto &token : scanDepsCommand) {
        depCommand << token << " ";
    }
    BUILDBOX_LOG_DEBUG(
        "Getting dependencies using the command: " << depCommand.str());

    const auto subprocessResult =
        Subprocess::execute(scanDepsCommand, true, true, RECC_DEPS_ENV);

    if (subprocessResult.d_exitCode != 0) {
        std::string errorMsg = "Failed to execute: ";
        for (const auto &token : scanDepsCommand) {
            errorMsg += (token + " ");
        }
        BUILDBOX_LOG_ERROR(errorMsg);
        BUILDBOX_LOG_ERROR("Exit status: " << subprocessResult.d_exitCode);
        BUILDBOX_LOG_DEBUG("stdout: " << subprocessResult.d_stdOut);
        BUILDBOX_LOG_DEBUG("stderr: " << subprocessResult.d_stdErr);
        throw subprocess_failed_error(subprocessResult.d_exitCode);
    }

    const std::string dependencies = subprocessResult.d_stdOut;

    buildboxcommon::FileUtils::createDirectory((depsdir + ".tmp").c_str());

    split_scan_deps_rules(dependencies, depsdir + ".tmp");

    // Rename complete dependencies directory to its final name
    if (rename((depsdir + ".tmp").c_str(), depsdir.c_str()) < 0) {
        BUILDBOXCOMMON_THROW_SYSTEM_EXCEPTION(
            std::system_error, errno, std::system_category,
            "Failed to rename dependencies directory");
    }
}

std::string
get_dependencies_directory(const CounterMetricCallback &recordCounterMetric)
{
    const std::string RECC_DEPENDENCIES = "recc-scan-deps.d";
    const std::string RECC_DEPENDENCIES_LOCK = RECC_DEPENDENCIES + ".lock";

    if (RECC_COMPILATION_DATABASE.empty()) {
        // Not enabled in configuration
        return "";
    }

    // Directory with compilation database
    std::string topbuilddir =
        buildboxcommon::SystemUtils::getCurrentWorkingDirectory();

    while (!buildboxcommon::FileUtils::isRegularFile(
        (topbuilddir + "/" + RECC_COMPILATION_DATABASE).c_str())) {
        // Compilation database doesn't exist in current working directory.
        // Check the ancestors of the working directory
        // (e.g. for `cmake` subdirectory builds with `make`).
        std::size_t slash = topbuilddir.find_last_of("/");
        if (slash == std::string::npos || slash == 0) {
            // Compilation database not found.
            // Incompatible build system or disabled.
            return "";
        }
        topbuilddir = topbuilddir.substr(0, slash);
    }

    // Allow the environment or config file to specify the command or path
    const std::string scanDepsPath =
        buildboxcommon::SystemUtils::getPathToCommand(CLANG_SCAN_DEPS);
    if (scanDepsPath.empty()) {
        // clang-scan-deps not available
        return "";
    }

    BUILDBOX_LOG_INFO("Using clang-scan-deps to get dependencies of "
                      << topbuilddir + "/" + RECC_COMPILATION_DATABASE);

    buildboxcommon::FileDescriptor topbuilddirfd(
        open(topbuilddir.c_str(), O_RDONLY | O_DIRECTORY));
    if (topbuilddirfd.get() < 0) {
        BUILDBOXCOMMON_THROW_SYSTEM_EXCEPTION(
            std::system_error, errno, std::system_category,
            "Error opening top build directory \"" << topbuilddir << "\"");
    }

    const std::string depsdir = topbuilddir + "/" + RECC_DEPENDENCIES;

    if (buildboxcommon::FileUtils::isDirectory(depsdir.c_str())) {
        // Dependencies directory already written by another recc process
        return depsdir;
    }

    buildboxcommon::FileDescriptor lockfd(
        openat(topbuilddirfd.get(), RECC_DEPENDENCIES_LOCK.c_str(),
               O_CREAT | O_RDWR, 0600));
    if (lockfd.get() < 0) {
        BUILDBOXCOMMON_THROW_SYSTEM_EXCEPTION(
            std::system_error, errno, std::system_category,
            "Error opening dependencies lock file \"" << RECC_DEPENDENCIES_LOCK
                                                      << "\"");
    }

    if (lockf(lockfd.get(), F_LOCK, 0) < 0) {
        BUILDBOXCOMMON_THROW_SYSTEM_EXCEPTION(
            std::system_error, errno, std::system_category,
            "Failed to lock file \"" << RECC_DEPENDENCIES_LOCK << "\"");
    }

    // We have an exclusive lock, check whether dependencies file has been
    // written by another recc process in the meantime
    if (buildboxcommon::FileUtils::isDirectory(depsdir.c_str())) {
        // Dependencies directory already written by another recc process
        return depsdir;
    }

    // This is the first recc process, invoke clang-scan-deps.
    try {
        populate_dependencies_directory(topbuilddir, scanDepsPath, depsdir);
    }
    catch (const std::runtime_error &) {
        recordCounterMetric(COUNTER_NAME_CLANG_SCAN_DEPS_INVOCATION_FAILURE,
                            1);

        // If clang-scan-deps fails, create empty dependencies directory such
        // that other recc processes won't try the same again.
        buildboxcommon::FileUtils::createDirectory(depsdir.c_str());

        unlinkat(topbuilddirfd.get(), RECC_DEPENDENCIES_LOCK.c_str(), 0);

        throw;
    }

    recordCounterMetric(COUNTER_NAME_CLANG_SCAN_DEPS_INVOCATION_SUCCESS, 1);

    unlinkat(topbuilddirfd.get(), RECC_DEPENDENCIES_LOCK.c_str(), 0);

    return depsdir;
}

} // unnamed namespace

bool ClangScanDeps::dependencies_for_target(
    const ParsedCommand &parsedCommand, const std::string &target,
    std::set<std::string> *result,
    const CounterMetricCallback &recordCounterMetric)
{
    if (!parsedCommand.is_clang() && !parsedCommand.is_gcc()) {
        return false;
    }

    try {
        const std::string depsdir =
            get_dependencies_directory(recordCounterMetric);

        if (!depsdir.empty()) {
            const auto targetDigest = DigestGenerator::make_digest(target);
            const std::string path = depsdir + "/" + targetDigest.hash();

            if (buildboxcommon::FileUtils::isRegularFile(path.c_str())) {
                const std::string rules =
                    buildboxcommon::FileUtils::getFileContents(path.c_str());

                auto rawDependencies =
                    Deps::dependencies_from_make_rules(rules, false);

                const auto depsTimestamp =
                    buildboxcommon::FileUtils::getFileMtime(depsdir.c_str());

                for (const auto &dep : rawDependencies) {
                    // Filter out the generated file for predefined macros
                    if (dep.find("recc-scan-deps.h") != std::string::npos) {
                        continue;
                    }
                    result->insert(dep);

                    if (!buildboxcommon::FileUtils::isRegularFile(
                            dep.c_str())) {
                        BUILDBOX_LOG_WARNING(
                            "\"" << dep
                                 << "\" was removed after the invocation of "
                                    "clang-scan-deps");
                        BUILDBOX_LOG_INFO(
                            "Falling back to dependencies command");
                        recordCounterMetric(
                            COUNTER_NAME_CLANG_SCAN_DEPS_TARGET_FAILURE, 1);
                        return false;
                    }

                    if (buildboxcommon::FileUtils::getFileMtime(dep.c_str()) >
                        depsTimestamp) {
                        BUILDBOX_LOG_WARNING(
                            "\"" << dep
                                 << "\" was modified after the invocation of "
                                    "clang-scan-deps");
                        BUILDBOX_LOG_INFO(
                            "Falling back to dependencies command");
                        recordCounterMetric(
                            COUNTER_NAME_CLANG_SCAN_DEPS_TARGET_FAILURE, 1);
                        return false;
                    }
                }

                if (!result->empty()) {
                    recordCounterMetric(
                        COUNTER_NAME_CLANG_SCAN_DEPS_TARGET_SUCCESS, 1);
                    return true;
                }
            }

            // This is expected for generated files
            BUILDBOX_LOG_WARNING(
                "clang-scan-deps returned no dependencies for \"" << target
                                                                  << "\"");
            BUILDBOX_LOG_INFO("Falling back to dependencies command");
            recordCounterMetric(COUNTER_NAME_CLANG_SCAN_DEPS_TARGET_FAILURE,
                                1);
        }
    }
    catch (const std::runtime_error &e) {
        BUILDBOX_LOG_ERROR("clang-scan-deps failed: " << e.what());
        BUILDBOX_LOG_INFO("Falling back to dependencies command");
        recordCounterMetric(COUNTER_NAME_CLANG_SCAN_DEPS_TARGET_FAILURE, 1);
    }

    return false;
}

#else

bool ClangScanDeps::dependencies_for_target(const ParsedCommand &,
                                            const std::string &,
                                            std::set<std::string> *,
                                            const CounterMetricCallback &)
{
    return false;
}

#endif

} // namespace recc
