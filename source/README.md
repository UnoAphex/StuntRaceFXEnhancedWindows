# Versioned builds and source

Every numbered directory is a self-contained historical iteration:

- `build/` contains only the runnable Windows distribution and licenses.
- `source/` contains the expanded source snapshot used for that build.

The folders are numbered chronologically so GitHub sorts them in development
order. Do not place a ROM or user save inside these checked-out directories.

Latest iteration: 17-wide-objects-v4.11

- Compatibility-renderer play remains authoritative and is the default.
- Opt-in expanded-world side viewports with GPU geometry and original background tiles.
- Original center and HUD remain unstretched; preview limitations are documented.
- Experimental native renderer unchanged from version 12.
- Original textured polygon packets and conservative edge-crossing OBJ composition.
- High-resolution center and objects culled before polygon submission remain pending.
- Matching archive: releases/windows/17-wide-objects-v4.11-windows.zip
