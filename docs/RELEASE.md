# Solaris Silverline release foundation

Version is sourced from the repository-root `VERSION.txt` file and propagated by CMake.

## Windows release build

Use a Visual Studio 2022 Developer PowerShell:

```powershell
.\scripts\dev-build.ps1 -Clean
.\scripts\package.ps1
```

The deterministic package root is `dist/SolarisSilverline-<version>-windows-x64/` and contains `VST3`, `Standalone`, `LICENSE.txt`, `THIRD_PARTY_NOTICES.md`, and `VERSION.txt`.

To install a packaged VST3 explicitly:

```powershell
.\scripts\install-vst3.ps1 -PluginPath '.\dist\SolarisSilverline-<version>-windows-x64\VST3\Solaris Silverline.vst3'
```

The default Windows VST3 destination is `%CommonProgramFiles%\VST3`. The script does not silently elevate privileges.

## Linux release build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSOLARIS_ENABLE_NEURAL_AUDIO=ON
cmake --build build --config Release --target SolarisSilverline_VST3 SolarisSilverline_Standalone SolarisBenchmark
bash scripts/package.sh build
```

## Validation

The normal `build` workflow checks Linux Release builds with NeuralAudio ON and OFF, Windows MSVC Release with NeuralAudio ON, VST3, Standalone, and an analogue benchmark smoke test. Artifacts are uploaded from `dist/`.

The heavier `release-validation` workflow runs pluginval strictness 5 headlessly on the Linux VST3. It runs on version tags, weekly, and on manual dispatch rather than every developer push.

## Assets and licensing

No NAM model or cabinet IR is part of the release package unless redistribution rights are explicitly documented. Factory IR catalog infrastructure intentionally ships empty until licensed assets are approved.

## Neural model sample rates

Solaris does not resample a neural model's output as a shortcut. NeuralAudio is configured with the host sample rate before model construction. Native-rate NAM models are supported directly. For WaveNet NAM models, NeuralAudio's load-time dilation scaling is accepted when the host rate is an integer multiple of the model rate. Other mismatches are rejected with a clear load error, because the pinned NeuralAudio backend does not provide a general realtime SRC path for those architectures.

Model construction, JSON/file access and prewarming remain on the NAM loader worker thread. Session restore records the desired model path immediately but delays model loading until the host has supplied the real processing sample rate in prepareToPlay().
