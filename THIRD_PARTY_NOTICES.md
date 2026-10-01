# Third-party notices

This file is a dependency/license checklist for Solaris Silverline. It does not replace legal review.

## JUCE

JUCE is fetched from https://github.com/juce-framework/JUCE

Use and distribution must comply with the JUCE licensing terms applicable to the build and product. Verify the appropriate JUCE licence before distributing a proprietary/commercial binary.

## NeuralAudio

Source: https://github.com/mikeoliphant/NeuralAudio

MIT License

Copyright (c) 2024 Mike Oliphant

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


## AudioDSPTools

Source: https://github.com/sdatkinson/AudioDSPTools

Pinned commit: `844680d118f0317565132c3c5e3aca5f5c976e7a`

MIT License

Copyright (c) 2023 Steven Atkinson

Solaris uses the realtime resampling components from AudioDSPTools for NAM
model/host sample-rate conversion. The resampling headers retain their upstream
iPlug2/WDL and Lanczos licensing notices; those components are permissively
licensed for this use. See the corresponding upstream source headers for the
full notices.

## NeuralAmpModelerCore

Source: https://github.com/mikeoliphant/NeuralAmpModelerCore

MIT License

Copyright (c) 2023 Steven Atkinson

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## RTNeural

Source: https://github.com/mikeoliphant/RTNeural

BSD 3-Clause License

Copyright (c) 2020, jatinchowdhury18
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## math_approx

Source: https://github.com/Chowdhury-DSP/math_approx

BSD 3-Clause License

Copyright (c) 2024, jatinchowdhury18
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

## nlohmann/json

Source: https://github.com/nlohmann/json

MIT License

Copyright (c) 2013-2026 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


## 0xFX factory cabinet IR

Source: https://github.com/averagenative/0xFX

Pinned source commit: `5b248779c96dce6e64d0ed29cfa598014dc4d3c4`

Asset: `resources/ir/bundled/1x12_open.wav`

The 0xFX project documents its four bundled real cabinet impulse responses,
including this open-back 1x12 IR, as CC0 / public-domain assets. Solaris embeds
the WAV data as a development factory fallback and labels it generically as
"Factory Open Back 1x12"; it is not represented as a Solaris Silverline 2x10
capture.

The 0xFX source code itself is MIT-licensed, copyright (c) 2026 Dan Michael.

## Models and impulse responses

A `.nam` file or cabinet impulse response can have its own copyright/licensing terms independent of the runtime code. Do not bundle third-party models or IRs unless redistribution rights are explicitly documented.


## Release build pins and validation tooling

The v0.4.0 release foundation pins JUCE 8.0.15 and NeuralAudio commit
`048d195c5113116e07d5ac25844b21380323db6f`. NeuralAudio's recursive
submodules supply the NAMCore, RTNeural, math_approx and nlohmann/json versions
used by that exact build input. These dependencies remain subject to their
respective licenses listed above.

### pluginval

- Project: pluginval by Tracktion
- Validation version: v1.0.4
- License: GNU General Public License v3.0
- Usage: CI/release validation only
- Distribution: pluginval is downloaded by the release-validation workflow and
  is not linked into, bundled with, or redistributed as part of Solaris Silverline.

### GitHub Actions

The repository uses GitHub-maintained `actions/checkout@v4` and
`actions/upload-artifact@v4` solely as CI infrastructure. They are not part of
the shipped plugin binaries.

## Model and IR asset policy

Solaris Silverline v0.4.0 does not bundle third-party NAM model files or cabinet
IR audio assets. The factory IR catalog infrastructure intentionally ships empty.
A NAM model or IR may only be added to a release resource pack after its
redistribution rights and required attribution have been explicitly documented
here.
