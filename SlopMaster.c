#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_PATH 1024
#define COMMAND_SIZE 4096

#define DEFAULT_MASTER 0
#define SYNTH_VOCAL_MASTER 1
#define ROCK_MASTER 2

// Function prototypes
int check_ffmpeg_installed(void);
void master_audio_file(const char* input_file, const char* output_file, int master_type);
int process_audio_files(const char* input_dir, const char* output_dir, int master_type);
void print_usage(const char* program_name);

// Global variable for logging
FILE* log_file = NULL;

int main(int argc, char *argv[]) {
    char input_dir[MAX_PATH] = ".";
    char output_dir[MAX_PATH] = ".";
    int opt;
    int master_type = DEFAULT_MASTER;

    // Open log file
    log_file = fopen("audioMaster.log", "a");
    if (log_file == NULL) {
        fprintf(stderr, "Error opening log file: %s\n", strerror(errno));
        return 1;
    }

    // Parse command line arguments
    while ((opt = getopt(argc, argv, "i:o:m:h")) != -1) {
        switch (opt) {
            case 'i':
                strncpy(input_dir, optarg, MAX_PATH - 1);
                input_dir[MAX_PATH - 1] = '\0';
                break;
            case 'o':
                strncpy(output_dir, optarg, MAX_PATH - 1);
                output_dir[MAX_PATH - 1] = '\0';
                break;
            case 'm':
                if (strcmp(optarg, "synth") == 0) {
                    master_type = SYNTH_VOCAL_MASTER;
                } else if (strcmp(optarg, "rock") == 0) {
                    master_type = ROCK_MASTER;
                } else if (strcmp(optarg, "default") == 0) {
                    master_type = DEFAULT_MASTER;
                } else {
                    fprintf(stderr, "Invalid master type. Using default.\n");
                }
                break;
            case 'h':
                print_usage(argv[0]);
                fclose(log_file);
                return 0;
            default:
                fprintf(stderr, "Unknown option: %c\n", opt);
                print_usage(argv[0]);
                fclose(log_file);
                return 1;
        }
    }

    if (!check_ffmpeg_installed()) {
        fprintf(stderr, "Error: FFmpeg is not installed or not in the system PATH.\n");
        fprintf(stderr, "Please install FFmpeg and make sure it's accessible from the command line. https://ffmpeg.org/\n");
        fprintf(log_file, "Error: FFmpeg not installed or not in PATH.\n");
        fclose(log_file);
        return 1;
    }

    printf("FFmpeg is installed. Processing audio files...\n");
    fprintf(log_file, "FFmpeg is installed. Processing audio files...\n");

    int result = process_audio_files(input_dir, output_dir, master_type);

    printf("Audio processing completed.\n");
    fprintf(log_file, "Audio processing completed.\n");

    fclose(log_file);
    return result;
}

int check_ffmpeg_installed(void) {
    return system("ffmpeg -version > /dev/null 2>&1") == 0;
}

void master_audio_file(const char* input_file, const char* output_file, int master_type) {
    char command[COMMAND_SIZE];
    const char* filter_complex;

    switch (master_type) {
        case SYNTH_VOCAL_MASTER:
            filter_complex = 
                "equalizer=f=100:width_type=o:width=2:g=1,"
                "equalizer=f=1000:width_type=o:width=2:g=2,"
                "equalizer=f=5000:width_type=o:width=2:g=3,"
                "equalizer=f=10000:width_type=o:width=2:g=1.5,"
                "compand=attacks=0:points=-80/-80|-50/-50|-40/-20|-30/-10|-20/-5|-10/-2|0/0|20/20,"
                "highpass=f=60,lowpass=f=16000,"
                "loudnorm=I=-10:LRA=5:TP=-1.5,"
                "acompressor=threshold=-8dB:ratio=3:attack=15:release=750:makeup=2dB";
            break;
        case ROCK_MASTER:
            filter_complex = 
                "equalizer=f=80:width_type=o:width=2:g=2,"
                "equalizer=f=250:width_type=o:width=2:g=-1,"
                "equalizer=f=2500:width_type=o:width=2:g=2,"
                "equalizer=f=5000:width_type=o:width=2:g=1.5,"
                "compand=attacks=0:points=-80/-80|-50/-50|-40/-25|-30/-15|-20/-10|-10/-5|0/0|20/20,"
                "highpass=f=40,lowpass=f=18000,"
                "loudnorm=I=-9:LRA=7:TP=-1,"
                "acompressor=threshold=-7dB:ratio=4:attack=20:release=1000:makeup=3dB";
            break;
        default:
            filter_complex = 
                "equalizer=f=100:width_type=o:width=2:g=1,"
                "equalizer=f=1000:width_type=o:width=2:g=1,"
                "equalizer=f=8000:width_type=o:width=2:g=2,"
                "compand=attacks=0:points=-80/-900|-45/-15|-27/-9|-15/-7|-5/-5|0/-3|20/-2,"
                "lowpass=f=18000,highpass=f=20,"
                "loudnorm=I=-8:LRA=6:TP=-1,"
                "acompressor=threshold=-6dB:ratio=4:attack=25:release=1000:makeup=2dB";
            break;
    }

    snprintf(command, COMMAND_SIZE,
        "ffmpeg -i \"%s\" -filter_complex \"%s\" -ar 44100 -acodec pcm_s24le \"%s\" 2>&1",
        input_file, filter_complex, output_file);

    FILE* fp = popen(command, "r");
    if (fp == NULL) {
        printf("Error executing FFmpeg command.\n");
        fprintf(log_file, "Error executing FFmpeg command for %s\n", input_file);
        return;
    }

    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        fprintf(log_file, "%s", buffer);
    }

    int status = pclose(fp);
    if (status == 0) {
        printf("Mastered: %s\n", input_file);
        fprintf(log_file, "Successfully mastered: %s\n", input_file);
    } else {
        printf("Error processing %s. FFmpeg exited with status: %d\n", input_file, status);
        fprintf(log_file, "Error processing %s. FFmpeg exited with status: %d\n", input_file, status);
    }
}

int process_audio_files(const char* input_dir, const char* output_dir, int master_type) {
    DIR *dir;
    struct dirent *entry;
    char input_file[MAX_PATH];
    char output_file[MAX_PATH];
    struct stat st;

    dir = opendir(input_dir);
    if (dir == NULL) {
        perror("Error opening input directory");
        fprintf(log_file, "Error opening input directory: %s\n", strerror(errno));
        return 1;
    }

    while ((entry = readdir(dir)) != NULL) {
        snprintf(input_file, MAX_PATH, "%s/%s", input_dir, entry->d_name);
        if (stat(input_file, &st) == 0 && S_ISREG(st.st_mode)) {  // If it's a regular file
            size_t name_len = strlen(entry->d_name);
            if (name_len > 4 && strcmp(entry->d_name + name_len - 4, ".wav") == 0) {
                snprintf(output_file, MAX_PATH, "%s/%.*sMastered.wav", output_dir, (int)(name_len - 4), entry->d_name);
                master_audio_file(input_file, output_file, master_type);
            }
        }
    }

    closedir(dir);
    return 0;
}

void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -i <input_dir>   Specify input directory (default: current directory)\n");
    printf("  -o <output_dir>  Specify output directory (default: current directory)\n");
    printf("  -m <master_type> Specify mastering type: default, synth, or rock\n");
    printf("  -h               Display this help message\n");
}