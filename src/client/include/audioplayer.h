#include <string>

class AudioPlayerImpl;

class AudioPlayer {
public:
    /**
     * @brief Constructs a new AudioPlayer object
     */
    AudioPlayer();

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

    /**
     * @brief Pause playback
     */
    void pause();

    /**
     * @brief Stop playback
     */
    void stop();

    /**
     * @brief Get the current playback position in seconds
     * @return Current position in seconds
     */
    double getPosition() const;

    /**
     * @brief Check if the player is currently playing
     * @return true if playing, false otherwise
     */
    bool isPlaying() const;

private:
    std::unique_ptr<AudioPlayerImpl> impl;
};