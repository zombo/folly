/*
 * Copyright 2016 Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <chrono>
#include <string>
#include <type_traits>

#include <folly/Conv.h>
#include <folly/Format.h>
#include <folly/String.h>
#include <glog/logging.h>

namespace folly {

// Default logger
enum class GoogleLoggerStyle { SECONDS, PRETTY };
template <GoogleLoggerStyle>
struct GoogleLogger;

/**
 * Automatically times a block of code, printing a specified log message on
 * destruction or whenever the log() method is called. For example:
 *
 *   AutoTimer t("Foo() completed");
 *   doWork();
 *   t.log("Do work finished");
 *   doMoreWork();
 *
 * This would print something like:
 *   "Do work finished in 1.2 seconds"
 *   "Foo() completed in 4.3 seconds"
 *
 * You can customize what you use as the logger and clock. The logger needs
 * to have an operator()(StringPiece, double) that gets a message and a duration
 * (in seconds). The clock needs to model Clock from std::chrono.
 *
 * The default logger logs usings glog. It only logs if the message is
 * non-empty, so you can also just use this class for timing, e.g.:
 *
 *   AutoTimer t;
 *   doWork()
 *   const auto how_long = t.log();
 */
template <
    class Logger = GoogleLogger<GoogleLoggerStyle::PRETTY>,
    class Clock = std::chrono::high_resolution_clock>
class AutoTimer final {
 public:
  explicit AutoTimer(
      std::string&& msg = "",
      double minTimetoLog = 0.0,
      Logger&& logger = Logger())
      : destructionMessage_(std::move(msg)),
        minTimeToLog_(minTimetoLog),
        logger_(std::move(logger)) {}

  // It doesn't really make sense to copy AutoTimer
  // Movable to make sure the helper method for creating an AutoTimer works.
  AutoTimer(const AutoTimer&) = delete;
  AutoTimer(AutoTimer&&) = default;
  AutoTimer& operator=(const AutoTimer&) = delete;
  AutoTimer& operator=(AutoTimer&&) = default;

  ~AutoTimer() {
    log(destructionMessage_);
  }

  double log(StringPiece msg = "") {
    return logImpl(Clock::now(), msg);
  }

  template <typename... Args>
  double log(Args&&... args) {
    auto now = Clock::now();
    return logImpl(now, to<std::string>(std::forward<Args>(args)...));
  }

  template <typename... Args>
  double logFormat(Args&&... args) {
    auto now = Clock::now();
    return logImpl(now, format(std::forward<Args>(args)...).str());
  }

 private:
  // We take in the current time so that we don't measure time to call
  // to<std::string> or format() in the duration.
  double logImpl(std::chrono::time_point<Clock> now, StringPiece msg) {
    double duration =
        std::chrono::duration_cast<std::chrono::duration<double>>(now - start_)
            .count();
    if (duration >= minTimeToLog_) {
      logger_(msg, duration);
    }
    start_ = Clock::now(); // Don't measure logging time
    return duration;
  }

  const std::string destructionMessage_;
  std::chrono::time_point<Clock> start_ = Clock::now();
  double minTimeToLog_;
  Logger logger_;
};

template <
    class Logger = GoogleLogger<GoogleLoggerStyle::PRETTY>,
    class Clock = std::chrono::high_resolution_clock>
auto makeAutoTimer(
    std::string&& msg = "",
    double minTimeToLog = 0.0,
    Logger&& logger = Logger()) {
  return AutoTimer<Logger, Clock>(
      std::move(msg), minTimeToLog, std::move(logger));
}

template <GoogleLoggerStyle Style>
struct GoogleLogger final {
  void operator()(StringPiece msg, double sec) const {
    if (msg.empty()) {
      return;
    }
    if (Style == GoogleLoggerStyle::PRETTY) {
      LOG(INFO) << msg << " in " << prettyPrint(sec, PrettyType::PRETTY_TIME);
    } else {
      LOG(INFO) << msg << " in " << sec << " seconds";
    }
  }
};
}
