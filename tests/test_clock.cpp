#include "test.h"
#include "simulation/clock.h"

using namespace sc2k;

TEST(calendar_month_year_season) {
    Calendar c;
    c.day = 0;   CHECK(c.month() == 0 && c.year() == 0 && c.phase() == 0 && c.season() == 0);
    c.day = 25;  CHECK(c.month() == 1 && c.phase() == 0);
    c.day = 24;  CHECK(c.month() == 0 && c.phase() == 24);
    c.day = 300; CHECK(c.month() == 0 && c.year() == 1);
    c.day = 299; CHECK(c.month() == 11 && c.season() == 0);
    c.day = 50;  CHECK(c.month() == 2 && c.season() == 1);   // March -> spring bucket
    c.day = 25 * 5; CHECK(c.season() == 2);                  // June
    c.day = 25 * 8; CHECK(c.season() == 3);                  // September
}
static int stepsOver(GameSpeed s, int ticks) {
    Clock c; c.speed = s; int n = 0;
    for (int i = 0; i < ticks; ++i) if (c.onTimerTick()) ++n;
    return n;
}
TEST(clock_rates_per_16_ticks) {
    CHECK(stepsOver(GameSpeed::Turtle, 16) == 4);    // every 4th tick = 800 ms
    CHECK(stepsOver(GameSpeed::Llama, 16) == 8);     // every 2nd = 400 ms
    CHECK(stepsOver(GameSpeed::Cheetah, 16) == 16);  // every tick = 200 ms
    CHECK(stepsOver(GameSpeed::AfricanSwallow, 16) == 16);
}
TEST(clock_disaster_mode_halves_rate) {
    Clock c; c.speed = GameSpeed::Cheetah; c.disasterMode = true; int n = 0;
    for (int i = 0; i < 16; ++i) if (c.onTimerTick()) ++n;
    CHECK(n == 8);
}
TEST(clock_paused_never_steps) {
    Clock c; c.speed = GameSpeed::Paused;
    CHECK(c.advance(100000) == 0);
}
TEST(clock_advance_accumulates) {
    Clock c; c.speed = GameSpeed::Cheetah;
    CHECK(c.advance(100) == 0);
    CHECK(c.advance(100) == 1);
    CHECK(c.advance(1000) == 5);
}
