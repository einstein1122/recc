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

#include <iostream>
#include <string>

#include <google/protobuf/util/time_util.h>

#include <buildboxcommon_executionstatsutils.h>
#include <buildboxcommon_fileutils.h>
#include <buildboxcommon_grpcerror.h>
#include <buildboxcommon_logging.h>
#include <buildboxcommon_systemutils.h>
#include <buildboxcommon_temporaryfile.h>
#include <buildboxcommon_timeutils.h>

#include <executioncontext.h>
#include <reccsignals.h>
#include <verifyutils.h>

#define VERIFY_ERROR(x)                                                       \
    {                                                                         \
        std::stringstream _msg;                                               \
        _msg << x;                                                            \
        BUILDBOX_LOG_ERROR(_msg.str());                                       \
        std::cerr << _msg.str() << "\n";                                      \
    }

namespace recc {

bool isObjectFile(const std::string &path)
{
    const std::string suffix = ".o";
    return path.size() > suffix.size() &&
           path.substr(path.size() - suffix.size()) == suffix;
}

bool verifyOutputFile(buildboxcommon::CASClient *casClient,
                      const buildboxcommon::OutputFile &localFile,
                      const buildboxcommon::OutputFile &remoteFile)
{
    if (localFile.path() == remoteFile.path()) {
        if (localFile.digest() == remoteFile.digest()) {
            BUILDBOX_LOG_INFO("recc verify: File digest match for '"
                              << localFile.path()
                              << "': " << localFile.digest().hash() << "/"
                              << localFile.digest().size_bytes());
            return true;
        }
        else if (isObjectFile(localFile.path())) {
            const std::string stripCommand =
                buildboxcommon::SystemUtils::getPathToCommand("strip");
            if (stripCommand.empty()) {
                VERIFY_ERROR("recc verify: strip: command not found");
                return false;
            }

            buildboxcommon::TemporaryFile tempLocalFile, tempRemoteFile;
            try {
                casClient->download(tempRemoteFile.fd(), remoteFile.digest());
            }
            catch (const std::exception &e) {
                VERIFY_ERROR("recc verify: Download failed for output "
                             << remoteFile.digest().hash() << "/"
                             << remoteFile.digest().size_bytes() << ": "
                             << e.what());
                return false;
            }
            try {
                buildboxcommon::FileUtils::copyFile(localFile.path().c_str(),
                                                    tempLocalFile.name());
            }
            catch (const std::exception &e) {
                VERIFY_ERROR("recc verify: Failed to copy local output "
                             << localFile.path() << ": " << e.what());
                return false;
            }
            int stripExit = buildboxcommon::SystemUtils::executeCommandAndWait(
                {stripCommand, tempLocalFile.name(), tempRemoteFile.name()});
            if (stripExit != 0) {
                VERIFY_ERROR("recc verify: strip failed with exit code "
                             << stripExit << " for '" << localFile.path()
                             << "'");
                return false;
            }

            const auto localStrippedDigest =
                buildboxcommon::CASHash::hash(tempLocalFile.fd());
            const auto remoteStrippedDigest =
                buildboxcommon::CASHash::hash(tempRemoteFile.fd());

            if (localStrippedDigest == remoteStrippedDigest) {
                BUILDBOX_LOG_INFO("recc verify: File digest match for '"
                                  << localFile.path() << "' after stripping: "
                                  << localStrippedDigest.hash() << "/"
                                  << localStrippedDigest.size_bytes());
                return true;
            }
        }
        VERIFY_ERROR("recc verify: File digest mismatch for '"
                     << localFile.path() << "': local "
                     << localFile.digest().hash() << "/"
                     << localFile.digest().size_bytes() << ", remote "
                     << remoteFile.digest().hash() << "/"
                     << remoteFile.digest().size_bytes());
        return false;
    }
    else {
        VERIFY_ERROR("recc verify: File path mismatch: local '"
                     << localFile.path() << "', remote '" << remoteFile.path()
                     << "'");
        return false;
    }
}

int verifyRemoteBuild(int argc, char **argv,
                      build::buildbox::CompilerExecutionData *compilationData)
{
    int exitCode;

    setupSignals();

    BUILDBOX_LOG_INFO("recc verify: Local build");
    setenv("RECC_SKIP_CACHE", "1", 1);
    setenv("RECC_CACHE_ONLY", "1", 1);
    setenv("RECC_CACHE_UPLOAD_LOCAL_BUILD", "1", 1);
    setenv("RECC_ACTION_SALT", "verify:local", 1);
    recc::ExecutionContext localReccContext;
    localReccContext.setStopToken(s_signalReceived);
    try {
        exitCode = localReccContext.execute(argc, argv);
    }
    catch (const std::exception &e) {
        if (s_signalReceived) {
            std::cerr << "recc: caught signal " << s_signalValue << "\n";
        }
        else {
            VERIFY_ERROR(
                "recc verify failed during local execution: " << e.what());
        }

        return 1;
    }
    setenv("RECC_SKIP_CACHE", "", 1);
    setenv("RECC_CACHE_ONLY", "", 1);
    setenv("RECC_CACHE_UPLOAD_LOCAL_BUILD", "", 1);
    if (exitCode != 0) {
        BUILDBOX_LOG_INFO("recc verify: Local build failed with exit code "
                          << exitCode);
    }

    if (localReccContext.getActionDigest().hash().empty()) {
        // No Action available, skip remote execution and verification
        BUILDBOX_LOG_INFO("recc verify: Not a compiler command");
        return exitCode;
    }

    BUILDBOX_LOG_INFO("recc verify: Remote execution");
    // We use output from local compilation, no need to download outputs
    setenv("RECC_DONT_SAVE_OUTPUT", "1", 1);
    setenv("RECC_ACTION_SALT", "verify:remote", 1);
    recc::ExecutionContext remoteReccContext;
    remoteReccContext.setStopToken(s_signalReceived);
    try {
        remoteReccContext.execute(argc, argv);
    }
    catch (const std::exception &e) {
        if (s_signalReceived) {
            std::cerr << "recc: caught signal " << s_signalValue << "\n";
        }
        else {
            VERIFY_ERROR(
                "recc verify failed during remote execution: " << e.what());
        }

        return 1;
    }

    auto reccData = compilationData->mutable_recc_data();

    reccData->mutable_action_digest()->CopyFrom(
        localReccContext.getActionDigest());

    BUILDBOX_LOG_INFO("recc verify: Local action digest "
                      << localReccContext.getActionDigest().hash() << "/"
                      << localReccContext.getActionDigest().size_bytes()
                      << ", remote action digest "
                      << remoteReccContext.getActionDigest().hash() << "/"
                      << remoteReccContext.getActionDigest().size_bytes());

    const auto &localResult = localReccContext.getActionResult();
    const auto &remoteResult = remoteReccContext.getActionResult();

    if (localResult.exit_code() != remoteResult.exit_code()) {
        VERIFY_ERROR("recc verify: Exit code mismatch: local "
                     << localResult.exit_code() << ", remote "
                     << remoteResult.exit_code());
        return 1;
    }

    buildboxcommon::CASClient *casClient = remoteReccContext.getCasClient();

    if (localResult.output_files().size() ==
        remoteResult.output_files().size()) {
        for (int i = 0; i < localResult.output_files().size(); i++) {
            if (!verifyOutputFile(casClient, localResult.output_files(i),
                                  remoteResult.output_files(i))) {
                exitCode = 1;
            }
        }
    }
    else {
        VERIFY_ERROR("recc verify: Different number of output files");
        exitCode = 1;
    }

    if (localResult.output_directories().size() ==
        remoteResult.output_directories().size()) {
        for (int i = 0; i < localResult.output_directories().size(); i++) {
            const auto &localDirectory = localResult.output_directories(i);
            const auto &remoteDirectory = remoteResult.output_directories(i);
            if (localDirectory.path() == remoteDirectory.path()) {
                if (localDirectory.tree_digest() ==
                    remoteDirectory.tree_digest()) {
                    BUILDBOX_LOG_INFO(
                        "recc verify: Directory digest match for '"
                        << localDirectory.path()
                        << "': " << localDirectory.tree_digest().hash() << "/"
                        << localDirectory.tree_digest().size_bytes());
                }
                else {
                    VERIFY_ERROR(
                        "recc verify: Directory digest mismatch for '"
                        << localDirectory.path() << "': local "
                        << localDirectory.tree_digest().hash() << "/"
                        << localDirectory.tree_digest().size_bytes()
                        << ", remote " << remoteDirectory.tree_digest().hash()
                        << "/" << remoteDirectory.tree_digest().size_bytes());
                    exitCode = 1;
                }
            }
            else {
                VERIFY_ERROR("recc verify: Directory path mismatch: local '"
                             << localDirectory.path() << "', remote '"
                             << remoteDirectory.path() << "'");
                exitCode = 1;
            }
        }
    }
    else {
        VERIFY_ERROR("recc verify: Different number of output directories");
        exitCode = 1;
    }

    return exitCode;
}
} // namespace recc
