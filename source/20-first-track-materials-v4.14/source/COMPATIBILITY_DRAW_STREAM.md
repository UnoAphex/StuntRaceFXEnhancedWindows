# Original polygon draw-stream inspection v4.8

This version captures an original polygon submission format and provides an
optional GPU outline overlay in the compatibility frontend. It does not yet
replace polygon rasterization, sharpen the normal game image, or provide true
ultrawide. Experimental native renderer code is unchanged.

## Findings verified against retail execution

Longer observation distinguishes the bank-07 pixel sites sampled in v4.7 from
the main bank-01 drawing pass. Frame 3298 of the clean-start scripted run visits
bank/R15 01:AA49 28421 times, including 27776 PLOT operations. A single sampled
presentation frame can entirely miss this lower-rate scene update.

The USA Rev 1 code at 01:A950 consumes a RAM draw list, decodes a count and
color, and prepares a polygon scan converter. The observed pipe state at
bank/R15 01:A971, opcode B1, exposes R12=count, R3=count*4 and R1=coordinate
pointer. The observer validates those relationships and RAM bounds before
reading. Frame 3298 yielded 21 polygons without invalid or dropped records.

This resembles the count/coordinate scan converter in the supplied development
source MDRAWP.MC, but the retail addresses and list format were established
from the retail ROM and execution, not assumed from development labels.
No private source or extracted data is included in the distribution.

The scan converter reads low bytes of word X/Y coordinates. This confirms a
precision loss before pixel filling for this path. These are already projected
screen coordinates, with observed clipping at the viewport boundaries. They
cannot recover extra scenery or fractional projection on their own. Other
polygon, line, sprite and textured drawing paths remain to be mapped.

## On-screen inspection

Use Inspect Original Polygon Stream.cmd or press F8 during compatibility play.
F8 cycles latest submission, previous submission, off. Cyan outlines are drawn
by SDL at output resolution over the unchanged original image. This is a
diagnostic overlay, not a quality preset. It is unavailable in native/hybrid
modes. Turning it off stops capture and clears retained records.

The overlay uses an experimental first-course placement of (24,32) within the
SNES image. It does not know SNES layer/window priority and can cross HUD or
sprites. Latest/previous submission comparison exposes timing discrepancies;
neither is certified as the displayed framebuffer's owner. Records expire
after 12 callbacks without a new polygon submission. Do not use the overlay as
evidence of correct final composition or judge gameplay quality with it enabled.

Normal play defaults off and retains the established motion/sprite behavior.
Live outline capture omits the expensive instruction histogram. It remains a
debug feature; no latency or 60 FPS guarantee is made for inspection mode.

## Offline captures

The benchmark accepts -CaptureFrame, -CaptureFrames (1..120), -GeometryProbe,
and -DrawStream with -DrawStreamPhase latest/previous. Use temporary output
outside the repo. CaptureFrames expands GSU observation backwards from the
specified frame; the final presented image is captured only at CaptureFrame.

Schema version 2 extends status to six u32 values: version, missed histogram
samples, missed register snapshots, observed instructions, missed polygons,
invalid polygons. Existing histogram/register formats remain unchanged.
The new geometry_polygons.bin is a sequence of 136-byte records: 68 little-endian
u16 values, count/color/RAM pointer/RAM bank followed by up to 32 X/Y pairs.
Unused pairs are zero. Count is limited to 3..32, storage to 2048 polygons.
An empty submission produces no polygon file. R15 is a pipe register, not
necessarily the current instruction address.

view_draw_stream.py produces an SVG diagnostic from a polygon capture, preserving
RAM IDs and raw colors in tooltips. Output coordinates follow low-byte scan
conversion. This export is ROM-derived and must stay outside the repository.

## Validation and remaining work

The scripted race completes with capture and live overlays. Latest/previous
screenshots visibly show the submitted outlines, but alignment and display
ownership remain incomplete. In particular, this is not yet a faithful
replacement for all vehicle/terrain rendering. Normal output and game-memory
hashes matched v4.7 at frame 3300: presented image, GSU RAM, WRAM and GSU
registers. The final 3400-frame normal-path test completed, averaging 3.0570 ms
and peaking at 16.0850 ms at 512x384, uncapped, audio off. This is throughput
testing with capture overhead, not certification of 60 FPS pacing or audio.

Next: find the writer of these records, retain pre-clipping/fractional vertices,
map the remaining draw types, associate lists with the correct displayed
buffer, and resolve sprite/HUD window composition. Only then replace the
original polygon rasterization and expand culling/projection safely.
