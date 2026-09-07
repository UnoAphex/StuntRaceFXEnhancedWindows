# Versioned builds and source

Every numbered directory is a self-contained historical iteration:

- `build/` contains only the runnable Windows distribution and licenses.
- `source/` contains the expanded source snapshot used for that build.

The folders are numbered chronologically so GitHub sorts them in development
order. Do not place a ROM or user save inside these checked-out directories.

Latest iteration: 10-sprite-safe-smoothing

- Compatibility-renderer play remains authoritative.
- Sprite-Safe Smooth mode preserves small 2D animation updates while retaining
  low-latency smoothing for broad track and camera motion.
- Matching archive: releases/windows/10-sprite-safe-smoothing-windows.zip
