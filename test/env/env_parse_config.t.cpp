// Copyright 2019 Bloomberg Finance L.P
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

#include <env.h>
#include <executioncontext.h>

#include <gtest/gtest.h>

using namespace recc;

namespace {
void clearEnv()
{

    // Set globals to default
    RECC_COMPILATION_METADATA_UDP_PORT = "";
    RECC_VERIFY = false;

    // Clear environment variables
    unsetenv("RECC_VERIFY");
    unsetenv("RECC_CONFIG_DIRECTORY");
}
} // namespace

TEST(EnvTest, FromConfigDirectory)
{
    clearEnv();
    setenv("RECC_CONFIG_DIRECTORY", "data/datautils/config", 1);
    Env::try_to_parse_recc_config();
    EXPECT_EQ(RECC_VERIFY, false);
}

TEST(EnvTest, ParseConfigOption)
{
    clearEnv();
    ExecutionContext context;
    EXPECT_EQ(context.getParseConfigOption(), ExecutionContext::PARSE_CONFIG);

    context.disableConfigParsing();
    EXPECT_EQ(context.getParseConfigOption(), ExecutionContext::SKIP_PARSING);
}
