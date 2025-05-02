#pragma once

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include "client/include/audioplayer.h"

// Global variable to store the AudioPlayer instance for the callback
extern AudioPlayer* g_player;

// CoreAudio mock function declarations
extern "C" {
    // Mock AudioComponentFindNext
    AudioComponent MockAudioComponentFindNext(AudioComponent inComponent, const AudioComponentDescription* inDesc);
    
    // Mock AudioComponentInstanceNew
    OSStatus MockAudioComponentInstanceNew(AudioComponent inComponent, AudioUnit* outInstance);
    
    // Mock AudioUnitSetProperty
    OSStatus MockAudioUnitSetProperty(AudioUnit inUnit, 
                                     AudioUnitPropertyID inID, 
                                     AudioUnitScope inScope, 
                                     AudioUnitElement inElement, 
                                     const void* inData, 
                                     UInt32 inDataSize);
    
    // Mock AudioUnitInitialize
    OSStatus MockAudioUnitInitialize(AudioUnit inUnit);
    
    // Mock AudioOutputUnitStart
    OSStatus MockAudioOutputUnitStart(AudioUnit inUnit);
    
    // Mock AudioOutputUnitStop
    OSStatus MockAudioOutputUnitStop(AudioUnit inUnit);
    
    // Mock AudioUnitUninitialize
    OSStatus MockAudioUnitUninitialize(AudioUnit inUnit);
    
    // Mock AudioComponentInstanceDispose
    OSStatus MockAudioComponentInstanceDispose(AudioUnit inUnit);
    
    // Mock RenderCallback function
    OSStatus MockRenderCallback(void* inRefCon,
                               AudioUnitRenderActionFlags* ioActionFlags,
                               const AudioTimeStamp* inTimeStamp,
                               UInt32 inBusNumber,
                               UInt32 inNumberFrames,
                               AudioBufferList* ioData);
}

// Macro to enable CoreAudio mocks in test builds
#ifdef TESTING
    // Replace the real CoreAudio functions with our mocks
    #define AudioComponentFindNext MockAudioComponentFindNext
    #define AudioComponentInstanceNew MockAudioComponentInstanceNew
    #define AudioUnitSetProperty MockAudioUnitSetProperty
    #define AudioUnitInitialize MockAudioUnitInitialize
    #define AudioOutputUnitStart MockAudioOutputUnitStart
    #define AudioOutputUnitStop MockAudioOutputUnitStop
    #define AudioUnitUninitialize MockAudioUnitUninitialize
    #define AudioComponentInstanceDispose MockAudioComponentInstanceDispose
    #define RenderCallback MockRenderCallback
#endif
