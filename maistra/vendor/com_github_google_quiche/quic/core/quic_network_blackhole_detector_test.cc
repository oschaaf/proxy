// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quic/core/quic_network_blackhole_detector.h"

#include "quic/core/quic_one_block_arena.h"
#include "quic/platform/api/quic_test.h"
#include "quic/test_tools/quic_test_utils.h"

namespace quic {
namespace test {

class QuicNetworkBlackholeDetectorPeer {
 public:
  static QuicAlarm* GetAlarm(QuicNetworkBlackholeDetector* detector) {
    return detector->alarm_.get();
  }
};

namespace {
class MockDelegate : public QuicNetworkBlackholeDetector::Delegate {
 public:
  MOCK_METHOD(void, OnPathDegradingDetected, (), (override));
  MOCK_METHOD(void, OnBlackholeDetected, (), (override));
  MOCK_METHOD(void, OnPathMtuReductionDetected, (), (override));
};

const size_t kPathDegradingDelayInSeconds = 5;
const size_t kPathMtuReductionDelayInSeconds = 7;
const size_t kBlackholeDelayInSeconds = 10;

class QuicNetworkBlackholeDetectorTest : public QuicTest {
 public:
  QuicNetworkBlackholeDetectorTest()
      : detector_(&delegate_, &arena_, &alarm_factory_, /*context=*/nullptr),
        alarm_(static_cast<MockAlarmFactory::TestAlarm*>(
            QuicNetworkBlackholeDetectorPeer::GetAlarm(&detector_))),
        path_degrading_delay_(
            QuicTime::Delta::FromSeconds(kPathDegradingDelayInSeconds)),
        path_mtu_reduction_delay_(
            QuicTime::Delta::FromSeconds(kPathMtuReductionDelayInSeconds)),
        blackhole_delay_(
            QuicTime::Delta::FromSeconds(kBlackholeDelayInSeconds)) {
    clock_.AdvanceTime(QuicTime::Delta::FromSeconds(1));
  }

 protected:
  void RestartDetection() {
    detector_.RestartDetection(clock_.Now() + path_degrading_delay_,
                               clock_.Now() + blackhole_delay_,
                               clock_.Now() + path_mtu_reduction_delay_);
  }

  testing::StrictMock<MockDelegate> delegate_;
  QuicConnectionArena arena_;
  MockAlarmFactory alarm_factory_;

  QuicNetworkBlackholeDetector detector_;

  MockAlarmFactory::TestAlarm* alarm_;
  MockClock clock_;
  const QuicTime::Delta path_degrading_delay_;
  const QuicTime::Delta path_mtu_reduction_delay_;
  const QuicTime::Delta blackhole_delay_;
};

TEST_F(QuicNetworkBlackholeDetectorTest, StartAndFire) {
  EXPECT_FALSE(detector_.IsDetectionInProgress());

  RestartDetection();
  EXPECT_TRUE(detector_.IsDetectionInProgress());
  EXPECT_EQ(clock_.Now() + path_degrading_delay_, alarm_->deadline());

  // Fire path degrading alarm.
  clock_.AdvanceTime(path_degrading_delay_);
  EXPECT_CALL(delegate_, OnPathDegradingDetected());
  alarm_->Fire();

  // Verify path mtu reduction detection is still in progress.
  EXPECT_TRUE(detector_.IsDetectionInProgress());
  EXPECT_EQ(clock_.Now() + path_mtu_reduction_delay_ - path_degrading_delay_,
            alarm_->deadline());

  // Fire path mtu reduction detection alarm.
  clock_.AdvanceTime(path_mtu_reduction_delay_ - path_degrading_delay_);
  EXPECT_CALL(delegate_, OnPathMtuReductionDetected());
  alarm_->Fire();

  // Verify blackhole detection is still in progress.
  EXPECT_TRUE(detector_.IsDetectionInProgress());
  EXPECT_EQ(clock_.Now() + blackhole_delay_ - path_mtu_reduction_delay_,
            alarm_->deadline());

  // Fire blackhole detection alarm.
  clock_.AdvanceTime(blackhole_delay_ - path_mtu_reduction_delay_);
  EXPECT_CALL(delegate_, OnBlackholeDetected());
  alarm_->Fire();
  EXPECT_FALSE(detector_.IsDetectionInProgress());
}

TEST_F(QuicNetworkBlackholeDetectorTest, RestartAndStop) {
  RestartDetection();

  clock_.AdvanceTime(QuicTime::Delta::FromSeconds(1));
  RestartDetection();
  EXPECT_EQ(clock_.Now() + path_degrading_delay_, alarm_->deadline());

  detector_.StopDetection(/*permanent=*/false);
  EXPECT_FALSE(detector_.IsDetectionInProgress());
}

TEST_F(QuicNetworkBlackholeDetectorTest, PathDegradingFiresAndRestart) {
  EXPECT_FALSE(detector_.IsDetectionInProgress());
  RestartDetection();
  EXPECT_TRUE(detector_.IsDetectionInProgress());
  EXPECT_EQ(clock_.Now() + path_degrading_delay_, alarm_->deadline());

  // Fire path degrading alarm.
  clock_.AdvanceTime(path_degrading_delay_);
  EXPECT_CALL(delegate_, OnPathDegradingDetected());
  alarm_->Fire();

  // Verify path mtu reduction detection is still in progress.
  EXPECT_TRUE(detector_.IsDetectionInProgress());
  EXPECT_EQ(clock_.Now() + path_mtu_reduction_delay_ - path_degrading_delay_,
            alarm_->deadline());

  // After 100ms, restart detections on forward progress.
  clock_.AdvanceTime(QuicTime::Delta::FromMilliseconds(100));
  RestartDetection();
  // Verify alarm is armed based on path degrading deadline.
  EXPECT_EQ(clock_.Now() + path_degrading_delay_, alarm_->deadline());
}

}  // namespace

}  // namespace test
}  // namespace quic
