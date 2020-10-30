/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <random>

using namespace juce;


random_device rd;
mt19937 gen(rd());
uniform_real_distribution<> dis(0.0, 1.0);

//==============================================================================
MarkovChainGeneratorAudioProcessor::MarkovChainGeneratorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{

    // Insert arbitary chords here (text=
    chords = {
                 "F",
                 "Em7",
                 "A7",
                 "Dm",
                 "Dm7",
                 "Bb",
                 "C7",
                 "F",
                 "C",
                 "Dm7",
                 "G7",
                 "Bb",
                 "F",
                 "F",
                 "Em7",
                 "A7",
                 "Dm",
                 "Dm7",
                 "Bb",
                 "C7",
                 "F",
                 "C",
                 "Dm7",
                 "G7",
                 "Bb",
                 "F",
                 "A7sus4",
                 "A7",
                 "Dm",
                 "C",
                 "Bb",
                 "Dm",
                 "Gm6",
                 "C7",
                 "F",
                 "A7sus4",
                 "A7",
                 "Dm",
                 "C",
                 "Bb",
                 "Dm",
                 "Gm6",
                 "C7",
                 "F",
                 "Fsus4",
                 "F",
                 "F",
                 "Em7",
                 "A7",
                 "Dm",
                 "Dm7",
                 "Bb",
                 "C7",
                 "F",
                 "C",
                 "Dm7",
                 "G7",
                 "Bb",
                 "F",
                 "A7sus4",
                 "A7",
                 "Dm",
                 "C",
                 "Bb",
                 "Dm",
                 "Gm6",
                 "C7",
                 "F",
                 "A7sus4",
                 "A7",
                 "Dm",
                 "C",
                 "Bb",
                 "Dm",
                 "Gm6",
                 "C7",
                 "F",
                 "Fsus4",
                 "F",
                 "F",
                 "Em7",
                 "A7",
                 "Dm",
                 "Dm7",
                 "Bb",
                 "C7",
                 "F",
                 "C",
                 "Dm7",
                 "G7",
                 "Bb",
                 "F",
                 "G7",
                 "Bb",
                 "F"
    };
    numOfFrequencies = 0;

    // Make chord pairs
    for (int i = 0; i < chords.size() - 1; i++)
    {
        chordsBigrams.push_back(chords[i] + " " + chords[i + 1]);
    }

    // Calculate the frequency of chord pairs
    for (auto& c : chordsBigrams)
    {
        if (!chordsFrequencies.count(c))
            chordsFrequencies.insert(pair<string, int>(c, 1));
        else
            chordsFrequencies[c]++;
    }

    probabilities = chordsFrequencies;

    // Total num of frequencies
    for (auto f : chordsFrequencies)
        numOfFrequencies += f.second;

    // Calculate probablities (0-1) for every chord pair
    for (auto p : probabilities)
        probabilities[p.first] = probabilities[p.first] / numOfFrequencies;

    // Default first chord
    chord = "F";

    // MIDI values for F Major
    noteValues[0] = 65;
    noteValues[1] = 69;
    noteValues[2] = 72;
    noteValues[3] = 77;
    
    // Save current MIDI values
    currentMidiNotes[0] = 65;
    currentMidiNotes[1] = 69;
    currentMidiNotes[2] = 72;
    currentMidiNotes[3] = 77;

    // Arpeggiators inital values
    arpeggiators[0] = true;
    arpeggiators[1] = false;
    arpeggiators[2] = false;
    arpeggiators[3] = false;

    addParameter(isArpeggiated = new AudioParameterBool("ARP", "Arpeggiator", false));
    addParameter(chordTime = new AudioParameterInt("CHORDT", "Chord Time (ms)", 200, 5000, 2000));
    addParameter(arpTime = new AudioParameterInt("ARPT", "Arpeggiator Time (ms)", 50, 2000, 250));

    // TIMERS
    // Timer 0 for one chordLength
    startTimer(0, *chordTime);
    // Timer 1 for arp instance
    startTimer(1, *arpTime);

    // Flag used to turn off MIDI when changing chords
    triggerNoteOff = true;
}

MarkovChainGeneratorAudioProcessor::~MarkovChainGeneratorAudioProcessor()
{
    stopTimer(0);
    stopTimer(1);
}

//==============================================================================
const juce::String MarkovChainGeneratorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool MarkovChainGeneratorAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool MarkovChainGeneratorAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool MarkovChainGeneratorAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double MarkovChainGeneratorAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int MarkovChainGeneratorAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int MarkovChainGeneratorAudioProcessor::getCurrentProgram()
{
    return 0;
}

void MarkovChainGeneratorAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String MarkovChainGeneratorAudioProcessor::getProgramName (int index)
{
    return {};
}

void MarkovChainGeneratorAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void MarkovChainGeneratorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void MarkovChainGeneratorAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MarkovChainGeneratorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void MarkovChainGeneratorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Turn MIDI notes off when changing chords
    if(triggerNoteOff == true) 
    {
        midiMessages.addEvent(MidiMessage::noteOff(1, currentMidiNotes[0]), midiMessages.getLastEventTime() + 1);
        midiMessages.addEvent(MidiMessage::noteOff(1, currentMidiNotes[1]), midiMessages.getLastEventTime() + 1);
        midiMessages.addEvent(MidiMessage::noteOff(1, currentMidiNotes[2]), midiMessages.getLastEventTime() + 1);
        midiMessages.addEvent(MidiMessage::noteOff(1, currentMidiNotes[3]), midiMessages.getLastEventTime() + 1);

        triggerNoteOff = false;
    }
    else 
    {
        if (*isArpeggiated == false) {
            midiMessages.addEvent(MidiMessage::noteOn(1, noteValues[0], (juce::uint8) 100), midiMessages.getLastEventTime() + 1);
            midiMessages.addEvent(MidiMessage::noteOn(1, noteValues[1], (juce::uint8) 100), midiMessages.getLastEventTime() + 1);
            midiMessages.addEvent(MidiMessage::noteOn(1, noteValues[2], (juce::uint8) 100), midiMessages.getLastEventTime() + 1);
            midiMessages.addEvent(MidiMessage::noteOn(1, noteValues[3], (juce::uint8) 100), midiMessages.getLastEventTime() + 1);
        }

        currentMidiNotes[0] = noteValues[0];
        currentMidiNotes[1] = noteValues[1];
        currentMidiNotes[2] = noteValues[2];
        currentMidiNotes[3] = noteValues[3];

        if (*isArpeggiated == true)
        {
            if (arpeggiators[0])
            {
                midiMessages.addEvent(MidiMessage::noteOff(1, currentMidiNotes[3]), midiMessages.getLastEventTime() + 1);
                midiMessages.addEvent(MidiMessage::noteOn(1, noteValues[0], (juce::uint8) 100), midiMessages.getLastEventTime() + 1);
            }
            else if (arpeggiators[1])
            {
                midiMessages.addEvent(juce::MidiMessage::noteOff(1, currentMidiNotes[0]), midiMessages.getLastEventTime() + 1);
                midiMessages.addEvent(MidiMessage::noteOn(1, noteValues[1], (juce::uint8) 100), midiMessages.getLastEventTime() + 1);
            }
            else if (arpeggiators[2])
            {
                midiMessages.addEvent(juce::MidiMessage::noteOff(1, currentMidiNotes[1]), midiMessages.getLastEventTime() + 1);
                midiMessages.addEvent(MidiMessage::noteOn(1, noteValues[2], (juce::uint8) 100), midiMessages.getLastEventTime() + 1);
            }
            else if (arpeggiators[3])
            {
                midiMessages.addEvent(MidiMessage::noteOff(1, currentMidiNotes[2]), midiMessages.getLastEventTime() + 1);
                midiMessages.addEvent(MidiMessage::noteOn(1, noteValues[3], (juce::uint8) 100), midiMessages.getLastEventTime() + 1);
            }
        }
    }

}

//==============================================================================
bool MarkovChainGeneratorAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* MarkovChainGeneratorAudioProcessor::createEditor()
{
    return new MarkovChainGeneratorAudioProcessorEditor (*this);
}

//==============================================================================
void MarkovChainGeneratorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void MarkovChainGeneratorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MarkovChainGeneratorAudioProcessor();
}

void MarkovChainGeneratorAudioProcessor::timerCallback(int timerID)
{
    if (timerID == 0)
    {
        unordered_map<string, double> bigramsWithSelectedChord;
        pair<string, double> selected;

        do
        {
            for (auto p : probabilities)
            {
                if (p.first.find(chord) == 0)
                {
                    bigramsWithSelectedChord.insert(p);
                }
            }

            // Random probability (0-1)
            double probability = dis(gen);

            for (auto b : bigramsWithSelectedChord)
            {
                if (b.second > probability)
                {
                    selected = b;
                    break;
                }
            }
        } while (selected.second == 0);

        DBG(chord);

        char* token;

        token = strtok(&selected.first[0], " ");
        token = strtok(NULL, " ");

        chord = token;

        if (chord == "F")
        {
            noteValues[0] = 65;
            noteValues[1] = 69;
            noteValues[2] = 72;
            noteValues[3] = 77;
        }
        else if (chord == "Em7")
        {
            noteValues[0] = 64;
            noteValues[1] = 67;
            noteValues[2] = 71;
            noteValues[3] = 74;
        }
        else if (chord == "A7")
        {
            noteValues[0] = 69;
            noteValues[1] = 73;
            noteValues[2] = 76;
            noteValues[3] = 79;
        }
        else if (chord == "Dm")
        {
            noteValues[0] = 62;
            noteValues[1] = 65;
            noteValues[2] = 69;
            noteValues[3] = 74;
        }
        else if (chord == "Dm7")
        {
            noteValues[0] = 62;
            noteValues[1] = 65;
            noteValues[2] = 69;
            noteValues[3] = 72;
        }
        else if (chord == "Bb")
        {
            noteValues[0] = 70;
            noteValues[1] = 74;
            noteValues[2] = 77;
            noteValues[3] = 82;
        }
        else if (chord == "C7")
        {
            noteValues[0] = 84;
            noteValues[1] = 88;
            noteValues[2] = 91;
            noteValues[3] = 94;
        }
        else if (chord == "C")
        {
            noteValues[0] = 84;
            noteValues[1] = 88;
            noteValues[2] = 91;
            noteValues[3] = 96;
        }
        else if (chord == "G7")
        {
            noteValues[0] = 79;
            noteValues[1] = 83;
            noteValues[2] = 86;
            noteValues[3] = 89;
        }
        else if (chord == "A7sus4")
        {
            noteValues[0] = 64;
            noteValues[1] = 74;
            noteValues[2] = 76;
            noteValues[3] = 79;
        }
        else if (chord == "Gm6")
        {
            noteValues[0] = 76;
            noteValues[1] = 79;
            noteValues[2] = 82;
            noteValues[3] = 86;
        }
        else if (chord == "Fsus4")
        {
            noteValues[0] = 77;
            noteValues[1] = 82;
            noteValues[2] = 84;
            noteValues[3] = 89;
        }

        triggerNoteOff = true;

        stopTimer(0);
        startTimer(0, *chordTime);
    }

    if (timerID == 1)
    {
        if (arpeggiators[0])
        {
            arpeggiators[0] = false;
            arpeggiators[1] = true;
        }
        else if (arpeggiators[1])
        {
            arpeggiators[1] = false;
            arpeggiators[2] = true;
        }
        else if (arpeggiators[2])
        {
            arpeggiators[2] = false;
            arpeggiators[3] = true;
        }
        else if (arpeggiators[3])
        {
            arpeggiators[3] = false;
            arpeggiators[0] = true;
        }

        stopTimer(1);
        startTimer(1, *arpTime);
    }
}
