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
#define COMMAND_SIZE 262144  // 256KB

#define DEFAULT_MASTER 0
#define SYNTH_MASTER 1
#define SYNTH_VOCALS_MASTER 2
#define BASS_BOOST_MASTER 3
#define ROCK_MASTER 4
#define PIANO_MASTER 5

int check_ffmpeg_installed(void);
void master_audio_file(const char* input_file, const char* output_file, int master_type);
int process_audio_files(const char* input_dir, const char* output_dir, int master_type);
void print_usage(const char* program_name);

FILE* log_file = NULL;

int main(int argc, char *argv[]) {
    char input_dir[MAX_PATH] = ".";
    char output_dir[MAX_PATH] = ".";
    int opt;
    int master_type = DEFAULT_MASTER;

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
                if (strcmp(optarg, "synth") == 0) master_type = SYNTH_MASTER;
                else if (strcmp(optarg, "synthvocals") == 0) master_type = SYNTH_VOCALS_MASTER;
                else if (strcmp(optarg, "bassboost") == 0) master_type = BASS_BOOST_MASTER;
                else if (strcmp(optarg, "rock") == 0) master_type = ROCK_MASTER;
                else if (strcmp(optarg, "piano") == 0) master_type = PIANO_MASTER;
                else if (strcmp(optarg, "default") == 0) master_type = DEFAULT_MASTER;
                else fprintf(stderr, "Invalid master type. Using default.\n");
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
        fprintf(stderr, "Please install FFmpeg 4.3 or later and make sure it's accessible from the command line. https://ffmpeg.org/\n");
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
    char filter_complex[COMMAND_SIZE];
    const char* base_filter_complex;

    const char* common_filters = 
    "aformat=channel_layouts=stereo,"
    "highpass=f=20,lowpass=f=20000,"
    "adeclick,"
    "loudnorm=I=-14:TP=-1:LRA=11,"
    "acompressor=threshold=-16dB:ratio=2:attack=25:release=250:makeup=2dB,"
    "equalizer=f=1000:width_type=h:width=200:g=-1,"
    "acompressor=threshold=-6dB:ratio=4:attack=5:release=100:makeup=1dB,"
    "alimiter=limit=-1dB:level=disabled,"
    "volume=0.9";

switch (master_type) {
    case SYNTH_MASTER:
        base_filter_complex = 
            "equalizer=f=80:width_type=o:width=2:g=2,"
            "equalizer=f=150:width_type=o:width=2:g=1,"
            "equalizer=f=1000:width_type=o:width=2:g=1.5,"
            "equalizer=f=5000:width_type=o:width=2:g=2.5,"
            "equalizer=f=10000:width_type=o:width=2:g=2,"
            "stereotools=mlev=2:slev=1.2:sbal=0.5,"
            "acompressor=threshold=-12dB:ratio=3:attack=5:release=50:makeup=2dB,"
            "aecho=0.8:0.88:60:0.4,"
            "aphaser=type=t:speed=0.5:decay=0.5,"
            "asubboost=dry=0.5:wet=0.8:decay=0.7:cutoff=100,"
            "vibrato=f=4:d=0.2";
        break;
    case SYNTH_VOCALS_MASTER:
        base_filter_complex = 
            "equalizer=f=100:width_type=o:width=2:g=1,"
            "equalizer=f=1000:width_type=o:width=2:g=2,"
            "equalizer=f=3000:width_type=o:width=2:g=2.5,"
            "equalizer=f=5000:width_type=o:width=2:g=3,"
            "equalizer=f=10000:width_type=o:width=2:g=1.5,"
            "stereotools=mlev=2:slev=1:sbal=0.5,"
            "acompressor=threshold=-18dB:ratio=3:attack=5:release=50:makeup=2dB,"
            "aecho=0.8:0.88:60:0.4,"
            "adelay=20|40,"
            "anlmdn=s=7:p=0.002:r=0.01,"
            "dialoguenhance=original=0.5:enhance=0.5";
        break;
    case BASS_BOOST_MASTER:
        base_filter_complex = 
            "equalizer=f=50:width_type=o:width=2:g=4,"
            "equalizer=f=100:width_type=o:width=2:g=3,"
            "equalizer=f=200:width_type=o:width=2:g=2,"
            "equalizer=f=1000:width_type=o:width=2:g=0.5,"
            "equalizer=f=5000:width_type=o:width=2:g=1,"
            "stereotools=mlev=1.5:slev=1:sbal=0.5,"
            "acompressor=threshold=-20dB:ratio=4:attack=5:release=100:makeup=3dB,"
            "bass=g=6:f=110:w=0.6,"
            "lowpass=f=200,"
            "highpass=f=20,"
            "asubboost=dry=0.5:wet=1:decay=0.8:cutoff=150,"
            "stereowidener=widen=50";
        break;
    case ROCK_MASTER:
        base_filter_complex = 
            "equalizer=f=80:width_type=o:width=2:g=2,"
            "equalizer=f=250:width_type=o:width=2:g=-1,"
            "equalizer=f=1000:width_type=o:width=2:g=1,"
            "equalizer=f=2500:width_type=o:width=2:g=2,"
            "equalizer=f=5000:width_type=o:width=2:g=1.5,"
            "stereotools=mlev=2:slev=1.2:sbal=0.5,"
            "acompressor=threshold=-18dB:ratio=3:attack=5:release=50:makeup=2dB,"
            "aecho=0.6:0.3:30:0.3,"
            "bandpass=f=1000:width_type=h:width=8000,"
            "adeclick=w=55:o=2,"
            "asoftclip=type=tanh";
        break;
    case PIANO_MASTER:
        base_filter_complex = 
            "equalizer=f=100:width_type=o:width=2:g=0.5,"
            "equalizer=f=500:width_type=o:width=2:g=1,"
            "equalizer=f=1500:width_type=o:width=2:g=1.5,"
            "equalizer=f=4000:width_type=o:width=2:g=2,"
            "equalizer=f=8000:width_type=o:width=2:g=1.5,"
            "stereotools=mlev=1.5:slev=1:sbal=0.5,"
            "acompressor=threshold=-20dB:ratio=2:attack=10:release=100:makeup=1dB,"
            "aecho=0.8:0.9:40|50|70:0.4|0.3|0.2,"
            "areverse,aecho=0.8:0.7:40|60:0.4|0.3,areverse,"
            "atempo=1.01,"
            "asetrate=48000*0.995";
        break;
    default:
        base_filter_complex = 
            "equalizer=f=100:width_type=o:width=2:g=1,"
            "equalizer=f=1000:width_type=o:width=2:g=1,"
            "equalizer=f=8000:width_type=o:width=2:g=2,"
            "stereotools=mlev=1.5:slev=1:sbal=0.5,"
            "acompressor=threshold=-18dB:ratio=2:attack=5:release=50:makeup=1dB,"
            "agate=range=0.9:threshold=0.005:ratio=2:attack=10:release=100";
        break;
}
    // Construct the initial filter complex string
    snprintf(filter_complex, COMMAND_SIZE, "%s,%s", base_filter_complex, common_filters);

    char final_filter_complex[COMMAND_SIZE] = "";
    char *filters[] = {"equalizer", "stereotools", "acompressor", "agate", "aformat", 
                       "highpass", "lowpass", "adeclick", "loudnorm", "alimiter"};
    int num_filters = sizeof(filters) / sizeof(filters[0]);

    for (int i = 0; i < num_filters; i++) {
        char check_command[COMMAND_SIZE];
        snprintf(check_command, COMMAND_SIZE, "ffmpeg -filters | grep %s > /dev/null 2>&1", filters[i]);
        if (system(check_command) == 0) {
            // Filter is available, add it to the final filter complex
            char *start = strstr(filter_complex, filters[i]);
            while (start != NULL) {
                char *end = strchr(start, ',');
                if (end == NULL) end = start + strlen(start);
                strncat(final_filter_complex, start, end - start);
                strcat(final_filter_complex, ",");
                start = strstr(end, filters[i]);
            }
        }
    }

    // Remove trailing comma if present
    size_t len = strlen(final_filter_complex);
    if (len > 0 && final_filter_complex[len-1] == ',') {
        final_filter_complex[len-1] = '\0';
    }

    // Calculate the required buffer size
    size_t command_size = strlen("ffmpeg -i \"") + strlen(input_file) + strlen("\" -filter_complex '") +
                          strlen(final_filter_complex) + strlen("' -ar 48000 -c:a pcm_s24le \"") +
                          strlen(output_file) + strlen("\"") + 1;

    // Allocate memory for the command
    char* command = malloc(command_size);
    if (command == NULL) {
        fprintf(stderr, "Error: Unable to allocate memory for command.\n");
        fprintf(log_file, "Error: Unable to allocate memory for command.\n");
        return;
    }

    // Construct the full FFmpeg command
    snprintf(command, command_size,
        "ffmpeg -i \"%s\" -filter_complex '%s' -ar 48000 -c:a pcm_s24le \"%s\"",
        input_file, final_filter_complex, output_file);

    // For debugging, print the command
    printf("Executing command: %s\n", command);

    // Execute the command
    FILE* fp = popen(command, "r");
    if (fp == NULL) {
        printf("Error executing FFmpeg command.\n");
        fprintf(log_file, "Error executing FFmpeg command for %s\n", input_file);
        free(command);
        return;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        printf("%s", buffer);
        fprintf(log_file, "%s", buffer);
    }

    int status = pclose(fp);
    if (status == 0) {
        printf("Successfully mastered: %s\n", input_file);
        fprintf(log_file, "Successfully mastered: %s\n", input_file);
    } else {
        printf("Error processing %s. FFmpeg exited with status: %d\n", input_file, status);
        fprintf(log_file, "Error processing %s. FFmpeg exited with status: %d\n", input_file, status);
        printf("Command that caused the error:\n%s\n", command);
        fprintf(log_file, "Command that caused the error:\n%s\n", command);
    }

    // Free the allocated memory
    free(command);
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
        if (stat(input_file, &st) == 0 && S_ISREG(st.st_mode)) {
            size_t name_len = strlen(entry->d_name);
            if (name_len > 4 && (
                strcmp(entry->d_name + name_len - 4, ".wav") == 0 ||
                strcmp(entry->d_name + name_len - 4, ".mp3") == 0 ||
                strcmp(entry->d_name + name_len - 4, ".aac") == 0 ||
                strcmp(entry->d_name + name_len - 4, ".ogg") == 0 ||
                strcmp(entry->d_name + name_len - 5, ".flac") == 0)) {
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
    printf("  -m <master_type> Specify mastering type: default, synth, synthvocals, bassboost, rock, or piano\n");
    printf("  -h               Display this help message\n");
}