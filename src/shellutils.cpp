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

#include <shellutils.h>

namespace recc {

std::vector<std::string> ShellUtils::splitCommand(const std::string &command)
{
    std::vector<std::string> arguments;
    size_t pos = 0;
    while ((pos = command.find_first_not_of(" ", pos)) != std::string::npos) {
        std::string argument;
        do {
            char c = command[pos];
            if (c == '"') {
                pos++;
                while (pos < command.size()) {
                    c = command[pos];
                    if (c == '"') {
                        // Closing quote
                        pos++;
                        break;
                    }
                    else if (c == '\\') {
                        pos++;
                        if (pos < command.size()) {
                            argument.push_back(command[pos++]);
                        }
                    }
                    else {
                        size_t next = command.find_first_of("\\\"", pos);
                        if (next == std::string::npos) {
                            // Missing closing quote
                            next = command.size();
                        }
                        argument.append(command.substr(pos, next - pos));
                        pos = next;
                    }
                }
            }
            else if (c == '\'') {
                pos++;
                size_t endQuote = command.find('\'', pos);
                if (endQuote == std::string::npos) {
                    // Missing closing quote
                    endQuote = command.size();
                }
                argument.append(command.substr(pos, endQuote - pos));
                pos = endQuote + 1;
            }
            else if (c == '\\') {
                // A backslash preserves the next character
                pos++;
                if (pos < command.size()) {
                    argument.push_back(command[pos++]);
                }
            }
            else {
                size_t next = command.find_first_of(" \\\"\'", pos);
                if (next == std::string::npos) {
                    next = command.size();
                }
                argument.append(command.substr(pos, next - pos));
                pos = next;
            }
        } while (pos < command.size() && command[pos] != ' ');
        arguments.push_back(argument);
    }
    return arguments;
}

} // namespace recc
