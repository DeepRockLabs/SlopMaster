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

You can compile SlopMaster using GCC:
./gcc -Wall -Wextra -pedantic -std=c99 -o SlopMaster SlopMaster.c



## Usage
./SlopMaster [options]

Options:
  -i <input_dir>   Specify input directory (default: current directory)
  -o <output_dir>  Specify output directory (default: current directory)
  -m <master_type> Specify mastering type: default, synth, synthvocals, bassboost, rock, or piano
  -h               Display this help message

## Mastering Profiles

- default: General-purpose mastering suitable for most tracks
- synth: Optimized for instrumental synth tracks
- synthvocals: Tailored for synth tracks with vocals
- bassboost: Enhanced bass for dance and party music
- rock: Tailored for rock and guitar-heavy music
- piano: Optimized for piano and string-based relaxing music

## Supported File Formats

SlopMaster now supports processing the following audio file formats:
- WAV
- MP3
- AAC
- OGG
- FLAC

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
5. Stereo enhancement
6. Genre-specific effects (e.g., bass boost, reverb)

The exact parameters and effects vary depending on the chosen mastering profile.

## Output

Processed files are saved in WAV format with the following specifications:
- Sample rate: 48000 Hz
- Bit depth: 24-bit

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## Acknowledgements

This project would not be possible without [FFmpeg](https://ffmpeg.org/). We encourage users to support FFmpeg through donations or code contributions.
