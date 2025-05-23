#include "include/audioplayer.h"

#include <cstring>
#include <fstream>
#include <iostream>
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
    audioUnit = nullptr;
  }
}

bool AudioPlayer::load(const std::string& filePath) {
  // Always stop playback and cleanup when loading a new file
  if (audioUnit) {
    playing.store(false);
    AudioOutputUnitStop(audioUnit);
    AudioUnitUninitialize(audioUnit);
    AudioComponentInstanceDispose(audioUnit);
    audioUnit = nullptr;
  }

  std::ifstream file(filePath, std::ios::binary);
  if (!file) {
    std::cerr << "Could not open WAV file: " << filePath << std::endl;
    return false;
  }

  // Read header (checked tests)
  file.read(reinterpret_cast<char*>(&header), sizeof(WavHeader));
  if (std::strncmp(header.riff, "RIFF", 4) != 0 ||
      std::strncmp(header.wave, "WAVE", 4) != 0) {
    std::cerr << "Invalid WAV file format." << std::endl;
    return false;
  }

  // Read audio data
  audioData.clear();
  file.seekg(sizeof(WavHeader), std::ios::beg);
  audioData.assign(std::istreambuf_iterator<char>(file), {});
  currentPosition.store(sizeof(WavHeader));

  return setupAudioUnit();
}

bool AudioPlayer::loadFromMemory(const char* data, size_t size) {
  // Always stop playback and cleanup when loading a new file
  if (audioUnit) {
    playing.store(false);
    AudioOutputUnitStop(audioUnit);
    AudioUnitUninitialize(audioUnit);
    AudioComponentInstanceDispose(audioUnit);
    audioUnit = nullptr;
  }

  if (size < sizeof(WavHeader)) {
    std::cerr << "Data too small to be a valid WAV file." << std::endl;
    return false;
  }

  // Copy the header from the memory buffer
  std::memcpy(&header, data, sizeof(WavHeader));

  // Validate WAV header
  if (std::strncmp(header.riff, "RIFF", 4) != 0 ||
      std::strncmp(header.wave, "WAVE", 4) != 0) {
    std::cerr << "Invalid WAV file format in memory buffer." << std::endl;
    return false;
  }

  // Copy the audio data
  audioData.clear();
  audioData.assign(data + sizeof(WavHeader), data + size);
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
    return;
  }

  // Reset position to the beginning if we're at the end of the file
  unsigned int totalSize = sizeof(WavHeader) + audioData.size();
  if (currentPosition.load() >= totalSize) {
    currentPosition.store(sizeof(WavHeader));
  }

  // Only start the audio unit if we're not already playing
  if (!playing.load()) {
    playing.store(true);
    if (AudioOutputUnitStart(audioUnit) != noErr) {
      std::cerr << "Failed to start audio unit.\n";
      playing.store(false);
      return;
    }
    std::cout << "Started audio playback.\n";
  } else {
    std::cout << "Audio is already playing.\n";
  }
}

void AudioPlayer::pause() {
  if (playing.load()) {
    playing.store(false);
    AudioOutputUnitStop(audioUnit);
    std::cout << "Paused audio.\n";
  } else {
    std::cout << "Audio is already paused.\n";
  }
}

void AudioPlayer::resume() {
  if (!playing.load()) {
    playing.store(true);
    AudioOutputUnitStart(audioUnit);
    std::cout << "Resumed audio.\n";
  } else {
    std::cout << "Audio is already playing.\n";
  }
}

void AudioPlayer::stop() {
  if (playing.load()) {
    playing.store(false);
    currentPosition.store(sizeof(WavHeader));
    AudioOutputUnitStop(audioUnit);
    std::cout << "Stopped audio.\n";
  } else {
    std::cout << "Audio is already stopped.\n";
  }
}

unsigned int AudioPlayer::get_position() const {
  return currentPosition.load();
}

OSStatus AudioPlayer::RenderCallback(void* inRefCon,
                                     AudioUnitRenderActionFlags*,
                                     const AudioTimeStamp*, UInt32,
                                     UInt32 inNumberFrames,
                                     AudioBufferList* ioData) {
  AudioPlayer* player = static_cast<AudioPlayer*>(inRefCon);

  float* outBuffer = reinterpret_cast<float*>(ioData->mBuffers[0].mData);
  int channels = player->header.numChannels;
  int bytesPerSample = player->header.bitsPerSample / 8;
  int bytesPerFrame = bytesPerSample * channels;

  unsigned int position = player->currentPosition.load();
  unsigned int dataPosition =
      position -
      sizeof(WavHeader);  // Adjust position to be relative to audio data
  unsigned int bytesAvailable = player->audioData.size() - dataPosition;
  unsigned int framesAvailable = bytesAvailable / bytesPerFrame;

  UInt32 framesToRender = std::min(inNumberFrames, framesAvailable);

  // Read audio to buffer
  for (UInt32 i = 0; i < framesToRender; ++i) {
    for (int ch = 0; ch < channels; ++ch) {
      int idx = dataPosition + (i * bytesPerFrame) + (ch * bytesPerSample);
      int16_t sample =
          *reinterpret_cast<const int16_t*>(&player->audioData[idx]);
      outBuffer[i * channels + ch] = sample / 32768.0f;
    }
  }

  // Fill the rest with silence if done
  for (UInt32 i = framesToRender; i < inNumberFrames; ++i) {
    for (int ch = 0; ch < channels; ++ch) {
      outBuffer[i * channels + ch] = 0.0f;
    }
  }

  // Update position
  if (framesToRender > 0) {
    player->currentPosition.fetch_add(framesToRender * bytesPerFrame);
  }

  // If we've reached the end of the file, stop playback
  if (framesToRender < inNumberFrames) {
    player->playing.store(false);
    // Also stop the audio unit to prevent silent playback
    AudioOutputUnitStop(player->audioUnit);
  }

  return noErr;
}
