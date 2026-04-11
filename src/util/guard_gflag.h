#ifndef RAYJOIN_GUARD_GFLAG_H
#define RAYJOIN_GUARD_GFLAG_H

#include <gflags/gflags.h>
#include <memory>

class GflagsGuard {
 public:
  explicit GflagsGuard(int& argc, char**& argv, bool remove_flags = true) noexcept : argc_(argc), argv_(argv), remove_flags_(remove_flags) {
    gflags::ParseCommandLineFlags(&argc_, &argv_, remove_flags_);
  }

  explicit GflagsGuard(int& argc, char**& argv, const char* usage_message, bool remove_flags = true, bool show_usage_if_no_args = true) noexcept :
      argc_(argc), argv_(argv), remove_flags_(remove_flags) {
    gflags::SetUsageMessage(usage_message ? usage_message : "");

    if (show_usage_if_no_args && argc_ == 1) {
      gflags::ShowUsageWithFlags(argv_ ? argv_[0] : "");
      std::exit(1);
    }

    gflags::ParseCommandLineFlags(&argc_, &argv_, remove_flags_);
  }

  ~GflagsGuard() noexcept { gflags::ShutDownCommandLineFlags(); }

  GflagsGuard(const GflagsGuard&) = delete;
  GflagsGuard& operator=(const GflagsGuard&) = delete;
  GflagsGuard(GflagsGuard&&) noexcept = default;
  GflagsGuard& operator=(GflagsGuard&&) noexcept = delete;

 private:
  int& argc_;
  char**& argv_;
  bool remove_flags_;
};

// static inline GflagsGuard CreateGflagsGuard(int& argc, char**& argv, bool remove_flags = true) { return GflagsGuard(argc, argv, remove_flags); }
//
// static inline GflagsGuard CreateGflagsGuard(
//     int& argc, char**& argv, const char* usage_message, bool remove_flags = true, bool show_usage_if_no_args = true) {
//   return GflagsGuard(argc, argv, usage_message, remove_flags, show_usage_if_no_args);
// }

static inline std::unique_ptr<GflagsGuard> CreateGflagsGuard(int& argc, char**& argv, bool remove_flags = true) {
  return std::make_unique<GflagsGuard>(argc, argv, remove_flags);
}

static inline std::unique_ptr<GflagsGuard> CreateGflagsGuard(
    int& argc, char**& argv, const char* usage_message, bool remove_flags = true, bool show_usage_if_no_args = true) {
  return std::make_unique<GflagsGuard>(argc, argv, usage_message, remove_flags, show_usage_if_no_args);
}

#endif  // RAYJOIN_GUARD_GFLAG_H
