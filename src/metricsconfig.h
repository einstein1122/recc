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

#ifndef INCLUDED_METRICSCONFIG_H
#define INCLUDED_METRICSCONFIG_H

#include <buildboxcommonmetrics_durationmetricvalue.h>
#include <buildboxcommonmetrics_statsdpublishercreator.h>

#include <functional>

namespace recc {

typedef buildboxcommon::buildboxcommonmetrics::StatsDPublisherType
    StatsDPublisherType;

typedef std::function<void(
    const std::string &,
    buildboxcommon::buildboxcommonmetrics::DurationMetricValue)>
    DurationMetricCallback;

typedef std::function<void(const std::string &, int64_t)>
    CounterMetricCallback;

std::shared_ptr<StatsDPublisherType>
get_statsdpublisher_from_config(const std::string &metric_tag = "");

} // namespace recc
#endif
