# Versioned builds and source

Every numbered directory is a self-contained historical iteration:

- `build/` contains only the runnable Windows distribution and licenses.
- `source/` contains the expanded source snapshot used for that build.

The folders are numbered chronologically so GitHub sorts them in development
order. Do not place a ROM or user save inside these checked-out directories.

Latest iteration: 09-responsive-smoothing

- Compatibility-renderer play remains authoritative.
- Responsive Smooth mode reduces the visual input lag of the earlier 50/50
  blend while preserving the original simulation and controls.
- Matching archive: releases/windows/09-responsive-smoothing-windows.zip
