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

TEST(EnvTest, CasDefaultsToServerTest)
{
    const char *testEnviron[] = {"RECC_SERVER=http://somehost:1234", nullptr};
    std::string expectedReccServer = "http://somehost:1234";

    Env::parse_config_variables(testEnviron);
    // need this for testing, since we are calling parse_config_variables
    // directly.
    Env::handle_special_defaults();

    EXPECT_EQ(expectedReccServer, RECC_SERVER);
    EXPECT_EQ(expectedReccServer, RECC_CAS_SERVER);
}

TEST(EnvTest, CasDoNotGetCapabilitiesByDefault)
{
    const char *testEnviron[] = {"RECC_SERVER=http://somehost:1234", nullptr};

    Env::parse_config_variables(testEnviron);
    // need this for testing, since we are calling parse_config_variables
    // directly.
    Env::handle_special_defaults();

    EXPECT_FALSE(RECC_CAS_GET_CAPABILITIES);
}

TEST(EnvTest, CasInstanceDefaultsToServerInstanceTest)
{
    // Reset the values before the test
    RECC_CAS_INSTANCE = std::nullopt;
    RECC_ACTION_CACHE_INSTANCE = std::nullopt;
    const char *testEnviron[] = {"RECC_INSTANCE=test_instance", nullptr};
    std::string expectedReccInstance = "test_instance";

    Env::parse_config_variables(testEnviron);
    // need this for testing, since we are calling parse_config_variables
    // directly.
    Env::handle_special_defaults();

    EXPECT_EQ(expectedReccInstance, RECC_INSTANCE);
    EXPECT_EQ(expectedReccInstance, RECC_CAS_INSTANCE);
}

TEST(EnvTest, CasInstanceDefaultsToActionCacheInstanceTest)
{
    // Reset the values before the test
    RECC_CAS_INSTANCE = std::nullopt;
    RECC_ACTION_CACHE_INSTANCE = std::nullopt;
    const char *testEnviron[] = {"RECC_INSTANCE=test_instance",
                                 "RECC_ACTION_CACHE_INSTANCE=test_ac_instance",
                                 nullptr};
    std::string expectedReccInstance = "test_instance";
    std::string expectedReccCasInstance = "test_ac_instance";

    Env::parse_config_variables(testEnviron);
    // need this for testing, since we are calling parse_config_variables
    // directly.
    Env::handle_special_defaults();

    EXPECT_EQ(expectedReccInstance, RECC_INSTANCE);
    EXPECT_EQ(expectedReccCasInstance, RECC_ACTION_CACHE_INSTANCE);
    EXPECT_EQ(expectedReccCasInstance, RECC_CAS_INSTANCE);
}
