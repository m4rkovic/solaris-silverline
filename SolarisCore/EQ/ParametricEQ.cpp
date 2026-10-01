#include "ParametricEQ.h"
#include <cmath>

namespace solaris
{
    namespace { float clampF(double sr,float f) noexcept { return juce::jlimit(10.0f, static_cast<float>(sr*0.49), f); } }

    void ParametricEQ::prepare(double sr) noexcept { sampleRate=juce::jmax(1.0,sr); coefficientRampSamples=juce::jmax(16,static_cast<int>(sampleRate*0.002)); reset(); }
    void ParametricEQ::reset() noexcept { highPass.reset(); lowPass.reset(); for(auto& f:peakFilters) f.reset(); }
    void ParametricEQ::setHighPass(float f,bool b) noexcept { highPassFrequency.store(f); highPassBypassed.store(b); }
    void ParametricEQ::setLowPass(float f,bool b) noexcept { lowPassFrequency.store(f); lowPassBypassed.store(b); }
    void ParametricEQ::setBand(int i,float f,float g,float qv,bool b) noexcept
    {
        if(!juce::isPositiveAndBelow(i,numBands)) return;
        auto& x=bands[static_cast<std::size_t>(i)];
        x.frequencyHz.store(f); x.gainDb.store(juce::jlimit(-24.0f,24.0f,g)); x.q.store(juce::jlimit(0.1f,18.0f,qv)); x.bypassed.store(b);
    }

    void ParametricEQ::process(juce::AudioBuffer<float>& buffer) noexcept
    {
        const auto channels=juce::jmin(2,buffer.getNumChannels()); if(channels<=0) return;
        highPass.setBypassed(highPassBypassed.load()); highPass.setTarget(makeHighPass(sampleRate,highPassFrequency.load()),coefficientRampSamples); highPass.process(buffer,channels);
        for(int i=0;i<numBands;++i){ auto& p=bands[static_cast<std::size_t>(i)]; auto& f=peakFilters[static_cast<std::size_t>(i)]; f.setBypassed(p.bypassed.load()); f.setTarget(makePeak(sampleRate,p.frequencyHz.load(),p.gainDb.load(),p.q.load()),coefficientRampSamples); f.process(buffer,channels); }
        lowPass.setBypassed(lowPassBypassed.load()); lowPass.setTarget(makeLowPass(sampleRate,lowPassFrequency.load()),coefficientRampSamples); lowPass.process(buffer,channels);
    }

    juce::ValueTree ParametricEQ::createState() const
    {
        juce::ValueTree s("EQ"); s.setProperty("hpfFrequency",highPassFrequency.load(),nullptr); s.setProperty("hpfBypassed",highPassBypassed.load(),nullptr); s.setProperty("lpfFrequency",lowPassFrequency.load(),nullptr); s.setProperty("lpfBypassed",lowPassBypassed.load(),nullptr);
        for(int i=0;i<numBands;++i){ const auto& p=bands[static_cast<std::size_t>(i)]; juce::ValueTree b("BAND"); b.setProperty("index",i,nullptr); b.setProperty("frequency",p.frequencyHz.load(),nullptr); b.setProperty("gainDb",p.gainDb.load(),nullptr); b.setProperty("q",p.q.load(),nullptr); b.setProperty("bypassed",p.bypassed.load(),nullptr); s.addChild(b,-1,nullptr); }
        return s;
    }

    void ParametricEQ::restoreState(const juce::ValueTree& s)
    {
        if(!s.isValid()||!s.hasType("EQ")) return;
        setHighPass(static_cast<float>(s.getProperty("hpfFrequency",70.0f)),static_cast<bool>(s.getProperty("hpfBypassed",true)));
        setLowPass(static_cast<float>(s.getProperty("lpfFrequency",18000.0f)),static_cast<bool>(s.getProperty("lpfBypassed",true)));
        for(int i=0;i<s.getNumChildren();++i){ auto b=s.getChild(i); if(!b.hasType("BAND")) continue; setBand(static_cast<int>(b.getProperty("index",-1)),static_cast<float>(b.getProperty("frequency",1000.0f)),static_cast<float>(b.getProperty("gainDb",0.0f)),static_cast<float>(b.getProperty("q",0.707f)),static_cast<bool>(b.getProperty("bypassed",true))); }
    }

    void ParametricEQ::Biquad::reset() noexcept { for(auto& s:states)s={}; current={}; target={}; delta={}; samplesRemaining=0; }
    void ParametricEQ::Biquad::setBypassed(bool b) noexcept { if(bypassed==b)return; bypassed=b; if(bypassed)for(auto& s:states)s={}; }
    void ParametricEQ::Biquad::setTarget(const Coefficients& c,int ramp) noexcept
    {
        if(c.b0==target.b0&&c.b1==target.b1&&c.b2==target.b2&&c.a1==target.a1&&c.a2==target.a2) return;
        target=c; samplesRemaining=juce::jmax(1,ramp); const auto inv=1.0f/static_cast<float>(samplesRemaining);
        delta={(target.b0-current.b0)*inv,(target.b1-current.b1)*inv,(target.b2-current.b2)*inv,(target.a1-current.a1)*inv,(target.a2-current.a2)*inv};
    }
    void ParametricEQ::Biquad::advance() noexcept { if(samplesRemaining<=0)return; current.b0+=delta.b0; current.b1+=delta.b1; current.b2+=delta.b2; current.a1+=delta.a1; current.a2+=delta.a2; if(--samplesRemaining==0)current=target; }
    void ParametricEQ::Biquad::process(juce::AudioBuffer<float>& b,int channels) noexcept
    {
        if(bypassed)return; for(int n=0;n<b.getNumSamples();++n){ advance(); for(int ch=0;ch<channels;++ch){ auto& s=states[static_cast<std::size_t>(ch)]; auto* d=b.getWritePointer(ch); const auto x=d[n]; const auto y=current.b0*x+s.z1; s.z1=current.b1*x-current.a1*y+s.z2; s.z2=current.b2*x-current.a2*y; d[n]=y; } }
    }

    ParametricEQ::Coefficients ParametricEQ::normalise(float b0,float b1,float b2,float a0,float a1,float a2) noexcept { const auto i=1.0f/a0; return {b0*i,b1*i,b2*i,a1*i,a2*i}; }
    ParametricEQ::Coefficients ParametricEQ::makeHighPass(double sr,float f) noexcept { f=clampF(sr,f); const auto w=2.0*juce::MathConstants<double>::pi*f/sr,c=std::cos(w),a=std::sin(w)/(2.0*0.7071067811865476); return normalise(static_cast<float>((1+c)*.5),static_cast<float>(-(1+c)),static_cast<float>((1+c)*.5),static_cast<float>(1+a),static_cast<float>(-2*c),static_cast<float>(1-a)); }
    ParametricEQ::Coefficients ParametricEQ::makeLowPass(double sr,float f) noexcept { f=clampF(sr,f); const auto w=2.0*juce::MathConstants<double>::pi*f/sr,c=std::cos(w),a=std::sin(w)/(2.0*0.7071067811865476); return normalise(static_cast<float>((1-c)*.5),static_cast<float>(1-c),static_cast<float>((1-c)*.5),static_cast<float>(1+a),static_cast<float>(-2*c),static_cast<float>(1-a)); }
    ParametricEQ::Coefficients ParametricEQ::makePeak(double sr,float f,float g,float qv) noexcept { f=clampF(sr,f); qv=juce::jlimit(.1f,18.0f,qv); g=juce::jlimit(-24.0f,24.0f,g); const auto w=2.0*juce::MathConstants<double>::pi*f/sr,c=std::cos(w),alpha=std::sin(w)/(2.0*qv),A=std::pow(10.0,g/40.0); return normalise(static_cast<float>(1+alpha*A),static_cast<float>(-2*c),static_cast<float>(1-alpha*A),static_cast<float>(1+alpha/A),static_cast<float>(-2*c),static_cast<float>(1-alpha/A)); }
}
