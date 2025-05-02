#pragma once

#ifdef __APPLE__
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#endif
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
   * @brief Load a specific song from file
   * @param songPath path to the specific song
   * @return true if request was sent successfully, false otherwise
   */
  bool load(const std::string& songPath);

  /**
   * @brief Load a specific song from memory
   * @param data pointer to the audio data in memory
   * @param size size of the audio data in bytes
   * @return true if loaded successfully, false otherwise
   */
  bool loadFromMemory(const char* data, size_t size);

  /**
   * @brief play a loaded song
   */
  void play();

  /**
   * @brief pause the currently playing song
   */
  void pause();

  /**
   * @brief resume the currently paused song at the last position
   */
  void resume();

  /**
   * @brief stop the currently playing song and reset position
   */
  void stop();

  /**
   * @brief get the current position of the song
   * @return the current position in bytes
   */
  unsigned int get_position() const;

  /**
   * @brief Check if the player is currently playing
   * @return true if playing, false otherwise
   */
  bool isPlaying() const { return playing.load(); }

  /**
   * @brief Get the WAV header
   * @return The WAV header
   */
  const WavHeader& get_header() const { return header; }

  /**
   * @brief Get the audio data
   * @return The audio data
   */
  const std::vector<char>& get_audio_data() const { return audioData; }

  /**
   * @brief Set the current position (for testing)
   * @param position The new position
   */
  void set_position(unsigned int position) { currentPosition.store(position); }

  /**
   * @brief Set the playing state (for testing)
   * @param isPlaying The new playing state
   */
  void set_playing(bool isPlaying) { playing.store(isPlaying); }

 private:
  static OSStatus RenderCallback(void* inRefCon,
                                 AudioUnitRenderActionFlags* ioActionFlags,
                                 const AudioTimeStamp* inTimeStamp,
                                 UInt32 inBusNumber, UInt32 inNumberFrames,
                                 AudioBufferList* ioData);

  bool setupAudioUnit();

  WavHeader header;
  std::vector<char> audioData;

  std::atomic<bool> playing;
  std::atomic<unsigned int> currentPosition;

  AudioUnit audioUnit;
  AudioStreamBasicDescription audioFormat;
};