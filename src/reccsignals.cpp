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

#include <atomic>
#include <iostream>
#include <reccsignals.h>
#include <string>

namespace recc {

std::atomic_bool s_signalReceived(false);
volatile sig_atomic_t s_signalValue(0);

/**
 * Signal handler to mark the remote execution task for cancellation
 */
void setSignalReceived(int sig)
{
    s_signalReceived = true;
    s_signalValue = sig;
}

void setupSignals()
{
    struct sigaction sa;
    sa.sa_handler = setSignalReceived;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;

    if (sigaction(SIGINT, &sa, nullptr) == -1) {
        std::cerr << "Unable to register signal handler for SIGINT"
                  << std::endl;
    }
    if (sigaction(SIGTERM, &sa, nullptr) == -1) {
        std::cerr << "Unable to register signal handler for SIGTERM"
                  << std::endl;
    }
    if (sigaction(SIGHUP, &sa, nullptr) == -1) {
        std::cerr << "Unable to register signal handler for SIGHUP"
                  << std::endl;
    }
    if (sigaction(SIGPIPE, &sa, nullptr) == -1) {
        std::cerr << "Unable to register signal handler for SIGPIPE"
                  << std::endl;
    }
}

} // namespace recc
