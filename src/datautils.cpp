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

#include <algorithm>
#include <cctype>
#include <exception>
#include <fstream>
#include <locale>
#include <netinet/in.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include <buildboxcommon_cashash.h>
#include <buildboxcommon_platformutils.h>
#include <buildboxcommon_stringutils.h>
#include <buildboxcommon_systemutils.h>
#include <buildboxcommon_timeutils.h>

#include <datautils.h>
#include <env.h>

extern "C" char **environ;

namespace recc {

const std::set<std::string> SOURCE_FILE_SUFFIXES = {
    ".C", ".cc", ".cpp", ".CPP", ".c++", ".cp", ".cxx", ".c",
};

void DataUtils::collectCompilationData(
    const int argc, char const *const *argv,
    const std::string unresolvedPathToCommand,
    build::buildbox::CompilerExecutionData &compilationData)
{
    // Record timestamp before doing any other operations
    *compilationData.mutable_timestamp() = buildboxcommon::TimeUtils::now();

    // Compiler command
    // Store the first token as the "command" and the full string as the "full
    // command"
    std::string fullCommand;
    std::string sourceFile;
    for (int i = 0; i < argc; i++) {
        if (i == 0) {
            // Populate 'command' with the resolved path
            *compilationData.mutable_command() = argv[i];

            // Populate 'fullCommand' with the unresolved path
            fullCommand = unresolvedPathToCommand;
        }
        else {
            fullCommand += " ";
            fullCommand += argv[i];
        }

        // Check if this file ends with a source file ending to treat it as a
        // source file
        std::string arg(argv[i]);
        size_t suffixStart = arg.rfind(".");
        if (suffixStart != std::string::npos) {
            std::string suffix = arg.substr(suffixStart);
            if (SOURCE_FILE_SUFFIXES.count(suffix)) {
                try {
                    const buildboxcommon::Digest readDigest =
                        buildboxcommon::CASHash::hashFile(argv[i]);
                    build::bazel::remote::execution::v2::FileNode
                        *sourceFileInfo =
                            compilationData.add_source_file_info();
                    *sourceFileInfo->mutable_name() = arg;
                    sourceFileInfo->mutable_digest()->CopyFrom(readDigest);
                }
                catch (std::system_error &e) {
                    // Unable to open file
                }
            }
        }
    }
    *compilationData.mutable_full_command() = fullCommand;

    // Working directory
    *compilationData.mutable_working_directory() =
        buildboxcommon::SystemUtils::getCurrentWorkingDirectory();

    // Environment variables
    for (char **env = environ; *env != NULL; env++) {
        std::string envVarWithEquals = std::string(*env);
        std::string envVarName =
            envVarWithEquals.substr(0, envVarWithEquals.find("="));
        std::string envVarValue =
            envVarWithEquals.substr(envVarWithEquals.find("=") + 1);
        (*compilationData.mutable_environment_variables())[envVarName] =
            envVarValue;
    }

    // Platform
    build::bazel::remote::execution::v2::Platform platform;
    // ISA
    build::bazel::remote::execution::v2::Platform::Property *isaProperty =
        platform.add_properties();
    isaProperty->set_name("ISA");
    isaProperty->set_value(buildboxcommon::PlatformUtils::getHostISA());
    // OSFamily
    build::bazel::remote::execution::v2::Platform::Property *osProperty =
        platform.add_properties();
    osProperty->set_name("OSFamily");
    osProperty->set_value(buildboxcommon::PlatformUtils::getHostOSFamily());

    *compilationData.mutable_platform() = platform;

    // Correlated invocations id
    *compilationData.mutable_correlated_invocations_id() =
        RECC_CORRELATED_INVOCATIONS_ID;
}

void DataUtils::sendData(
    const build::buildbox::CompilerExecutionData &compilationData)
{
    uint16_t port;
    char *portEnd; // Needed for strtol
    long parsedPort =
        strtol(RECC_COMPILATION_METADATA_UDP_PORT.c_str(), &portEnd, 10);
    if (parsedPort <= 0 || parsedPort > UINT16_MAX) {
        // Invalid values
        return;
    }
    port = (uint16_t)parsedPort;

    // Construct destination address
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddr.sin_port = htons(port);

    // Use a blocking socket because Linux queue lengths can be small
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        // TODO: Figure out how to report this error
        return;
    }

    const std::string serializedData = compilationData.SerializeAsString();
    long bytesSent =
        sendto(sockfd, serializedData.c_str(), serializedData.length(), 0,
               (struct sockaddr *)&serverAddr, sizeof(struct sockaddr_in));
    if (bytesSent == -1) {
        // TODO: Figure out how to report this error
    }
    else if ((size_t)bytesSent != serializedData.length()) {
        // It's possible we didn't manage to fit the entire protobuf into the
        // datagram, which is highly problematic as the receiver will not
        // receive something it can fully parse

        // TODO: Figure out how to report this error
    }
    close(sockfd);
}

} // namespace recc
