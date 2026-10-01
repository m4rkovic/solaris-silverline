# Solaris Effects DSP

This directory contains product-agnostic real-time effect interfaces, chain/state infrastructure, and the original Solaris pedal algorithms.

Signal order in Silverline remains:

`INPUT -> PRE FX -> AMP -> CAB -> POST FX -> EQ -> OUTPUT`

## Real-time contract

- No heap allocation, file I/O, or locking in effect `process()` callbacks.
- Buffers and delay lines are allocated in `prepare()`.
- Continuous controls use smoothing.
- Effect bypass is crossfaded and, for oversampled nonlinear effects, the dry path is latency-aligned.
- Mono and stereo are supported.
- `EffectChain` stores effect order separately from effect ownership so UI drag/reorder can be added without rewriting DSP classes.
- APVTS stores effect bypass/control automation; the preset `PEDALS` tree stores PRE/POST order.

## PRE FX topology notes

- **Leveler**: OTA-style gain-control behaviour driven by a rectified/envelope-following detector, with attack/release character and gentle output coloration.
- **Turbo Drive**: frequency-selective feedback-style overdrive. Upper guitar frequencies are driven harder, clipping is asymmetric, and a small direct component preserves pick dynamics.
- **Badland Dist**: cascaded transistor/op-amp-inspired nonlinear stages with broad three-band contour shaping.
- **Vermin Drive**: high-gain, band-limited amplifier into sharp diode-style clipping followed by a one-knob low-pass filter.
- **Void Fuzz**: two consecutive band-limited clipping stages followed by a passive-style low/high tone blend.

The four nonlinear drive/fuzz effects use 4x JUCE polyphase-IIR oversampling. Their clipping functions are intentionally different rather than variants of one waveshaper.

## POST FX topology notes

- **Orbit**: six-stage modulated first-order allpass phaser with feedback.
- **Chorus**: stereo phase-offset LFO driving cubic-interpolated fractional delay lines.
- **Pulse**: bias-style amplitude modulation with a continuously variable sine-to-triangle shape.
- **Echo 404**: cubic-interpolated variable delay with smoothed time changes, filtered/modulated feedback, light feedback-path saturation, and stereo cross-feed.
- **Sanctum**: hybrid spring/space algorithm using short dispersive Schroeder allpasses around damped parallel feedback delay lines. It does not use `juce::Reverb`.

## Design references

The implementations here are original Solaris code. No GPL/BYOD source is copied.

The design work follows standard circuit/DSP concepts documented in sources such as ElectroSmash circuit analyses (OTA compressor, feedback overdrive, hard-diode distortion, two-stage sustaining fuzz), Schroeder allpass reverberation literature, spring-reverb dispersion research by Abel/Berners/Costello/Smith, and standard cubic fractional-delay interpolation.

No additional third-party dependency is introduced by this effects implementation.
