// Copyright 2023 Bloomberg Finance L.P
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

#include <linkdeps.h>

#include <compilerdefaults.h>
#include <deps.h>
#include <env.h>
#include <fileutils.h>
#include <shellutils.h>
#include <subprocess.h>

#include <buildboxcommon_fileutils.h>
#include <buildboxcommon_logging.h>
#include <buildboxcommon_stringutils.h>
#include <buildboxcommon_systemutils.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <glob.h>
#include <iostream>
#include <regex>
#include <sstream>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>

namespace recc {

/**
 * Determine DT_NEEDED dependencies of a library.
 */
std::set<std::string> linux_get_needed_libraries(const std::string &path)
{
    std::set<std::string> neededLibraries;

    const std::vector<std::string> command = {"objdump", "-p", path};

    const auto objdumpResult = Subprocess::execute(command, true, true);

    if (objdumpResult.d_exitCode != 0) {
        const std::vector<std::string> baseLibraries = {
            "libc.so",       "libgcc_s.so",  "libm.so",
            "libpthread.so", "libstdc++.so", "libgfortran.so"};
        for (const auto &baseLibrary : baseLibraries) {
            if (path.substr(path.size() - baseLibrary.size()) == baseLibrary) {
                // These .so files are commonly linker scripts,
                // however, as they are base system/toolchain libraries,
                // there is no need to determine indirect dependencies.
                return neededLibraries;
            }
        }

        std::string errorMsg = "Failed to execute: ";
        for (const auto &token : command) {
            errorMsg += (token + " ");
        }
        BUILDBOX_LOG_ERROR(errorMsg);
        BUILDBOX_LOG_ERROR("Exit status: " << objdumpResult.d_exitCode);
        BUILDBOX_LOG_DEBUG("stdout: " << objdumpResult.d_stdOut);
        BUILDBOX_LOG_DEBUG("stderr: " << objdumpResult.d_stdErr);
        throw subprocess_failed_error(objdumpResult.d_exitCode);
    }

    // Example of output format:
    // NEEDED               libc.so.6

    std::regex re("\\s+NEEDED\\s+([^\\s]+)", std::regex::extended);
    auto needed_begin = std::sregex_iterator(objdumpResult.d_stdOut.begin(),
                                             objdumpResult.d_stdOut.end(), re);
    auto needed_end = std::sregex_iterator();
    for (std::sregex_iterator i = needed_begin; i != needed_end; ++i) {
        std::smatch match = *i;
        neededLibraries.insert(match[1]);
    }

    return neededLibraries;
}

/**
 * Determine DT_NEEDED dependencies of a library.
 */
std::set<std::string> solaris_get_needed_libraries(const std::string &path)
{
    std::set<std::string> neededLibraries;

    const std::vector<std::string> command = {"elfdump", "-d", path};

    const auto elfdumpResult = Subprocess::execute(command, true, true);

    if (elfdumpResult.d_exitCode != 0) {
        std::string errorMsg = "Failed to execute: ";
        for (const auto &token : command) {
            errorMsg += (token + " ");
        }
        BUILDBOX_LOG_ERROR(errorMsg);
        BUILDBOX_LOG_ERROR("Exit status: " << elfdumpResult.d_exitCode);
        BUILDBOX_LOG_DEBUG("stdout: " << elfdumpResult.d_stdOut);
        BUILDBOX_LOG_DEBUG("stderr: " << elfdumpResult.d_stdErr);
        throw subprocess_failed_error(elfdumpResult.d_exitCode);
    }

    // Example of output format:
    // [0]  NEEDED            0x7d4     libc.so.1

    std::regex re("\\[\\d+\\]\\s+NEEDED\\s+0x[0-9a-f]+\\s+([^\\s]+)",
                  std::regex::extended);
    auto needed_begin = std::sregex_iterator(elfdumpResult.d_stdOut.begin(),
                                             elfdumpResult.d_stdOut.end(), re);
    auto needed_end = std::sregex_iterator();
    for (std::sregex_iterator i = needed_begin; i != needed_end; ++i) {
        std::smatch match = *i;
        neededLibraries.insert(match[1]);
    }

    return neededLibraries;
}

/**
 * Determine the default library search path of the linker.
 */
std::vector<std::string>
get_library_search_path(const ParsedCommand &compilerCommand,
                        const ParsedCommand &linkerCommand)
{
#ifdef __sun
    (void)compilerCommand; // Unused parameter
    return linkerCommand.d_defaultLibraryDirs;
#else
    (void)linkerCommand; // Unused parameter
    std::vector<std::string> searchDirs;

    const std::set<std::string> options = {"-m32", "-m64"};

    std::vector<std::string> command;
    command.push_back(compilerCommand.d_originalCommand.front());
    for (const auto &arg : compilerCommand.d_originalCommand) {
        if (options.count(arg) > 0) {
            command.push_back(arg);
        }
    }
    command.push_back("-Wl,--verbose");

    const auto ldVerboseResult = Subprocess::execute(command, true, true);

    const std::string output = ldVerboseResult.d_stdOut;

    // Example of output format:
    // SEARCH_DIR("/usr/local/lib"); SEARCH_DIR("/usr/lib");

    std::regex re("SEARCH_DIR\\(\"([^\n\"]+)\"\\)", std::regex::extended);
    auto search_dir_begin =
        std::sregex_iterator(output.begin(), output.end(), re);
    auto search_dir_end = std::sregex_iterator();
    for (std::sregex_iterator i = search_dir_begin; i != search_dir_end; ++i) {
        std::smatch match = *i;
        const std::string searchDir = match[1];
        searchDirs.push_back(searchDir);
    }

    return searchDirs;
#endif
}

/**
 * Parse /etc/ld.so.conf to get the search path of the runtime linker.
 */
void parse_ld_so_conf(const std::string &filename,
                      std::vector<std::string> *directories)
{
    const std::string include = "include";

    std::ifstream confStream(filename);
    std::string line;
    while (getline(confStream, line)) {
        // Remove comment
        size_t hashPos = line.find('#');
        if (hashPos != std::string::npos) {
            line = line.substr(0, hashPos);
        }

        line = buildboxcommon::StringUtils::trim(line);

        if (line.substr(0, include.size()) == include &&
            isblank(line[include.size()])) {
            std::string includePattern =
                buildboxcommon::StringUtils::trim(line.substr(include.size()));
            if (includePattern[0] != '/') {
                // If the include pattern is a relative path, interpret it
                // relative to the parent directory of the main config file
                const auto lastSlash = filename.rfind('/');
                if (lastSlash != std::string::npos) {
                    includePattern =
                        filename.substr(0, lastSlash + 1) + includePattern;
                }
            }
            glob_t gl;
            int result = glob(includePattern.c_str(), 0, nullptr, &gl);
            if (result == 0) {
                for (size_t i = 0; i < gl.gl_pathc; i++) {
                    parse_ld_so_conf(gl.gl_pathv[i], directories);
                }
            }
            else if (result == GLOB_NOMATCH) {
                // Glob pattern doesn't match any files
            }
            else {
                BUILDBOX_LOG_ERROR(
                    "Failed to evaluate include pattern in ld.so.conf: "
                    "glob() returned "
                    << result);
            }
        }
        else {
            directories->push_back(line);
        }
    }
}

/**
 * Append directories from an environment variable such as
 * `LD_LIBRARY_PATH` to a vector.
 */
void add_directories_from_path(std::vector<std::string> *directories,
                               const std::string &envName)
{
    char *path_pointer = getenv(envName.c_str());
    if (path_pointer == nullptr) {
        // Environment variable is not set, nothing to add
        return;
    }

    std::istringstream iss(path_pointer);

    char separator = ':';
    for (std::string token; std::getline(iss, token, separator);) {
        if (buildboxcommon::FileUtils::isDirectory(token.c_str())) {
            directories->push_back(token);
        }
    }
}

ParsedCommand parse_linker_command(const ParsedCommand &compilerCommand)
{
    std::vector<std::string> subprocessCommand(
        compilerCommand.d_originalCommand.begin(),
        compilerCommand.d_originalCommand.end());

    // gcc option to print the commands that would be executed,
    // including the linker command
    subprocessCommand.push_back("-###");

    const auto subprocessResult =
        Subprocess::execute(subprocessCommand, true, true);

    if (subprocessResult.d_exitCode != 0) {
        std::string errorMsg = "Failed to execute: ";
        for (const auto &token : subprocessCommand) {
            errorMsg += (token + " ");
        }
        BUILDBOX_LOG_ERROR(errorMsg);
        BUILDBOX_LOG_ERROR("Exit status: " << subprocessResult.d_exitCode);
        BUILDBOX_LOG_DEBUG("stdout: " << subprocessResult.d_stdOut);
        BUILDBOX_LOG_DEBUG("stderr: " << subprocessResult.d_stdErr);
        throw subprocess_failed_error(subprocessResult.d_exitCode);
    }

    std::istringstream verboseStream(subprocessResult.d_stdErr);
    std::string line;
    std::vector<std::string> linkerArgs;
    bool commandFound = false;
    while (std::getline(verboseStream, line)) {
        if ((compilerCommand.is_gcc() || compilerCommand.is_clang()) &&
            line.front() == ' ') {
            if (commandFound) {
                // Pure link commands shouldn't execute multiple subprocesses
                throw std::runtime_error("Unexpected second command");
            }
            commandFound = true;
            BUILDBOX_LOG_DEBUG("Linker command: " << line);
            linkerArgs = ShellUtils::splitCommand(line);
        }
        else if (compilerCommand.is_sun_studio() && line.front() != '#') {
            std::vector<std::string> commandArgs =
                ShellUtils::splitCommand(line);
            if (buildboxcommon::FileUtils::pathBasename(
                    commandArgs[0].c_str()) == "ld") {
                if (commandFound) {
                    // There should only be one linker command
                    throw std::runtime_error("Unexpected second command");
                }
                commandFound = true;
                BUILDBOX_LOG_DEBUG("Linker command: " << line);
                linkerArgs = commandArgs;

                // Drop stderr redirection
                if (linkerArgs.size() > 2 &&
                    linkerArgs[linkerArgs.size() - 2] == "2>") {
                    linkerArgs.pop_back();
                    linkerArgs.pop_back();
                }
            }
        }
    }

    if (!commandFound) {
        std::string errorMsg = "Unable to determine linker command: ";
        for (const auto &token : subprocessCommand) {
            errorMsg += (token + " ");
        }
        BUILDBOX_LOG_ERROR(errorMsg);
        BUILDBOX_LOG_DEBUG("stderr: " << subprocessResult.d_stdErr);
        throw std::runtime_error("Unable to determine linker command");
    }

    const std::string cwd = FileUtils::getCurrentWorkingDirectory();
    return ParsedCommandFactory::createParsedLinkerCommand(linkerArgs,
                                                           cwd.c_str());
}

CommandFileInfo LinkDeps::get_file_info(const ParsedCommand &parsedCommand)
{
    CommandFileInfo result;
    auto products = Deps::determine_products(parsedCommand);

    // This is a pure link command without source files as input.

    for (const auto &product : products) {
        result.d_possibleProducts.insert(
            buildboxcommon::FileUtils::normalizePath(product.c_str()));
    }

    if (!parsedCommand.is_gcc() && !parsedCommand.is_clang() &&
        !parsedCommand.is_sun_studio()) {
        BUILDBOX_LOG_INFO("Unsupported compiler in link command");
        return result;
    }

#if defined(__linux__) || defined(__sun)
    // Determine linker command and parse it
    const auto linkerCommand = parse_linker_command(parsedCommand);
    if (!linkerCommand.is_linker_command()) {
        throw std::runtime_error("Unsupported linker command");
    }

    // All direct input files are dependencies of the link command
    for (const auto &inputFile : linkerCommand.d_inputFiles) {
        result.d_dependencies.insert(inputFile);
    }
    for (const auto &inputFile : linkerCommand.d_auxInputFiles) {
        result.d_dependencies.insert(inputFile);
    }

    // Default library search path of the linker
    auto defaultLibrarySearchPath =
        get_library_search_path(parsedCommand, linkerCommand);

    // Effective library search path: Directories specified on the command
    // line (-L) are searched before the default directories.
    auto libraryDirs = linkerCommand.d_libraryDirs;
    libraryDirs.insert(libraryDirs.end(), defaultLibrarySearchPath.begin(),
                       defaultLibrarySearchPath.end());

    auto staticLibraries = linkerCommand.d_staticLibraries;

    // Queue of shared libraries to gather indirect dependencies from
    std::deque<std::string> sharedLibraryQueue;
    // Set of shared libraries whose dependencies have been processed
    std::set<std::string> processedSharedLibraries;

    // First try to find a shared library for each `-l` option
    for (const auto &library : linkerCommand.d_libraries) {
        // Determine filename based on the value of a `-l` option
        std::string filename;
        if (library.front() == ':') {
            filename = library.substr(1);
        }
        else {
            filename = "lib" + library + ".so";
        }
        bool found = false;
        for (const auto &libraryDir : libraryDirs) {
            const auto libraryPath = libraryDir + "/" + filename;
            if (buildboxcommon::FileUtils::isRegularFile(
                    libraryPath.c_str())) {
                // Normalize path but don't follow symlinks, otherwise the
                // linker can't find the library with remote execution.
                const auto normalized =
                    std::filesystem::path(libraryPath).lexically_normal();
                result.d_dependencies.insert(normalized);
                sharedLibraryQueue.push_back(normalized);
                found = true;
                break;
            }
        }
        if (!found) {
            staticLibraries.insert(library);
        }
    }

    // Find static libraries for each `-l` option where a shared library
    // wasn't found or a static library was explicitly requested (`-static`)
    for (const auto &library : staticLibraries) {
        // Determine filename based on the value of a `-l` option
        std::string filename;
        if (library.front() == ':') {
            filename = library.substr(1);
        }
        else {
            filename = "lib" + library + ".a";
        }
        bool found = false;
        for (const auto &libraryDir : libraryDirs) {
            const auto libraryPath = libraryDir + "/" + filename;
            if (buildboxcommon::FileUtils::isRegularFile(
                    libraryPath.c_str())) {
                result.d_dependencies.insert(libraryPath);
                found = true;
                break;
            }
        }
        if (!found) {
            throw std::runtime_error("Library not found: " + filename);
        }
    }

    // Build the search path for dependencies of shared libraries
    // (i.e., indirect dependencies of the main target).
    // This is based on the `-rpath-link` documentation of `ld`.
    // --rpath-link
    std::vector<std::string> rpathDirs = linkerCommand.d_rpathLinkDirs;
    // --rpath
    rpathDirs.insert(rpathDirs.end(), linkerCommand.d_rpathDirs.begin(),
                     linkerCommand.d_rpathDirs.end());
    if (rpathDirs.empty()) {
        add_directories_from_path(&rpathDirs, "LD_RUN_PATH");
    }
    add_directories_from_path(&rpathDirs, "LD_LIBRARY_PATH");
#ifdef __linux__
    parse_ld_so_conf("/etc/ld.so.conf", &rpathDirs);
#endif
    rpathDirs.insert(rpathDirs.end(), defaultLibrarySearchPath.begin(),
                     defaultLibrarySearchPath.end());

    // Gather indirect dependencies (DT_NEEDED of dependencies)
    while (!sharedLibraryQueue.empty()) {
        const auto sharedLibrary = sharedLibraryQueue.front();
        sharedLibraryQueue.pop_front();
        if (!processedSharedLibraries.insert(sharedLibrary).second) {
            // Shared library has already been processed
            continue;
        }

#if defined(__linux__)
        const auto neededLibraries = linux_get_needed_libraries(sharedLibrary);
#elif defined(__sun)
        const auto neededLibraries =
            solaris_get_needed_libraries(sharedLibrary);
#endif
        for (const auto &filename : neededLibraries) {
            bool found = false;
            for (const auto &rpathDir : rpathDirs) {
                const auto libraryPath = rpathDir + "/" + filename;
                if (buildboxcommon::FileUtils::isRegularFile(
                        libraryPath.c_str())) {
                    // Normalize path but don't follow symlinks, otherwise the
                    // linker can't find the library with remote execution.
                    const auto normalized =
                        std::filesystem::path(libraryPath).lexically_normal();
                    result.d_dependencies.insert(normalized);
                    sharedLibraryQueue.push_back(normalized);
                    found = true;
                    break;
                }
            }
            if (!found) {
                throw std::runtime_error("Library not found: " + filename);
            }
        }
    }
#else
    BUILDBOX_LOG_INFO("Unsupported platform for link command");
#endif

    return result;
}

} // namespace recc
