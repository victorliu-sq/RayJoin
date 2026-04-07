#ifndef RAYJOIN_GUARD_GLOG_H
#define RAYJOIN_GUARD_GLOG_H

#include <filesystem>

#include "glog/logging.h"

class GlogGuard {
 public:
  explicit GlogGuard(const char* name, const char* log_dir = "tmp/logs") : log_dir_(log_dir ? log_dir : "tmp/logs") {
    std::filesystem::create_directories(log_dir_);
    FLAGS_log_dir = log_dir_;
    google::InitGoogleLogging(name);
    google::InstallFailureSignalHandler();
  }

  ~GlogGuard() { google::ShutdownGoogleLogging(); }

  GlogGuard(const GlogGuard&) = delete;
  GlogGuard& operator=(const GlogGuard&) = delete;
  GlogGuard(GlogGuard&&) noexcept = default;
  GlogGuard& operator=(GlogGuard&&) noexcept = delete;

  const std::string& LogDir() const noexcept { return log_dir_; }

 private:
  std::string log_dir_;
};

static inline GlogGuard CreateGlogGuard(const char* test_name, const char* log_dir = "tmp/logs") { return GlogGuard(test_name, log_dir); }

static inline GlogGuard CreateGlogGuardAlsoToStderr(const char* test_name, const char* log_dir = "tmp/logs") {
  FLAGS_alsologtostderr = true;
  return GlogGuard(test_name, log_dir);
}

#endif  // RAYJOIN_GUARD_GLOG_H
