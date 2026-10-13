# Finance (phase 0, VA 0x473980) and ordinance costs (0x42B590)

Status: **ported** (`src/simulation/finance.cpp`), from decompilation, with exact checks against saved cities.

## Original implementation
- Budget record (16 categories, see map-layers doc): zone categories hold `currentCosts = population·10` and `fundingPercent = tax rate`; other categories hold maintenance units and funding %.
- **Settlement:** when the year flag (`MISC 911`, armed in December) is set and the month is January: `treasury += yearToDate / (divisor[i]·12)` for each category (`divisor` table @0x4E8EA0 = `75,75,75,75,−100,−1,−1,−2,−4,−1,−1000,−500,−400,−250,−250,−250`), then `yearToDate = 0`, flag cleared. (UI side effects — title refresh, "bankrupt" menu state — are not ported.)
- **Monthly roll:** for each category `countMonth[m] = currentCosts`, `fundMonth[m] = fundingPercent`, `yearToDate += currentCosts·fundingPercent`; in December set the year flag.
- **Maintenance units** (from the tile census `c[id]`): bonds = debt (`MISC 6`); police `c[0xD2]/9`; fire `c[0xD3]/9`; health `c[0xD1]/9`; schools `c[0xD6]/9`; colleges `c[0xD9]/16`; roads = Σ c[0x1D–0x2B, 0x3F–0x46, 0x4B, 0x4C, 0x5D–0x60] + (c[0xEC]/4)·250; rail = Σ c[0x2C–0x3E, 0x45–0x48, 0x4D, 0x4E, 0x6C–0x6F] + c[0xED]; bridges = Σ c[0x51–0x5C, 0x6A, 0x6B]; highways = Σ c[0x49–0x50, 0x61–0x6B]; tunnels = Σ c[0x3F–0x42]; subways = c[0xE9] + `MISC 1018`; ordinances = Σ over enacted bits of `ordinanceCost(bit)`.
- **Ordinance cost table** (`switch`, VERIFIED): costs/incomes are fractions of the R/C/I populations·10 (e.g. #0 = commercial, #1 = residential, #2 = 2×commercial, #4 = −residential/3, #13 = −industrial, #16 = −(extraPop + population10)).
- **Random enactment:** if `MISC 1024 == 0` and `rand()&7 == 0` and `(rand()&0xFFFF) + 50000 < treasury`, enact ordinance `rand()%20` (message 0x29). (Why the game does this is UNKNOWN — possibly the "city council" feature.)

## Validation (docs/validation/005)
- **All maintenance categories 3–15 reproduce exactly** from the census in all six files saved at phase 0 (LAKELAND, PARIS.SCN, TOMG-B1..4), including the ordinance sum — verifies class ranges, divisors, `MISC 1018`, and the ordinance cost table.
- `yearToDate == Σ_{m≤month} countMonth[m]·fundMonth[m]` in 48/61 files (the 13 exceptions are mostly hand-edited scenarios).
- `CityState::writeBack` round-trips all 61 MISC arrays byte-exactly.

## Open
Which function consumes `budget.currentCosts·funding` outside January (e.g. UI projections); the unexplained unknown call `0x44DD10` at settlement; whether `treasury` also changes elsewhere monthly (bond interest?).

## Bonds (session 14)
`src/simulation/bonds.*`: each bond is $10,000, at most 50 (`MISC 6` = number of bonds, `MISC 388..437` = interest rate of each, oldest first). A new bond gets rate = federal rate level + 1 and is only offered while `(bonds * 2500) / (cityValue + 1) < 6`, where `cityValue` (0x469BF0, **verified identical** to the original on 12 runs via `sc2k-emu-diff value`) is the sum over the tile census of building / road / plant values read from the exe's cost tables. Issuing adds $10,000 and a rate; repaying always takes the oldest bond (needs $10,000). `budget[Bond]` holds `currentCosts = bonds` and `fundingPercent = sum(rates) * 10000 / bonds`, so the yearly interest is `bonds * fp / 100` (divisor -100) = 1 % of $10,000 per rate point. The new-city setup 0x42DDE0 (start funds 20000/10000/10000 by difficulty, hardest gets one bond at rate 3, event years = table + rand%20) is documented in the journal but not ported.
