#ifndef RAYJOIN_DUMP_H
#define RAYJOIN_DUMP_H

#include <sstream>
#include <string>
#include <unordered_set>

namespace rayjoin {
inline std::unordered_set<std::string> ParseDumpStages(const std::string& s) {
  std::unordered_set<std::string> out;
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, ',')) {
    if (!item.empty()) out.insert(item);
  }
  return out;
}

inline bool ShouldDumpStage(const std::string& dump_results, const std::string& stage) {
  if (dump_results.empty()) return false;
  auto stages = ParseDumpStages(dump_results);
  return stages.count("all") || stages.count(stage);
}

inline std::string DumpSubdir(const std::string& base, const std::string& subdir) {
  if (base.empty()) return subdir;
  if (base.back() == '/') return base + subdir;
  return base + "/" + subdir;
}

// inline double TruncateForDump(double v, int decimals = 9) {
//   const double scale = std::pow(10.0, decimals);
//   return std::trunc(v * scale) / scale;
// }
inline double TruncateForDump(double v, int decimals = 7) {
  const double scale_hi = std::pow(10.0, decimals + 3);
  const double scale_lo = std::pow(10.0, decimals);

  // First round to a finer grid to suppress tiny binary noise,
  // then truncate to the final displayed precision.
  const double rounded_hi = std::round(v * scale_hi) / scale_hi;
  return std::trunc(rounded_hi * scale_lo) / scale_lo;
}

}  // namespace rayjoin

#endif  // RAYJOIN_DUMP_H
