#include "../include/coreaudio_mocks.h"
#include <algorithm>
#include <cstring>

// Global variable to store the AudioPlayer instance for the callback
AudioPlayer* g_player = nullptr;

// CoreAudio mock function implementations
extern "C" {
    // Mock AudioComponentFindNext
    AudioComponent MockAudioComponentFindNext(AudioComponent inComponent, const AudioComponentDescription* inDesc) {
        return (AudioComponent)0x1234; // Return a dummy component
    }
    
    // Mock AudioComponentInstanceNew
    OSStatus MockAudioComponentInstanceNew(AudioComponent inComponent, AudioUnit* outInstance) {
        *outInstance = (AudioUnit)0x5678; // Return a dummy audio unit
        return noErr;
    }
    
    // Mock AudioUnitSetProperty
    OSStatus MockAudioUnitSetProperty(AudioUnit inUnit, 
                                     AudioUnitPropertyID inID, 
                                     AudioUnitScope inScope, 
                                     AudioUnitElement inElement, 
                                     const void* inData, 
                                     UInt32 inDataSize) {
        return noErr;
    }
    
    // Mock AudioUnitInitialize
    OSStatus MockAudioUnitInitialize(AudioUnit inUnit) {
        return noErr;
    }
    
    // Mock AudioOutputUnitStart
    OSStatus MockAudioOutputUnitStart(AudioUnit inUnit) {
        return noErr;
    }
    
    // Mock AudioOutputUnitStop
    OSStatus MockAudioOutputUnitStop(AudioUnit inUnit) {
        return noErr;
    }
    
    // Mock AudioUnitUninitialize
    OSStatus MockAudioUnitUninitialize(AudioUnit inUnit) {
        return noErr;
    }
    
    // Mock AudioComponentInstanceDispose
    OSStatus MockAudioComponentInstanceDispose(AudioUnit inUnit) {
        return noErr;
    }
    
    // Mock RenderCallback function
    OSStatus MockRenderCallback(void* inRefCon,
                               AudioUnitRenderActionFlags* ioActionFlags,
                               const AudioTimeStamp* inTimeStamp,
                               UInt32 inBusNumber,
                               UInt32 inNumberFrames,
                               AudioBufferList* ioData) {
        // Use the global player instance
        AudioPlayer* player = g_player;
        if (!player) {
            return -1; // Error
        }
        
        // Get the current position
        unsigned int position = player->get_position();
        unsigned int dataPosition = position - sizeof(WavHeader);  // Adjust position to be relative to audio data
        
        // Get the audio data
        const std::vector<char>& audioData = player->get_audio_data();
        
        // Calculate bytes per frame
        int channels = player->get_header().numChannels;
        int bytesPerSample = player->get_header().bitsPerSample / 8;
        int bytesPerFrame = bytesPerSample * channels;
        
        // Calculate how many frames we can provide
        unsigned int bytesAvailable = audioData.size() - dataPosition;
        unsigned int framesAvailable = bytesAvailable / bytesPerFrame;
        
        // Calculate how many frames to render
        UInt32 framesToRender = std::min(inNumberFrames, framesAvailable);
        
        // Fill the output buffer with audio data
        float* outBuffer = reinterpret_cast<float*>(ioData->mBuffers[0].mData);
        
        // Read audio to buffer
        for (UInt32 i = 0; i < framesToRender; ++i) {
            for (int ch = 0; ch < channels; ++ch) {
                int idx = dataPosition + (i * bytesPerFrame) + (ch * bytesPerSample);
                int16_t sample = *reinterpret_cast<const int16_t*>(&audioData[idx]);
                outBuffer[i * channels + ch] = sample / 32768.0f;
            }
        }
        
        // Fill the rest with silence if done
        for (UInt32 i = framesToRender; i < inNumberFrames; ++i) {
            for (int ch = 0; ch < channels; ++ch) {
                outBuffer[i * channels + ch] = 0.0f;
            }
        }
        
        // Update the position
        unsigned int currentPos = player->get_position();
        player->set_position(currentPos + framesToRender * bytesPerFrame);
        
        // If we've reached the end of the audio data, stop playing
        if (framesToRender < inNumberFrames) {
            player->set_playing(false);
        }
        
        return noErr;
    }
}
