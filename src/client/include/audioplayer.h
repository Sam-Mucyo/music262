#pragma once

#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <atomic>
#include <string>
#include <vector>
#include "wavheader.h"

class AudioPlayer {
public:
    /**
     * @brief Constructs a new AudioPlayer object
     */
    AudioPlayer();
    ~AudioPlayer();

    /**
     * @brief Load a specific song
     * @param filePath path to the specific song
     * @return true if the file was loaded successfully, false otherwise
     */
    bool load(const std::string& filePath);

    /**
     * @brief Play a loaded song
     */
    void play();

private:
    static OSStatus RenderCallback(void* inRefCon,
                                   AudioUnitRenderActionFlags* ioActionFlags,
                                   const AudioTimeStamp* inTimeStamp,
                                   UInt32 inBusNumber,
                                   UInt32 inNumberFrames,
                                   AudioBufferList* ioData);

    bool setupAudioUnit();

    WavHeader header;
    std::vector<char> audioData;

    std::atomic<bool> playing;
    std::atomic<unsigned int> currentPosition;

    AudioUnit audioUnit;
    AudioStreamBasicDescription audioFormat;
};