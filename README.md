# Solaris Silverline

Solaris Silverline is a C++20/JUCE guitar amplifier and effects-suite project focused on a small, high-quality signal chain rather than a large collection of generic models.

Current development version: **0.3.0 integration foundation**.

## Current signal path

```
Input
  -> input gain / calibration
  -> PRE FX slot
  -> amp engine
  -> cabinet IR engine
  -> POST FX slot
  -> 4-band parametric EQ + HPF/LPF
  -> output gain
```

The PRE FX and POST FX slots are intentionally transparent in v0.3. Their UI exists, but the pedal DSP implementations are the next development phase.

## Amp engine

The amp layer is model-agnostic through `IAmpModel` and `AmpRegistry`.

Two backends are supported:

- **Silverline 68 Analogue DSP** - the default backend. It uses oversampled nonlinear processing, dynamic/sag-inspired behaviour, tone shaping, tremolo and a compact spring-style reverb prototype.
- **Neural NAM backend** - optional NeuralAudio integration for loading Neural Amp Modeler (`.nam`) captures without replacing the analogue backend.

Neural models are loaded and pre-warmed outside the real-time audio callback. The audio thread only processes already-prepared models. The current neural backend requires the NAM model sample rate to match the host sample rate exactly; dedicated arbitrary-rate conversion is planned instead of silently running a model at the wrong rate.

No NAM capture is bundled yet. This avoids shipping a third-party capture with unclear redistribution/model rights.

## Cabinet

The cabinet engine provides two independent JUCE convolution slots with:

- mono or stereo IRs
- Mic A / Mic B blend
- phase inversion
- wet/dry control
- preset-state persistence for external IR paths

IR loading is a non-audio-thread operation. The processing callback only performs convolution and mixing.

The UI already presents a 2x10 dual-mic concept. Position and distance controls remain disabled until a real multi-IR interpolation/morphing layer exists.

## EQ

Post-rig EQ includes:

- HPF
- LPF
- four parametric peaking bands
- frequency / gain / Q parameters
- smoothed coefficient transitions
- draggable UI nodes tied to host-automatable parameters

The spectrum line in the current UI is decorative; a real FFT analyser is a later phase.

## Tuner

The tuner engine runs analysis on a worker thread and publishes a lightweight thread-safe snapshot to the UI. The overlay displays detected note, frequency, cents and confidence. Tuner mute clears the processed output while analysis continues from the input signal.

## UI

The current UI is a modular dark/silver/amber design with dedicated pages for:

- PRE FX
- AMP
- CAB
- POST FX
- EQ

The AMP and EQ controls are bound to APVTS parameters so automation and session recall stay in sync with the engine.

## Presets / state

Plugin state uses a versioned preset wrapper with `schemaVersion`, amp model ID, APVTS parameters, cabinet state, EQ state and a reserved pedal-state section.

## Build

Requirements:

- CMake 3.24+
- C++20 compiler
- Git
- internet access during initial dependency fetch

Windows:

```powershell
./scripts/configure.ps1
./scripts/build.ps1
```

Or directly:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target SolarisSilverline_VST3
```

Linux:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target SolarisSilverline_VST3 --parallel
```

NeuralAudio/NAM can be disabled for a lighter development build:

```bash
cmake -S . -B build -DSOLARIS_ENABLE_NEURAL_AUDIO=OFF
```

## Development status

Working foundation:

- modular amp registry
- Silverline 68 analogue DSP
- optional NeuralAudio/NAM backend
- dual-slot cabinet convolution
- 4-band post EQ
- worker-thread tuner
- versioned state/preset infrastructure
- premium modular UI shell
- VST3 + Standalone targets

Next priorities:

1. production PRE FX and POST FX pedal DSP
2. bundled/licensed Silverline 2x10 IR pack
3. NAM model import/browser UI and model metadata
4. sample-rate conversion strategy for neural models
5. real spectrum analyser and input/output metering
6. preset browser and factory presets
7. Windows plugin validation, performance profiling and installer

## Third-party dependencies

See `THIRD_PARTY_NOTICES.md`.

Before distributing a proprietary/commercial build, verify that the selected JUCE licensing terms and every bundled model/IR asset permit that distribution.
