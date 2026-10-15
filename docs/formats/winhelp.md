# Windows help files (SC2USA.HLP, SCURK.HLP)

Read by `src/resources/winhelp.*` (text and hyperlinks only; pictures are skipped) and shown by `src/app/help_viewer.*`. Both files are help format 3.1 (`|SYSTEM` minor 21); SC2USA.HLP is LZ77-compressed with a `|Phrases` table, SCURK.HLP is not compressed.
Confidence: VERIFIED = the port reads all 167 / 126 topics with their titles in the order of `|TTLBTREE`, 80 % or more of the hyperlinks resolve; the layouts below follow the public descriptions of the format (Pete Davis' articles, HelpDeco).

* **Container**: `3F 5F 03 00`, directory offset, free list, file size. The directory is a B+ tree (38-byte header `0x293B`, pages of 1024 bytes) of (name, offset); each internal file starts with `reserved u32, used u32, flags u8`.
* **`|TOPIC`**: blocks of 4096 bytes; each has a 12-byte header, then (LZ77 when `|SYSTEM` flags are 4 or 8) the record stream. Records: `TOPICLINK` (`blockSize, dataLen2, prev, next, dataLen1, type`) + LinkData1 + LinkData2. A record never starts in the last bytes of a block (zero padding); it may continue in the next block. Type 2 starts a topic (title in LinkData2), type 0x20 is a paragraph.
* **LZ77**: flag byte, per bit (low first) a literal or a 16-bit copy (12-bit distance - 1, 4-bit length - 3).
* **Phrases** (`|Phrases`): `count u16, 0x100, decompressed size u32`, `count + 1` offsets, then the (LZ77) phrase bytes. In LinkData2 a byte 1..15 followed by `n` selects phrase `(256 * (b - 1) + n) / 2`, plus a space when the index is odd.
* **Paragraph record**: LinkData1 = compressed long, compressed word, 4 skipped bytes, a flags word and the optional spacing / indent / border / tab fields, then formatting commands; LinkData2 holds NUL-terminated text pieces. The loop outputs one piece, then one command: `80` font (3 bytes), `81` line break, `82` paragraph end, `83` tab, `89` end of hotspot, `E0..E3, E6, E7` hotspot start (4-byte context hash; 5 bytes), `86..88` picture (skipped), `C8 CC EA EB EE EF` length-prefixed (skipped), `FF` end.
* **Links**: `|CONTEXT` is a B+ tree hash -> topic offset; the offsets of `|TTLBTREE` in ascending order are the topics in stream order, so a hash resolves to the last topic whose offset is not greater.
* Not read: pictures, tables (record 0x23; none in these files), keywords (`|KWBTREE`), macros.
