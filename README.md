# Stunt Race FX Enhanced Windows

Stunt Race FX Enhanced Windows is a ready-to-run Windows distribution of the Stunt Race FX static recompilation project. It packages the required runtime, launchers, and corresponding source code to make setup and play on modern Windows systems straightforward.

An experimental Windows enhancement project built around the original Stunt Race FX game logic, with a compatibility renderer and an optional native-track research path.

This repository does not include the original game ROM, any commercial ROM, save data, save state, replay, capture, or ROM-derived export, or any copyrighted Nintendo assets. You must provide your own legally obtained USA Rev 1 ROM at runtime.

## Latest build

[17-wide-objects-v4.11](source/17-wide-objects-v4.11/build)
adds original texture-mapped polygon packets to the expanded view and a guarded
edge-sprite compositor, with the race-window bezel excluded from the wider world.
The center remains original-resolution.
Use either **Play True Wide Preview** launcher; Ctrl+W toggles the extension.
Fully offscreen objects that the Super FX code rejects before face submission,
including some rival cars, remain pending. See the included research notes.
The experimental native renderer is unchanged from version 12.

[Download the latest Windows build](releases/windows/17-wide-objects-v4.11-windows.zip)

## Version history

| Version | Focus |
| --- | --- |
| [17-wide-objects-v4.11](source/17-wide-objects-v4.11) | Original textured side polygons, guarded edge sprites, and crisp-center architecture findings |
| [16-wide-cleanup-v4.10](source/16-wide-cleanup-v4.10) | Side palette/brightness, border-line removal, texture sampling and cache-lifetime cleanup |
| [15-compat-wide-v4.9](source/15-compat-wide-v4.9) | First expanded-world compatibility preview; displayed-bitmap synchronization and GPU side geometry |
| [14-compat-draw-stream-v4.8](source/14-compat-draw-stream-v4.8) | Original polygon capture and optional latest/previous outline comparison |
| [13-compat-geometry-probe-v4.7](source/13-compat-geometry-probe-v4.7) | Playable compatibility baseline with opt-in geometry diagnostics; no new visual rendering yet |
| [12-source-informed-native-v4.6](source/12-source-informed-native-v4.6) | XLR8-informed road scale, wheel pose, camera, and native ordering |
| [11-compat-performance-v4.5](source/11-compat-performance-v4.5) | Compatibility profiling, stable frame pacing, and hot-path cleanup |
| [10-sprite-safe-smoothing](source/10-sprite-safe-smoothing) | 2D-safe smoothing and complete sprite animation |
| [09-responsive-smoothing](source/09-responsive-smoothing) | Lower-latency compatibility smoothing |
| [08-native-track-v4.2-road-vehicle](source/08-native-track-v4.2-road-vehicle) | Road-width and vehicle alignment |
| [07-native-track-v4.1-stability](source/07-native-track-v4.1-stability) | Native-renderer stability |
| [06-native-track-v4.0](source/06-native-track-v4.0) | First native track reconstruction |
| [05-n64-style-v3](source/05-n64-style-v3) | N64-style materials and presentation |
| [04-modern-playable-v2](source/04-modern-playable-v2) | Modern display and control options |
| [03-enhanced-playable-v1](source/03-enhanced-playable-v1) | First enhanced playable build |
| [02-compatibility-renderer](source/02-compatibility-renderer) | Known-good compatibility renderer |
| [01-milestone-2](source/01-milestone-2) | Second porting milestone |
| [00-milestone-1](source/00-milestone-1) | Initial milestone |

Each version keeps its runnable files in build/ and the exact expanded source in source/. Build-only Windows archives are under releases/windows/.
