# Versioned builds and source

Every numbered directory is a self-contained historical iteration:

- `build/` contains only the runnable Windows distribution and licenses.
- `source/` contains the expanded source snapshot used for that build.

The folders are numbered chronologically so GitHub sorts them in development
order. Do not place a ROM or user save inside these checked-out directories.

Latest iteration: 19-session-recorder-v4.13

- Compatibility-renderer play remains authoritative and is the default.
- Dedicated reconstruction launcher records complete timestamped runtime-data
  sessions into the root-level ignored `recordings/` directory.
- Lossless chunk deduplication, WRAM page deltas, and XPRESS Huffman compression
  keep full sessions practical while preserving exact source data.
- Included inspection tool verifies containers and reconstructs all accumulated
  channel state at any recorded frame.
- v4.12 output-resolution center/side geometry and separate original HUD/sprite
  composition remain intact.
- Experimental native renderer unchanged from version 12.
- Matching archive: releases/windows/19-session-recorder-v4.13-windows.zip
