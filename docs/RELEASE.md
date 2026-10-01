# Solaris Silverline release foundation

Version is sourced from the repository-root `VERSION` file and propagated by CMake.

## Windows release build

Use a Visual Studio 2022 Developer PowerShell:

```powershell
.\scripts\dev-build.ps1 -Clean
.\scripts\package.ps1
```

The deterministic package root is `dist/SolarisSilverline-<version>-windows-x64/` and contains `VST3`, `Standalone`, `LICENSE.txt`, `THIRD_PARTY_NOTICES.md`, and `VERSION`.

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
