# Responsive smoothing v4.3

## Why the old Smooth mode felt heavy

The original compatibility path produces scene updates at the game’s own rate
while the frontend presents at approximately 60 Hz. The earlier temporal blend
started each new scene frame at an even 50/50 mix with the previous frame. That
made camera and vehicle movement look softer, but it also retained too much old
visual information immediately after a steering input. Physics and input were
still correct; the delayed presentation made the response feel stiff.

## New blend policy

- A new scene update begins 75% toward the newest frame.
- While left or right is held, it begins 87.5% toward the newest frame.
- The next repeated presentation advances to the fully current frame.
- The HUD and central player/control-feedback region always use current pixels.
- Strongly changing, high-contrast pixels also use the current frame to reduce
  doubled vehicle and scenery silhouettes.

This keeps mild temporal smoothing in the background and road motion while
prioritizing immediate control feedback. It does not change controller polling,
physics, collision, simulation timing, game speed, or the Super FX clock.

## Limitations and comparison

This is a low-latency temporal blend, not optical-flow frame generation. It
cannot produce a physically correct in-between pose that the original game did
not render. Press M during play to compare responsive smoothing on and off.
The dedicated Play Responsive Smooth 60 FPS.cmd launcher selects the known-good
compatibility renderer and the correct 60.1 Hz presentation cap.