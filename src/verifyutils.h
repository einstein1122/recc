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

#ifndef INCLUDED_VERIFYUTILS
#define INCLUDED_VERIFYUTILS

#include <string>

#include <buildboxcommon_casclient.h>

#include <build/buildbox/local_execution.pb.h>

namespace recc {

bool isObjectFile(const std::string &path);

bool verifyOutputFile(buildboxcommon::CASClient *casClient,
                      const buildboxcommon::OutputFile &localFile,
                      const buildboxcommon::OutputFile &remoteFile);

int verifyRemoteBuild(int argc, char **argv,
                      build::buildbox::CompilerExecutionData *compilationData);

} // namespace recc
#endif
