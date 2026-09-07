# Versioned builds and source

Every numbered directory is a self-contained historical iteration:

- `build/` contains only the runnable Windows distribution and licenses.
- `source/` contains the expanded source snapshot used for that build.

The folders are numbered chronologically so GitHub sorts them in development
order. Do not place a ROM or user save inside these checked-out directories.

Latest iteration: 12-source-informed-native-v4.6

- Compatibility-renderer play remains authoritative and is the default.
- The optional native renderer now uses source-informed road scale and ordered
  live wheel pose while keeping private original source out of the repository.
- Matching archive: releases/windows/12-source-informed-native-v4.6-windows.zip
