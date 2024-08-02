# SlopMaster

SlopMaster is a C program that processes audio files using [FFmpeg](https://ffmpeg.org/), applying various audio filters for mastering. It now supports multi-threading for faster processing and allows users to specify the output format.


## About FFmpeg

This project relies heavily on [FFmpeg](https://ffmpeg.org/), a powerful multimedia framework. FFmpeg is an essential tool in the audio and video processing world, and it's what makes SlopMaster possible. 

**Please consider supporting FFmpeg:**
- Donate to FFmpeg: https://ffmpeg.org/donations.html
- Contribute code: https://ffmpeg.org/developer.html

Your support helps maintain and improve this crucial open-source project.

## Prerequisites

- GCC compiler
- [FFmpeg](https://ffmpeg.org/) 4.3 or later installed and accessible from the command line
- POSIX-compliant system (Linux, macOS, etc.)
- pthread library

## Compilation

You can compile SlopMaster using GCC:
gcc -o SlopMaster SlopMaster.c -lpthread $(pkg-config --cflags --libs libavcodec libavformat libavutil libswresample) -lm

## Usage
./SlopMaster [options]

Options:
  -i <input_dir>   Specify input directory (default: current directory)
  -o <output_dir>  Specify output directory (default: current directory)
  -v               Enable vocal mode for processing songs with vocals
  -f <format>      Specify output format (wav, flac, or mp3; default: wav)
  -n               Enable verbose mode
  -h               Display this help message

## Supported File Formats

SlopMaster supports processing the following audio file formats:
- WAV
- MP3
- AAC
- OGG
- FLAC

## How It Works

SlopMaster uses FFmpeg's powerful audio filtering capabilities to apply a series of audio processing steps:

1. Channel layout adjustment
2. High-pass and low-pass filtering
3. Noise reduction
4. Multi-band compression
5. Equalization
6. Compression
7. Stereo enhancement
8. Loudness normalization
9. Limiting
10. Volume adjustment

Additional vocal-specific processing is applied when vocal mode is enabled.

## Output

Processed files are saved in the specified format (WAV, FLAC, or MP3) with the following specifications:
- Sample rate: 48000 Hz
- Bit depth: 24-bit (for WAV and FLAC)

## Multi-threading

SlopMaster now utilizes multi-threading to process multiple files simultaneously, significantly improving performance on multi-core systems.


## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Acknowledgements

This project would not be possible without [FFmpeg](https://ffmpeg.org/). We encourage users to support FFmpeg through donations or code contributions.
