#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_PATH 1024
#define COMMAND_SIZE 524288
#define MAX_THREADS 4

FILE* log_file = NULL;
int total_files = 0;
int processed_files = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int check_ffmpeg_installed(void);
void master_audio_file(const char* input_file, const char* output_file, int vocal_mode, const char* output_format);
int process_audio_files(const char* input_dir, const char* output_dir, int vocal_mode, const char* output_format);
void print_usage(const char* program_name);
void* process_file_thread(void* arg);
void update_progress();
int is_directory_writable(const char* path);

typedef struct {
    char input_file[MAX_PATH];
    char output_file[MAX_PATH];
    int vocal_mode;
    char output_format[10];
} ThreadArgs;

int main(int argc, char *argv[]) {
    char input_dir[MAX_PATH] = ".";
    char output_dir[MAX_PATH] = ".";
    int opt, vocal_mode = 0;
    int verbose = 0;
    char output_format[10] = "wav";

    log_file = fopen("audioMaster.log", "a");
    if (!log_file) {
        fprintf(stderr, "Error opening log file: %s\n", strerror(errno));
        return 1;
    }

    while ((opt = getopt(argc, argv, "i:o:vhf:n")) != -1) {
        switch (opt) {
            case 'i': strncpy(input_dir, optarg, MAX_PATH - 1); break;
            case 'o': strncpy(output_dir, optarg, MAX_PATH - 1); break;
            case 'v': vocal_mode = 1; break;
            case 'f': strncpy(output_format, optarg, 9); break;
            case 'n': verbose = 1; break;
            case 'h': print_usage(argv[0]); fclose(log_file); return 0;
            default: fprintf(stderr, "Unknown option: %c\n", opt);
                     print_usage(argv[0]); fclose(log_file); return 1;
        }
    }

    if (!is_directory_writable(input_dir) || !is_directory_writable(output_dir)) {
        fprintf(stderr, "Error: Input or output directory is not writable\n");
        fclose(log_file);
        return 1;
    }
    
    if (!check_ffmpeg_installed()) {
        fprintf(stderr, "Error: FFmpeg is not installed or not in the system PATH.\n");
        fclose(log_file);
        return 1;
    }

    int result = process_audio_files(input_dir, output_dir, vocal_mode, output_format);
    fclose(log_file);
    return result;
}

int check_ffmpeg_installed(void) {
    return system("ffmpeg -version > /dev/null 2>&1") == 0;
}

void master_audio_file(const char* input_file, const char* output_file, int vocal_mode, const char* output_format) {
    char filter_complex[COMMAND_SIZE / 2];
    snprintf(filter_complex, COMMAND_SIZE / 2,
        "aformat=channel_layouts=stereo:sample_rates=48000,"
        "highpass=f=20,lowpass=f=20000,"
        "afftdn=nr=10:nf=-25,"
        "compand=attacks=0:points=-80/-900|-45/-15|-27/-9|-15/-5|-5/-2|0/-1|20/0,"
         // Detailed EQ
        "equalizer=f=60:t=q:w=1.5:g=1,"    // Sub-bass
        "equalizer=f=120:t=q:w=1:g=-1,"    // Low-end cut
        "equalizer=f=1000:t=q:w=1.5:g=-1," // Mids cut
        "equalizer=f=4000:t=q:w=1:g=2,"
        "equalizer=f=6000:t=q:w=1:g=1.5,"  // Presence
        "equalizer=f=8000:t=q:w=1:g=1,"
        "equalizer=f=12000:t=q:w=1.5:g=1," // Air
        "stereotools=mlev=1:slev=1.2:sbal=0.2:phase=0:mode=lr>lr,"
        "loudnorm=I=-14:TP=-1:LRA=9.0,"
        "alimiter=level_in=1:level_out=1:limit=0.95:attack=5:release=30,"
        "volume=1.1,pan=stereo|c0=c0|c1=c1"
    );

    if (vocal_mode) {
        char vocal_filters[COMMAND_SIZE / 4];
        snprintf(vocal_filters, COMMAND_SIZE / 4,
            ",highpass=f=100,equalizer=f=200:t=q:w=1:g=-2,"
            "equalizer=f=2500:t=q:w=1:g=2,equalizer=f=6000:t=q:w=1:g=1,"
            "acompressor=threshold=0.15:ratio=3:attack=2:release=40:makeup=1:knee=2,"
            "adeclick=w=100:o=50:a=100,deesser"
        );
        strncat(filter_complex, vocal_filters, COMMAND_SIZE / 2 - strlen(filter_complex) - 1);
    }

    char* command = malloc(COMMAND_SIZE);
    if (!command) {
        fprintf(stderr, "Memory allocation failed for command\n");
        return;
    }

    char ffmpeg_command[COMMAND_SIZE / 4];
    char output_options[COMMAND_SIZE / 4];

    snprintf(ffmpeg_command, COMMAND_SIZE / 4, "ffmpeg -i \"%s\" -filter_complex '", input_file);
    snprintf(output_options, COMMAND_SIZE / 4, "' -ar 48000 -c:a %s \"%s\" -y", 
             strcmp(output_format, "wav") == 0 ? "pcm_s24le" : 
             strcmp(output_format, "flac") == 0 ? "flac" : "libmp3lame", 
             output_file);

    size_t remaining_space = COMMAND_SIZE - strlen(ffmpeg_command) - strlen(output_options) - 1;
    if (strlen(filter_complex) > remaining_space) {
        fprintf(stderr, "Filter complex too long for command buffer\n");
        free(command);
        return;
    }

    snprintf(command, COMMAND_SIZE, "%s%s%s", ffmpeg_command, filter_complex, output_options);

    FILE* fp = popen(command, "r");
    if (!fp) {
        fprintf(stderr, "Error executing FFmpeg command for %s\n", input_file);
        free(command);
        return;
    }

    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        fprintf(log_file, "%s", buffer);
    }

    int status = pclose(fp);
    if (status == 0) {
        fprintf(log_file, "Successfully mastered: %s\n", input_file);
    } else {
        fprintf(stderr, "Error processing %s. FFmpeg exited with status: %d\n", input_file, status);
        fprintf(log_file, "Command that caused the error:\n%s\n", command);
    }

    free(command);
    pthread_mutex_lock(&mutex);
    processed_files++;
    update_progress();
    pthread_mutex_unlock(&mutex);
}

int process_audio_files(const char* input_dir, const char* output_dir, int vocal_mode, const char* output_format) {
    DIR *dir = opendir(input_dir);
    if (!dir) {
        fprintf(stderr, "Error opening input directory: %s\n", strerror(errno));
        return 1;
    }

    struct dirent *entry;
    char input_file[MAX_PATH], output_file[MAX_PATH];
    struct stat st;

    // Count total files
    while ((entry = readdir(dir)) != NULL) {
        snprintf(input_file, MAX_PATH, "%s/%s", input_dir, entry->d_name);
        if (stat(input_file, &st) == 0 && S_ISREG(st.st_mode)) {
            size_t name_len = strlen(entry->d_name);
            const char *ext = entry->d_name + name_len - 4;
            if (name_len > 4 && (strcmp(ext, ".wav") == 0 || strcmp(ext, ".mp3") == 0 ||
                strcmp(ext, ".aac") == 0 || strcmp(ext, ".ogg") == 0 ||
                (name_len > 5 && strcmp(entry->d_name + name_len - 5, ".flac") == 0))) {
                total_files++;
            }
        }
    }

    rewinddir(dir);

    pthread_t threads[MAX_THREADS];
    int thread_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        snprintf(input_file, MAX_PATH, "%s/%s", input_dir, entry->d_name);
        if (stat(input_file, &st) == 0 && S_ISREG(st.st_mode)) {
            size_t name_len = strlen(entry->d_name);
            const char *ext = entry->d_name + name_len - 4;
            if (name_len > 4 && (strcmp(ext, ".wav") == 0 || strcmp(ext, ".mp3") == 0 ||
                strcmp(ext, ".aac") == 0 || strcmp(ext, ".ogg") == 0 ||
                (name_len > 5 && strcmp(entry->d_name + name_len - 5, ".flac") == 0))) {
                snprintf(output_file, MAX_PATH, "%s/%.*sMastered.%s", output_dir, (int)(name_len - 4), entry->d_name, output_format);
                
                ThreadArgs* args = malloc(sizeof(ThreadArgs));
                strcpy(args->input_file, input_file);
                strcpy(args->output_file, output_file);
                args->vocal_mode = vocal_mode;
                strcpy(args->output_format, output_format);

                pthread_create(&threads[thread_count], NULL, process_file_thread, args);
                thread_count++;

                if (thread_count == MAX_THREADS) {
                    for (int i = 0; i < MAX_THREADS; i++) {
                        pthread_join(threads[i], NULL);
                    }
                    thread_count = 0;
                }
            }
        }
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    closedir(dir);
    return 0;
}

void* process_file_thread(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    master_audio_file(args->input_file, args->output_file, args->vocal_mode, args->output_format);
    free(arg);
    return NULL;
}

void update_progress() {
    float progress = (float)processed_files / total_files * 100;
    printf("\rProgress: [%-20s] %.2f%%", "==================== ", progress);
    fflush(stdout);
}

int is_directory_writable(const char* path) {
    char test_file[MAX_PATH];
    snprintf(test_file, sizeof(test_file), "%s/test_write", path);
    FILE* fp = fopen(test_file, "w");
    if (fp) {
        fclose(fp);
        remove(test_file);
        return 1;
    }
    return 0;
}

void print_usage(const char* program_name) {
    printf("Usage: %s [options]\n"
           "Options:\n"
           "  -i <input_dir>   Specify input directory (default: current directory)\n"
           "  -o <output_dir>  Specify output directory (default: current directory)\n"
           "  -v               Enable vocal mode for processing songs with vocals\n"
           "  -f <format>      Specify output format (wav, flac, or mp3; default: wav)\n"
           "  -n               Enable verbose mode\n"
           "  -h               Display this help message\n", program_name);
}