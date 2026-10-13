# Newspaper, news queue, advisors and opinion poll

Port: `src/simulation/news.{h,cpp}` (grammar, queue), `advisors.{h,cpp}` (advisor advice, opinion poll), `src/app/info_windows.cpp` (windows). Differential test: `sc2k-emu-diff advisor` -- the advice selection of all eight advisors on 400 random city states and 300 opinion polls per city: identical (VERIFIED).

## News queue
Every `ZoneHooks::postMessage(id, arg)` of the simulation is the original's `FUN_0047BA90(id, arg)`: an entry (id, priority from DATA 1004[id], arg, three 0xFF cache bytes) is inserted into the first seven of nine 8-byte slots ordered by priority (new entries go in front of equal ones); `newsAge` (0x479F4B, from the monthly event phase) subtracts DATA 1005[id] from every priority and sorts. Slots 6..8 start with the fillers 0xB..0x13. Ids 3, 4, 5 and 0x24 make the newspaper pop up. A message id is also the number of the rule group of its story.

## Text grammar (0x47A700)
`DATA/DATA_USA.IDX` (little-endian id / offset pairs) and `.DAT` (big-endian) of the player's installation hold: 1000 / 1001 (250 rule groups: first rule and number of alternatives), 1002 (2500 rule offsets), 1003 (149 KB of NUL-terminated rules), 1004 / 1005 (priority and decay of the 80 message ids). The rules are compressed with a digraph table in the exe (@0x4E9218, byte -> rule). Tokens: `$` the message argument, `%N` / `<N` a random number 2..N (`<` cached per story), `&N` alternative `arg` of group N, `*N` random alternative of group N, `[N` random alternative cached per article, `@N` / `^N` cached per story, `=` city name, `>` label 0xFB + arg, `~` label 0, `-` paragraph break, `\x` literal, `+` ends the headline. The headline gets Title Case; the article gets sentence capitals. The original seeds the C library's `rand()` before each text (`srand(seed + 0x1c)` for the front headline, `srand(7k + seed)` for the side headlines, seed = start tick + 500 x page + day/25): the port uses its own `CrtRand` with the same seeds, so opening the paper does not disturb the simulation's random sequence (the original does).
The text parts are verified only by reading the code; the compressed data decode into the real stories ("Fire Cooks Charlie's Feed Store", "Presidente Threatened").

## Page layout
`reset` ports 0x479C00 (six pages x {paper name, layout, motto, topic, ...} shuffled with the RNG). The port draws one layout (name, date, main story with article, four side headlines, poll); the original's six layouts with photographs and fonts (rectangles tables at 0x4CC640 / 0x4E9188) are not reproduced.

## Opinion poll (0x4729F0)
100 random citizens each pick a concern from a roulette weighted by traffic, pollution, crime, 3 x tax rate, unemployment, education shortfall (100 - score) and health shortfall (70 - score); the land-value level + 50 is the weight of "no complaint". The number without complaint is the mayor's approval rating (@0x4E88F4, also written into the Mayor's House microsim record by 0x414600); the concerns are sorted and the top four with their counts are printed in the paper (strings 367..373, "Mayor Rated %d%%" 353). It draws from the shared `rand()` and runs when the paper opens.

## Advisors (FUN_00419E10)
Eight advisors (portraits RT_BITMAP 197..204) each map the indicators to an advice index k, string id 294 + k:
0 demand sum and taxes (11, 12, 13), 1 pollution / power / crime / ordinances (14..20), 2 finances and bonds (1, 2, 3, 4), 3 crime (5, 6, 7, 16), 4 fire coverage (8, 9, 10), 5 health coverage and ordinances (21..26, one `rand()`), 6 schools and colleges (27, 28, 29), 7 transport funding (30, 31, 32). The treasury is compared with the population as an **unsigned** number (a negative treasury gives "no advice" from advisor 0, as in the original). The budget projections (+0x6C of each budget record) are the budget window's `(11 - month) * cost * rate + year-to-date`. The advisors' titles in the window are the port's guesses (the exe holds only the portraits).
