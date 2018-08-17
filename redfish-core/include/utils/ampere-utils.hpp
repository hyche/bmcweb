/*
// Copyright (c) 2018 Ampere Computing LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/
#pragma once

#include <string>
#include <chrono>
#include <array>

namespace redfish {

// TODO: Consider finding a better way to retrieve domain name. Not to
//       hardcode to a specific value
static constexpr auto DOMAIN_NAME = ".amperecomputing.com";

using SystemClock = std::chrono::system_clock;
template<typename Duration>
using SystemTimePoint = std::chrono::time_point<SystemClock, Duration>;
using Milliseconds = std::chrono::milliseconds;


/**
 * @brief Converts the duration time since epoch to formatted date.
 * @param[in] duration - time since the Epoch
 * @param[in] format   - date format for strftime
 * @return converted date with given format.
 */
template<typename Duration>
std::string getDateTime(Duration duration, const char* format) {
  std::array<char, 128> dateTime;
  auto time = SystemClock::to_time_t(SystemTimePoint<Duration>{duration});
  if (std::strftime(dateTime.begin(), dateTime.size(), format,
                    std::localtime(&time))) {
      return dateTime.data();
  }
  return "";
}

/**
 * @brief get the current formatted datetime.
 * @param[in] format   - date format for strftime
 * @return converted current date with given format.
 */
std::string getCurrentDateTime(const char* format) {
  return getDateTime(SystemClock::now().time_since_epoch(), format);
}

}   // namespace redfish;
