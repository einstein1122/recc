// Copyright 2022 Bloomberg Finance L.P
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

#ifndef INCLUDED_DATAUTILS
#define INCLUDED_DATAUTILS

#include <sys/time.h>

#include <build/buildbox/local_execution.pb.h>

namespace recc {

struct DataUtils {
    static void collectCompilationData(
        const int argc, char const *const *argv,
        const std::string unresolvedPathToCommand,
        build::buildbox::CompilerExecutionData &compilationData);
    static void
    sendData(const build::buildbox::CompilerExecutionData &compilationData);
};

} // namespace recc
#endif
