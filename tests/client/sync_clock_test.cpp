#include "include/sync_clock.h"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "audio_sync.pb.h"

class SyncClockTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Initialize test objects
    sync_clock_ = std::make_unique<SyncClock>();
  }

  void TearDown() override {
    // Clean up resources
    sync_clock_.reset();
  }

  std::unique_ptr<SyncClock> sync_clock_;
};

// Test ProcessPingResponse function
TEST_F(SyncClockTest, ProcessPingResponse) {
  // Create a test ping response
  client::PingResponse response;

  // Simulate time values (in nanoseconds)
  // t0: client sends request
  // t1: server receives request
  // t2: server sends response
  // t3: client receives response

  TimePointNs t0 = 1000000000;  // 1 second
  TimePointNs t1 = 1000010000;  // 1 second + 10000 ns
  TimePointNs t2 = 1000020000;  // 1 second + 20000 ns
  TimePointNs t3 = 1000030000;  // 1 second + 30000 ns

  // Set t1 and t2 in the response
  response.set_t1(t1);
  response.set_t2(t2);

  // Process the response
  auto [offset, rtt] = sync_clock_->ProcessPingResponse(t0, t3, response);

  // Expected results:
  // Round-trip time: (t3-t0) - (t2-t1) = (1000030000-1000000000) -
  // (1000020000-1000010000) = 30000 - 10000 = 20000 ns Clock offset: ((t1-t0) +
  // (t2-t3))/2 = ((1000010000-1000000000) + (1000020000-1000030000))/2 = (10000
  // + -10000)/2 = 0 ns

  EXPECT_FLOAT_EQ(offset, 0.0f);
  EXPECT_FLOAT_EQ(rtt, 20000.0f);
}

// Test CalculateAverageOffset function
TEST_F(SyncClockTest, CalculateAverageOffset) {
  // Create test offset values
  std::vector<float> offsets = {100.0f, 200.0f, 300.0f, 400.0f, 500.0f};

  // Calculate average
  float avg = sync_clock_->CalculateAverageOffset(offsets);

  // Expected average: (100 + 200 + 300 + 400 + 500) / 5 = 300
  EXPECT_FLOAT_EQ(avg, 300.0f);

  // Verify the average was stored in the SyncClock
  EXPECT_FLOAT_EQ(sync_clock_->GetAverageOffset(), 300.0f);
}

// Test setting and getting offset and RTT
TEST_F(SyncClockTest, SetAndGetOffsetAndRtt) {
  // Set values
  sync_clock_->SetAverageOffset(123.456f);
  sync_clock_->SetMaxRtt(789.012f);

  // Verify values
  EXPECT_FLOAT_EQ(sync_clock_->GetAverageOffset(), 123.456f);
  EXPECT_FLOAT_EQ(sync_clock_->GetMaxRtt(), 789.012f);
}

// Test AdjustTargetTime function
TEST_F(SyncClockTest, AdjustTargetTime) {
  TimePointNs target_time = 2000000000;  // 2 seconds
  float clock_offset = 500000;           // 0.5 ms

  // Adjust the target time
  TimePointNs adjusted_time =
      SyncClock::AdjustTargetTime(target_time, clock_offset);

  // Expected adjusted time: 2000000000 - 500000 = 1999500000 ns
  EXPECT_EQ(adjusted_time, 1999500000);
}

// Test CalculateTargetExecutionTime function
TEST_F(SyncClockTest, CalculateTargetExecutionTime) {
  // Set a known max RTT
  float test_rtt = 10000.0f;  // 10 μs
  sync_clock_->SetMaxRtt(test_rtt);

  // Calculate target time with a safety margin of 5000 ns (5 μs)
  float safety_margin = 5000.0f;
  TimePointNs target_time =
      sync_clock_->CalculateTargetExecutionTime(safety_margin);

  // The target time should be in the future, approximately current time + RTT +
  // safety margin
  TimePointNs current_time = SyncClock::GetCurrentTimeNs();
  TimePointNs expected_min =
      current_time + static_cast<TimePointNs>(test_rtt + safety_margin -
                                              1000);  // Allow small variance

  EXPECT_GT(target_time, current_time);
  EXPECT_GE(target_time, expected_min);
}

// Test sleep functionality to make sure it sleeps approximately the right
// amount of time
TEST_F(SyncClockTest, SleepUntil) {
  // Current time
  TimePointNs current_time = SyncClock::GetCurrentTimeNs();

  // Target time 10ms in the future
  TimePointNs target_time = current_time + 10000000;  // 10ms

  // Start time measurement
  auto start = std::chrono::high_resolution_clock::now();

  // Sleep until target time
  SyncClock::SleepUntil(target_time);

  // End time measurement
  auto end = std::chrono::high_resolution_clock::now();

  // Calculate actual sleep duration in nanoseconds
  auto sleep_duration =
      std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

  // Check that we slept for at least 9ms (allowing for some system scheduling
  // variance)
  EXPECT_GE(sleep_duration, 9000000);
}
