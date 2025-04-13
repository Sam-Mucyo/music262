#include <fstream>
#include <iostream>
#include <cstring>
#include <thread>
#include "include/audioplayer.h"
#define _GLIBCXX_USE_NANOSLEEP

// Constructor
AudioPlayer::AudioPlayer()
    : playing(false), currentPosition(0), audioUnit(nullptr) {}

// Destructor
AudioPlayer::~AudioPlayer() {
    playing.store(false);
    if (audioUnit) {
        AudioOutputUnitStop(audioUnit);
        AudioUnitUninitialize(audioUnit);
        AudioComponentInstanceDispose(audioUnit);
    }
}

bool AudioPlayer::load(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) {
        std::cerr << "Could not open WAV file: " << filePath << std::endl;
        return false;
    }

    // Read header
    file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));
    if (std::strncmp(header.riff, "RIFF", 4) != 0 ||
        std::strncmp(header.wave, "WAVE", 4) != 0) {
        std::cerr << "Invalid WAV file format." << std::endl;
        return false;
    }

    // Read audio data
    audioData.clear();
    file.seekg(sizeof(WavHeader), std::ios::beg);
    audioData.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    currentPosition.store(0);

    return setupAudioUnit();
}

bool AudioPlayer::setupAudioUnit() {
    AudioComponentDescription desc = {};
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent component = AudioComponentFindNext(nullptr, &desc);
    if (!component) {
        std::cerr << "Audio component not found." << std::endl;
        return false;
    }

    if (AudioComponentInstanceNew(component, &audioUnit) != noErr) {
        std::cerr << "Failed to create audio unit." << std::endl;
        return false;
    }

    // Configure format
    audioFormat.mSampleRate = header.sampleRate;
    audioFormat.mFormatID = kAudioFormatLinearPCM;
    audioFormat.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    audioFormat.mFramesPerPacket = 1;
    audioFormat.mChannelsPerFrame = header.numChannels;
    audioFormat.mBitsPerChannel = 32;
    audioFormat.mBytesPerFrame = audioFormat.mChannelsPerFrame * sizeof(float);
    audioFormat.mBytesPerPacket = audioFormat.mBytesPerFrame;

    if (AudioUnitSetProperty(audioUnit, kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Input, 0, &audioFormat,
                             sizeof(audioFormat)) != noErr) {
        std::cerr << "Failed to set stream format." << std::endl;
        return false;
    }

    AURenderCallbackStruct callbackStruct = {};
    callbackStruct.inputProc = RenderCallback;
    callbackStruct.inputProcRefCon = this;

    if (AudioUnitSetProperty(audioUnit, kAudioUnitProperty_SetRenderCallback,
                             kAudioUnitScope_Input, 0, &callbackStruct,
                             sizeof(callbackStruct)) != noErr) {
        std::cerr << "Failed to set render callback." << std::endl;
        return false;
    }

    if (AudioUnitInitialize(audioUnit) != noErr) {
        std::cerr << "Failed to initialize audio unit." << std::endl;
        return false;
    }

    return true;
}

void AudioPlayer::play() {
    if (audioData.empty()) {
        std::cerr << "No audio data loaded.\n";
    }

    if (AudioOutputUnitStart(audioUnit) != noErr) {
        std::cerr << "Failed to start audio unit.\n";
    }

    std::cout << "Playing audio...\n";

    playing.store(true);

    while (playing.load()) {
        // Wait for playback to finish
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Playback finished.\n";
}

OSStatus AudioPlayer::RenderCallback(void* inRefCon,
                                     AudioUnitRenderActionFlags*,
                                     const AudioTimeStamp*,
                                     UInt32,
                                     UInt32 inNumberFrames,
                                     AudioBufferList* ioData) {
    AudioPlayer* player = static_cast<AudioPlayer*>(inRefCon);

    float* outBuffer = reinterpret_cast<float*>(ioData->mBuffers[0].mData);
    int channels = player->header.numChannels;
    int bytesPerSample = player->header.bitsPerSample / 8;
    int bytesPerFrame = bytesPerSample * channels;

    unsigned int position = player->currentPosition.load();
    unsigned int bytesAvailable = player->audioData.size() - position;
    unsigned int framesAvailable = bytesAvailable / bytesPerFrame;

    UInt32 framesToRender = std::min(inNumberFrames, framesAvailable);

    for (UInt32 i = 0; i < framesToRender; ++i) {
        for (int ch = 0; ch < channels; ++ch) {
            int idx = position + (i * bytesPerFrame) + (ch * bytesPerSample);
            int16_t sample = *reinterpret_cast<int16_t*>(&player->audioData[idx]);
            outBuffer[i * channels + ch] = sample / 32768.0f;
        }
    }

    // Fill the rest with silence if done
    for (UInt32 i = framesToRender; i < inNumberFrames; ++i) {
        for (int ch = 0; ch < channels; ++ch) {
            outBuffer[i * channels + ch] = 0.0f;
        }
    }

    player->currentPosition.fetch_add(framesToRender * bytesPerFrame);
    if (framesToRender < inNumberFrames) {
        player->playing.store(false);
    }

    return noErr;
}