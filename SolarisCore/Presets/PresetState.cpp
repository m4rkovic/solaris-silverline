#include "PresetState.h"

namespace solaris
{
    juce::ValueTree PresetState::create(juce::AudioProcessorValueTreeState& parameters,const juce::String& ampModelId,const juce::ValueTree& cabinetState,const juce::ValueTree& eqState,const juce::ValueTree& pedalState)
    {
        juce::ValueTree p("SOLARIS_PRESET"); p.setProperty("schemaVersion",currentSchemaVersion,nullptr); p.setProperty("productId","solaris-silverline",nullptr); p.setProperty("ampModelId",ampModelId,nullptr); p.addChild(parameters.copyState(),-1,nullptr);
        juce::ValueTree globals("GLOBALS"); if(auto* v=parameters.getRawParameterValue("inputGain"))globals.setProperty("inputGainDb",v->load(),nullptr); if(auto* v=parameters.getRawParameterValue("outputGain"))globals.setProperty("outputGainDb",v->load(),nullptr); p.addChild(globals,-1,nullptr);
        p.addChild(cabinetState.isValid()?cabinetState.createCopy():juce::ValueTree("CAB"),-1,nullptr);
        p.addChild(eqState.isValid()?eqState.createCopy():juce::ValueTree("EQ"),-1,nullptr);
        p.addChild(pedalState.isValid()?pedalState.createCopy():juce::ValueTree("PEDALS"),-1,nullptr);
        return p;
    }

    juce::ValueTree PresetState::normalise(const juce::ValueTree& incoming,juce::Identifier parameterStateType)
    {
        if(!incoming.isValid())return{};
        if(incoming.hasType("SOLARIS_PRESET"))return migrateToCurrent(incoming.createCopy());
        if(incoming.hasType(parameterStateType))
        {
            juce::ValueTree p("SOLARIS_PRESET"); p.setProperty("schemaVersion",0,nullptr); p.setProperty("productId","solaris-silverline",nullptr); p.setProperty("ampModelId","silverline68",nullptr); p.addChild(incoming.createCopy(),-1,nullptr); return migrateToCurrent(std::move(p));
        }
        return{};
    }

    juce::ValueTree PresetState::parameterState(const juce::ValueTree& p,juce::Identifier t){return p.getChildWithName(t);} 
    juce::ValueTree PresetState::cabinetState(const juce::ValueTree& p){return p.getChildWithName("CAB");}
    juce::ValueTree PresetState::eqState(const juce::ValueTree& p){return p.getChildWithName("EQ");}
    juce::ValueTree PresetState::pedalState(const juce::ValueTree& p){return p.getChildWithName("PEDALS");}
    juce::String PresetState::ampModelId(const juce::ValueTree& p){return p.getProperty("ampModelId","silverline68").toString();}

    juce::ValueTree PresetState::migrateToCurrent(juce::ValueTree p)
    {
        auto v=static_cast<int>(p.getProperty("schemaVersion",0));
        if(v<1){ if(!p.getChildWithName("GLOBALS").isValid())p.addChild(juce::ValueTree("GLOBALS"),-1,nullptr); if(!p.getChildWithName("CAB").isValid())p.addChild(juce::ValueTree("CAB"),-1,nullptr); if(!p.getChildWithName("EQ").isValid())p.addChild(juce::ValueTree("EQ"),-1,nullptr); if(!p.getChildWithName("PEDALS").isValid())p.addChild(juce::ValueTree("PEDALS"),-1,nullptr); v=1; }
        p.setProperty("schemaVersion",v,nullptr); return p;
    }
}
