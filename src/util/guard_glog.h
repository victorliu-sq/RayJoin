#ifndef RAYJOIN_GUARD_GLOG_H
#define RAYJOIN_GUARD_GLOG_H

#include <filesystem>

#include "glog/logging.h"

class GlogGuard {
 public:
  explicit GlogGuard(const char* name, const char* log_dir = "tmp/logs")
      : log_dir_(log_dir) {
    std::filesystem::create_directories(log_dir_);  // ok if already exists
    FLAGS_log_dir = log_dir;
    google::InitGoogleLogging(name);
    google::InstallFailureSignalHandler();
  }

  ~GlogGuard() { google::ShutdownGoogleLogging(); }

  // Disable copy and move to ensure one active guard per process
  GlogGuard(const GlogGuard&) = delete;
  GlogGuard& operator=(const GlogGuard&) = delete;
  GlogGuard(GlogGuard&&) = delete;
  GlogGuard& operator=(GlogGuard&&) = delete;

  const std::string& LogDir() const noexcept { return log_dir_; }

 private:
  std::string log_dir_;
};

static inline auto CreateGlogGuard(const char* test_name) {
  return std::make_unique<GlogGuard>(test_name);
}

using GlogGuardUptr = std::unique_ptr<GlogGuard>;

#endif  // RAYJOIN_GUARD_GLOG_H
