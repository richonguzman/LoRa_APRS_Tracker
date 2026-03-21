// test/test_map_gps_filter.cpp
#include <gtest/gtest.h>
#include "map_gps_filter.h"
#include <string>
#include <cmath>
#include <cstdio>
#ifdef UNIT_TEST
#include "mock_arduino.h"
#endif

// Mock implementation of STATION_Utils::douglasPeuckerSimplify for unit tests
namespace STATION_Utils {
    void douglasPeuckerSimplify(TracePoint* trace, int start, int end, bool* keep, float epsilon) {
        // Simplified mock: keep all points between start and end (inclusive)
        for (int i = start; i <= end; i++) {
            keep[i] = true;
        }
    }
}

// Helper to build a gps_fix with specified values
gps_fix makeFix(double lat, double lon, unsigned int sats, double hdop,
                double speed_kmph = 0.0,
                bool valid_location = true, bool valid_hdop = true, bool valid_speed = true) {
    gps_fix fix;

    if (valid_location) {
        fix.valid.location = true;
        fix.latitudeL(static_cast<int32_t>(lat * 1e7));
        fix.longitudeL(static_cast<int32_t>(lon * 1e7));
    }

    fix.valid.satellites = true;
    fix.satellites = sats;

    if (valid_hdop) {
        fix.valid.hdop = true;
        fix.hdop = static_cast<uint16_t>(hdop * 1000);
    }

    if (valid_speed) {
        fix.valid.speed = true;
        // NeoGPS speed is stored internally; speed_kph() returns from internal representation
        // We set it via the whole/frac fields
        fix.spd.whole = static_cast<uint16_t>(speed_kmph * 100);  // speed in cm/s * factor
    }

    return fix;
}

// Fixture pour initialiser le filtre avant chaque test
class MapGPSFilterTest : public ::testing::Test {
protected:
    MapGPSFilter filter;

    void SetUp() override {
#ifdef UNIT_TEST
        MockArduino::reset_millis();
#endif
        filter.reset();
    }
};

TEST_F(MapGPSFilterTest, InitialState) {
    EXPECT_FALSE(filter.isOwnPositionValid());
    EXPECT_EQ(0.0f, filter.getOwnLat());
    EXPECT_EQ(0.0f, filter.getOwnLon());
    EXPECT_EQ(0, filter.getOwnTraceCount());
    EXPECT_EQ(0, filter.getOwnTraceHead());

    double lat, lon;
    EXPECT_FALSE(filter.getUiPosition(&lat, &lon));
}

TEST_F(MapGPSFilterTest, UpdateWithInvalidLocation) {
    gps_fix fix = makeFix(0.0, 0.0, 0, 99.9, 0.0, false);
    filter.updateFilteredOwnPosition(fix);
    EXPECT_FALSE(filter.isOwnPositionValid());
}

TEST_F(MapGPSFilterTest, UpdateWithLowSatellites) {
    gps_fix fix = makeFix(37.7749, -122.4194, 2, 1.0);
    filter.updateFilteredOwnPosition(fix);
    EXPECT_FALSE(filter.isOwnPositionValid());
}

TEST_F(MapGPSFilterTest, UpdateWithMinSatellites) {
    gps_fix fix = makeFix(37.7749, -122.4194, 7, 1.0);
    filter.updateFilteredOwnPosition(fix);
    EXPECT_TRUE(filter.isOwnPositionValid());
    EXPECT_NEAR(37.7749f, filter.getOwnLat(), 0.001f);
    EXPECT_NEAR(-122.4194f, filter.getOwnLon(), 0.001f);
}

TEST_F(MapGPSFilterTest, FirstFilteredUpdate) {
    gps_fix fix = makeFix(37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(fix);
    EXPECT_TRUE(filter.isOwnPositionValid());
    EXPECT_NEAR(37.7750f, filter.getOwnLat(), 0.001f);
    EXPECT_NEAR(-122.4195f, filter.getOwnLon(), 0.001f);
}

TEST_F(MapGPSFilterTest, AddTracePoint) {
    gps_fix fix = makeFix(37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(fix);

    filter.addOwnTracePoint();
    EXPECT_EQ(1, filter.getOwnTraceCount());
    const TracePoint& point = filter.getOwnTracePoint(0);
    EXPECT_NEAR(37.7750f, point.lat, 0.001f);
    EXPECT_NEAR(-122.4195f, point.lon, 0.001f);
}

TEST_F(MapGPSFilterTest, AddCloseTracePointNoAdd) {
    gps_fix fix1 = makeFix(37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(fix1);
    filter.addOwnTracePoint();

    gps_fix fix2 = makeFix(37.7750 + 0.00002, -122.4195 + 0.00002, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(fix2);
    filter.addOwnTracePoint();

    EXPECT_EQ(1, filter.getOwnTraceCount());
}

TEST_F(MapGPSFilterTest, AddFarTracePointAdd) {
    gps_fix fix1 = makeFix(37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(fix1);
    filter.addOwnTracePoint();

    gps_fix fix2 = makeFix(37.7750 + 0.0005, -122.4195 + 0.0005, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(fix2);
    filter.addOwnTracePoint();

    EXPECT_EQ(2, filter.getOwnTraceCount());
}

TEST_F(MapGPSFilterTest, CircularBufferWrapAround) {
    for (int i = 0; i < MapGPSFilter::OWN_TRACE_MAX_POINTS; ++i) {
        gps_fix fix = makeFix(37.7750 + i * 0.001, -122.4195, 7, 1.0, 10.0);
        filter.updateFilteredOwnPosition(fix);
        filter.addOwnTracePoint();
    }
    EXPECT_EQ(MapGPSFilter::OWN_TRACE_MAX_POINTS, filter.getOwnTraceCount());

    gps_fix fix_extra = makeFix(40.0, -120.0, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(fix_extra);
    filter.addOwnTracePoint();

    EXPECT_EQ(MapGPSFilter::OWN_TRACE_MAX_POINTS, filter.getOwnTraceCount());
}

TEST_F(MapGPSFilterTest, GetUiPosition) {
    gps_fix fix1 = makeFix(37.7749, -122.4194, 7, 1.0);
    filter.updateFilteredOwnPosition(fix1);

    double lat, lon;
    EXPECT_TRUE(filter.getUiPosition(&lat, &lon));
    EXPECT_NEAR(37.7749f, lat, 0.001f);
    EXPECT_NEAR(-122.4194f, lon, 0.001f);
}

TEST_F(MapGPSFilterTest, ResetClearsAll) {
    gps_fix fix = makeFix(37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(fix);
    filter.addOwnTracePoint();

    filter.reset();

    EXPECT_FALSE(filter.isOwnPositionValid());
    EXPECT_EQ(0, filter.getOwnTraceCount());
    double lat, lon;
    EXPECT_FALSE(filter.getUiPosition(&lat, &lon));
}

TEST_F(MapGPSFilterTest, JumpFilterReject) {
    gps_fix fix1 = makeFix(37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(fix1);

#ifdef UNIT_TEST
    MockArduino::current_millis = 1000;
#endif

    // ~111 km in 1s (400000 km/h)
    gps_fix fix_jump = makeFix(38.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(fix_jump);

    EXPECT_NEAR(37.7750f, filter.getOwnLat(), 0.001f);
}

TEST_F(MapGPSFilterTest, JumpFilterAccept) {
    gps_fix fix1 = makeFix(37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(fix1);

#ifdef UNIT_TEST
    MockArduino::current_millis = 60000;
#endif

    // ~0.001° in 60s (~4 km/h)
    gps_fix fix2 = makeFix(37.7760, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(fix2);

    EXPECT_GT(filter.getOwnLat(), 37.7750f);
}

TEST_F(MapGPSFilterTest, JitterFilterReject) {
    gps_fix fix1 = makeFix(37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(fix1);

    gps_fix fix_jitter = makeFix(37.7751, -122.4196, 7, 1.0, 1.0); // speed < 3 km/h
    filter.updateFilteredOwnPosition(fix_jitter);

    EXPECT_NEAR(37.7750f, filter.getOwnLat(), 0.001f);
}

TEST_F(MapGPSFilterTest, PoorSignalFreeze) {
    gps_fix fix1 = makeFix(37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(fix1);

#ifdef UNIT_TEST
    MockArduino::current_millis = 1000;
#endif
    gps_fix fix_poor = makeFix(37.7751, -122.4196, 7, 10.0, 3.0); // HDOP=10
    filter.updateFilteredOwnPosition(fix_poor);

    EXPECT_NEAR(37.7750f, filter.getOwnLat(), 0.001f);
}

TEST_F(MapGPSFilterTest, InvalidSpeedDoesNotBlock) {
    gps_fix fix1 = makeFix(37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(fix1);

#ifdef UNIT_TEST
    MockArduino::current_millis = 1000;
#endif
    gps_fix fix2 = makeFix(37.7751, -122.4196, 7, 1.0, 0.0, true, true, false);
    filter.updateFilteredOwnPosition(fix2);

    // Should be accepted since jitter filter requires valid speed
    EXPECT_GT(filter.getOwnLat(), 37.7750f);
}

TEST_F(MapGPSFilterTest, LongIntervalDisablesSpeedCheck) {
    gps_fix fix1 = makeFix(37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(fix1);

#ifdef UNIT_TEST
    MockArduino::current_millis = 121000;
#endif
    // +1 degree in 121s => normally 3300 km/h but dt > 120s => distance check only (<5km fails)
    gps_fix fix_huge = makeFix(38.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(fix_huge);

    // Should be rejected (distance > 5000m after long gap)
    EXPECT_NEAR(37.7750f, filter.getOwnLat(), 0.001f);
}

#if defined(UNIT_TEST)
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
