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

#include <env.h>

#include <gtest/gtest.h>

using namespace recc;

TEST(EnvTest, DifferentCASServerTest)
{
    const char *testEnviron[] = {"RECC_SERVER=somehost:1234",
                                 "RECC_CAS_SERVER=someotherhost:5678",
                                 nullptr};

    std::string expectedReccServer = "somehost:1234";
    std::string expectedCasServer = "someotherhost:5678";

    Env::parse_config_variables(testEnviron);

    EXPECT_EQ(expectedReccServer, RECC_SERVER);
    EXPECT_EQ(expectedCasServer, RECC_CAS_SERVER);
}

TEST(EnvTest, DifferentCasInstanceTest)
{
    // Reset the values before the test
    RECC_CAS_INSTANCE = std::nullopt;
    RECC_ACTION_CACHE_INSTANCE = std::nullopt;
    const char *testEnviron[] = {
        "RECC_INSTANCE=test_instance", "RECC_CAS_INSTANCE=test_cas_instance",
        "RECC_ACTION_CACHE_INSTANCE=test_action_cache_instance", nullptr};
    std::string expectedReccCasInstance = "test_cas_instance";

    Env::parse_config_variables(testEnviron);
    // need this for testing, since we are calling parse_config_variables
    // directly.
    Env::handle_special_defaults();
    EXPECT_EQ(expectedReccCasInstance, RECC_CAS_INSTANCE);
}

TEST(EnvTest, EmptyCasInstanceTest)
{
    // Reset the values before the test
    RECC_CAS_INSTANCE = std::nullopt;
    RECC_ACTION_CACHE_INSTANCE = std::nullopt;
    const char *testEnviron[] = {
        "RECC_INSTANCE=test_instance", "RECC_CAS_INSTANCE=",
        "RECC_ACTION_CACHE_INSTANCE=test_action_cache_instance", nullptr};
    std::string expectedReccCasInstance = "";

    Env::parse_config_variables(testEnviron);
    // need this for testing, since we are calling parse_config_variables
    // directly.
    Env::handle_special_defaults();
    EXPECT_EQ(expectedReccCasInstance, RECC_CAS_INSTANCE);
}

TEST(EnvTest, EnableCasGetCapabilitiesTest)
{
    const char *testEnviron[] = {"RECC_SERVER=somehost:1234",
                                 "RECC_CAS_SERVER=someotherhost:5678",
                                 "RECC_CAS_GET_CAPABILITIES=true", nullptr};

    std::string expectedReccServer = "somehost:1234";
    std::string expectedCasServer = "someotherhost:5678";

    Env::parse_config_variables(testEnviron);

    EXPECT_EQ(expectedReccServer, RECC_SERVER);
    EXPECT_EQ(expectedCasServer, RECC_CAS_SERVER);
    EXPECT_TRUE(RECC_CAS_GET_CAPABILITIES);
}
