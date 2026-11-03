# Reduce VCO pitch range

## Why
Operators need useful pitch travel with preserved 110/220/330-Hz starting pitches.

## What Changes
- Set the continuous exponential VCO pitch range to 27.5–1760 Hz (A0–A6).
- Preserve registered starting pitches through reset.
- Verify mapping and running phase travel at 44.1, 48 and 96 kHz.

## Impact
- App DSP and parameter defaults, their regressions, and operator documentation.
- The independent ring carrier retains its 20–5000-Hz range.
