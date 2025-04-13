#ifndef WAV_HEADER_H
#define WAV_HEADER_H

// WAV file header structure
struct WavHeader {
  char riff[4];               // "RIFF"
  unsigned int fileSize;      // Total file size minus 8 bytes
  char wave[4];               // "WAVE"
  char fmt[4];                // "fmt "
  unsigned int fmtSize;       // Size of format chunk (16 for PCM)
  unsigned short audioFormat; // Audio format (1 for PCM)
  unsigned short numChannels; // Number of channels
  unsigned int sampleRate;    // Sample rate
  unsigned int byteRate;      // Byte rate (SampleRate * NumChannels * BitsPerSample/8)
  unsigned short blockAlign;  // Block align (NumChannels * BitsPerSample/8)
  unsigned short bitsPerSample; // Bits per sample
  char data[4];               // "data"
  unsigned int dataSize;      // Size of data chunk
};

#endif // WAV_HEADER_H