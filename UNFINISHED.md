# Unfinished

## Product decision

- Decide whether to remove `ao_starfish` completely and standardize webOS on
  Starfish video plus ALSA audio. The legacy AAC encoder is gone; the smaller
  PCM-only output remains available because removing a user-visible audio mode
  is a product decision.

## Hands-on TV verification

- Judge talking-head lip-sync at both 24 fps and 60 fps, then perform the full
  20-seek rapid-scrub and early pause/resume torture pass. Automated logs show
  stable split-clock convergence and clean seek/pause teardown, but cannot
  replace the subjective check.
- Select **Starfish** audio once and compare audible lag against the logged
  `fed_a - clock` lead with the new 0.4 second feed window. The current saved TV
  setting is ALSA, and changing user preferences unattended was intentionally
  avoided.
