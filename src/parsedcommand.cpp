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
#include <fileutils.h>
#include <parsedcommand.h>

#include <buildboxcommon_fileutils.h>
#include <buildboxcommon_logging.h>
#include <buildboxcommon_systemutils.h>

#include <system_error>

namespace recc {

ParsedCommand::ParsedCommand(const std::vector<std::string> &command,
                             const std::string &workingDirectory)
    : d_compilerCommand(false), d_isClang(false),
      d_producesSunMakeRules(false), d_containsUnsupportedOptions(false),
      d_dependencyFileAIX(nullptr)
{
    if (command.empty()) {
        return;
    }

    const std::string &compiler = command[0];

    if (compiler.empty()) {
        return;
    }

    d_compiler = commandBasename(compiler);
    if (SupportedCompilers::Gcc.count(d_compiler)) {
        d_defaultDepsCommand = SupportedCompilers::GccDefaultDeps;
        d_isClang = (d_compiler == "clang" || d_compiler == "clang++");
        d_isGcc = !d_isClang;
    }

    else if (SupportedCompilers::SunCPP.count(d_compiler)) {
        d_defaultDepsCommand = SupportedCompilers::SunCPPDefaultDeps;
        d_producesSunMakeRules = true;
        d_isSunStudio = true;
    }

    else if (SupportedCompilers::AIX.count(d_compiler)) {
        d_defaultDepsCommand = SupportedCompilers::AIXDefaultDeps;
        d_producesSunMakeRules = true;
        // Create a temporary file, which will be used to write
        // dependency information to, on AIX.
        // The lifetime of the temporary file is the same as the
        // instance of parsedcommand.
        d_dependencyFileAIX = std::make_unique<buildboxcommon::TemporaryFile>(
            buildboxcommon::TemporaryFile());
        // Push back created file name to vector of the dependency
        // command->
        d_defaultDepsCommand.push_back(d_dependencyFileAIX->strname());
    }

    if (d_isClang && RECC_DEPS_GLOBAL_PATHS) {
        // Clang mentions where it found crtbegin.o in
        // stderr with this flag.
        d_defaultDepsCommand.push_back("-v");
    }

    // Preinsert the compiler path into the command and dependencies command.
    // However, don't normalize it when modifying it for the remote setting.
    //
    // We don't normalize the compiler path because normalization can
    // sometimes remove all slashes from a path like "./gcc".
    //
    // Per the remote execution API, the command must be specificed as an
    // absolute or relative path. To distinguish between relative paths and
    // arbitrary commands, we disallow non-slashed commands like "gcc", as
    // allowing those could cause the user to accidentally rely on an
    // executable in a remote worker's PATH.
    const std::string &replacedCompilerPath =
        FileUtils::modifyPathForRemote(compiler, workingDirectory, false);

    this->d_command.push_back(replacedCompilerPath);
    this->d_dependenciesCommand.push_back(compiler);

    // Insert everything except the original compiler command for parsing
    this->d_originalCommand.insert(this->d_originalCommand.begin(),
                                   std::next(command.begin()), command.end());
}

std::string ParsedCommand::commandBasename(const std::string &path,
                                           int symlinks)
{
    const auto lastSlash = path.rfind('/');
    const auto basename =
        (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);

    if (SupportedCompilers::CCompilers.count(basename)) {
        // Resolve symlink to get the final basename (e.g. cc => gcc).
        // Only do this for standard compiler names as we don't want to
        // follow symlinks such as clang++ => clang.
        const std::string absolutePath =
            buildboxcommon::SystemUtils::getPathToCommand(path);
        if (!absolutePath.empty() &&
            buildboxcommon::FileUtils::isSymlink(absolutePath.c_str())) {
            const int max_nested_links = 40;
            if (symlinks >= max_nested_links) {
                BUILDBOXCOMMON_THROW_SYSTEM_EXCEPTION(
                    std::system_error, ELOOP, std::system_category,
                    "Too many levels of symlinks for compiler command: "
                        << absolutePath);
            }
            const auto symlinkTarget = FileUtils::resolveSymlink(absolutePath);
            return commandBasename(symlinkTarget, symlinks + 1);
        }

#ifdef RECC_PLATFORM_COMPILER
        return RECC_PLATFORM_COMPILER;
#else
        return basename;
#endif
    }

    auto length = basename.length();

    // We get rid of "_r" suffixes in, for example, "./xlc++_r":
    const std::string rSuffix = "_r";
    if (length > 2 && basename.substr(length - rSuffix.length()) == rSuffix) {
        length -= 2;
    }
    else if (length > 3 &&
             basename.substr(length - 3, rSuffix.length()) == rSuffix) {
        length -= 3;
    }

    const auto is_version_character = [](const char character) {
        return (isdigit(character)) || character == '.' || character == '-';
    };

    while (length > 0 && is_version_character(basename[length - 1])) {
        --length;
    }

    return basename.substr(0, length);
}

} // namespace recc
