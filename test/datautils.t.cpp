// Copyright 2021 Bloomberg Finance L.P
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

// component under test
#include <build/buildbox/local_execution.pb.h>
#include <datautils.h>
#include <env.h>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/message_differencer.h>

#include <netinet/in.h>
#include <sys/resource.h>
#include <sys/socket.h>

#include <buildboxcommon_platformutils.h>
#include <buildboxcommon_systemutils.h>

// third party includes
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace recc {
namespace test {

using namespace ::testing;

namespace {
void buildCompilationData(
    build::buildbox::CompilerExecutionData &compilationData)
{
    std::string cwd =
        buildboxcommon::SystemUtils::getCurrentWorkingDirectory();

    // Setting the last param to 0 prevents overwrite if exists
    setenv("FAVORITE_FOOD", "PIZZA", 0);
    setenv("EMPTY_VARIABLE", "", 0);

    const char *argv[] = {"recc", "gcc",    "-c", "data/hello/hello.cpp",
                          "-o",   "hello.o"};
    const size_t argc = (sizeof(argv) / sizeof(const char *));

    const int cmd_argc = argc - 1;
    const char **cmd_argv = &argv[1];

    DataUtils::collectCompilationData(cmd_argc, cmd_argv, "gcc",
                                      compilationData);
}

void clearEnv()
{

    // Set globals to default
    RECC_COMPILATION_METADATA_UDP_PORT = "";
    RECC_VERIFY = false;

    // Clear environment variables
    unsetenv("RECC_COMPILATION_METADATA_UDP_PORT");
    unsetenv("RECC_VERIFY");
    unsetenv("RECC_CONFIG_DIRECTORY");
}

} // namespace

TEST(CollectCompilationData, AllExtensionsAsExpected)
{
    clearEnv();
    std::string cwd =
        buildboxcommon::SystemUtils::getCurrentWorkingDirectory();
    std::string absoluteFilePath = cwd + "/data/datautils/hello/hello.cpp";
    std::vector<const char *> sourceFiles = {
        "data/datautils/hello/hello.cpp",
        "data/datautils/hello/hello.m.cpp",
        "data/datautils/hello/hello.cc",
        "data/datautils/hello/hello.c++",
        "data/datautils/hello/hello.cp",
        "data/datautils/hello/hello.cxx",
        "data/datautils/hello/hello.c",
        "data/datautils/hello/uppercase/hello.C",
        "data/datautils/hello/uppercase/hello.CPP",
        absoluteFilePath.c_str()};

    std::string envVar = "MYENVVAR";
    std::string envVarVal = "MYENVVARVAL";
    // Setting the last param to 0 prevents overwrite if exists
    setenv(envVar.c_str(), envVarVal.c_str(), 0);
    std::string expectedEnvVal = std::string(getenv(envVar.c_str()));

    for (const char *sourceFile : sourceFiles) {
        const char *argv[] = {"recc",     "gcc", "-c",
                              sourceFile, "-o",  "hello.o"};
        const size_t argc = sizeof(argv) / sizeof(const char *);

        const int cmd_argc = argc - 1;
        const char **cmd_argv = &argv[1];

        std::string expectedFullCommand =
            "gcc -c " + std::string(sourceFile) + " -o hello.o";

        build::buildbox::CompilerExecutionData compilationData;
        DataUtils::collectCompilationData(cmd_argc, cmd_argv, "gcc",
                                          compilationData);

        EXPECT_EQ(compilationData.command(), "gcc");
        EXPECT_EQ(compilationData.full_command(), expectedFullCommand);
        EXPECT_EQ(compilationData.working_directory(), cwd);
        EXPECT_EQ(compilationData.environment_variables().at(envVar),
                  expectedEnvVal);
        EXPECT_EQ(compilationData.has_recc_data(), false);
        EXPECT_EQ(compilationData.source_file_info()[0].name(), sourceFile);
        EXPECT_EQ(compilationData.source_file_info()[0].digest().hash(),
                  "ae6b1296207e23a3d6a15f398bef2446b0b7173b6809afc139b9bb59128"
                  "6b7bf");
        EXPECT_EQ(compilationData.source_file_info()[0].digest().size_bytes(),
                  76);
    }
}

TEST(CollectCompilationData, CompileAndLinkCommand)
{
    clearEnv();
    std::string envVar = "MYENVVAR";
    std::string envVarVal = "MYENVVARVAL";
    // Setting the last param to 0 prevents overwrite if exists
    setenv(envVar.c_str(), envVarVal.c_str(), 0);
    std::string expectedEnvVal = std::string(getenv(envVar.c_str()));

    const char *argv[] = {"recc",
                          "gcc",
                          "data/datautils/foomain.c++",
                          "data/datautils/foodep.CPP",
                          "-o",
                          "foo"};
    const size_t argc = sizeof(argv) / sizeof(const char *);
    const int cmd_argc = argc - 1;
    const char **cmd_argv = &argv[1];
    build::buildbox::CompilerExecutionData compilationData;
    DataUtils::collectCompilationData(cmd_argc, cmd_argv, "gcc",
                                      compilationData);
    EXPECT_EQ(compilationData.command(), "gcc");
    EXPECT_EQ(
        compilationData.full_command(),
        "gcc data/datautils/foomain.c++ data/datautils/foodep.CPP -o foo");
    EXPECT_EQ(compilationData.working_directory(),
              buildboxcommon::SystemUtils::getCurrentWorkingDirectory());
    EXPECT_EQ(compilationData.environment_variables().at(envVar),
              expectedEnvVal);
    EXPECT_EQ(compilationData.has_recc_data(), false);
    EXPECT_EQ(compilationData.source_file_info()[0].name(),
              "data/datautils/foomain.c++");
    EXPECT_EQ(
        compilationData.source_file_info()[0].digest().hash(),
        "4c85c53dbac2dfed1cedb666fb2c66f6860a46e09adcb30d87f8385db3bd29ec");
    EXPECT_EQ(compilationData.source_file_info()[0].digest().size_bytes(),
              114);
    EXPECT_EQ(compilationData.source_file_info()[1].name(),
              "data/datautils/foodep.CPP");
    EXPECT_EQ(
        compilationData.source_file_info()[1].digest().hash(),
        "a5a01fe1170b4a516ea8a5d7eb772a47132ae8e8267530d6bd9d2f0e22971992");
    EXPECT_EQ(compilationData.source_file_info()[1].digest().size_bytes(), 52);
}

TEST(SendData, AsExpected)
{
    clearEnv();
    setenv("RECC_COMPILATION_METADATA_UDP_PORT", "9080", 0);
    RECC_COMPILATION_METADATA_UDP_PORT = "9080";

    long portToSend = atoi(getenv("RECC_COMPILATION_METADATA_UDP_PORT"));

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddr.sin_port =
        htons(atoi(getenv("RECC_COMPILATION_METADATA_UDP_PORT")));

    // Use a blocking socket because Linux queue lengths can be small
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        // If opening the socket fails, just skip the rest of the test
        std::cerr << "Unable to use socket() for test: " << strerror(errno)
                  << std::endl;
        return;
    }

    if (bind(sockfd, (const struct sockaddr *)&serverAddr,
             sizeof(serverAddr)) == -1) {
        std::cerr << "Unable to use bind() for test: " << strerror(errno)
                  << std::endl;
        return;
    }

    // Create an CompilerExecutionData object and populate it with a fake
    // envvar
    build::buildbox::CompilerExecutionData sentData;
    (*sentData.mutable_environment_variables())["TESTENVVAR"] = "TESTENVVALUE";

    // This should be big enough to contain a mostly empty proto
    char recvbuffer[10000];

    DataUtils::sendData(sentData);
    int receivedLen =
        recvfrom(sockfd, recvbuffer, sizeof(recvbuffer), 0, 0, 0);
    if (receivedLen == -1) {
        // If the recvfrom fails, just skip the rest of the test
        std::cerr << "Unable to use recvfrom() for test" << strerror(errno)
                  << std::endl;
        return;
    }
    build::buildbox::CompilerExecutionData receivedData;
    receivedData.ParseFromArray(recvbuffer, receivedLen);
    EXPECT_EQ((*sentData.mutable_environment_variables())["TESTENVVAR"],
              (*receivedData.mutable_environment_variables())["TESTENVVAR"]);

    close(sockfd);
}

TEST(SendData, NullCharTest)
{
    clearEnv();
    setenv("RECC_COMPILATION_METADATA_UDP_PORT", "9080", 0);
    RECC_COMPILATION_METADATA_UDP_PORT = "9080";

    long portToSend = atoi(getenv("RECC_COMPILATION_METADATA_UDP_PORT"));

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddr.sin_port =
        htons(atoi(getenv("RECC_COMPILATION_METADATA_UDP_PORT")));

    // Use a blocking socket because Linux queue lengths can be small
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        // If opening the socket fails, just skip the rest of the test
        std::cerr << "Unable to use socket() for test: " << strerror(errno)
                  << std::endl;
        return;
    }

    if (bind(sockfd, (const struct sockaddr *)&serverAddr,
             sizeof(serverAddr)) == -1) {
        std::cerr << "Unable to use bind() for test: " << strerror(errno)
                  << std::endl;
        return;
    }

    // Create an CompilerExecutionData object and populate it
    build::buildbox::CompilerExecutionData sentCompilationData;
    buildCompilationData(sentCompilationData);

    // Allocate a socket read buffer
    const size_t bufferSize = 1024 * 1024 * 10;
    char *recvbuffer = new char[bufferSize];

    // Send the data
    DataUtils::sendData(sentCompilationData);
    const ssize_t receivedLen =
        recvfrom(sockfd, recvbuffer, bufferSize, 0, 0, 0);
    ASSERT_TRUE(receivedLen > 0);

    // Assert that we need to create the string buffer using `receivedLen`
    std::string truncatedStringBuffer(recvbuffer);
    std::string stringBuffer(recvbuffer, receivedLen);
    ASSERT_TRUE(truncatedStringBuffer.size() < stringBuffer.size());

    build::buildbox::CompilerExecutionData receivedData;
    receivedData.ParseFromString(stringBuffer);

    // Verify the data we recieved matches the data sent
    EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
        receivedData, sentCompilationData));

    // Clean up
    close(sockfd);
    delete[] recvbuffer;
}

TEST(ParseConfig, FromReccDefaults)
{
    clearEnv();
    Env::set_config_locations();
    Env::parse_config_variables();

    EXPECT_EQ(RECC_COMPILATION_METADATA_UDP_PORT, "");
    EXPECT_EQ(RECC_VERIFY, false);
}

TEST(ParseConfig, FromConfigDirectory)
{
    clearEnv();
    setenv("RECC_CONFIG_DIRECTORY", "data/datautils/config", 1);
    Env::set_config_locations();
    Env::parse_config_variables();

    EXPECT_EQ(RECC_COMPILATION_METADATA_UDP_PORT, "29111");
    EXPECT_EQ(RECC_VERIFY, false);
}

TEST(ParseConfig, FromEnvOverride)
{
    clearEnv();
    setenv("RECC_CONFIG_DIRECTORY", "data/datautils/config", 1);
    setenv("RECC_COMPILATION_METADATA_UDP_PORT", "39111", 1);
    setenv("RECC_VERIFY", "1", 1);
    Env::set_config_locations();
    Env::parse_config_variables();

    EXPECT_EQ(RECC_COMPILATION_METADATA_UDP_PORT, "39111");
    EXPECT_EQ(RECC_VERIFY, true);
}

TEST(ParseConfig, FromUnsetEnvOverride)
{
    clearEnv();
    setenv("RECC_CONFIG_DIRECTORY", "data/datautils/config", 1);
    setenv("RECC_COMPILATION_METADATA_UDP_PORT", "39111", 1);
    setenv("RECC_VERIFY", "1", 1);
    Env::set_config_locations();
    Env::parse_config_variables();

    EXPECT_EQ(RECC_COMPILATION_METADATA_UDP_PORT, "39111");
    EXPECT_EQ(RECC_VERIFY, true);
}

} // namespace test
} // namespace recc
