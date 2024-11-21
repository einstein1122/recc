// Copyright 2018-2021 Bloomberg Finance L.P
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

// bin/recc.cpp
//
// Runs a build command remotely. If the given command is not a build command,
// it's actually run locally.

#include <string>

#include <google/protobuf/util/time_util.h>

#include <buildboxcommon_executionstatsutils.h>
#include <buildboxcommon_fileutils.h>
#include <buildboxcommon_grpcerror.h>
#include <buildboxcommon_logging.h>
#include <buildboxcommon_systemutils.h>
#include <buildboxcommon_temporaryfile.h>
#include <buildboxcommon_timeutils.h>

#include <build/buildbox/local_execution.pb.h>
#include <datautils.h>
#include <digestgenerator.h>
#include <env.h>
#include <executioncontext.h>
#include <reccdefaults.h>
#include <reccsignals.h>
#include <remoteexecutionsignals.h>
#include <requestmetadata.h>
#include <verifyutils.h>

using namespace recc;

namespace {
/**
 * NOTE: If a variable is intended to be used in a configuration file, omit the
 * "RECC_" prefix.
 */
const std::string HELP(
    "USAGE: recc <command>\n"
    "\n"
    "If the given command is a compile command, runs it on a remote build\n"
    "server. Otherwise, runs it locally.\n"
    "\n"
    "If the command is to be executed remotely, it must specify either a \n"
    "relative or absolute path to an executable.\n"
    "\n"
    "The following environment variables can be used to change recc's\n"
    "behavior. To set them in a recc.conf file, omit the \"RECC_\" prefix.\n"
    "\n"
    "RECC_SERVER - the URI of the server to use (e.g. http://localhost:8085)\n"
    "\n"
    "RECC_CAS_SERVER - the URI of the CAS server to use (by default, \n"
    "                  uses RECC_ACTION_CACHE_SERVER if set. Else "
    "RECC_SERVER)\n"
    "\n"
    "RECC_ACTION_CACHE_SERVER - the URI of the Action Cache server to use (by "
    "default,\n"
    "                  uses RECC_CAS_SERVER. Else RECC_SERVER)\n"
    "\n"
    "RECC_INSTANCE - the instance name to pass to the server (defaults "
    "to \"" DEFAULT_RECC_INSTANCE "\") \n"
    "\n"
    "RECC_CAS_INSTANCE - the instance name to pass to the cas(by\n"
    "                    default, uses RECC_ACTION_CACHE_INSTANCE if set. "
    "Else RECC_INSTANCE)\n"
    "\n"
    "RECC_ACTION_CACHE_INSTANCE - the instance name to pass to the action\n"
    "                             cache (by default, uses "
    "RECC_CAS_INSTANCE if set. Else RECC_INSTANCE)\n"
    "\n"
    "RECC_CACHE_ONLY - if set to any value, runs recc in cache-only mode. In\n"
    "                  this mode, recc will build anything not available in \n"
    "                  the remote cache locally, rather than failing to "
    "build.\n"
    "\n"
    "RECC_CACHE_UPLOAD_FAILED_BUILD - Upload action results regardless of the "
    "exit\n"
    "                                 code of the sub-process executing the "
    "action.\n"
    "                                 This setting defaults to true. If set "
    "to false\n"
    "                                 only successful action results(exit "
    "codes equal to zero)\n"
    "                                 will be uploaded.\n"
    "\n"
    "RECC_RUNNER_COMMAND - if set, run the specified command to invoke a\n"
    "                      BuildBox runner for local execution.\n"
    "\n"
    "RECC_PROJECT_ROOT - the top-level directory of the project source.\n"
    "                    If the command contains paths inside the root, they\n"
    "                    will be rewritten to relative paths (by default, \n"
    "                    uses the current working directory)\n"
    "\n"
    "RECC_SERVER_AUTH_GOOGLEAPI - use default google authentication when\n"
    "                             communicating over gRPC, instead of\n"
    "                             using an insecure connection\n"
    "\n"
    "RECC_ACCESS_TOKEN_PATH - path specifying location of access token (JWT, "
    "OAuth, etc) to be attached to all secure connections.\n"
    "                         Defaults to \"" DEFAULT_RECC_ACCESS_TOKEN_PATH
    "\"\n"
    "RECC_LOG_LEVEL - logging verbosity level [optional, default "
    "= " DEFAULT_RECC_LOG_LEVEL ", supported = " +
    buildboxcommon::logging::stringifyLogLevels() +
    "] \n"
    "RECC_LOG_DIRECTORY - if set to a directory, output log messages to files "
    "in that location\n"
    "\n"
    "RECC_VERBOSE - if set to any value, equivalent to RECC_LOG_LEVEL=debug\n"
    "\n"
    "RECC_ENABLE_METRICS - if set to any value, enable metric collection \n"
    "\n"
    "RECC_METRICS_TAG_[key] - tag added to all published\n"
    "metrics, using format specified by RECC_STATSD_FORMAT.\n"
    "\n"
    "RECC_STATSD_FORMAT - if set to any value, the format used by statsd\n"
    "publisher, when tagging is set. Supports 'influx', 'graphite' and"
    "'dog'.\n"
    "\n"
    "RECC_METRICS_FILE - write metrics to that file (Default/Empty string — "
    "stderr). Cannot be used with RECC_METRICS_UDP_SERVER.\n"
    "\n"
    "RECC_METRICS_UDP_SERVER - write metrics to the specified host:UDP_Port.\n"
    " Cannot be used with RECC_METRICS_FILE\n"
    "\n"
    "RECC_COMPILATION_METADATA_UDP_PORT - if set, publish the higher-level "
    "compilation metadata to the specified localhost's UDP_Port.\n"
    "\n"
    "RECC_VERIFY - if set to any value, invoke the command both locally and "
    "remotely for verification purposes. Output digests are compared and "
    "logged.\n"
    "\n"
    "RECC_NO_PATH_REWRITE - if set to any value, do not rewrite absolute "
    "paths to be relative.\n"
    "\n"
    "RECC_COMPILE_CACHE_ONLY - equivalent to RECC_CACHE_ONLY but only for "
    "compile commands\n"
    "RECC_COMPILE_REMOTE_PLATFORM_[key] - equivalent to RECC_REMOTE_PLATFORM "
    "but only for compile commands\n"
    "\n"
    "RECC_LINK - if set to any value, use remote execution or remote caching\n"
    "            also for link commands\n"
    "RECC_LINK_METRICS_ONLY - if set to any value, enable metric collection\n"
    "                         for link commands without remote execution or\n"
    "                         caching\n"
    "RECC_LINK_CACHE_ONLY - equivalent to RECC_CACHE_ONLY but only for link "
    "commands\n"
    "RECC_LINK_REMOTE_PLATFORM_[key] - equivalent to RECC_REMOTE_PLATFORM but "
    "only for link commands\n"
    "\n"
    "RECC_FORCE_REMOTE - if set to any value, send all commands to the \n"
    "                    build server. (Non-compile commands won't be \n"
    "                    executed locally, which can cause some builds to \n"
    "                    fail.)\n"
    "\n"
    "RECC_ACTION_UNCACHEABLE - if set to any value, sets `do_not_cache` \n"
    "                          flag to indicate that the build action can \n"
    "                          never be cached\n"
    "\n"
    "RECC_SKIP_CACHE - if set to any value, sets `skip_cache_lookup` flag \n"
    "                  to re-run the build action instead of looking it up \n"
    "                  in the cache\n"
    "\n"
    "RECC_DONT_SAVE_OUTPUT - if set to any value, prevent build output from \n"
    "                        being saved to local disk\n"
    "\n"
    "RECC_DEPS_GLOBAL_PATHS - if set to any value, report all entries \n"
    "                         returned by the dependency command, even if \n"
    "                         they are absolute paths\n"
    "\n"
    "RECC_DEPS_OVERRIDE - comma-separated list of files to send to the\n"
    "                     build server (by default, run `deps` to\n"
    "                     determine this)\n"
    "\n"
    "RECC_DEPS_DIRECTORY_OVERRIDE - directory to send to the build server\n"
    "                               (if both this and RECC_DEPS_OVERRIDE\n"
    "                               are set, this one is used)\n"
    "\n"
    "RECC_OUTPUT_FILES_OVERRIDE - comma-separated list of files to\n"
    "                             request from the build server (by\n"
    "                             default, `deps` guesses)\n"
    "\n"
    "RECC_OUTPUT_DIRECTORIES_OVERRIDE - comma-separated list of\n"
    "                                   directories to request (by\n"
    "                                   default, `deps` guesses)\n"
    "\n"
    "RECC_DEPS_EXCLUDE_PATHS - comma-separated list of paths to exclude from\n"
    "                          the input root\n"
    "\n"
    "RECC_DEPS_EXTRA_SYMLINKS - comma-separated list of paths to symlinks to\n"
    "                           add to the input root\n"
    "\n"
    "RECC_DEPS_ENV_[var] - sets [var] for local dependency detection\n"
    "                      commands\n"
    "\n"
    "RECC_COMPILATION_DATABASE - filename of compilation database to use\n"
    "                            with `clang-scan-deps` to determine\n"
    "                            dependencies\n"
    "\n"
    "RECC_PRESERVE_ENV - if set to any value, preserve all non-recc \n"
    "                    environment variables in the remote"
    "\n"
    "RECC_ENV_TO_READ - comma-separated list of specific environment \n"
    "                       variables to preserve from the local environment\n"
    "                       (can be used to preserve RECC_ variables, unlike\n"
    "                       RECC_PRESERVE_ENV)\n"
    "\n"
    "RECC_REMOTE_ENV_[var] - sets [var] in the remote build environment\n"
    "\n"
    "RECC_REMOTE_PLATFORM_[key] - specifies a platform property,\n"
    "                             which the build server uses to select\n"
    "                             the build worker\n"
    "\n"
    "RECC_RETRY_LIMIT - number of times to retry failed requests (default "
    "0).\n"
    "\n"
    "RECC_RETRY_DELAY - base delay (in ms) between retries\n"
    "                   grows exponentially (default 1000ms)\n"
    "\n"
    "RECC_REQUEST_TIMEOUT - how long to wait for gRPC request responses\n"
    "                       in seconds. (default: no timeout))\n"
    "\n"
    "RECC_MIN_THROUGHPUT - minimum throughput in bytes per second to extend\n"
    "                      the timeout. The value may be suffixed with\n"
    "                      K, M, G or T. (default: no dynamic timeout)\n"
    "\n"
    "RECC_KEEPALIVE_TIME - period for gRPC keepalive pings\n"
    "                      in seconds. (default: no keepalive pings))\n"
    "\n"
    "RECC_PREFIX_MAP - specify path mappings to replace. The source and "
    "destination must both be absolute paths. \n"
    "Supports multiple paths, separated by "
    "colon(:). Ex. RECC_PREFIX_MAP=/usr/bin=/usr/local/bin)\n"
    "\n"
    "RECC_CAS_DIGEST_FUNCTION - specify what hash function to use to "
    "calculate digests.\n"
    "                           (Default: "
    "\"" DEFAULT_RECC_CAS_DIGEST_FUNCTION "\")\n"
    "                           Supported values: " +
    DigestGenerator::supportedDigestFunctionsList() +
    "\n\n"
    "RECC_WORKING_DIR_PREFIX - directory to prefix the command's working\n"
    "                          directory, and input paths relative to it\n"
    "RECC_MAX_THREADS -   Allow some operations to utilize multiple cores."
    "Default: 4 \n"
    "                     A value of -1 specifies use all available cores.\n"
    "RECC_REAPI_VERSION - Version of the Remote Execution API to use. "
    "(Default: \"" DEFAULT_RECC_REAPI_VERSION "\")\n"
    "                     Supported values: " +
    proto::reapiSupportedVersionsList() +
    "\n"
    "RECC_NO_EXECUTE    - If set, only attempt to build an Action and "
    "calculate its digest,\n"
    "                     without running the command");

enum ReturnCode {
    RC_OK = 0,
    RC_USAGE = 100,
    RC_EXEC_FAILURE = 101,
    RC_GRPC_ERROR = 102
};
} // namespace

int main(int argc, char *argv[])
{
    Env::setup_logger_from_environment(argv[0]);
    Env::try_to_parse_recc_config();

    const bool verify = RECC_VERIFY;
    const bool enableMetadataPublishing =
        (!RECC_COMPILATION_METADATA_UDP_PORT.empty());

    if (argc <= 1) {
        std::cerr << "USAGE: recc <command>" << std::endl;
        std::cerr << "(run \"recc --help\" for details)" << std::endl;
        return RC_USAGE;
    }
    else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        std::cout << HELP << std::endl;
        return RC_OK;
    }
    else if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        const std::string version =
            RequestMetadataGenerator::RECC_METADATA_TOOL_VERSION;
        const std::string versionMessage = "recc version: " + version;
        std::cout << versionMessage << std::endl;
        return RC_OK;
    }
    else if (argv[1][0] == '-') {
        std::cerr << "recc: unrecognized option '" << argv[1] << "'"
                  << std::endl;
        std::cerr << "USAGE: recc <command>" << std::endl;
        std::cerr << "(run \"recc --help\" for details)" << std::endl;
        return RC_USAGE;
    }

    // Start gathering data while we're waiting for compiliation to finish
    build::buildbox::CompilerExecutionData compilationData;
    std::string unresolvedPathToCommand = argv[1];
    DataUtils::collectCompilationData(
        argc - 1, &argv[1], unresolvedPathToCommand, compilationData);

    // Perform the actual executions
    int exit_code;
    if (verify) {
        exit_code = verifyRemoteBuild(argc - 1, &argv[1], &compilationData);
    }
    else {
        ExecutionContext context;
        setupSignals();
        context.setStopToken(s_signalReceived);

        try {
            // Parsing of recc options is complete. The remaining arguments are
            // the compiler command line.
            context.disableConfigParsing();
            exit_code = context.execute(argc - 1, &argv[1]);
        }
        catch (const std::invalid_argument &e) {
            return RC_USAGE;
        }
        catch (const buildboxcommon::GrpcError &e) {
            if (e.status.error_code() == grpc::StatusCode::CANCELLED) {
                exit(130); // Ctrl+C exit code
            }
            return RC_GRPC_ERROR;
        }
        catch (const std::exception &e) {
            return RC_EXEC_FAILURE;
        }

        // Collect publishable metrics from the ExecutionContext
        if (enableMetadataPublishing) {
            auto reccData = compilationData.mutable_recc_data();

            const auto durationMetrics = context.getDurationMetrics();
            for (auto const &iter : *durationMetrics) {
                (*reccData->mutable_duration_metrics())[iter.first] =
                    google::protobuf::util::TimeUtil::MicrosecondsToDuration(
                        iter.second.value().count());
            }

            const auto counterMetrics = context.getCounterMetrics();
            for (auto const &iter : *counterMetrics) {
                (*reccData->mutable_counter_metrics())[iter.first] =
                    iter.second;
            }

            reccData->mutable_action_digest()->CopyFrom(
                context.getActionDigest());
        }
    }

    // Send the compilation metadata
    if (enableMetadataPublishing) {
        // Record elapsed time since timestamp value is record
        *compilationData.mutable_duration() =
            buildboxcommon::TimeUtils::now() - compilationData.timestamp();

        *compilationData.mutable_local_resource_usage() =
            buildboxcommon::ExecutionStatsUtils::getChildrenProcessRusage();

        DataUtils::sendData(compilationData);
    }

    return exit_code;
}
