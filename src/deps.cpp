// Copyright 2018 Bloomberg Finance L.P
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

#include <deps.h>

#include <clangscandeps.h>
#include <compilerdefaults.h>
#include <env.h>
#include <fileutils.h>
#include <linkdeps.h>
#include <subprocess.h>

#include <buildboxcommon_fileutils.h>
#include <buildboxcommon_logging.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <regex>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>

namespace recc {

std::set<std::string>
Deps::dependencies_from_make_rules(const std::string &rules,
                                   bool is_sun_format)
{
    std::set<std::string> result;
    bool saw_colon_on_line = false;
    bool saw_backslash = false;

    std::string current_filename;
    for (const char &character : rules) {
        if (saw_backslash) {
            saw_backslash = false;
            if (character != '\n' && saw_colon_on_line) {
                current_filename += character;
            }
        }
        else if (character == '\\') {
            saw_backslash = true;
        }
        else if (character == ':' && !saw_colon_on_line) {
            saw_colon_on_line = true;
        }
        else if (character == '\n') {
            saw_colon_on_line = false;
            if (!current_filename.empty()) {
                result.insert(current_filename);
            }
            current_filename.clear();
        }
        else if (character == ' ') {
            if (is_sun_format) {
                if (!current_filename.empty() && saw_colon_on_line) {
                    current_filename += character;
                }
            }
            else {
                if (!current_filename.empty()) {
                    result.insert(current_filename);
                }
                current_filename.clear();
            }
        }
        else if (saw_colon_on_line) {
            current_filename += character;
        }
    }

    if (!current_filename.empty()) {
        result.insert(current_filename);
    }

    return result;
}

std::string Deps::crtbegin_from_clang_v(const std::string &str)
{
    // Look for lines of the form:
    // ^Selected GCC installation: <path>$
    // and
    // ^Selected multilib: <path>;.*$
    // Then return these two paths joined (in order) with crtbegin.o appended.
    //
    // Reference:
    // https://github.com/llvm-mirror/clang/blob/69f63a0cc21da9f587125760f10610146c8c47c3/lib/Driver/ToolChains/Gnu.cpp#L1747

    std::regex re(
        "Selected GCC installation: ([^\n]*).*Selected multilib: ([^;\n]*)",
        std::regex::extended);
    std::smatch m;
    if (!std::regex_search(str, m, re)) {
        BUILDBOX_LOG_DEBUG("Failed to locate crtbegin.o for clang");
        return "";
    }

    std::ostringstream s;
    s << m[1];
    if (m[2] != ".") {
        // Avoid redundant .'s in the path.
        s << "/" << m[2];
    }
    s << "/crtbegin.o";

    std::string crtbegin_file = s.str();
    BUILDBOX_LOG_DEBUG("Found crtbegin.o for clang: " << crtbegin_file);

    return crtbegin_file;
}

CommandFileInfo Deps::get_file_info(const ParsedCommand &parsedCommand)
{
    return get_file_info(parsedCommand, [](auto &&...) {});
}

CommandFileInfo
Deps::get_file_info(const ParsedCommand &parsedCommand,
                    const CounterMetricCallback &recordCounterMetric)
{
    if (parsedCommand.is_linker_command()) {
        return LinkDeps::get_file_info(parsedCommand);
    }

    CommandFileInfo result;
    auto products = determine_products(parsedCommand);

    std::vector<std::string> objectTargets;
    for (const auto &product : products) {
        result.d_possibleProducts.insert(
            buildboxcommon::FileUtils::normalizePath(product.c_str()));

        if (product.substr(product.size() - 2) == ".o") {
            objectTargets.push_back(product);
        }
    }

    // Use clang-scan-deps to get dependencies, if available and configured.
    if (objectTargets.size() == 1) {
        if (ClangScanDeps::dependencies_for_target(
                parsedCommand, objectTargets[0], &result.d_dependencies,
                recordCounterMetric)) {
            return result;
        }
    }

    if (RECC_VERBOSE == true) {
        std::ostringstream dep_command;
        for (auto &depc : parsedCommand.get_dependencies_command()) {
            dep_command << depc << " ";
        }
        BUILDBOX_LOG_DEBUG(
            "Getting dependencies using the command: " << dep_command.str());
    }

    const auto subprocessResult = Subprocess::execute(
        parsedCommand.get_dependencies_command(), true, true, RECC_DEPS_ENV);

    if (subprocessResult.d_exitCode != 0) {
        std::string errorMsg = "Failed to execute get dependencies command: ";
        for (const auto &token : parsedCommand.get_dependencies_command()) {
            errorMsg += (token + " ");
        }
        BUILDBOX_LOG_ERROR(errorMsg);
        BUILDBOX_LOG_ERROR("Exit status: " << subprocessResult.d_exitCode);
        BUILDBOX_LOG_DEBUG("stdout: " << subprocessResult.d_stdOut);
        BUILDBOX_LOG_DEBUG("stderr: " << subprocessResult.d_stdErr);
        throw subprocess_failed_error(subprocessResult.d_exitCode);
    }

    std::string dependencies = subprocessResult.d_stdOut;

    // If AIX compiler, read dependency information from temporary file.

    if (parsedCommand.is_AIX()) {
        dependencies = buildboxcommon::FileUtils::getFileContents(
            parsedCommand.get_aix_dependency_file_name().c_str());
    }

    result.d_dependencies = dependencies_from_make_rules(
        dependencies, parsedCommand.produces_sun_make_rules());

    if (RECC_DEPS_GLOBAL_PATHS && parsedCommand.is_clang()) {
        // Clang tries to locate GCC installations by looking for crtbegin.o
        // and then adjusts its system include paths. We need to upload this
        // file as if it were an input.
        std::string crtbegin =
            crtbegin_from_clang_v(subprocessResult.d_stdErr);
        if (crtbegin != "") {
            result.d_dependencies.insert(crtbegin);
        }
    }

    for (const auto &inputFile : parsedCommand.d_inputFiles) {
        if (is_aux_input_file(inputFile, parsedCommand)) {
            result.d_dependencies.insert(inputFile);
        }
    }

    return result;
}

std::set<std::string>
Deps::determine_products(const ParsedCommand &parsedCommand)
{
    // Validate the inputs.
    std::set<std::string> headers, sources, objects, result;
    for (const auto &sourceFile : parsedCommand.d_inputFiles) {
        if (parsedCommand.is_compiler_command() &&
            is_header_file(sourceFile)) {
            headers.insert(sourceFile);
        }
        else if (parsedCommand.is_compiler_command() &&
                 is_source_file(sourceFile)) {
            sources.insert(sourceFile);
        }
        else if (parsedCommand.is_compiler_command() &&
                 is_aux_input_file(sourceFile, parsedCommand)) {
            // An input file that doesn't produce a separate output file
        }
        else if (parsedCommand.is_linker_command() &&
                 is_object_file(sourceFile)) {
            objects.insert(sourceFile);
        }
        else {
            throw std::invalid_argument(
                "File '" + sourceFile +
                "' uses a file suffix unsupported for caching");
        }
    }

    if (headers.empty() && sources.empty() && objects.empty()) {
        // No products without inputs
        return result;
    }

    // Determine which products to assume as base products.
    if (!parsedCommand.get_products().empty()) {
        for (const auto &product : parsedCommand.get_products()) {
            result.insert(product);
        }
    }
    else if (parsedCommand.is_linker_command()) {
        result.insert("a.out");
    }
    else {
        for (const auto &header : headers) {
            // Leave this file in the header directory.
            result.insert(header + ".gch");
        }
        for (const auto &source : sources) {
            result.insert(FileUtils::stripDirectory(
                FileUtils::replaceSuffix(source, ".o")));
        }
    }

    // If -MD/MMD was set and -MF was not specified, include make dependency
    // files. The priority order for determining product names are:
    // - Any explicitly specified values. Do not modify.
    // - The -o output filename. Strips the last suffix and adds ".d".
    // - The header or source input. Strips the last suffix and adds ".d".
    // The xlc -qmakedep option works similarly but uses ".u" as suffix.
    if (parsedCommand.d_md_option_set || parsedCommand.d_qmakedep_option_set) {
        const auto suffix = parsedCommand.d_md_option_set ? ".d" : ".u";
        if (!parsedCommand.get_deps_products().empty()) {
            for (const auto &product : parsedCommand.get_deps_products()) {
                result.insert(product);
            }
        }
        else if (!parsedCommand.get_products().empty()) {
            for (const auto &product : parsedCommand.get_products()) {
                result.insert(FileUtils::replaceSuffix(product, suffix));
            }
        }
        else {
            for (const auto &header : headers) {
                result.insert(FileUtils::stripDirectory(
                    FileUtils::replaceSuffix(header, suffix)));
            }
            for (const auto &source : sources) {
                result.insert(FileUtils::stripDirectory(
                    FileUtils::replaceSuffix(source, suffix)));
            }
        }
    }

    // Add any coverage products if configured.
    // The priority order for determining product names are:
    // - Any explicitly specified values. Do not modify.
    // - The -o output filename. Strips the last suffix and adds ".gnco".
    // - The header or source input. Strips the last suffix and adds ".gnco".
    if (parsedCommand.d_coverage_option_set) {
        if (!parsedCommand.get_coverage_products().empty()) {
            for (const auto &product : parsedCommand.get_coverage_products()) {
                result.insert(product);
            }
        }
        else if (!parsedCommand.get_products().empty()) {
            for (const auto &product : parsedCommand.get_products()) {
                result.insert(FileUtils::replaceSuffix(product, ".gcno"));
            }
        }
        else {
            for (const auto &header : headers) {
                result.insert(FileUtils::stripDirectory(
                    FileUtils::replaceSuffix(header, ".gcno")));
            }
            for (const auto &source : sources) {
                result.insert(FileUtils::stripDirectory(
                    FileUtils::replaceSuffix(source, ".gcno")));
            }
        }
    }

    // Add dwarf files if -gsplit-dwarf is set.
    // The priority order for determining product names are:
    // - The -o output filename. Strips the last suffix and adds ".dwo".
    //   - Skip if -o being is used for renaming a ".gch" output.
    // - The source input. Strips the last suffix and adds ".dwo".
    if (parsedCommand.d_split_dwarf_option_set) {
        if (!parsedCommand.get_products().empty()) {
            if (!sources.empty()) {
                for (const auto &product : parsedCommand.get_products()) {
                    result.insert(FileUtils::replaceSuffix(product, ".dwo"));
                }
            }
        }
        else {
            for (const auto &source : sources) {
                result.insert(FileUtils::stripDirectory(
                    FileUtils::replaceSuffix(source, ".dwo")));
            }
        }
    }

    return result;
}

bool Deps::is_header_file(const std::string &file)
{
    const std::set<std::string> header_suffixes = {
        "h", "hh", "H", "hp", "hxx", "hpp", "HPP", "h++", "tcc"};
    const std::size_t pos = file.find_last_of(".");
    if (pos != std::string::npos) {
        const std::string suffix = file.substr(pos + 1);
        return header_suffixes.find(suffix) != header_suffixes.end();
    }
    return false;
}

bool Deps::is_source_file(const std::string &file)
{
    const std::set<std::string> source_suffixes = {"cc",  "c",   "cp",  "cxx",
                                                   "cpp", "CPP", "c++", "C"};
    const std::size_t pos = file.find_last_of(".");
    if (pos != std::string::npos) {
        const std::string suffix = file.substr(pos + 1);
        return source_suffixes.find(suffix) != source_suffixes.end();
    }
    return false;
}

bool Deps::is_aux_input_file(const std::string &file,
                             const ParsedCommand &parsedCommand)
{
    if (parsedCommand.is_sun_studio()) {
        const std::set<std::string> suffixes = {"il"};
        const std::size_t pos = file.find_last_of(".");
        if (pos != std::string::npos) {
            const std::string suffix = file.substr(pos + 1);
            return suffixes.find(suffix) != suffixes.end();
        }
    }
    return false;
}

bool Deps::is_object_file(const std::string &file)
{
    const std::set<std::string> object_suffixes = {"a", "o", "so"};
    const std::size_t pos = file.find_last_of(".");
    if (pos != std::string::npos) {
        const std::string suffix = file.substr(pos + 1);
        return object_suffixes.find(suffix) != object_suffixes.end();
    }
    return false;
}

} // namespace recc
