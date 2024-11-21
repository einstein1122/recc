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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <shellutils.h>

using namespace recc;
using namespace testing;

TEST(ShellUtilsTest, SplitCommand)
{
    // Simple case
    EXPECT_THAT(ShellUtils::splitCommand("echo hello, world"),
                ElementsAre("echo", "hello,", "world"));

    // Quoted arguments
    EXPECT_THAT(ShellUtils::splitCommand("echo 'hello, world'"),
                ElementsAre("echo", "hello, world"));
    EXPECT_THAT(ShellUtils::splitCommand("echo \"hello, world\""),
                ElementsAre("echo", "hello, world"));

    // Mix of quoting styles in a single argument
    EXPECT_THAT(ShellUtils::splitCommand("echo 'hello, '\"world\""),
                ElementsAre("echo", "hello, world"));

    // Escaped characters in double quoted argument
    EXPECT_THAT(ShellUtils::splitCommand("echo \"foo=\\\"bar\\\"\""),
                ElementsAre("echo", "foo=\"bar\""));
    EXPECT_THAT(ShellUtils::splitCommand("echo \"foo \\\\ bar\""),
                ElementsAre("echo", "foo \\ bar"));

    // Backslash in single quoted argument (no escaping)
    EXPECT_THAT(ShellUtils::splitCommand("echo 'foo \\\\ bar'"),
                ElementsAre("echo", "foo \\\\ bar"));

    // Escaped characters outside quotes
    EXPECT_THAT(ShellUtils::splitCommand("echo foo \\\\ bar"),
                ElementsAre("echo", "foo", "\\", "bar"));
    EXPECT_THAT(ShellUtils::splitCommand("echo \\\"hello, world\\\""),
                ElementsAre("echo", "\"hello,", "world\""));
}
