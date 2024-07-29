# SlopMaster

SlopMaster is a C program that processes WAV audio files using [FFmpeg](https://ffmpeg.org/), applying various audio filters for mastering. It offers different mastering profiles suitable for various genres and styles.

## About FFmpeg

This project relies heavily on [FFmpeg](https://ffmpeg.org/), a powerful multimedia framework. FFmpeg is an essential tool in the audio and video processing world, and it's what makes SlopMaster possible. 

**Please consider supporting FFmpeg:**
- Donate to FFmpeg: https://ffmpeg.org/donations.html
- Contribute code: https://ffmpeg.org/developer.html

Your support helps maintain and improve this crucial open-source project.

## Prerequisites

- GCC compiler
- [FFmpeg](https://ffmpeg.org/) installed and accessible from the command line

## Compilation

You can compile SlopMaster using the provided Makefile:
./make

Or manually with GCC:
./gcc -Wall -Wextra -pedantic -std=c99 -o SlopMaster SlopMaster.c



## Usage
./SlopMaster [options]

Options:
  -i <input_dir>   Specify input directory (default: current directory)
  -o <output_dir>  Specify output directory (default: current directory)
  -m <master_type> Specify mastering type: default, synth, or rock
  -h               Display this help message

## Mastering Profiles

- default: General-purpose mastering suitable for most tracks
- synth: Optimized for synth tracks with vocals
- rock: Tailored for rock music

## Examples

Process all WAV files in the current directory using the default mastering profile:
./SlopMaster

Process files in a specific directory using the synth mastering profile:
./SlopMaster -m synth

## How It Works

SlopMaster uses FFmpeg's powerful audio filtering capabilities to apply a series of audio processing steps:

1. Equalization
2. Compression
3. Limiting
4. Normalization

The exact parameters vary depending on the chosen mastering profile.

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Acknowledgements

This project would not be possible without [FFmpeg](https://ffmpeg.org/). We encourage users to support FFmpeg through donations or code contributions.