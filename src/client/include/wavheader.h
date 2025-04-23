#ifndef WAV_HEADER_H
#define WAV_HEADER_H

/**
 * @file wavheader.h
 * @brief Defines the WAV file header structure
 * 
 * This file contains the definition of the WavHeader structure that represents
 * the header of a standard WAV audio file. It follows the RIFF format specification.
 */

/**
 * @struct WavHeader
 * @brief Structure representing a standard WAV file header
 * 
 * This structure exactly matches the binary format of a WAV file header,
 * making it possible to read WAV files directly into this structure.
 */
struct WavHeader {
  char riff[4];               /**< "RIFF" chunk descriptor (4 bytes) */
  unsigned int fileSize;      /**< Total file size minus 8 bytes */
  char wave[4];               /**< "WAVE" format identifier */
  char fmt[4];                /**< "fmt " chunk descriptor */
  unsigned int fmtSize;       /**< Size of format chunk (16 for PCM) */
  unsigned short audioFormat; /**< Audio format (1 for PCM) */
  unsigned short numChannels; /**< Number of channels */
  unsigned int sampleRate;    /**< Sample rate in Hz */
  unsigned int byteRate;      /**< Byte rate (SampleRate * NumChannels * BitsPerSample/8) */
  unsigned short blockAlign;  /**< Block align (NumChannels * BitsPerSample/8) */
  unsigned short bitsPerSample; /**< Bits per sample */
  char data[4];               /**< "data" chunk descriptor */
  unsigned int dataSize;      /**< Size of data chunk in bytes */
};

#endif // WAV_HEADER_H