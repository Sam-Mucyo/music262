#pragma once

#include <chrono>
#include <functional>
#include <mutex>
#include <vector>

// Forward declarations
namespace client {
class PingResponse;
}

// Type for time measurement functions
using TimePointNs = int64_t;

/**
 * @class SyncClock
 * @brief Class to handle NTP-style clock synchronization between peers
 *
 * This class implements clock synchronization logic based on NTP principles
 * to allow multiple clients to coordinate timing for synchronized playback.
 */
class SyncClock {
 public:
  SyncClock();
  ~SyncClock() = default;

  /**
   * @brief Process a ping response to calculate clock offset and RTT
   *
   * @param t0 Time at client before sending request (ns)
   * @param t3 Time at client after receiving response (ns)
   * @param response The ping response containing t1 and t2 times
   * @return A pair of (offset, rtt) values in nanoseconds
   */
  std::pair<float, float> ProcessPingResponse(
      TimePointNs t0, TimePointNs t3, const client::PingResponse& response);

  /**
   * @brief Calculate the average clock offset based on recent measurements
   *
   * @param offsets Vector of clock offset measurements
   * @return The average offset in nanoseconds
   */
  float CalculateAverageOffset(const std::vector<float>& offsets);

  /**
   * @brief Get the current stored average offset
   *
   * @return The average offset in nanoseconds
   */
  float GetAverageOffset() const;

  /**
   * @brief Set the average offset directly
   *
   * @param offset The offset to set in nanoseconds
   */
  void SetAverageOffset(float offset);

  /**
   * @brief Get the maximum RTT observed
   *
   * @return The maximum RTT in nanoseconds
   */
  float GetMaxRtt() const;

  /**
   * @brief Set the maximum RTT directly
   *
   * @param rtt The RTT to set in nanoseconds
   */
  void SetMaxRtt(float rtt);

  /**
   * @brief Calculate a target execution time in the future
   *
   * This function calculates a time in the future that accounts for
   * network delay and processing time, ensuring all peers can execute
   * a command at approximately the same time.
   *
   * @param safety_margin_ns Additional safety margin in nanoseconds
   * @return The target execution time in nanoseconds since epoch
   */
  TimePointNs CalculateTargetExecutionTime(float safety_margin_ns = 1000000.0f);

  /**
   * @brief Get the current time in nanoseconds since epoch
   *
   * @return The current time in nanoseconds
   */
  static TimePointNs GetCurrentTimeNs();

  /**
   * @brief Sleep until a specific target time
   *
   * @param target_time_ns The target time in nanoseconds since epoch
   */
  static void SleepUntil(TimePointNs target_time_ns);

  /**
   * @brief Adjust a target time based on clock offset
   *
   * @param target_time_ns The target time to adjust
   * @param clock_offset The clock offset to apply
   * @return The adjusted target time
   */
  static TimePointNs AdjustTargetTime(TimePointNs target_time_ns,
                                      float clock_offset);

 private:
  mutable std::mutex mutex_;
  float avg_offset_;  // Average clock offset from peers in nanoseconds
  float max_rtt_;     // Maximum round-trip time observed in nanoseconds
};