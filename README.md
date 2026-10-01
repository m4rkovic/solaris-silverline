# Solaris Silverline

**Solaris Silverline** is a boutique guitar amp/effects VST project built with C++20 and JUCE.

The first rig targets an American silver-panel, vintage clean / edge-of-breakup character. The architecture is intentionally modular so additional amp models can be added later without rewriting the plugin shell.

## Planned UI

Top navigation:

`PRE FX | AMP | CAB | POST FX | EQ`

Persistent utilities:

`Preset Browser | Tuner | Input | Output | Settings`

### PRE FX
- Compressor inspired by the Boss CS-2 control philosophy
- Turbo-style overdrive
- 3-band analog-style distortion
- RAT-style drive
- Big Muff-style fuzz

### AMP
- Silverline 68
- Future amp models are loaded through a generic amp interface/registry

### CAB
- 2x10-focused cabinet workflow
- Two movable microphones
- Position / distance / blend / phase
- Custom IR loading

### POST FX
- Phaser
- Chorus
- Tremolo
- Analog-style delay
- Multi-mode reverb

### EQ
- Full parametric EQ
- HPF / LPF
- Draggable bands
- Spectrum analyzer

## What works in v0.2 DSP prototype

- VST3 target
- Standalone target
- CMake project
- JUCE fetched automatically
- Resizable placeholder GUI
- Factory-driven `IAmpModel` registry
- Stable normalized amp parameter contract with reserved Mid / Presence / Master fields
- Fixed real-time signal shell: Input -> PRE FX -> AMP -> CAB -> POST FX -> EQ -> Output
- Transparent placeholder stages for PRE FX, CAB, POST FX and EQ
- Silverline 68 non-neural DSP prototype
- 4x oversampling around nonlinear processing
- Input conditioning and pre-emphasis
- Asymmetric preamp saturation
- Broad passive-style tone shaping
- Secondary power-stage-like saturation with simple sag response
- High-frequency anti-fizz filtering before downsampling
- Smoothed Custom / Vintage voicing
- Tremolo and spring-inspired multi-tap reverb prototype
- Smoothed input/output gain and amp controls
- No allocations, file I/O or mutex locking in `processBlock`
- Replaceable nonlinear-stage interface for a future neural backend
- Plugin state serialization through APVTS

The current DSP is intentionally a **non-neural musical prototype**, not a component-level circuit clone. The architecture is set up so a later neural amp stage can replace the analogue nonlinear backend without changing the plugin shell.

## Requirements

- Windows 10/11 x64
- Visual Studio Community with **Desktop development with C++**
- Windows 11 SDK
- CMake 3.24+
- Git

JUCE is downloaded automatically during CMake configure.

## First build

Open **Developer PowerShell for Visual Studio**, then:

```powershell
cd path\to\SolarisSilverline
.\scripts\configure.ps1
.\scripts\build.ps1
```

The build output will be under:

```text
build\SolarisSilverline_artefacts\Release\
```

Because `COPY_PLUGIN_AFTER_BUILD` is enabled, JUCE will also attempt to copy the VST3 to the normal system VST3 location.

Typical Windows VST3 location:

```text
C:\Program Files\Common Files\VST3
```

Then rescan plugins in Studio One.

## Verify toolchain

```powershell
.\scripts\check-env.ps1
```

## Architecture

```text
Solaris Silverline
│
├── Host / Plugin Shell
│   └── Input -> PRE FX -> AMP -> CAB -> POST FX -> EQ -> Output
├── SolarisCore
│   ├── DSP
│   ├── Effects
│   ├── Amp
│   │   ├── IAmpModel
│   │   ├── AmpRegistry / factories
│   │   └── INonlinearAmpStage
│   ├── Cab
│   ├── EQ
│   ├── Tuner
│   └── Presets
│
└── Silverline
    ├── AmpModels
    │   ├── Silverline68Amp
    │   └── Silverline68AnalogueStage
    ├── Pedals
    └── Assets
```

`SolarisCore` must stay product-agnostic. Product-specific models/assets belong under `Silverline`.

The intended future Silverline 68 path is:

```text
analogue front-end DSP -> replaceable nonlinear/neural amp stage -> tone/output DSP
```

No NAM/RTNeural dependency is required by the current prototype.

## Next milestones

1. Listening tests and gain-staging calibration with DI guitar references
2. Cabinet + convolution
3. PRE FX
4. POST FX
5. Parametric EQ
6. Tuner
7. Preset browser
8. Optional neural amp backend evaluation
9. Performance, regression tests and release builds
