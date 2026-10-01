#include <JuceHeader.h>
#include "../Silverline/AmpModels/Silverline68Amp.h"
#include "../SolarisCore/Amp/NeuralAmpModel.h"
#include <cmath>
#include <iostream>

namespace
{
    struct Result
    {
        juce::String name;
        double elapsedMs = 0.0;
        double audioSeconds = 0.0;
        double realtimeRatio = 0.0;
    };

    template <typename Processor>
    Result runBenchmark(juce::String name, Processor& processor, double sampleRate, int blockSize, double seconds)
    {
        juce::AudioBuffer<float> source(2, blockSize);
        juce::AudioBuffer<float> working(2, blockSize);
        double phase = 0.0;
        const auto delta = juce::MathConstants<double>::twoPi * 110.0 / sampleRate;
        for (int n = 0; n < blockSize; ++n)
        {
            const auto sample = static_cast<float>(0.15 * std::sin(phase));
            phase += delta;
            source.setSample(0, n, sample);
            source.setSample(1, n, sample);
        }

        const auto blocks = juce::jmax(1, static_cast<int>(std::ceil(seconds * sampleRate / blockSize)));
        for (int i = 0; i < 64; ++i)
        {
            working.makeCopyOf(source, true);
            processor.process(working);
        }

        const auto start = juce::Time::getMillisecondCounterHiRes();
        for (int i = 0; i < blocks; ++i)
        {
            working.makeCopyOf(source, true);
            processor.process(working);
        }
        const auto elapsed = juce::Time::getMillisecondCounterHiRes() - start;
        const auto audioSeconds = static_cast<double>(blocks * blockSize) / sampleRate;
        return { std::move(name), elapsed, audioSeconds, (elapsed / 1000.0) / audioSeconds };
    }

    void printResult(const Result& result)
    {
        std::cout << result.name << ": elapsed_ms=" << result.elapsedMs
                  << " audio_seconds=" << result.audioSeconds
                  << " realtime_ratio=" << result.realtimeRatio << std::endl;
    }
}

int main(int argc, char** argv)
{
    double sampleRate = 48000.0;
    constexpr int blockSize = 128;
    double seconds = 5.0;
    juce::File namFile;

    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg(argv[i]);
        if (arg == "--nam" && i + 1 < argc)
            namFile = juce::File(argv[++i]);
        else if (arg == "--seconds" && i + 1 < argc)
            seconds = juce::jlimit(0.25, 60.0, juce::String(argv[++i]).getDoubleValue());
        else if (arg == "--sample-rate" && i + 1 < argc)
            sampleRate = juce::jlimit(8000.0, 384000.0,
                                      juce::String(argv[++i]).getDoubleValue());
    }

    solaris::AmpPrepareSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = blockSize;
    spec.numChannels = 2;

    solaris::AmpParameters parameters;
    parameters.enabled = true;

    solaris::Silverline68Amp analogue;
    analogue.prepare(spec);
    analogue.setParameters(parameters);
    printResult(runBenchmark("analogue", analogue, sampleRate, blockSize, seconds));

   #if SOLARIS_ENABLE_NEURAL_AUDIO
    if (namFile != juce::File())
    {
        solaris::NeuralAmpModel neural;
        neural.prepare(spec);
        neural.setParameters(parameters);
        if (!neural.loadFromFile(namFile))
        {
            std::cerr << "NAM benchmark failed: " << neural.lastLoadError() << std::endl;
            return 2;
        }

        juce::AudioBuffer<float> smoke(2, blockSize);
        for (int n = 0; n < blockSize; ++n)
        {
            const auto x = 0.12f * std::sin(
                static_cast<float>(juce::MathConstants<double>::twoPi * 220.0
                                   * static_cast<double>(n) / sampleRate));
            smoke.setSample(0, n, x);
            smoke.setSample(1, n, x);
        }

        neural.process(smoke);
        float peak = 0.0f;
        for (int channel = 0; channel < smoke.getNumChannels(); ++channel)
        {
            for (int n = 0; n < smoke.getNumSamples(); ++n)
            {
                const auto sample = smoke.getSample(channel, n);
                if (!std::isfinite(sample))
                {
                    std::cerr << "NAM smoke test produced non-finite output" << std::endl;
                    return 3;
                }
                peak = juce::jmax(peak, std::abs(sample));
            }
        }

        if (peak <= 1.0e-7f || peak > 32.0f)
        {
            std::cerr << "NAM smoke test produced invalid output peak=" << peak << std::endl;
            return 4;
        }

        std::cout << "NAM smoke: host_rate=" << sampleRate
                  << " model_rate=" << neural.modelMetadata().modelSampleRate
                  << " mode=" << neural.modelMetadata().sampleRateMode
                  << " latency_samples=" << neural.latencySamples()
                  << " peak=" << peak << std::endl;

        printResult(runBenchmark("neural", neural, sampleRate, blockSize, seconds));
    }
   #else
    juce::ignoreUnused(namFile);
   #endif

    return 0;
}
