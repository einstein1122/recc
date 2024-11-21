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

namespace recc {

// Supported Compilers
const SupportedCompilers::CompilerListType SupportedCompilers::Gcc = {
    "gcc", "g++", "c99-gcc", "c++", "clang", "clang++"};
const SupportedCompilers::CompilerListType
    SupportedCompilers::GccPreprocessor = {"gcc-preprocessor"};
const SupportedCompilers::CompilerListType SupportedCompilers::SunCPP = {"CC"};
const SupportedCompilers::CompilerListType SupportedCompilers::AIX = {
    "xlc", "xlc++", "xlC", "xlCcore", "xlc++core"};
const SupportedCompilers::CompilerListType SupportedCompilers::CCompilers = {
    "cc", "c89", "c99"};

// Default Deps
const std::vector<std::string> SupportedCompilers::GccDefaultDeps = {"-M"};
const std::vector<std::string> SupportedCompilers::SunCPPDefaultDeps = {"-xM"};
const std::vector<std::string> SupportedCompilers::AIXDefaultDeps = {
    "-qsyntaxonly", "-M", "-MF"};

// Default languages
const std::set<std::string> SupportedCompilers::GccSupportedLanguages = {
    "c",
    "c++",
    "c-header",
    "c++-header",
    "c++-system-header",
    "c++-user-header"};

} // namespace recc
