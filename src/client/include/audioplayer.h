#include <string>

class AudioPlayer {
public:
    /**
     * @brief Constructs a new AudioPlayer object
     */
    AudioPlayer();

    /**
     * @brief Load a specific song
     * @param songPath path to the specific song
     * @return true if request was sent successfully, false otherwise
     */
    bool loadSong(const std::string& songPath);

    /**
     * @brief play a loaded song
     */
    void play();
};