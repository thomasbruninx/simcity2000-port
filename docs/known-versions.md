# Known versions

| Edition | File | PE timestamp | SHA-256 | Source of identification | Status |
|---|---|---|---|---|---|
| **1995 CD Collection, build .022** (this project's supplied copy) | `SIMCITY.EXE` | `0x302FEA8A` | `c2f735be…ea1b062c33b` (full hash in [executable-analysis.md](executable-analysis.md)) | Version resource (PrivateBuild .022), `VERSION.INF` (8/14/95), and sc2kfix timestamp table | **Target** |
| 1996 Special Edition | `SIMCITY.EXE` | `0x313E706E` | – (not supplied) | sc2kfix `dllmain.cpp` | Not available; sc2kfix's address tables target it |
| 1995 Interactive Demo | – | `0x3103B687` | – | sc2kfix | Not available |
| SCURK 1995 ("primary") | `WINSCURK.EXE` | `0xBC7B1F0E` | `fb8fdcbe…f2088467fc` | Matches sc2kfix constant | Supplied |
| SCURK 1996 (Network Edition) | – | `0x6C261F75` | – | sc2kfix | Not available |

Supplied SCURK DLL `SCURKUSA.DLL` has an anomalous timestamp (2019); see [inventory notes](original-file-inventory.md).

Planned `identifyGameVersion(path)`: hash `SIMCITY.EXE` (SHA-256) and fall back to PE timestamp; refuse unknown hashes with a clear message (Phase 22/23). Regional variants: only a `USA` data set (`DATA_USA`, `TEXT_USA`, `SC2USA.HLP`) is present. Other regions are UNKNOWN.

**Important consequence:** the community patch (sc2kfix) deprecates this 1995 edition and only ships a handful of hooks for it. Most of its documented fixes (military bases, rail neighbour connections, bond system…) were developed against the 1996 SE. Whether the same bugs exist in build .022 must be verified per-bug in our own binary.
