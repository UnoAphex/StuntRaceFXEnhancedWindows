# Stunt Race FX Enhanced Windows

An experimental Windows enhancement project built around the original Stunt
Race FX game logic, with a compatibility renderer and an optional native-track
research path.

No commercial ROM, save data, save state, replay, capture, or ROM-derived
export is included. Supply your own legally obtained USA Rev 1 ROM at runtime.

## Latest build

[09-responsive-smoothing](source/09-responsive-smoothing/build) reduces the
presentation delay of Smooth mode while retaining the original compatibility
renderer, physics, input, collision, timing, and game speed.

[Download the latest Windows build](releases/windows/09-responsive-smoothing-windows.zip)

## Version history

| Version | Focus |
| --- | --- |
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

Each version keeps its runnable files in build/ and the exact expanded source in
source/. Build-only Windows archives are under releases/windows/.