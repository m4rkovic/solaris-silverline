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

## What works in v0.1

- VST3 target
- Standalone target
- CMake project
- JUCE fetched automatically
- Resizable placeholder GUI
- Modular amp interface and registry
- Silverline 68 placeholder model
- Audio passthrough
- Plugin state skeleton

The goal of v0.1 is intentionally boring: **build successfully, load in Studio One, pass audio cleanly.**

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
├── SolarisCore
│   ├── DSP
│   ├── Effects
│   ├── Amp
│   ├── Cab
│   ├── EQ
│   ├── Tuner
│   └── Presets
│
└── Silverline
    ├── AmpModels
    ├── Pedals
    └── Assets
```

`SolarisCore` must stay product-agnostic. Product-specific models/assets belong under `Silverline`.

## Next milestones

1. Proper parameter/state architecture
2. Input/output gain and meters
3. Tab/navigation visual system
4. Tuner
5. Silverline 68 amp engine
6. Cabinet + convolution
7. PRE FX
8. POST FX
9. Parametric EQ
10. Preset browser
11. Performance, validation and release builds

## Git

```powershell
git init
git add .
git commit -m "Initial Solaris Silverline skeleton"
git branch -M main
git remote add origin YOUR_GITHUB_REPO_URL
git push -u origin main
```
