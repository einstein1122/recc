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

#include <buildboxcommonmetrics_testingutils.h>
#include <buildboxcommonmetrics_totaldurationmetricvalue.h>
#include <env.h>
#include <executioncontext.h>

#include <string>

#include <gtest/gtest.h>

using namespace recc;
using namespace buildboxcommon::buildboxcommonmetrics;

class ProtectedContext : public ExecutionContext {
  public:
    const std::string wrapGenerateTag() { return generateMetricTag(); }
};

TEST(MetricTest, VerifyNoAddedTag)
{
    std::string result;
    ProtectedContext context;
    result = context.wrapGenerateTag();
    assert(result == "");

    const char *testEnvironNoTags[] = {"RECC_SERVER=http://server:1234",
                                       "RECC_STATSD_FORMAT=influx", nullptr};
    Env::parse_config_variables(testEnvironNoTags);
    result = context.wrapGenerateTag();
    assert(result == "");

    const char *testEnvironNoFormat[] = {
        "RECC_SERVER=http://server:1234", "RECC_STATSD_FORMAT=none",
        "RECC_METRICS_TAG_dummy=test", nullptr};
    Env::parse_config_variables(testEnvironNoFormat);
    assert(result == "");
}

TEST(MetricTest, VerifyStatsdFormats)
{
    const char *testEnvironInflux[] = {"RECC_SERVER=http://server:1234",
                                       "RECC_STATSD_FORMAT=influx",
                                       "RECC_METRICS_TAG_dummy=test", nullptr};
    Env::parse_config_variables(testEnvironInflux);
    std::string result;
    ProtectedContext context;
    result = context.wrapGenerateTag();
    assert(result == ",dummy=test");

    const char *testEnvironGraphite[] = {
        "RECC_SERVER=http://server:1234", "RECC_STATSD_FORMAT=graphite",
        "RECC_METRICS_TAG_dummy=test", nullptr};
    Env::parse_config_variables(testEnvironGraphite);

    result = context.wrapGenerateTag();
    assert(result == ";dummy=test");

    const char *testEnvironDog[] = {"RECC_SERVER=http://server:1234",
                                    "RECC_STATSD_FORMAT=dog",
                                    "RECC_METRICS_TAG_dummy=test", nullptr};
    Env::parse_config_variables(testEnvironDog);

    result = context.wrapGenerateTag();
    assert(result == "|#dummy=test");
}

TEST(MetricTest, VerifyMultipleTags)
{
    const char *testEnvironMultipleTags[] = {
        "RECC_SERVER=http://server:1234", "RECC_STATSD_FORMAT=influx",
        "RECC_METRICS_TAG_dummy=test",    "RECC_METRICS_TAG_saltTag=test_salt",
        "RECC_METRICS_TAG_foo=bar",       nullptr};
    Env::parse_config_variables(testEnvironMultipleTags);
    std::string result;
    ProtectedContext context;
    result = context.wrapGenerateTag();
    assert(result == ",dummy=test,foo=bar,saltTag=test_salt");
}
