// test/integration/test_gps_chain.cpp
#include "map_gps_filter.h"
#include "mock_arduino.h"
#include <gtest/gtest.h>
#include <vector>

struct FixSequence {
  double lat, lon;
  int sats;
  float hdop;
  float speed_kmph;
  uint32_t millis_delta;
};

class GPSChainTest : public ::testing::Test {
protected:
  MapGPSFilter filter;

  void SetUp() override {
    MockArduino::reset_millis();
    filter.reset();
  }

  void feedSequence(const std::vector<FixSequence> &seq) {
    uint32_t now = 0;
    for (const auto &f : seq) {
      now += f.millis_delta;
      MockArduino::current_millis = now;

      gps_fix fix;
      fix.valid.location = true;
      fix.latitudeL(static_cast<int32_t>(f.lat * 1e7));
      fix.longitudeL(static_cast<int32_t>(f.lon * 1e7));
      fix.valid.satellites = true;
      fix.satellites = f.sats;
      fix.valid.hdop = true;
      fix.hdop = static_cast<uint16_t>(f.hdop * 1000);
      fix.valid.speed = true;
      fix.spd.whole = static_cast<uint16_t>(f.speed_kmph * 100);

      filter.updateFilteredOwnPosition(fix);
      filter.addOwnTracePoint();
    }
  }
};

// Indoor jitter: HDOP > 3.0 and/or sats < 7 → all updates frozen after init
TEST_F(GPSChainTest, Stationary_Indoor_Jitter_Should_Freeze) {
  std::vector<FixSequence> seq = {
      {48.8566, 2.3522, 9, 1.8, 10.0, 0},     // init: good fix
      {48.85661, 2.35221, 7, 3.2, 1.2, 2000},  // HDOP 3.2 > 3.0 → freeze
      {48.85658, 2.35218, 7, 4.1, 0.8, 2000},  // HDOP 4.1 > 3.0 → freeze
      {48.85662, 2.35223, 6, 5.5, 2.1, 2000},  // sats 6 < 7 → rejected
  };

  feedSequence(seq);

  EXPECT_NEAR(48.8566, filter.getOwnLat(), 0.00005);
  EXPECT_NEAR(2.3522, filter.getOwnLon(), 0.00005);
  EXPECT_LE(filter.getOwnTraceCount(), 1);
}

// Outdoor walking/cycling: good sats, low HDOP → position follows with low-pass smoothing
TEST_F(GPSChainTest, Moving_Outdoor_Normal_Should_Follow) {
  std::vector<FixSequence> seq = {
      {48.8566, 2.3522, 10, 1.2, 0.0, 0},       // init
      {48.8570, 2.3525, 10, 1.1, 12.0, 5000},    // ~15 km/h
      {48.8578, 2.3531, 10, 1.3, 15.0, 4000},
      {48.8585, 2.3538, 10, 1.0, 18.0, 4000},
  };

  feedSequence(seq);

  // Low-pass smoothing (alpha ~0.5) means position lags behind last fix
  EXPECT_NEAR(48.8585, filter.getOwnLat(), 0.001);
  EXPECT_NEAR(2.3538, filter.getOwnLon(), 0.001);
  EXPECT_GE(filter.getOwnTraceCount(), 3);
}

// Long gap: accept if close (<5km), reject if far (>5km)
TEST_F(GPSChainTest, LongGap_Accept_If_Close_Rejects_If_Far) {
  std::vector<FixSequence> seq_close = {
      {48.8566, 2.3522, 9, 1.5, 5.0, 0},
      {48.8567, 2.3523, 9, 1.6, 5.0, 150000},   // +150s, ~40m
  };

  feedSequence(seq_close);
  EXPECT_NEAR(48.8567, filter.getOwnLat(), 0.0002);

  filter.reset();
  MockArduino::reset_millis();

  std::vector<FixSequence> seq_far = {
      {48.8566, 2.3522, 9, 1.5, 5.0, 0},
      {49.8566, 2.3522, 9, 1.6, 5.0, 150000},   // +150s, ~111 km → rejected
  };

  feedSequence(seq_far);
  EXPECT_NEAR(48.8566, filter.getOwnLat(), 0.0001);
}

// Cross-check: low speed + small distance → Doppler jitter rejected
TEST_F(GPSChainTest, DopplerJitter_LowSpeed_SmallDist_Rejected) {
  std::vector<FixSequence> seq = {
      {48.8566, 2.3522, 10, 1.5, 10.0, 0},       // init
      {48.85662, 2.35222, 10, 1.5, 6.0, 2000},   // speed 6 < 8, dist ~2m < 25 → rejected
      {48.85665, 2.35225, 10, 1.5, 7.0, 2000},   // speed 7 < 8, dist ~5m < 25 → rejected
  };

  feedSequence(seq);

  EXPECT_NEAR(48.8566, filter.getOwnLat(), 0.00005);
  EXPECT_NEAR(2.3522, filter.getOwnLon(), 0.00005);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
