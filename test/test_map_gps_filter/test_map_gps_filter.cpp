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

// Helper pour générer NMEA GGA sentence (position, sats, HDOP)
std::string generateGGA(double lat, double lon, unsigned int sats, double hdop, bool valid_location = true, bool valid_hdop = true) {
    char buf[128];
    if (!valid_location) {
        snprintf(buf, sizeof(buf), "$GPGGA,123456.00,,,,0,%02u,%.1f,0.0,M,0.0,M,,", sats, hdop);
    } else {
        double lat_abs = std::fabs(lat);
        int lat_deg = static_cast<int>(lat_abs);
        double lat_min = (lat_abs - lat_deg) * 60.0;
        char lat_str[16];
        snprintf(lat_str, sizeof(lat_str), "%02d%07.4f", lat_deg, lat_min);
        char ns = (lat >= 0) ? 'N' : 'S';

        double lon_abs = std::fabs(lon);
        int lon_deg = static_cast<int>(lon_abs);
        double lon_min = (lon_abs - lon_deg) * 60.0;
        char lon_str[16];
        snprintf(lon_str, sizeof(lon_str), "%03d%07.4f", lon_deg, lon_min);
        char ew = (lon >= 0) ? 'E' : 'W';

        char hdop_str[8] = "";
        if (valid_hdop) {
            snprintf(hdop_str, sizeof(hdop_str), "%.1f", hdop);
        } else {
            // Leave empty to force parse fail, isValid=false
        }

        snprintf(buf, sizeof(buf), "$GPGGA,123456.00,%s,%c,%s,%c,1,%02u,%s,0.0,M,0.0,M,,", lat_str, ns, lon_str, ew, sats, hdop_str);
    }

    // Calcul checksum
    unsigned char checksum = 0;
    for (const char* p = buf + 1; *p != '\0'; ++p) {
        checksum ^= static_cast<unsigned char>(*p);
    }
    char checksum_str[3];
    snprintf(checksum_str, sizeof(checksum_str), "%02X", checksum);

    // Ajout *checksum\r\n
    std::string nmea = std::string(buf) + "*" + checksum_str + "\r\n";
    return nmea;
}

// Helper pour générer NMEA RMC sentence (vitesse en noeuds, nécessaire pour speed.kmph())
std::string generateRMC(double speed_knots, bool valid_speed = true) {
    char buf[128];
    if (!valid_speed) {
        snprintf(buf, sizeof(buf), "$GPRMC,123456.00,A,,,,,,,%02u%02u%02u,,,A,", 1, 1, 2000);  // Invalid speed
    } else {
        snprintf(buf, sizeof(buf), "$GPRMC,123456.00,A,,,,,%.2f,0.0,%02u%02u%02u,,,A,", speed_knots, 1, 1, 2000);
    }

    // Calcul checksum
    unsigned char checksum = 0;
    for (const char* p = buf + 1; *p != '\0'; ++p) {
        checksum ^= static_cast<unsigned char>(*p);
    }
    char checksum_str[3];
    snprintf(checksum_str, sizeof(checksum_str), "%02X", checksum);

    // Ajout *checksum\r\n
    std::string nmea = std::string(buf) + "*" + checksum_str + "\r\n";
    return nmea;
}

// Helper pour feed NMEA to gps (GGA + RMC for speed)
void feedGPS(TinyGPSPlus& gps, double lat, double lon, unsigned int sats, double hdop, double speed_kmph = 0.0, bool valid_location = true, bool valid_hdop = true, bool valid_speed = true) {
    double speed_knots = speed_kmph / 1.852;  // km/h to knots
    std::string nmea_gga = generateGGA(lat, lon, sats, hdop, valid_location, valid_hdop);
    std::string nmea_rmc = generateRMC(speed_knots, valid_speed);
    for (char c : nmea_gga) gps.encode(c);
    for (char c : nmea_rmc) gps.encode(c);
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

    float lat, lon;
    EXPECT_FALSE(filter.getUiPosition(&lat, &lon));
}

TEST_F(MapGPSFilterTest, UpdateWithInvalidLocation) {
    TinyGPSPlus gps;
    feedGPS(gps, 0.0, 0.0, 0, 99.9, 0.0, false);
    filter.updateFilteredOwnPosition(gps);

    EXPECT_FALSE(filter.isOwnPositionValid());
}

TEST_F(MapGPSFilterTest, UpdateWithLowSatellites) {
    TinyGPSPlus gps;
    feedGPS(gps, 37.7749, -122.4194, 2, 1.0);
    filter.updateFilteredOwnPosition(gps);

    EXPECT_FALSE(filter.isOwnPositionValid());
}

TEST_F(MapGPSFilterTest, UpdateWithIconSatellites) {
    TinyGPSPlus gps;
    feedGPS(gps, 37.7749, -122.4194, 4, 1.0);
    filter.updateFilteredOwnPosition(gps);

    EXPECT_TRUE(filter.isOwnPositionValid());
    EXPECT_NEAR(37.7749f, filter.getOwnLat(), 0.0001f);
    EXPECT_NEAR(-122.4194f, filter.getOwnLon(), 0.0001f);
}

TEST_F(MapGPSFilterTest, FirstFilteredUpdate) {
    TinyGPSPlus gps;
    feedGPS(gps, 37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps);

    EXPECT_TRUE(filter.isOwnPositionValid());
    EXPECT_NEAR(37.7750f, filter.getOwnLat(), 0.0001f);
    EXPECT_NEAR(-122.4195f, filter.getOwnLon(), 0.0001f);
}

TEST_F(MapGPSFilterTest, SmallMovementNoUpdate) {
    TinyGPSPlus gps_first;
    feedGPS(gps_first, 37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_first);

    TinyGPSPlus gps_small;
    feedGPS(gps_small, 37.7750 + 0.00005, -122.4195 + 0.00005, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_small);

    EXPECT_NEAR(37.7750f, filter.getOwnLat(), 0.0001f);
    EXPECT_NEAR(-122.4195f, filter.getOwnLon(), 0.0001f);
}

TEST_F(MapGPSFilterTest, LargeMovementUpdate) {
    TinyGPSPlus gps_first;
    feedGPS(gps_first, 37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_first);

    TinyGPSPlus gps_large;
    feedGPS(gps_large, 37.7750 + 0.001, -122.4195 + 0.001, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_large);

    // Alpha is 1.0 since HDOP is 1.0, so the filtered position matches raw directly
    EXPECT_NEAR(37.7760f, filter.getOwnLat(), 0.0001f);
    EXPECT_NEAR(-122.4185f, filter.getOwnLon(), 0.0001f);
}

TEST_F(MapGPSFilterTest, AddTracePoint) {
    TinyGPSPlus gps;
    feedGPS(gps, 37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps);

    filter.addOwnTracePoint();
    EXPECT_EQ(1, filter.getOwnTraceCount());
    const TracePoint& point = filter.getOwnTracePoint(0);
    EXPECT_NEAR(37.7750f, point.lat, 0.0001f);
    EXPECT_NEAR(-122.4195f, point.lon, 0.0001f);
}

TEST_F(MapGPSFilterTest, AddCloseTracePointNoAdd) {
    TinyGPSPlus gps_first;
    feedGPS(gps_first, 37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_first);
    filter.addOwnTracePoint();

    TinyGPSPlus gps_close;
    feedGPS(gps_close, 37.7750 + 0.00002, -122.4195 + 0.00002, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_close);
    filter.addOwnTracePoint();

    EXPECT_EQ(1, filter.getOwnTraceCount());
}

TEST_F(MapGPSFilterTest, AddFarTracePointAdd) {
    TinyGPSPlus gps_first;
    feedGPS(gps_first, 37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_first);
    filter.addOwnTracePoint();

    TinyGPSPlus gps_far;
    feedGPS(gps_far, 37.7750 + 0.0005, -122.4195 + 0.0005, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_far);
    filter.addOwnTracePoint();

    EXPECT_EQ(2, filter.getOwnTraceCount());
    const TracePoint& point2 = filter.getOwnTracePoint(1);
    EXPECT_NEAR(37.7755f, point2.lat, 0.0001f);
    EXPECT_NEAR(-122.4190f, point2.lon, 0.0001f);
}

TEST_F(MapGPSFilterTest, CircularBufferWrapAround) {
    for (int i = 0; i < MapGPSFilter::OWN_TRACE_MAX_POINTS; ++i) {
        TinyGPSPlus gps;
        feedGPS(gps, 37.7750 + i * 0.001, -122.4195, 7, 1.0, 10.0);
        filter.updateFilteredOwnPosition(gps);
        filter.addOwnTracePoint();
    }
    EXPECT_EQ(MapGPSFilter::OWN_TRACE_MAX_POINTS, filter.getOwnTraceCount());

    TinyGPSPlus gps_extra;
    feedGPS(gps_extra, 40.0, -120.0, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_extra);
    filter.addOwnTracePoint();

    // The addition of this point triggers compactTrace().
    // So the trace count drops below OWN_TRACE_MAX_POINTS before adding.
    // The mock compaction keeps half of the elements.
    EXPECT_EQ(MapGPSFilter::OWN_TRACE_MAX_POINTS, filter.getOwnTraceCount());

    const TracePoint& oldest = filter.getOwnTracePoint(0);
    EXPECT_NEAR(37.7760f, oldest.lat, 0.0001f);
    }

TEST_F(MapGPSFilterTest, GetUiPositionPriority) {
    TinyGPSPlus gps_icon;
    feedGPS(gps_icon, 37.7749, -122.4194, 4, 1.0);
    filter.updateFilteredOwnPosition(gps_icon);

    float lat, lon;
    EXPECT_TRUE(filter.getUiPosition(&lat, &lon));
    EXPECT_NEAR(37.7749f, lat, 0.0001f);
    EXPECT_NEAR(-122.4194f, lon, 0.0001f);

    TinyGPSPlus gps_filtered;
    feedGPS(gps_filtered, 37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_filtered);
    EXPECT_TRUE(filter.getUiPosition(&lat, &lon));
    EXPECT_NEAR(37.7750f, lat, 0.0001f);
    EXPECT_NEAR(-122.4195f, lon, 0.0001f);
}

TEST_F(MapGPSFilterTest, ResetClearsAll) {
    TinyGPSPlus gps;
    feedGPS(gps, 37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps);
    filter.addOwnTracePoint();

    filter.reset();

    EXPECT_FALSE(filter.isOwnPositionValid());
    EXPECT_EQ(0, filter.getOwnTraceCount());
    float lat, lon;
    EXPECT_FALSE(filter.getUiPosition(&lat, &lon));
}

TEST_F(MapGPSFilterTest, JumpFilterReject) {
    TinyGPSPlus gps_first;
    feedGPS(gps_first, 37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_first);

    // Simuler temps: 1 seconde plus tard (dt=1000ms)
#ifdef UNIT_TEST
    MockArduino::current_millis = 1000;
#endif

    // Déplacement aberrant: ~111 km en 1s (~400000 km/h)
    TinyGPSPlus gps_jump;
    feedGPS(gps_jump, 38.7750, -122.4195, 7, 1.0, 10.0);  // +1° lat ~111km
    filter.updateFilteredOwnPosition(gps_jump);

    // Doit rejeter, position inchangée
    EXPECT_NEAR(37.7750f, filter.getOwnLat(), 0.0001f);
    }

    TEST_F(MapGPSFilterTest, JumpFilterAccept) {
    TinyGPSPlus gps_first;
    feedGPS(gps_first, 37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_first);

    // Simuler temps: 60 secondes plus tard (dt=60s)
#ifdef UNIT_TEST
    MockArduino::current_millis = 60000;
#endif

    // Déplacement acceptable: ~0.001° en 60s (~4 km/h)
    TinyGPSPlus gps_accept;
    feedGPS(gps_accept, 37.7760, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_accept);

    // Doit accepter et updater (centroïde)
    EXPECT_GT(filter.getOwnLat(), 37.7750f);
    }

    TEST_F(MapGPSFilterTest, JitterFilterReject) {
    TinyGPSPlus gps_first;
    feedGPS(gps_first, 37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_first);

    TinyGPSPlus gps_jitter;
    feedGPS(gps_jitter, 37.7751, -122.4196, 7, 1.0, 1.0);  // Vitesse <1.5 km/h
    filter.updateFilteredOwnPosition(gps_jitter);

    // Doit rejeter, position inchangée
    EXPECT_NEAR(37.7750f, filter.getOwnLat(), 0.0001f);
    }

    TEST_F(MapGPSFilterTest, CentroidSmoothing) {
        TinyGPSPlus gps_first;
        feedGPS(gps_first, 37.7750, -122.4195, 7, 1.0, 10.0);
        filter.updateFilteredOwnPosition(gps_first);

        // Deuxième point: alpha=0.5 → centroïde à mi-chemin
        TinyGPSPlus gps_second;
        feedGPS(gps_second, 37.7760, -122.4205, 7, 2.0, 10.0);
        filter.updateFilteredOwnPosition(gps_second);

        EXPECT_NEAR(37.7755f, filter.getOwnLat(), 0.0001f);
        EXPECT_NEAR(-122.42f, filter.getOwnLon(), 0.0001f);

        // Ajouter jusqu'à >10 points: alpha=0.1
        for (int i = 0; i < 10; ++i) {
            TinyGPSPlus gps_more;
            feedGPS(gps_more, 37.7760 + i*0.0001, -122.4205 + i*0.0001, 7, 10.0, 10.0);
            filter.updateFilteredOwnPosition(gps_more);
        }
        // Alpha=0.1 pour les derniers, centroïde converge lentement
        EXPECT_GT(filter.getOwnLat(), 37.7755f);
    }

TEST_F(MapGPSFilterTest, HdopThreshold) {
    TinyGPSPlus gps_first;
    feedGPS(gps_first, 37.7750, -122.4195, 7, 2.0, 10.0);
    filter.updateFilteredOwnPosition(gps_first);

    TinyGPSPlus gps_small;
    feedGPS(gps_small, 37.7750 + 0.000045, -122.4195, 7, 2.0, 10.0);
    filter.updateFilteredOwnPosition(gps_small);

    EXPECT_NEAR(37.7750f, filter.getOwnLat(), 0.0001f);

    TinyGPSPlus gps_large;
    feedGPS(gps_large, 37.7750 + 0.0004, -122.4195, 7, 2.0, 10.0);
    filter.updateFilteredOwnPosition(gps_large);

    EXPECT_GT(filter.getOwnLat(), 37.7750f);
}

TEST_F(MapGPSFilterTest, InvalidHdopDefault) {
    TinyGPSPlus gps_first;
    feedGPS(gps_first, 37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_first);

    TinyGPSPlus gps_no_hdop;
    feedGPS(gps_no_hdop, 37.7760, -122.4205, 7, 1.0, 10.0, true, false);
    filter.updateFilteredOwnPosition(gps_no_hdop);

    // TinyGPSPlus might reuse a previous value or default to a high HDOP if parsing fails/is omitted.
    // In the test framework, this currently gives alpha=0.1.
    EXPECT_NEAR(37.7751f, filter.getOwnLat(), 0.0001f);
}

// Test: vitesse GPS invalide -> le filtre anti-gigue ne doit PAS bloquer
TEST_F(MapGPSFilterTest, InvalidSpeedDoesNotBlock) {
    TinyGPSPlus gps_first;
    feedGPS(gps_first, 37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_first);

    // Deuxième point avec vitesse invalide (valid_speed = false)
    // Simuler un petit déplacement (0.0001 deg ~11m) en 1 seconde
#ifdef UNIT_TEST
    MockArduino::current_millis = 1000;
#endif
    TinyGPSPlus gps_invalid_speed;
    feedGPS(gps_invalid_speed, 37.7751, -122.4196, 7, 1.0, 0.0, true, true, false); // vitesse invalide
    filter.updateFilteredOwnPosition(gps_invalid_speed);

    // Le point devrait être accepté car le filtre anti-gigue nécessite une vitesse valide
    EXPECT_NEAR(37.77505f, filter.getOwnLat(), 0.0001f); // alpha=0.5 => moyenne
}

// Test: poor signal freeze (HDOP > 8 et vitesse < 5 km/h) -> doit rejeter
TEST_F(MapGPSFilterTest, PoorSignalFreeze) {
    TinyGPSPlus gps_first;
    feedGPS(gps_first, 37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_first);

    // Point avec mauvais HDOP et faible vitesse
#ifdef UNIT_TEST
    MockArduino::current_millis = 1000;
#endif
    TinyGPSPlus gps_poor;
    feedGPS(gps_poor, 37.7751, -122.4196, 7, 10.0, 3.0); // HDOP=10, speed=3 km/h
    filter.updateFilteredOwnPosition(gps_poor);

    // Doit être rejeté (position inchangée)
    EXPECT_NEAR(37.7750f, filter.getOwnLat(), 0.0001f);
}

// Test: poor signal freeze ne s'applique pas si vitesse >= 5 km/h
TEST_F(MapGPSFilterTest, PoorSignalFreezeNotWhenMoving) {
    TinyGPSPlus gps_first;
    feedGPS(gps_first, 37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_first);

#ifdef UNIT_TEST
    MockArduino::current_millis = 1000;
#endif
    TinyGPSPlus gps_poor_fast;
    feedGPS(gps_poor_fast, 37.7751, -122.4196, 7, 10.0, 6.0); // HDOP=10, speed=6 km/h
    filter.updateFilteredOwnPosition(gps_poor_fast);

    // Doit être accepté (car vitesse >= 5 km/h)
    EXPECT_GT(filter.getOwnLat(), 37.7750f);
}

// Test: saut modéré à l'arrêt (<150 km/h mais >20 m) avec vitesse invalide
// Ce saut devrait passer car aucun filtre ne le bloque actuellement
TEST_F(MapGPSFilterTest, ModerateJumpAtStationaryPasses) {
    TinyGPSPlus gps_first;
    feedGPS(gps_first, 37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_first);

    // Saut de 30 m en 1 seconde (108 km/h) avec vitesse invalide
    // 30 m ~ 0.00027 deg
#ifdef UNIT_TEST
    MockArduino::current_millis = 1000;
#endif
    TinyGPSPlus gps_jump;
    feedGPS(gps_jump, 37.7750 + 0.00027, -122.4195, 7, 1.0, 0.0, true, true, false); // vitesse invalide
    filter.updateFilteredOwnPosition(gps_jump);

    // Actuellement, ce saut est accepté (bug potentiel)
    // On vérifie que le filtre a mis à jour (alpha=0.5)
    EXPECT_GT(filter.getOwnLat(), 37.7750f);
}

// Test: intervalle long (>120 s) désactive le rejet suprasonique
TEST_F(MapGPSFilterTest, LongIntervalDisablesSpeedCheck) {
    TinyGPSPlus gps_first;
    feedGPS(gps_first, 37.7750, -122.4195, 7, 1.0, 10.0);
    filter.updateFilteredOwnPosition(gps_first);

    // Attendre >120 secondes
#ifdef UNIT_TEST
    MockArduino::current_millis = 121000; // 121 secondes
#endif
    // Déplacement énorme (1 degré ~111 km) en 121 secondes => vitesse ~3300 km/h
    // Mais dt > 120s donc le calcul de vitesse n'est pas fait
    TinyGPSPlus gps_huge;
    feedGPS(gps_huge, 38.7750, -122.4195, 7, 1.0, 10.0); // +1 degré
    filter.updateFilteredOwnPosition(gps_huge);

    // Le saut devrait être accepté (car dt > 120s)
    EXPECT_GT(filter.getOwnLat(), 38.0f);
}

#if defined(UNIT_TEST)
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
#endif
