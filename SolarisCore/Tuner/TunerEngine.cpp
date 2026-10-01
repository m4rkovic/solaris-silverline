#include "TunerEngine.h"
#include <algorithm>
#include <cmath>

namespace solaris
{
    namespace
    {
        constexpr float minGuitar=41.2034f;
        constexpr float maxGuitar=1318.51f;
        constexpr float yinThreshold=0.15f;
        constexpr float minRms=0.0005f;
    }

    TunerEngine::TunerEngine():juce::Thread("Solaris Tuner"){}
    TunerEngine::~TunerEngine(){ stop(); }

    void TunerEngine::prepare(double hostSampleRate)
    {
        stop(); fifo.reset(); history.fill(0); frame.fill(0); yin.fill(0); recentFrequencies.fill(0);
        historyWrite=historyValid=freshAnalysisSamples=frequencyHistoryCount=frequencyHistoryWrite=invalidFrames=0;
        decimationCount=0; decimationAccumulator=0;
        const auto safe=juce::jmax(1.0,hostSampleRate);
        decimationFactor=juce::jmax(1,static_cast<int>(std::ceil(safe/12000.0)));
        analysisSampleRate=safe/static_cast<double>(decimationFactor);
        publishInvalid(); startThread();
    }

    void TunerEngine::stop()
    {
        if(isThreadRunning()){ signalThreadShouldExit(); stopThread(1000); }
        fifo.reset(); publishInvalid();
    }

    void TunerEngine::pushSamples(const juce::AudioBuffer<float>& buffer) noexcept
    {
        if(buffer.getNumChannels()<=0||buffer.getNumSamples()<=0)return;
        const auto requested=juce::jmin(buffer.getNumSamples(),fifo.getFreeSpace()); if(requested<=0)return;
        int s1=0,n1=0,s2=0,n2=0; fifo.prepareToWrite(requested,s1,n1,s2,n2); const auto* src=buffer.getReadPointer(0);
        if(n1>0)juce::FloatVectorOperations::copy(fifoBuffer.data()+s1,src,n1);
        if(n2>0)juce::FloatVectorOperations::copy(fifoBuffer.data()+s2,src+n1,n2);
        fifo.finishedWrite(n1+n2);
    }

    void TunerEngine::setReferenceA4(float f) noexcept { referenceA4.store(juce::jlimit(400.0f,480.0f,f)); }
    float TunerEngine::getReferenceA4() const noexcept { return referenceA4.load(); }

    TunerSnapshot TunerEngine::getSnapshot() const noexcept
    {
        for(;;)
        {
            const auto before=resultSequence.load(std::memory_order_acquire); if(before&1u)continue;
            TunerSnapshot s; s.valid=resultValid.load(); s.frequencyHz=resultFrequency.load(); s.cents=resultCents.load(); s.confidence=resultConfidence.load(); s.midiNote=resultMidiNote.load();
            if(before==resultSequence.load(std::memory_order_acquire))return s;
        }
    }

    void TunerEngine::run(){ while(!threadShouldExit()){ drainFifo(); juce::Thread::sleep(8); } }

    void TunerEngine::drainFifo()
    {
        while(!threadShouldExit())
        {
            const auto ready=fifo.getNumReady(); if(ready<=0)break; const auto req=juce::jmin(ready,4096);
            int s1=0,n1=0,s2=0,n2=0; fifo.prepareToRead(req,s1,n1,s2,n2);
            if(n1>0)consumeHostSamples(fifoBuffer.data()+s1,n1); if(n2>0)consumeHostSamples(fifoBuffer.data()+s2,n2); fifo.finishedRead(n1+n2);
        }
    }

    void TunerEngine::consumeHostSamples(const float* samples,int count)
    {
        for(int i=0;i<count;++i)
        {
            decimationAccumulator+=samples[i]; if(++decimationCount<decimationFactor)continue;
            const auto sample=decimationAccumulator/static_cast<float>(decimationFactor); decimationAccumulator=0; decimationCount=0;
            history[static_cast<std::size_t>(historyWrite)]=sample; historyWrite=(historyWrite+1)%analysisSize; historyValid=juce::jmin(analysisSize,historyValid+1); ++freshAnalysisSamples;
            if(historyValid==analysisSize&&freshAnalysisSamples>=hopSize)
            {
                freshAnalysisSamples=0;
                for(int n=0;n<analysisSize;++n)frame[static_cast<std::size_t>(n)]=history[static_cast<std::size_t>((historyWrite+n)%analysisSize)];
                analyseFrame();
            }
        }
    }

    void TunerEngine::analyseFrame()
    {
        double energy=0; for(auto x:frame)energy+=static_cast<double>(x)*x;
        if(std::sqrt(energy/analysisSize)<minRms){ if(++invalidFrames>=3)publishInvalid(); return; }
        const auto minLag=juce::jmax(2,static_cast<int>(std::floor(analysisSampleRate/maxGuitar)));
        const auto maxLag=juce::jmin(maxLagStorage-1,static_cast<int>(std::ceil(analysisSampleRate/minGuitar)));
        if(minLag>=maxLag){ publishInvalid(); return; }
        yin[0]=1.0f;
        for(int tau=1;tau<=maxLag+1;++tau)
        {
            double d=0; for(int i=0;i<analysisSize-tau;++i){ const auto delta=frame[static_cast<std::size_t>(i)]-frame[static_cast<std::size_t>(i+tau)]; d+=static_cast<double>(delta)*delta; }
            yin[static_cast<std::size_t>(tau)]=static_cast<float>(d);
        }
        double running=0; for(int tau=1;tau<=maxLag+1;++tau){ running+=yin[static_cast<std::size_t>(tau)]; yin[static_cast<std::size_t>(tau)]=running>0?static_cast<float>(yin[static_cast<std::size_t>(tau)]*tau/running):1.0f; }
        int best=-1;
        for(int tau=minLag;tau<=maxLag;++tau){ if(yin[static_cast<std::size_t>(tau)]>=yinThreshold)continue; while(tau+1<=maxLag&&yin[static_cast<std::size_t>(tau+1)]<yin[static_cast<std::size_t>(tau)])++tau; best=tau; break; }
        if(best<0)
        {
            float m=yin[static_cast<std::size_t>(minLag)]; best=minLag;
            for(int tau=minLag+1;tau<=maxLag;++tau){ if(yin[static_cast<std::size_t>(tau)]<m){m=yin[static_cast<std::size_t>(tau)];best=tau;} }
            if(m>0.35f){ if(++invalidFrames>=3)publishInvalid(); return; }
        }
        double lag=best;
        if(best>1&&best+1<=maxLag+1)
        {
            const auto y0=static_cast<double>(yin[static_cast<std::size_t>(best-1)]), y1=static_cast<double>(yin[static_cast<std::size_t>(best)]), y2=static_cast<double>(yin[static_cast<std::size_t>(best+1)]);
            const auto den=2.0*(2.0*y1-y2-y0); if(std::abs(den)>1e-12)lag+=(y2-y0)/den;
        }
        if(lag<=0){publishInvalid();return;}
        auto freq=static_cast<float>(analysisSampleRate/lag); if(freq<minGuitar||freq>maxGuitar){if(++invalidFrames>=3)publishInvalid();return;}
        recentFrequencies[static_cast<std::size_t>(frequencyHistoryWrite)]=freq; frequencyHistoryWrite=(frequencyHistoryWrite+1)%5; frequencyHistoryCount=juce::jmin(5,frequencyHistoryCount+1);
        std::array<float,5> sorted{}; for(int i=0;i<frequencyHistoryCount;++i)sorted[static_cast<std::size_t>(i)]=recentFrequencies[static_cast<std::size_t>(i)]; std::sort(sorted.begin(),sorted.begin()+frequencyHistoryCount); freq=sorted[static_cast<std::size_t>(frequencyHistoryCount/2)];
        const auto midiFloat=69.0+12.0*std::log2(static_cast<double>(freq)/referenceA4.load()); const auto midi=static_cast<int>(std::lround(midiFloat));
        TunerSnapshot s; s.valid=true; s.frequencyHz=freq; s.midiNote=midi; s.cents=static_cast<float>((midiFloat-midi)*100.0); s.confidence=juce::jlimit(0.0f,1.0f,1.0f-yin[static_cast<std::size_t>(best)]); invalidFrames=0; publish(s);
    }

    void TunerEngine::publish(const TunerSnapshot& s) noexcept
    {
        resultSequence.fetch_add(1,std::memory_order_acq_rel); resultValid.store(s.valid); resultFrequency.store(s.frequencyHz); resultCents.store(s.cents); resultConfidence.store(s.confidence); resultMidiNote.store(s.midiNote); resultSequence.fetch_add(1,std::memory_order_release);
    }
    void TunerEngine::publishInvalid() noexcept { publish(TunerSnapshot{}); }
}
