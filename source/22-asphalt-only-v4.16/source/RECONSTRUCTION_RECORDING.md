# Reconstruction session recording (retained in v4.16)

## Purpose

The recorder preserves the runtime evidence needed to reconstruct courses,
objects, materials, and sprite relationships after a play session. It is a
read-only observer: gameplay, physics, Super FX timing, input, and compatibility
rendering are unchanged.

Use `Record Reconstruction Session 3440x1440.cmd`. Recording begins before the
first emulated frame and stops cleanly when the program closes. `Ctrl+Shift+R`
can stop or begin a new session manually.

Inside the canonical checkout, each run is stored under:

```text
recordings/session-YYYYMMDD-HHMMSS-pPROCESSID/
  manifest.json
  frames.csv
  session.srfxrec
```

The root `recordings/` directory is protected by `.gitignore`. These files are
ROM-derived personal research data and must not be committed or distributed.

## Captured channels

The binary container records frame timing, controller input, scene/race flags,
source dimensions, and changed values for these channels:

| Tag | Contents |
| --- | --- |
| `POLY` | Original projected polygon stream |
| `CAMR` | Original camera-space polygon packets |
| `DISP` | Display-synchronized/interpolated camera packets |
| `CGRM` | SNES color RAM |
| `COLR` | Decoded live compatibility palette |
| `PPUR` | Relevant live PPU registers |
| `VRAM` | Complete SNES VRAM |
| `DMAS` | Captured Super FX bitmap-transfer metadata |
| `BKGD` | Reconstructed extended background layer |
| `WREF` | Reconstructed center world reference |
| `SPRT` | Identified extended 2D sprite layer |
| `GRAM` | Super FX RAM |
| `GREG` | Super FX register snapshot |
| `GTRC` | Per-frame Super FX instruction trace buffer |
| `RWRT` | Relevant RAM-write trace buffer |
| `WRAM` | Complete SNES work RAM |
| `WDEL` | Lossless changed 256-byte WRAM pages after the initial full copy |
| `FBUF` | Authoritative decoded compatibility framebuffer |
| `HOVR` | Separated HD HUD/sprite/unsupported-primitive overlay |
| `STAT` | Exact serialized emulator checkpoint |

`STAT` is written on the first recorded frame and every 300 frames by default.
The interval can be changed with `SRF_RECORD_CHECKPOINT_INTERVAL`. All other
channels are losslessly deduplicated: a missing tag in a frame means its previous
value remains active. Changed chunks use Windows XPRESS Huffman compression when
that produces a smaller payload. No lossy image/video compression is used.

The game ROM itself is never copied into the recording. The manifest stores its
byte size and FNV-1a fingerprint so a session can be matched to the user's own
legal ROM later.

## Inspection and extraction

Verify and summarize a finished recording:

```text
python source/22-asphalt-only-v4.16/source/tools/recording/inspect_recording.py recordings/<session>/session.srfxrec --verify
```

Reconstruct and extract the complete accumulated state at a chosen frame:

```text
python source/22-asphalt-only-v4.16/source/tools/recording/inspect_recording.py recordings/<session>/session.srfxrec --verify --frame 2500 --output <folder>
```

Extraction is optional and should also be directed to an ignored/local folder.
The packed session is the canonical reference copy.

## Storage and performance

The format prioritizes exact reconstruction evidence over small files. Session
size varies with scene activity and can grow substantially during a full race.
Use a drive with several gigabytes free for long multi-track sessions. The
recorder flushes both files every 60 frames and writes an `END!` footer on clean
shutdown, allowing the inspection tool to detect interrupted recordings.

A deterministic 2,500-frame first-track session occupied approximately 93 MB
after lossless compression, compared with 1.09 GB before compression. At
3440x1440 with recording active, the same run averaged 16.5987 ms delivered
frame time, a 59.38 FPS rolling 1% low, and an 18.9557 ms worst frame.
