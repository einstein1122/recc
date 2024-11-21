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

#ifndef INCLUDED_LINKDEPS
#define INCLUDED_LINKDEPS

#include <deps.h>
#include <parsedcommand.h>
#include <string>
#include <vector>

namespace recc {

struct LinkDeps {
    /**
     * Returns the names of the files needed to run the command.
     *
     * The command must be a supported linker command.
     */
    static CommandFileInfo get_file_info(const ParsedCommand &command);
};

} // namespace recc

#endif
