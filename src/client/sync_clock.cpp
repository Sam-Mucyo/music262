#include "include/sync_clock.h"

#include <algorithm>
#include <numeric>
#include <thread>

#include "../common/include/logger.h"
#include "audio_sync.pb.h"

SyncClock::SyncClock() : avg_offset_(0.0f), max_rtt_(0.0f) {
  LOG_DEBUG("SyncClock initialized");
}

std::pair<float, float> SyncClock::ProcessPingResponse(
    TimePointNs t0, TimePointNs t3, const client::PingResponse& response) {
  // Get t1, t2 from response (times at the server)
  TimePointNs t1 = response.receiver_time_recv();
  TimePointNs t2 = response.receiver_time_send();

  // Calculate round-trip time: (t3-t0) - (t2-t1)
  // Total time - server processing time
  float current_rtt = static_cast<float>((t3 - t0) - (t2 - t1));

  // Calculate clock offset: ((t1-t0) + (t2-t3))/2
  // Estimate how much the client clock differs from the server
  float current_offset = static_cast<float>((t1 - t0) + (t2 - t3)) / 2.0f;

  LOG_DEBUG("Ping response: RTT={} ns, Offset={} ns", current_rtt,
            current_offset);

  return {current_offset, current_rtt};
}

float SyncClock::CalculateAverageOffset(const std::vector<float>& offsets) {
  if (offsets.empty()) {
    LOG_DEBUG("No offsets to calculate average from");
    return 0.0f;
  }

  // Calculate the average offset
  float average =
      std::accumulate(offsets.begin(), offsets.end(), 0.0f) / offsets.size();

  // Update the stored average
  SetAverageOffset(average);

  LOG_INFO("Calculated average offset: {} ns from {} measurements", average,
           offsets.size());

  return average;
}

float SyncClock::GetAverageOffset() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return avg_offset_;
}

void SyncClock::SetAverageOffset(float offset) {
  std::lock_guard<std::mutex> lock(mutex_);
  avg_offset_ = offset;
  LOG_DEBUG("Set average offset to {} ns", offset);
}

float SyncClock::GetMaxRtt() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return max_rtt_;
}

void SyncClock::SetMaxRtt(float rtt) {
  std::lock_guard<std::mutex> lock(mutex_);
  max_rtt_ = rtt;
  LOG_DEBUG("Set max RTT to {} ns", rtt);
}

TimePointNs SyncClock::CalculateTargetExecutionTime(float safety_margin_ns) {
  // Calculate the target execution time in the future
  // We want all peers to execute this command at approximately the same time
  float total_margin = GetMaxRtt() + safety_margin_ns;

  auto now = std::chrono::high_resolution_clock::now();
  auto target_time =
      now + std::chrono::nanoseconds(static_cast<int64_t>(total_margin));

  // Convert to nanoseconds since epoch
  auto target_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            target_time.time_since_epoch())
                            .count();

  LOG_DEBUG("Calculated target execution time: {} ns (current time + {} ns)",
            target_time_ns, total_margin);

  return target_time_ns;
}

TimePointNs SyncClock::GetCurrentTimeNs() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::high_resolution_clock::now().time_since_epoch())
      .count();
}

void SyncClock::SleepUntil(TimePointNs target_time_ns) {
  auto current_time_ns = GetCurrentTimeNs();

  if (target_time_ns > current_time_ns) {
    // Convert target time from nanoseconds to a timepoint
    auto target_timepoint = std::chrono::high_resolution_clock::time_point(
        std::chrono::nanoseconds(target_time_ns));

    LOG_DEBUG("Sleeping until target time: {} ns (waiting {} ns)",
              target_time_ns, target_time_ns - current_time_ns);

    // Sleep until the target time
    std::this_thread::sleep_until(target_timepoint);
  } else {
    LOG_DEBUG("Target time already passed, no sleep needed");
  }
}

TimePointNs SyncClock::AdjustTargetTime(TimePointNs target_time_ns,
                                        float clock_offset) {
  // Adjust the target time based on our clock offset
  TimePointNs adjusted_time =
      target_time_ns - static_cast<TimePointNs>(clock_offset);

  LOG_DEBUG("Adjusted target time from {} to {} (offset: {})", target_time_ns,
            adjusted_time, clock_offset);

  return adjusted_time;
}