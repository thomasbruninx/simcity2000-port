# Validation 004 — environment scan vs saved cities

**Method.** `sc2k-verify env SC2K/SIMCITY.EXE 0 <files>`: for each sample file build a `World`, clear the 0x08 marks (the power scan does this just before the phase), run `runEnvironmentScan` with inputs from `MISC` and the tables from `SIMCITY.EXE`, and compare every output layer with the saved one. **Important:** a save is only comparable at the right point of the 25-phase month. `MISC` word 4 (`day`) gives the phase (`day % 25`) the game had reached:
- phase 0–1: environment layers are from the previous month;
- **phase 2: layers were just recomputed → exact agreement expected for deterministic layers;**
- phases 3–19: zone growth (phases 3–18) has since changed buildings, so exactness degrades;
- phases ≥ 20: marks have been cleared by the water scan.

**Results.**
- **City centre** (MISC 1030/1031): ours == saved in 54/61 files (the rest were saved at phases where buildings changed after the scan).
- **Developed marks**: all 4096 cells equal in **39 of the 43 files saved at phases 2–19**; the four exceptions are TOMG-C1 (zero marks saved), CERES (phase 3), RETREAT (phase 10) and HERMAN (phase 15). Files saved at phase 0–1 or ≥ 20 have all marks cleared (expected: power scan at phase 1 and water scan at phase 20 clear them).
- **Phase-2 files (10 compared in the regression test): `XPOP` (density), `XPLC` (police), `XFIR` (fire) are 100.0% exact** in all of NYC, ATLANTA, BARCELON, CHARLEST, CHICAGO, DAVENPOR, HOLLYWOO, HOMESTEA, MALIBU, MANHATTA. This verifies the tables `population`@0x4E8638, the +2/+12 constants, the coverage spreader weights, funding formulas, power-halving, origin-flag rule, interior-only loop and the ordinance bonuses.
- Across all 61 files (any phase): `XPLC` exact in 53 files, `XFIR` in 52, `XPOP` in 19 (others differ only where buildings changed after the scan).
- **Not exactly reproducible** (depend on previous-month state that a save doesn't contain): `XPLT` mean exact 85%, mean |d| 0.21; `XVAL` mean exact 80%, mean |d| 2.1 (0.9–3.2 on phase-2 files); `XCRM` 90%, |d| 0.30; `XROG` 92.5%, |d| 0.31 — small, unbiased-looking differences: evidence of structural correctness, not of bit-exactness. The totals agree within ~1–3% (e.g. NYC pollution 47,495 vs 47,493; crime 34,171 vs 33,778).
- **Layer 0x4C8F70 = XTER** (STRONG): XVAL mean|d| 1.36 (XTER) vs 8.76 (XUND) on NYC; 1.39 vs 9.89 on Tokyo.

**Anomalies.**
- `CENTERVL.SC2`: saved `XPLC`/`XFIR` are entirely zero although 59 police and 41 fire stations are powered; every other layer fits. Unexplained (hand-edited/test city?).
- `MUNS-ISL.SC2` (phase 7): `XPLC` 71.9% — probably stations built after the scan.

**Phase-aware reading of the earlier water validation (003).** Saves at phases 1–19 contain water flags from the *previous* month's scan, so a changed pipe network between scans explains many diffs; 9 of 11 files saved at phase ≥ 20 (water scan fresh) match exactly. Two phase-20 saves (TOMG-C4, FOURCITI) still differ and remain unexplained (edits between the tick and the save are possible at any phase).
