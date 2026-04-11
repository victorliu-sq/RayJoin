#ifndef RAYJOIN_TEST_VK_FIXTURE_H
#define RAYJOIN_TEST_VK_FIXTURE_H

#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "util/guard_glog.h"
#include "vk/core/vk_global_context.h"
#include "vk/engine/vk_compute_context.h"
#include "vk/map/context_ns.h"

namespace rayjoin::vk {

class TestVkFixture : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    glog_guard_ = CreateGlogGuardAlsoToStderr("vk-algo-test");
    vk_runtime_ = CreateVkGlobalRuntime();
  }

  static void TearDownTestSuite() {
    vk_runtime_.reset();
    glog_guard_.reset();
  }

  void SetUp() override {
    auto info = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string test_suite = info->test_suite_name();
    std::string test_name = info->name();
    std::string prefix = glog_guard_->LogDir() + "/log-" + test_suite + "-" + test_name;

    google::SetLogDestination(google::INFO, (prefix + ".INFO.").c_str());
    google::SetLogDestination(google::WARNING, (prefix + ".WARNING.").c_str());
    google::SetLogDestination(google::ERROR, (prefix + ".ERROR.").c_str());
    google::SetLogDestination(google::FATAL, (prefix + ".FATAL.").c_str());

    LOG(INFO) << "SetUp: configured log destinations for test " << test_suite << "." << test_name << " prefix=" << prefix;
  }

  void TearDown() override {
    auto info = ::testing::UnitTest::GetInstance()->current_test_info();
    LOG(INFO) << "TearDown: finished test " << info->test_suite_name() << "." << info->name();
  }

  inline static std::unique_ptr<GlogGuard> glog_guard_;
  inline static std::unique_ptr<VkGlobalRuntime> vk_runtime_;

  // Specfic to Xsect
  using context_t = ContextNS<double>;
  using coord_t = typename context_t::coord_t;
  using xsect_t = typename context_t::xsect_t;
  using edge_t = typename context_t::edge_t;
  using point_t = typename context_t::point_t;

  static xsect_t MakeXsect(coord_t x, coord_t y, index_t eid0, index_t eid1, polygon_id_t mid = DONTKNOW) {
    xsect_t v{};
    v.x = x;
    v.y = y;
    v.eid0 = eid0;
    v.eid1 = eid1;
    v.mid_point_polygon_id = mid;
    return v;
  }

  static bool SameXsect(const xsect_t& a, const xsect_t& b) {
    return a.x == b.x && a.y == b.y && a.eid0 == b.eid0 && a.eid1 == b.eid1 && a.mid_point_polygon_id == b.mid_point_polygon_id;
  }
};

}  // namespace rayjoin::vk

#endif  // RAYJOIN_TEST_VK_FIXTURE_H
