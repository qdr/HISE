/*  ===========================================================================
*
*   This file is part of HISE.
*   Copyright 2016 Christoph Hart
*
*   HISE is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   HISE is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
*
*   Commercial licenses for using HISE in an closed source project are
*   available on request. Please visit the project's website to get more
*   information about commercial licensing:
*
*   http://www.hise.audio/
*
*   HISE is based on the JUCE library,
*   which must be separately licensed for cloused source applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/

ProcessorEditorBody *ModulatorSynthChain::createEditor(ProcessorEditor *parentEditor)
{
#if USE_BACKEND

	return new ModulatorSynthChainBody(parentEditor);

#else 

	ignoreUnused(parentEditor);
	jassertfalse;
	return nullptr;

#endif
};

void ModulatorSynthChain::numSourceChannelsChanged()
{
	for (int i = 0; i < getHandler()->getNumProcessors(); i++)
	{
		RoutableProcessor *rp = dynamic_cast<RoutableProcessor*>(getHandler()->getProcessor(i));

		jassert(rp != nullptr);

		rp->getMatrix().setNumDestinationChannels(getMatrix().getNumSourceChannels());
	}

	ModulatorSynth::numSourceChannelsChanged();

	
}

void ModulatorSynthChain::numDestinationChannelsChanged()
{
	
}

void ModulatorSynthChain::addProcessorsWhenEmpty()
{
	
}

void ModulatorSynthChain::compileAllScripts()
{
	if (getMainController()->isCompilingAllScriptsOnPresetLoad())
	{
		Processor::Iterator<JavascriptProcessor> it(this);

		JavascriptProcessor *sp;

		while ((sp = it.getNextProcessor()) != 0)
		{
			sp->compileScript();
		}
	}
}

void ModulatorSynthChain::renderNextBlockWithModulators(AudioSampleBuffer &buffer, const HiseEventBuffer &inputMidiBuffer)
{
	jassert(isOnAir());

	if (isBypassed()) return;

	ADD_GLITCH_DETECTOR(this, DebugLogger::Location::SynthChainRendering);

	ScopedLock sl(getSynthLock());

	if (getMainController()->getMainSynthChain() == this && !activeChannels.areAllChannelsEnabled())
	{
		HiseEventBuffer::Iterator it(inputMidiBuffer);

		while (HiseEvent* e = it.getNextEventPointer())
		{
			const int channelIndex = e->getChannel() - 1;
			const bool ignoreThisEvent = !activeChannels.isChannelEnabled(channelIndex);

			if (ignoreThisEvent)
				e->ignoreEvent(true);
		}
	}

	const int numSamples = getBlockSize();//buffer.getNumSamples();

	jassert(numSamples <= buffer.getNumSamples());

	initRenderCallback();

	


#if FRONTEND_IS_PLUGIN

	effectChain->renderMasterEffects(buffer);

#else

	processHiseEventBuffer(inputMidiBuffer, numSamples);

	// Shrink the internal buffer to the output buffer size 
	internalBuffer.setSize(getMatrix().getNumSourceChannels(), numSamples, true, false, true);

	// Process the Synths and add store their output in the internal buffer
	for (int i = 0; i < synths.size(); i++) if (!synths[i]->isBypassed()) synths[i]->renderNextBlockWithModulators(internalBuffer, eventBuffer);

	HiseEventBuffer::Iterator eventIterator(eventBuffer);

	while (auto e = eventIterator.getNextConstEventPointer(true, false))
	{
		if (!(e->isController() || e->isPitchWheel()))
		{
			continue;
		}

		handleHiseEvent(*e);
	}

	postVoiceRendering(0, numSamples);

	effectChain->renderMasterEffects(internalBuffer);

	if (internalBuffer.getNumChannels() != 2)
	{
		jassert(internalBuffer.getNumChannels() == getMatrix().getNumSourceChannels());

		for (int i = 0; i < internalBuffer.getNumChannels(); i++)
		{
			const int sourceIndex = i;
			const int destinationIndex = getMatrix().getConnectionForSourceChannel(i);

			if (destinationIndex >= 0 && destinationIndex < buffer.getNumChannels())
			{
				FloatVectorOperations::addWithMultiply(buffer.getWritePointer(destinationIndex, 0), internalBuffer.getReadPointer(sourceIndex, 0), getGain() * getBalance(i % 2 != 0), numSamples);
			}
		}

		if (getMatrix().isEditorShown())
		{
			float gainValues[NUM_MAX_CHANNELS];

			for (int i = 0; i < internalBuffer.getNumChannels(); i++)
			{
				gainValues[i] = FloatVectorOperations::findMaximum(internalBuffer.getReadPointer(i), numSamples);
			}

			getMatrix().setGainValues(gainValues, true);

			for (int i = 0; i < buffer.getNumChannels(); i++)
			{
				gainValues[i] = FloatVectorOperations::findMaximum(buffer.getReadPointer(i), numSamples);
			}

			getMatrix().setGainValues(gainValues, false);
		}
	}
	else // save some cycles on non multichannel buffers...
	{
		FloatVectorOperations::addWithMultiply(buffer.getWritePointer(0, 0), internalBuffer.getReadPointer(0, 0), getGain() * getBalance(false), numSamples);
		FloatVectorOperations::addWithMultiply(buffer.getWritePointer(1, 0), internalBuffer.getReadPointer(1, 0), getGain() * getBalance(true), numSamples);
	}

	// Display the output
	handlePeakDisplay(numSamples);

#endif
}

void ModulatorSynthChain::reset()
{
	sendDeleteMessage();

	Processor::Iterator<Processor> iter(this, false);

	while (auto p = iter.getNextProcessor())
	{
		p->sendDeleteMessage();
	}

    this->getHandler()->clear();
    
    midiProcessorChain->getHandler()->clear();
    gainChain->getHandler()->clear();
    effectChain->getHandler()->clear();
    getMatrix().resetToDefault();
    getMatrix().setNumSourceChannels(2);

#if USE_BACKEND
    clearAllViews();
#endif
    
    clearAllMacroControls();
    
    
    for(int i = 0; i < parameterNames.size(); i++)
    {
        setAttribute(i, getDefaultValue(i), dontSendNotification);
    }
    
    sendChangeMessage();
}

int ModulatorSynthChain::getNumActiveVoices() const
{
	int totalVoices = 0;

	for (int i = 0; i < handler.getNumProcessors(); i++)
	{
		totalVoices += dynamic_cast<const ModulatorSynth*>(handler.getProcessor(i))->getNumActiveVoices();
	}

	return totalVoices;
}

void ModulatorSynthChain::saveInterfaceValues(ValueTree &v)
{
	ValueTree interfaceData("InterfaceData");

	for (int i = 0; i < midiProcessorChain->getNumChildProcessors(); i++)
	{
		JavascriptMidiProcessor *sp = dynamic_cast<JavascriptMidiProcessor*>(midiProcessorChain->getChildProcessor(i));

		if (sp != nullptr && sp->isFront())
		{
			ValueTree spv = sp->getScriptingContent()->exportAsValueTree();

			spv.setProperty("Processor", sp->getId(), nullptr);

			interfaceData.addChild(spv, -1, nullptr);
		}
	}

	v.addChild(interfaceData, -1, nullptr);
}

void ModulatorSynthChain::restoreInterfaceValues(const ValueTree &v)
{
	for (int i = 0; i < midiProcessorChain->getNumChildProcessors(); i++)
	{
		JavascriptMidiProcessor *sp = dynamic_cast<JavascriptMidiProcessor*>(midiProcessorChain->getChildProcessor(i));

		if (sp != nullptr && sp->isFront())
		{
			for (int j = 0; j < v.getNumChildren(); j++)
			{
				const ValueTree child = v.getChild(j);

				if (child.getProperty("Processor") == sp->getId())
				{
					ScriptingApi::Content *content = sp->getScriptingContent();

					content->restoreAllControlsFromPreset(child);

					break;
				}

			}
		}
	}
}

bool ModulatorSynthChain::hasDefinedFrontInterface() const
{   
    for (int i = 0; i < midiProcessorChain->getNumChildProcessors(); i++)
    {
        if (JavascriptMidiProcessor *sp = dynamic_cast<JavascriptMidiProcessor*>(midiProcessorChain->getChildProcessor(i)))
        {
            if (sp->isFront())
            {
                return true;
            }
        }
    }
    
    return false;
}

ProcessorEditorBody *ModulatorSynthGroup::createEditor(ProcessorEditor *parentEditor)
{
#if USE_BACKEND

	return new GroupBody(parentEditor);

	
#else 

	ignoreUnused(parentEditor);
	jassertfalse;
	return nullptr;

#endif
};


ModulatorSynthGroup::ModulatorSynthGroup(MainController *mc, const String &id, int numVoices):
	ModulatorSynth(mc, id, numVoices),
	numVoices(numVoices),
	handler(this),
	vuValue(0.0f),
	fmEnabled(false),
	carrierIndex(-1),
	modIndex(-1),
	fmState("FM disabled"),
	fmCorrectlySetup(false),
	sampleStartChain(new ModulatorChain(mc, "Sample Start Modulation", numVoices, Modulation::GainMode, this))
{
	setFactoryType(new ModulatorSynthChainFactoryType(numVoices, this));

	getFactoryType()->setConstrainer(new SynthGroupConstrainer());

	setGain(1.0);


	allowStates.clear();
		
	for(int i = 0; i < numVoices; i++) addVoice(new ModulatorSynthGroupVoice(this));
	addSound (new ModulatorSynthGroupSound());	

};

/** Calls the base class startNote() for the group itself and all child synths.  */
void ModulatorSynthGroupVoice::startNote (int midiNoteNumber, float velocity, SynthesiserSound*, int )
{
	ModulatorSynthVoice::startNote(midiNoteNumber, velocity, nullptr, -1);

	// The uptime is not used, but it must be > 0, or the voice is not rendered.
	uptimeDelta = 1.0;

	ModulatorSynthGroup::ChildSynthIterator iterator(static_cast<ModulatorSynthGroup*>(ownerSynth), ModulatorSynthGroup::ChildSynthIterator::IterateAllSynths);
	ModulatorSynth *childSynth;

	for(int i = 0; i < childSynths.size(); i++)
	{
		ModulatorSynthVoice *childVoice = childVoices.getUnchecked(i);
		childSynth = childSynths.getUnchecked(i);

		if(static_cast<ModulatorSynthGroup*>(ownerSynth)->allowStates[i])
		{

			ModulatorSynthSound *soundToPlay = nullptr;

			for(int j = 0; j < childSynth->getNumSounds(); j++)
			{
				ModulatorSynthSound *s = static_cast<ModulatorSynthSound*>(childSynth->getSound(j));

				if(s->appliesToMessage(1, midiNoteNumber, (int)(velocity * 127)))
				{
					soundToPlay = s;
					break; // only one sound at a time (can be changed later)
				}
			}

			if (soundToPlay == nullptr) continue;

			ModulatorChain *g = static_cast<ModulatorChain*>(childSynth->getChildProcessor(ModulatorSynth::GainModulation));
			ModulatorChain *p = static_cast<ModulatorChain*>(childSynth->getChildProcessor(ModulatorSynth::PitchModulation));

			g->startVoice(voiceIndex);
			p->startVoice(voiceIndex);

			childVoice->startNote(midiNoteNumber, velocity, soundToPlay, -1);
		}
		else
		{
			childVoice->setInactive();
			
		}
	}

};

void ModulatorSynthGroupVoice::stopNote (float, bool)
{
	
	ModulatorSynthGroup::ChildSynthIterator iterator(static_cast<ModulatorSynthGroup*>(ownerSynth), ModulatorSynthGroup::ChildSynthIterator::IterateAllSynths);
	ModulatorSynth *childSynth;

	while(iterator.getNextAllowedChild(childSynth))
	{
		ModulatorChain *g = static_cast<ModulatorChain*>(childSynth->getChildProcessor(ModulatorSynth::GainModulation));
		ModulatorChain *p = static_cast<ModulatorChain*>(childSynth->getChildProcessor(ModulatorSynth::PitchModulation));

		g->stopVoice(voiceIndex);
		p->stopVoice(voiceIndex);
	}

	ModulatorSynthVoice::stopNote(1.0f, true);

	//checkRelease();
	
};

void ModulatorSynthGroupVoice::checkRelease()
{
	

	ModulatorChain *ownerGainChain = static_cast<ModulatorChain*>(ownerSynth->getChildProcessor(ModulatorSynth::GainModulation));
	//ModulatorChain *ownerPitchChain = static_cast<ModulatorChain*>(ownerSynth->getChildProcessor(ModulatorSynth::PitchModulation));

	if( killThisVoice && (killFadeLevel < 0.001f) )
	{
		resetVoice();
		

		ModulatorSynthGroup::ChildSynthIterator iterator(static_cast<ModulatorSynthGroup*>(ownerSynth), ModulatorSynthGroup::ChildSynthIterator::IterateAllSynths);
		ModulatorSynth *childSynth;

		while(iterator.getNextAllowedChild(childSynth))
		{
			ModulatorSynthVoice *childVoice = static_cast<ModulatorSynthVoice*>(childSynth->getVoice(voiceIndex));

			childSynth->setPeakValues(0.0f, 0.0f);

			childVoice->resetVoice();
		}

		return;

	}

	if(! ownerGainChain->isPlaying(voiceIndex))
	{
		resetVoice();

		//ownerGainChain->reset(voiceIndex);
		//ownerPitchChain->reset(voiceIndex);

		//clearCurrentNote();
		//uptimeDelta = 0.0;

		ModulatorSynthGroup::ChildSynthIterator iterator(static_cast<ModulatorSynthGroup*>(ownerSynth), ModulatorSynthGroup::ChildSynthIterator::IterateAllSynths);
		ModulatorSynth *childSynth;

		while(iterator.getNextAllowedChild(childSynth))
		{
			ModulatorSynthVoice *childVoice = static_cast<ModulatorSynthVoice*>(childSynth->getVoice(voiceIndex));

			childSynth->setPeakValues(0.0f, 0.0f);

			childVoice->resetVoice();

			/*
			ModulatorChain *c = static_cast<ModulatorChain*>(childSynth->getChildProcessor(ModulatorSynth::GainModulation));
			ModulatorChain *p = static_cast<ModulatorChain*>(childSynth->getChildProcessor(ModulatorSynth::PitchModulation));

			c->reset(voiceIndex);
			p->reset(voiceIndex);

			childVoice->clearCurrentNote();
			childVoice->uptimeDelta = 0.0;
			*/
		}
	}


}

void ModulatorSynthGroup::setInternalAttribute(int index, float newValue)
{
	if (index < ModulatorSynth::numModulatorSynthParameters)
	{
		ModulatorSynth::setInternalAttribute(index, newValue);
		return;
	}

	switch (index)
	{
	case EnableFM:		 fmEnabled = (newValue > 0.5f); break;
						 
	case ModulatorIndex: modIndex = (int)newValue; break;
	case CarrierIndex:	 carrierIndex = (int)newValue; break;
	default:			 jassertfalse;
	}

	checkFmState();
}

float ModulatorSynthGroup::getAttribute(int index) const
{
	if (index < ModulatorSynth::numModulatorSynthParameters)
	{
		return ModulatorSynth::getAttribute(index);
	}

	switch (index)
	{
	case EnableFM:		 return fmEnabled ? 1.0f : 0.0f;
	case ModulatorIndex: return (float)modIndex;
	case CarrierIndex:	 return (float)carrierIndex;
	default:			 jassertfalse; return -1.0f;
	}
}

void ModulatorSynthGroup::preHiseEventCallback(const HiseEvent &m)
{
	ModulatorSynth::preHiseEventCallback(m);

	ModulatorSynth *child;
	ChildSynthIterator iterator(this, ChildSynthIterator::SkipUnallowedSynths);

	while(iterator.getNextAllowedChild(child))
	{
		child->preHiseEventCallback(m);
	}
};

void ModulatorSynthGroup::restoreFromValueTree(const ValueTree &v)
{
	ModulatorSynth::restoreFromValueTree(v);

	loadAttribute(EnableFM, "EnableFM");
	loadAttribute(CarrierIndex, "CarrierIndex");
	loadAttribute(ModulatorIndex, "ModulatorIndex");

}

ValueTree ModulatorSynthGroup::exportAsValueTree() const
{
	ValueTree v = ModulatorSynth::exportAsValueTree();

	saveAttribute(EnableFM, "EnableFM");
	saveAttribute(CarrierIndex, "CarrierIndex");
	saveAttribute(ModulatorIndex, "ModulatorIndex");

	return v;
}

void ModulatorSynthGroup::checkFmState()
{
	if (fmEnabled)
	{
		if (carrierIndex == -1 || getChildProcessor(carrierIndex) == nullptr)
		{
			fmState = "The carrier syntesizer is not valid.";
			fmCorrectlySetup = false;
		}
		else if (modIndex == -1 || getChildProcessor(modIndex) == nullptr)
		{
			fmState = "The modulation synthesizer is not valid.";
			fmCorrectlySetup = false;
		}
		else if (modIndex == carrierIndex)
		{
			fmState = "You can't use the same synthesiser as carrier and modulator.";
			fmCorrectlySetup = false;
		}
		else
		{
			fmState = "FM is working.";
			fmCorrectlySetup = true;
		}
	}
	else
	{
		fmState = "FM is deactivated";
		fmCorrectlySetup = false;
	}

	if (fmCorrectlySetup)
	{
		enablePitchModulation(true);

		static_cast<ModulatorSynth*>(getChildProcessor(carrierIndex))->enablePitchModulation(true);
	}

	

	sendChangeMessage();
}

void ModulatorSynthGroupVoice::calculateBlock(int startSample, int numSamples)
{
	ScopedLock sl(ownerSynth->getSynthLock());

	// Clear the buffer, since all child voices are added to this so it must be empty.
	voiceBuffer.clear();

	ModulatorSynthGroup *group = static_cast<ModulatorSynthGroup*>(getOwnerSynth());

	
	const float *voicePitchValues = getVoicePitchValues();

	if (group->fmIsCorrectlySetup())
	{
		// Calculate the modulator

		ModulatorSynth *modSynth = static_cast<ModulatorSynth*>(group->getChildProcessor(group->modIndex));
		jassert(modSynth != nullptr);

		ModulatorSynthVoice *modVoice = static_cast<ModulatorSynthVoice*>(modSynth->getVoice(voiceIndex));

		if (modSynth->isBypassed() || modVoice->isInactive()) return;

		modVoice->calculateVoicePitchValues(startSample, numSamples);

		const float modGain = modSynth->getGain();

		float *modPitchValues = modVoice->getVoicePitchValues();

		if (voicePitchValues != nullptr && modPitchValues != nullptr)
		{
			FloatVectorOperations::multiply(modPitchValues + startSample, voicePitchValues + startSample, numSamples);
		}

		modVoice->calculateBlock(startSample, numSamples);
		

		const float *modValues = modVoice->getVoiceValues(0, startSample); // Channel is the same;

		FloatVectorOperations::copy(fmModBuffer, modValues, numSamples);

		FloatVectorOperations::multiply(fmModBuffer, group->modSynthGainValues.getReadPointer(0, startSample), numSamples);

		FloatVectorOperations::multiply(fmModBuffer, modGain, numSamples);

		const float peak = FloatVectorOperations::findMaximum(fmModBuffer, numSamples);

		FloatVectorOperations::add(fmModBuffer, 1.0f, numSamples);

		modSynth->setPeakValues(peak, peak);

		// Calculate the carrier voice

		ModulatorSynth *carrierSynth = static_cast<ModulatorSynth*>(group->getChildProcessor(group->carrierIndex));
		jassert(carrierSynth != nullptr);

		ModulatorSynthVoice *carrierVoice = static_cast<ModulatorSynthVoice*>(carrierSynth->getVoice(voiceIndex));
		

		if (carrierSynth->isBypassed() || carrierVoice->isInactive()) return;
		
		carrierVoice->calculateVoicePitchValues(startSample, numSamples);

		float *carrierPitchValues = carrierVoice->getVoicePitchValues();

		FloatVectorOperations::add(carrierPitchValues + startSample, voicePitchValues + startSample, numSamples);

		// This is the magic FM command
		FloatVectorOperations::multiply(carrierPitchValues + startSample, fmModBuffer, numSamples);
        
#if JUCE_WINDOWS
		FloatVectorOperations::clip(carrierPitchValues + startSample, carrierPitchValues + startSample, 0.00000001f, 1000.0f, numSamples);
#endif
		carrierVoice->calculateBlock(startSample, numSamples);

		

		const float carrierGain = carrierSynth->getGain();

		voiceBuffer.copyFrom(0, startSample, carrierVoice->getVoiceValues(0, startSample), numSamples, carrierGain * carrierSynth->getBalance(false));
		voiceBuffer.copyFrom(1, startSample, carrierVoice->getVoiceValues(1, startSample), numSamples, carrierGain * carrierSynth->getBalance(true));

		const float peak2 = FloatVectorOperations::findMaximum(carrierVoice->getVoiceValues(0, startSample), numSamples);

		carrierSynth->setPeakValues(peak2, peak2);
	}
	else
	{
		ModulatorSynthGroup::ChildSynthIterator iterator(static_cast<ModulatorSynthGroup*>(ownerSynth), ModulatorSynthGroup::ChildSynthIterator::IterateAllSynths);
		ModulatorSynth *childSynth;

		while (iterator.getNextAllowedChild(childSynth))
		{
			ModulatorSynthVoice *childVoice = static_cast<ModulatorSynthVoice*>(childSynth->getVoice(voiceIndex));

			if (childSynth->isBypassed() || childVoice->isInactive()) continue;

			const float gain = childSynth->getGain();

			if (childVoice->isPitchModulationActive())
			{
				childVoice->calculateVoicePitchValues(startSample, numSamples);

				float *childPitchValues = childVoice->getVoicePitchValues();

				if (childPitchValues != nullptr && voicePitchValues != nullptr)
				{
					FloatVectorOperations::add(childPitchValues + startSample, voicePitchValues + startSample, numSamples);
				}
				
				
			}

			childVoice->calculateBlock(startSample, numSamples);

			childSynth->setPeakValues(gain, gain);

			voiceBuffer.addFrom(0, startSample, childVoice->getVoiceValues(0, startSample), numSamples, gain * childSynth->getBalance(false));
			voiceBuffer.addFrom(1, startSample, childVoice->getVoiceValues(1, startSample), numSamples, gain * childSynth->getBalance(true));
		}
	}

	

	getOwnerSynth()->effectChain->renderVoice(voiceIndex, voiceBuffer, startSample, numSamples);

	const float *modValues = getVoiceGainValues(startSample, numSamples);

	FloatVectorOperations::multiply(voiceBuffer.getWritePointer(0, startSample), modValues + startSample, numSamples);
	FloatVectorOperations::multiply(voiceBuffer.getWritePointer(1, startSample), modValues + startSample, numSamples);
};

void ModulatorSynthGroup::ModulatorSynthGroupHandler::add(Processor *newProcessor, Processor * /*siblingToInsertBefore*/)
{
	ScopedLock sl(group->lock);

	ModulatorSynth *m = dynamic_cast<ModulatorSynth*>(newProcessor);

	// Check incompatibilites with SynthGroups

	if(m->getChildProcessor(ModulatorSynth::EffectChain)->getNumChildProcessors() != 0)
	{
		if(AlertWindow::showOkCancelBox(AlertWindow::AlertIconType::WarningIcon, "Effects detected", "Synths that are added to a SynthGroup are not allowed to have effects.\n Press OK to create the synth with all effects removed"))
		{
			dynamic_cast<EffectProcessorChain*>(m->getChildProcessor(ModulatorSynth::EffectChain))->getHandler()->clear();
			m->setEditorState(ModulatorSynth::EffectChainShown, false);
		}
		else
		{
			return;
		}
	}
	else if(dynamic_cast<ModulatorSampler*>(m) != nullptr && m->getAttribute(ModulatorSampler::VoiceAmount) != group->getNumVoices())
	{
		if(AlertWindow::showOkCancelBox(AlertWindow::AlertIconType::WarningIcon, "Different Voice Amount detected", "StreamingSamplers that are added to a SynthGroup must have the same voice number as the SynthGroup\n Press OK to resize the voice amount."))
		{
			dynamic_cast<ModulatorSampler*>(m)->setAttribute(ModulatorSampler::VoiceAmount, (float)group->getNumVoices(), sendNotification);
		}
		else
		{
			return;
		}
	}

	m->setGroup(group);

	jassert(m != nullptr);
	group->synths.add(m);

	group->allowStates.setBit(group->synths.indexOf(m), true);

			

	for(int i = 0; i < group->getNumVoices(); i++)
	{
		static_cast<ModulatorSynthGroupVoice*>(group->getVoice(i))->addChildSynth(m);
	}

	m->prepareToPlay(group->getSampleRate(), group->getBlockSize());

	group->checkFmState();
	group->sendChangeMessage();

	sendChangeMessage();
}

NoMidiInputConstrainer::NoMidiInputConstrainer()
{
	Array<FactoryType::ProcessorEntry> typeNames;

    ADD_NAME_TO_TYPELIST(PolyFilterEffect);
    ADD_NAME_TO_TYPELIST(HarmonicFilter);
    ADD_NAME_TO_TYPELIST(StereoEffect);

	forbiddenModulators.addArray(typeNames);

	EnvelopeModulatorFactoryType envelopes(0, Modulation::Mode::GainMode, nullptr);

	forbiddenModulators.addArray(envelopes.getAllowedTypes());

	VoiceStartModulatorFactoryType voiceStart(0, Modulation::Mode::GainMode, nullptr);

	forbiddenModulators.addArray(voiceStart.getAllowedTypes());
}

SynthGroupConstrainer::SynthGroupConstrainer()
{
	Array<FactoryType::ProcessorEntry> typeNames;

	ADD_NAME_TO_TYPELIST(ModulatorSynthChain);
	ADD_NAME_TO_TYPELIST(GlobalModulatorContainer);
	ADD_NAME_TO_TYPELIST(ModulatorSynthGroup);

	forbiddenModulators.addArray(typeNames);
}
