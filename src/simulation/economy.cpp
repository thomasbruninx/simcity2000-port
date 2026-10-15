// Economy / job-market model: literal port of SIMCITY.EXE build .022 function 0x4686C0 (phase 21, after the demand valves).
// x87 arithmetic runs at 53-bit precision in the original (Win32 default), so IEEE doubles in the same order are exact.
// Build with -ffp-contract=off. See docs/simulation/economy.md.
#include <cmath>

#include "simulation/sim_context.h"

namespace sc2k {

int g_econStopStage = 0;   // debugging aid for the emulator diff tool: return after stage N (0 = run everything)

namespace {

// __ftol (0x48C38C): truncate to a 64-bit integer; out-of-range / NaN gives the "integer indefinite" value. Callers use the low 32 bits.
uint32_t ftol(double v) {
    if (!(v > -9223372036854775808.0 && v < 9223372036854775808.0)) return 0;   // low dword of 0x8000000000000000
    return static_cast<uint32_t>(static_cast<uint64_t>(static_cast<int64_t>(v)));
}
double u64(uint32_t v) { return static_cast<double>(static_cast<uint64_t>(v)); }   // FILD qword of a zero-extended dword
int cls(int s) { return s < 0x2D ? 0 : (s < 0x3C ? 1 : (s >= 0x4B ? 3 : 2)); }

}  // namespace

void runEconomy(SimContext& c) {
    CityState& s = c.s;
    const GameTables& t = c.t;
    auto rnd = [&] { return c.rng.next(); };
    auto rmod = [&](int n) { return static_cast<int>(static_cast<uint32_t>(rnd() & 0xFFFF) % static_cast<uint32_t>(n)); };
    const double K10 = t.econK[0], K18 = t.econK[1], K20 = t.econK[2], K28 = t.econK[3], K30 = t.econK[4], K38 = t.econK[5], K40 = t.econK[6], K48 = t.econK[7];
    auto era = [&] { return static_cast<int16_t>(s.era); };
    auto eraTab = [&](int i) -> int { return t.econEra[static_cast<size_t>(i) & 3]; };

    // ---- national population and GNP drift ----
    {
        const double p = (u64(s.nationalPop) * static_cast<double>(era())) * K10;
        const uint32_t d = ftol(p);
        s.nationalPop = s.nationalPop > 5000000u ? s.nationalPop - d : s.nationalPop + d;
    }
    {
        const double p = (static_cast<double>(eraTab(era())) * u64(s.gnp)) * K10;
        const uint32_t d = ftol(p);
        s.gnp = s.gnp > 0x3567E0u ? s.gnp - d : s.gnp + d;
    }
    if (rmod(10) == 0) {
        const double q = (u64(s.gnp) / u64(s.nationalPop + 1)) * K18;
        const int sc = static_cast<int32_t>(ftol(q));
        if (rmod(5) < 2) {
            const int m = static_cast<int16_t>(s.rateLevel) * 25;
            if (m != 0 && static_cast<int>(rnd() & 0xFFFF) % m < sc) { s.rateLevel = static_cast<int16_t>(s.rateLevel + 1); c.hooks.postMessage(9, s.rateLevel & 0xFF); }
            const int m2 = static_cast<int16_t>(s.rateLevel) * 25;
            if (m2 != 0 && sc < static_cast<int>(rnd() & 0xFFFF) % m2) {
                s.rateLevel = static_cast<int16_t>(s.rateLevel - 1);
                if (s.rateLevel == 0) s.rateLevel = 1; else c.hooks.postMessage(10, s.rateLevel & 0xFF);
            }
        }
        if (rmod(3) == 0) {
            const int k = cls(sc);
            if (era() != k) { s.era = k; c.hooks.postMessage(7, k); }
        }
    }

    // ---- the four neighbouring cities ----
    for (size_t i = 0; i < 4; ++i) {
        const uint32_t a = s.nbr9d28[i];
        if (a == 0) continue;
        const int m = rmod(3) + era();
        const uint32_t d = ftol((static_cast<double>(m) * u64(a)) * K10);
        s.nbr9d28[i] = a > 5000000u ? a - d : a + d;
        if (d == 0) s.nbr9d28[i] += static_cast<uint32_t>(rnd() & 1);
        const int sc = static_cast<int32_t>(ftol((u64(s.nbr981c[i]) / u64(s.nbr9d28[i])) * K18));
        const int k = cls(sc);
        const int ie = rmod(5) + eraTab(era()) - static_cast<int16_t>(s.rateLevel) - k;
        const double p = (static_cast<double>(ie) * u64(s.nbr981c[i])) * K10;
        const uint32_t v = s.nbr981c[i];
        const uint32_t d2 = ftol(p);
        s.nbr981c[i] = v > 0x3567E0u ? v - d2 : v + d2;
    }
    if ((rnd() & 0x3F) == 0) {
        const size_t k = static_cast<size_t>(rnd() & 3);
        s.nbr9d28[k] = ftol(u64(s.nbr9d28[k]) * K20);
        s.nbr981c[k] = ftol(u64(s.nbr981c[k]) * K28);
    }

    // ---- sector targets by year, then drift of the 11 sectors ----
    uint32_t T[11];
    uint32_t sumA408 = 0;
    {
        const int idx = std::max(0, (static_cast<int16_t>(s.startYear) - 0x76C) / 0x32 + static_cast<int16_t>(s.yearOffset()) / 0x32);   // (the port never reads before the table, for start years under 1900 either)
        const int rem = static_cast<int16_t>(s.yearOffset()) % 0x32;
        for (size_t k = 0; k < 11; ++k) {
            if (idx < 4) {
                const uint32_t hi = t.econYearTab[static_cast<size_t>(idx) * 11 + 11 + k], lo = t.econYearTab[static_cast<size_t>(idx) * 11 + k];
                T[k] = (hi * static_cast<uint32_t>(rem) + static_cast<uint32_t>(0x32 - rem) * lo) / 0x32u;
            } else {
                T[k] = t.econFinal[k];
            }
            sumA408 += static_cast<uint32_t>(s.sec408[k]);
        }
    }
    int32_t V[11] = {};
    uint32_t total = 0;
    for (size_t k = 0; k < 11; ++k) {
        const int s0 = s.sec940c[k];
        int r = 0;
        for (int j = 0; j < 4; ++j) r += static_cast<int16_t>(c.lfsr.mask(0x7F));
        int32_t prod = static_cast<int32_t>(static_cast<uint32_t>(r) * T[k]);
        int32_t n = ((prod + ((prod >> 31) & 0xFF)) >> 8) + s0 * 3;
        s.sec940c[k] = static_cast<int16_t>((n + ((n >> 31) & 3)) >> 2);
        V[k] = s.sec940c[k];
        auto mul = [&](int slot, double f) { V[slot] = static_cast<int32_t>(ftol(static_cast<double>(V[slot]) * f)); };
        if (s.ordinances & 0x80000) { mul(0, K30); mul(1, K30); mul(2, K30); mul(5, K30); }
        if (s.populationGrowth != 0) mul(4, K38);
        if (s.cb4cc > 0x82) { mul(6, K40); mul(9, K40); }
        else if (s.cb4cc > 0x64) { mul(2, K38); mul(5, K38); mul(7, K38); mul(8, K38); mul(6, K38); mul(9, K38); }
        else if (s.cb4cc < 0x3C) { mul(6, K48); mul(9, K48); }
        V[k] -= s.sec3f8[k];
        if (V[k] <= 0) V[k] = 0; else total += static_cast<uint32_t>(V[k]);
    }
    const uint32_t popTot = static_cast<uint32_t>(s.catPop[2]);
    if (sumA408 > popTot) {
        const uint32_t diff100 = (sumA408 - popTot) * 100u;
        for (size_t k = 0; k < 11; ++k) {
            const int32_t cur = s.sec408[k];
            const uint32_t x = static_cast<uint32_t>(diff100 * static_cast<uint32_t>(cur)) / sumA408;
            const int32_t q = static_cast<int32_t>(x) / 100;
            s.sec408[k] = cur - q;
            const int rr = rmod(100);
            if (rr < static_cast<int32_t>(x) - q * 100) --s.sec408[k];
        }
    } else if (total != 0) {
        const uint32_t diff100 = (popTot - sumA408) * 100u;
        for (size_t k = 0; k < 11; ++k) {
            if (V[k] == 0) continue;
            const uint32_t x = static_cast<uint32_t>(diff100 * static_cast<uint32_t>(V[k])) / total;
            const int32_t q = static_cast<int32_t>(x) / 100;
            s.sec408[k] += q;
            const int rr = rmod(100);
            if (rr < static_cast<int32_t>(x) - q * 100) ++s.sec408[k];
        }
    }
    {
        const uint32_t sum4 = static_cast<uint32_t>(s.sec408[1] + s.sec408[5] + s.sec408[2] + s.sec408[0]);
        const uint32_t v = (sum4 * 100u) / (popTot + 1u);
        s.c8440 = v < 0x14 ? static_cast<int16_t>(-1) : static_cast<int16_t>((static_cast<int32_t>(v) - 0x14) / 0x1E);
        uint32_t best = 0;
        for (size_t k = 0; k < 11; ++k) {
            const uint32_t q = (static_cast<uint32_t>(s.sec408[k]) * 100u) / (popTot + 1u);
            if (q > best) best = q;
        }
        s.c9200 = static_cast<int>(best) < 0x14 ? 0 : static_cast<int16_t>((static_cast<int32_t>(best) - 0x14) / 5);
    }
    if (g_econStopStage == 1) return;
    if (s.population10 == 0) {
        for (size_t i = 0; i < 20; ++i) s.jobsB[i] = s.jobsC[i] = s.jobsA[i] = 0;
        return;
    }

    // ---- job market ----
    const auto& n = s.census;
    const auto& bud = s.budget;
    const int32_t A_ = (static_cast<int16_t>(n[0xD6] / 9) * bud[8].fundingPercent * 15) / 100;
    const int32_t B_ = (static_cast<int16_t>((n[0xD9] + (((int)n[0xD9] >> 31) & 0xF)) >> 4) * bud[9].fundingPercent * 50) / 100;
    int32_t C_ = (static_cast<int16_t>(n[0xD1] / 9) * bud[7].fundingPercent * 25) / 100;
    int32_t base = 0x55;
    if (s.ordinances & 0x200) base = 0x5A;
    if (s.ordinances & 0x400) base += 5;
    if (s.ordinances & 0x20) base += 5;
    if (s.ordinances & 0x40) C_ += bud[0].currentCosts / 400;
    int div = 5;
    for (size_t i = 1; i < 20; ++i, div += 5) {
        const uint32_t cnt = s.jobsA[i];
        if (cnt == 0) continue;
        const int32_t pct = static_cast<int32_t>(((s.jobsC[i] / cnt) * 100u)) / div;
        if (pct >= 100) continue;
        const uint32_t tv = static_cast<uint32_t>(100 - pct) * cnt / 0x18u;
        uint32_t ebx = static_cast<uint32_t>(static_cast<int32_t>(tv) / 100);
        const int rr = rmod(100);
        if (rr < static_cast<int32_t>(tv) - static_cast<int32_t>(ebx) * 100) ++ebx;
        if (cnt < ebx) ebx = cnt;
        if (ebx == 0) continue;
        const uint32_t b = s.jobsB[i];
        uint32_t x = b * ebx / cnt;
        if (x > b) x = b;
        s.jobsB[i] = b - x;
        const uint32_t cc = s.jobsC[i];
        s.jobsC[i] = cc - cc * ebx / cnt;
        s.jobsA[i] -= ebx;
        s.populationGrowth += ebx;
    }
    if (g_econStopStage == 2) return;
    uint32_t pollF = static_cast<uint32_t>(s.pollutionTotal) / (s.population10 + static_cast<uint32_t>(s.zonePop[7]) * 10u + 1u);
    if (static_cast<int32_t>(pollF) > 3) pollF = 3;
    for (int i = 18; i >= 0; --i) {
        const uint32_t cnt = s.jobsA[static_cast<size_t>(i)];
        if (cnt == 0) continue;
        uint32_t u = cnt / 0x3C;
        const uint32_t rr = static_cast<uint32_t>(rnd() & 0xFFFF) % 0x3C;
        if (!(rr >= cnt - 60u * u)) ++u;   // JNC: increment when rr < cnt % 60
        if (cnt < u) u = cnt;
        if (u == 0) continue;
        const size_t ii = static_cast<size_t>(i);
        uint32_t x = s.jobsB[ii] * u / cnt;
        s.jobsB[ii] -= x;
        if (i < 2) {
            uint32_t m = static_cast<uint32_t>(A_);
            if (m > u) m = u;
            x += m * 35u;
        } else if (i == 2) {
            uint32_t m = static_cast<uint32_t>(B_);
            if (m > u) m = u;
            x += (m * x / u) >> 1;
        }
        if (!(s.ordinances & 0x100)) x -= u;
        s.jobsB[ii + 1] += x;
        const uint32_t cc = s.jobsC[ii];
        const uint32_t y = cc * u / cnt;
        s.jobsC[ii] = cc - y;
        s.jobsC[ii + 1] += y - pollF;
        s.jobsA[ii] -= u;
        s.jobsA[ii + 1] += u;
    }
    if (g_econStopStage == 3) return;
    {
        const int32_t sum = static_cast<int32_t>(s.jobsA[5] + s.jobsA[6] + s.jobsA[7] + s.jobsA[8] + s.jobsA[4]);
        uint32_t k = static_cast<uint32_t>(sum / 300);
        const int rr = rmod(300);
        if (rr < sum - 300 * static_cast<int32_t>(pollF)) ++k;
        if (k != 0) {
            uint32_t m = static_cast<uint32_t>(C_);
            if (k < m) m = k;
            s.jobsC[0] += static_cast<uint32_t>(base - 0x23) * m + k * 35u;
            s.jobsB[0] += (k * s.cb4cc) / 5u;
            s.jobsA[0] += k;
            s.populationDecline += k;
        }
    }
    if (g_econStopStage == 4) return;
    if (s.populationDecline < s.populationGrowth) {
        s.populationGrowth -= s.populationDecline;
        const uint32_t chunk = (s.populationGrowth >> 4) + 1;
        while (s.populationGrowth != 0) {
            int cc = 4;
            for (size_t idx = 4; idx < 8; ++idx, ++cc) {
                const uint32_t take = chunk > s.populationGrowth ? s.populationGrowth : chunk;
                s.jobsC[idx] += static_cast<uint32_t>(0x41 - cc) * take;
                s.jobsB[idx] += static_cast<uint32_t>(0x5A - cc) * take;
                s.jobsA[idx] += take;
                s.populationGrowth -= take;
            }
            if (s.populationGrowth == 0) break;
            int c2 = 0, mult = 0x11;
            for (size_t idx = 0; idx < 12; ++idx, ++c2, mult += 0x23) {
                if (s.populationGrowth == 0) goto done;
                const uint32_t take = chunk > s.populationGrowth ? s.populationGrowth : chunk;
                s.jobsC[idx] += static_cast<uint32_t>(0x41 - c2) * take;
                s.jobsB[idx] += (idx < 3 ? static_cast<uint32_t>(mult) : static_cast<uint32_t>(0x5A - c2)) * take;
                s.jobsA[idx] += take;
                s.populationGrowth -= take;
            }
        }
    } else if (s.populationGrowth < s.populationDecline) {
        s.populationDecline -= s.populationGrowth;
        uint32_t R0 = s.populationDecline;
        while (s.populationDecline != 0) {
            for (size_t i = 0; i < 20; ++i) {
                if (s.populationDecline == 0) break;
                const uint32_t cnt = s.jobsA[i];
                if (cnt == 0) continue;
                const double nA = u64(cnt);
                const double ratio = static_cast<double>(static_cast<int32_t>(R0)) / u64(s.populationDecline + s.population10);
                uint32_t e = ftol(ratio * nA);
                if (e == 0 && (rnd() & 3) == 0) e = 1;
                if (cnt < e) e = cnt;
                if (s.populationDecline < e) e = s.populationDecline;
                if (e == 0) continue;
                const double ed = u64(e);
                const uint32_t cc = s.jobsC[i];
                s.jobsC[i] = cc - ftol((u64(cc) / nA) * ed);
                const uint32_t bb = s.jobsB[i];
                s.jobsB[i] = bb - ftol(ed * (u64(bb) / nA));
                s.jobsA[i] -= e;
                s.populationDecline -= e;
            }
            R0 = s.populationDecline;   // [ESP+0x18] is refreshed after every pass
        }
    }
done:
    if (g_econStopStage == 5) return;
    s.cb4cc = 0; s.ca428 = 0; s.ca478 = 0;
    for (size_t i = 4; i < 11; ++i) { s.ca428 += s.jobsC[i]; s.cb4cc += s.jobsB[i]; s.ca478 += s.jobsA[i]; }
    if (s.ca478 != 0) {
        s.ca428 /= s.ca478;
        s.cb4cc /= s.ca478;
        s.ca478 = (s.ca478 * 100u) / (s.population10 + 1u);
    }
}

}  // namespace sc2k
