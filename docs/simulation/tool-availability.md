# Tool availability by year (0x469A10)

`ToolAvailability` (src/simulation/availability.*) ports the set-up routine run when a city is loaded; **verified identical** against the original code on 3 variants x 3 cities (`sc2k-emu-diff avail`).
Base masks (one word per tool category, bit = subtype button) come from the exe (`@0x4E8550`); every dated event that already fired (`eventYears[i] == 0`, MISC 462..478) is applied again, and the tick applies a newly fired event through `ZoneHooks::scheduledEvent(i)` (the original also posts a news message).

| event | unlock |
|---|---|
| 0, 1, 2, 3, 4, 5 | plant mask bits 3, 4 (nuclear — only without the Nuclear Free Zone ordinance 0x20000), 6, 5, 7, 8: gas, nuclear, solar, wind, microwave, fusion (base mask 7 = coal, hydro, oil) |
| 6 | airport zone (category 8 bit 1) |
| 7 / 8 | highway + onramp / bus depot (category 6 bits 1,3 / 4) |
| 9 | subway, subway station, rail-subway link (category 7 bits 1,3,4) |
| 10 / 11 | water treatment / desalination (category 4 bits 3 / 4) |
| 12–15 | arcology designs 1..4 (count in `@0x4CCB44`) |
| 16 | nothing |

Plant bit order matches the plant descriptions 1000–1008 of TEXT_USA.DAT (coal, hydro, oil, gas, nuclear, wind, solar, microwave, fusion) and the plant costs of the toolbar table. Category 2 (emergency dispatch) is enabled by 0x44F290 when a disaster starts (police / fire / military counts; `@0x4CB4FC` is the military base type). The reward buildings 0..4 are granted by the city council (0x441800 flags), which is not modelled: they are always enabled in the port.
