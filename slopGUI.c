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
#include <gtk/gtk.h>
#include <glib.h>
#include <time.h>

#define MAX_PATH 4096
#define COMMAND_SIZE 524288
#define MAX_THREADS 4

FILE* log_file = NULL;
int total_files = 0;
int processed_files = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

GtkWidget *window;
GtkWidget *master_button;
GtkWidget *vocal_checkbox;
GtkWidget *file_list;
GtkWidget *status_label;
GtkWidget *reverb_checkbox;
GtkWidget *bass_booster_checkbox;
GtkWidget *wet_checkbox;
GtkWidget *reverb_delay_scale;
GtkWidget *reverb_decay_scale;

int check_ffmpeg_installed(void);
void master_audio_file(const char* input_file, const char* output_file, int vocal_mode, const char* output_format);
void process_audio_files(void);
void print_usage(const char* program_name);
void* process_file_thread(void* arg);
void update_progress(const char* message, double fraction);
int is_directory_writable(const char* path);
void create_gui(void);
void on_master_clicked(GtkWidget *widget, gpointer data);
void update_file_list(void);
void apply_dark_theme(void);
void force_theme_update(GtkWidget *widget);
void on_reverb_toggled(GtkToggleButton *button, gpointer user_data);

typedef struct {
    char input_file[MAX_PATH];
    char output_file[MAX_PATH];
    int vocal_mode;
    char output_format[10];
} ThreadArgs;

char current_dir[MAX_PATH] = ".";
char output_format[10] = "wav";

gboolean refresh_file_list(gpointer user_data) {
    update_file_list();
    update_progress("Processing completed", 1.0);
    return G_SOURCE_CONTINUE;
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    g_set_prgname("SlopMaster");
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE, NULL);
    apply_dark_theme();

    log_file = fopen("audioMaster.log", "a");
    if (!log_file) {
        fprintf(stderr, "Error opening log file: %s\n", strerror(errno));
        return 1;
    }

    if (!check_ffmpeg_installed()) {
        fprintf(stderr, "Error: FFmpeg is not installed or not in the system PATH.\n");
        fclose(log_file);
        return 1;
    }

    create_gui();
    gtk_main();
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    fclose(log_file);
    gtk_widget_destroy(window);
    return 0;
}

void on_reverb_toggled(GtkToggleButton *button, gpointer user_data) {
    gboolean active = gtk_toggle_button_get_active(button);
    gtk_widget_set_sensitive(reverb_delay_scale, active);
    gtk_widget_set_sensitive(reverb_decay_scale, active);
}

void force_theme_update(GtkWidget *widget) {
    gtk_style_context_add_class(gtk_widget_get_style_context(widget), "force-theme");
    if (GTK_IS_CONTAINER(widget)) {
        gtk_container_forall(GTK_CONTAINER(widget), (GtkCallback)force_theme_update, NULL);
    }
}

void create_gui(void) {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "SlopMaster GUI");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    gtk_widget_set_size_request(window, 600, 400);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    GtkWidget *left_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(main_box), left_panel, FALSE, FALSE, 0);
    gtk_box_set_spacing(GTK_BOX(left_panel), 10);

    master_button = gtk_button_new_with_label("Master");
    gtk_widget_set_name(master_button, "master-button");
    gtk_widget_set_size_request(master_button, 150, 50);
    g_signal_connect(master_button, "clicked", G_CALLBACK(on_master_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(left_panel), master_button, FALSE, FALSE, 0);

    GtkWidget *extras_label = gtk_label_new("Extras:");
    gtk_box_pack_start(GTK_BOX(left_panel), extras_label, FALSE, FALSE, 0);

    vocal_checkbox = gtk_check_button_new_with_label("Vocals");
    gtk_box_pack_start(GTK_BOX(left_panel), vocal_checkbox, FALSE, FALSE, 0);

    reverb_checkbox = gtk_check_button_new_with_label("Reverb");
    gtk_widget_set_name(reverb_checkbox, "reverb-checkbox");
    gtk_box_pack_start(GTK_BOX(left_panel), reverb_checkbox, FALSE, FALSE, 0);
    g_signal_connect(reverb_checkbox, "toggled", G_CALLBACK(on_reverb_toggled), NULL);

    GtkWidget *delay_label = gtk_label_new("Reverb Delay");
    gtk_widget_set_name(delay_label, "slider-label");
    gtk_box_pack_start(GTK_BOX(left_panel), delay_label, FALSE, FALSE, 0);

    reverb_delay_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 20, 100, 1);
    gtk_widget_set_size_request(reverb_delay_scale, 150, -1);
    gtk_range_set_value(GTK_RANGE(reverb_delay_scale), 60);
    gtk_box_pack_start(GTK_BOX(left_panel), reverb_delay_scale, FALSE, FALSE, 0);

    GtkWidget *decay_label = gtk_label_new("Reverb Decay");
    gtk_widget_set_name(decay_label, "slider-label");
    gtk_box_pack_start(GTK_BOX(left_panel), decay_label, FALSE, FALSE, 0);

    reverb_decay_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.1, 0.9, 0.1);
    gtk_widget_set_size_request(reverb_decay_scale, 150, -1);
    gtk_range_set_value(GTK_RANGE(reverb_decay_scale), 0.5);
    gtk_box_pack_start(GTK_BOX(left_panel), reverb_decay_scale, FALSE, FALSE, 0);

    gtk_widget_set_sensitive(reverb_delay_scale, FALSE);
    gtk_widget_set_sensitive(reverb_decay_scale, FALSE);

    bass_booster_checkbox = gtk_check_button_new_with_label("Bass Booster");
    gtk_box_pack_start(GTK_BOX(left_panel), bass_booster_checkbox, FALSE, FALSE, 0);

    wet_checkbox = gtk_check_button_new_with_label("Wet");
    gtk_widget_set_name(wet_checkbox, "wet-checkbox");
    gtk_box_pack_start(GTK_BOX(left_panel), wet_checkbox, FALSE, FALSE, 0);

    status_label = gtk_label_new("Ready");
    gtk_widget_set_name(status_label, "status-label");
    gtk_widget_set_size_request(status_label, -1, 40);
    gtk_box_pack_end(GTK_BOX(left_panel), status_label, FALSE, FALSE, 0);

    GtkWidget *right_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(main_box), right_panel, TRUE, TRUE, 0);
    gtk_box_set_spacing(GTK_BOX(right_panel), 10);

    GtkWidget *file_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_scroll), 
    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(file_scroll, 200, 300);
    gtk_box_pack_start(GTK_BOX(right_panel), file_scroll, TRUE, TRUE, 0);

    file_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_add(GTK_CONTAINER(file_scroll), file_list);

    update_file_list();
    force_theme_update(window);
    gtk_widget_show_all(window);
}

void apply_dark_theme(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);

    gtk_style_context_add_provider_for_screen(screen,
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    const char *css =
    "window, box, scrolledwindow { background-color: #220033; }"
    "button { background-color: #000000; color: #00cc88; border: none; padding: 10px; font-weight: bold; }"
    "button:hover { background-color: #006699; }"
    "button#master-button { background-color: #000000; color: #00cc88; border: none; padding: 10px; font-weight: bold; }"
    "button#master-button:hover { background-color: #006699; }"
    "checkbutton { color: #00cc88; }"
    "checkbutton:checked { color: #00cc88; }"
    "label#status-label { color: #00cc88; font-size: 14px; font-weight: bold; }"
    "scrollbar slider { background-color: #006699; }"
    "scrollbar slider:hover { background-color: #00cc88; }"
    "checkbutton#file-checkbox { color: #00cc88; }"
    "checkbutton#file-checkbox:checked { color: #00cc88; }"
    "checkbutton#wet-checkbox { color: #00cc88; }"
    "checkbutton#wet-checkbox:checked { color: #00cc88; }"
    "label#slider-label { color: #00cc88; font-size: 12px; margin-top: 5px; }";

    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    g_object_unref(provider);
}

void update_file_list(void) {
    GList *children, *iter;
    children = gtk_container_get_children(GTK_CONTAINER(file_list));
    for(iter = children; iter != NULL; iter = g_list_next(iter))
        gtk_widget_destroy(GTK_WIDGET(iter->data));
    g_list_free(children);

    DIR *dir = opendir(current_dir);
    if (!dir) {
        fprintf(stderr, "Error opening directory: %s\n", strerror(errno));
        return;
    }

    struct dirent *entry;
    char input_file[MAX_PATH];
    struct stat st;

    while ((entry = readdir(dir)) != NULL) {
        char *input_file = g_strdup_printf("%s/%s", current_dir, entry->d_name);
        if (stat(input_file, &st) == 0 && S_ISREG(st.st_mode)) {
            size_t name_len = strlen(entry->d_name);
            const char *ext = entry->d_name + name_len - 4;
            if (name_len > 4 && (strcmp(ext, ".wav") == 0 || strcmp(ext, ".mp3") == 0 ||
                strcmp(ext, ".aac") == 0 || strcmp(ext, ".ogg") == 0 ||
                (name_len > 5 && strcmp(entry->d_name + name_len - 5, ".flac") == 0))) {
                GtkWidget *checkbox = gtk_check_button_new_with_label(entry->d_name);
                gtk_widget_set_name(checkbox, "file-checkbox");
                gtk_box_pack_start(GTK_BOX(file_list), checkbox, FALSE, FALSE, 0);
                gtk_widget_show(checkbox);
            }
        }
        g_free(input_file);
    }

    closedir(dir);
}

void on_master_clicked(GtkWidget *widget, gpointer data) {
    update_progress("Processing", 0.0);
    int vocal_mode = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vocal_checkbox));
    total_files = 0;
    processed_files = 0;
    process_audio_files();
}

void process_audio_files(void) {
    GList *children, *iter;
    children = gtk_container_get_children(GTK_CONTAINER(file_list));

    pthread_t threads[MAX_THREADS];
    int thread_count = 0;

    for(iter = children; iter != NULL; iter = g_list_next(iter)) {
        GtkWidget *checkbox = GTK_WIDGET(iter->data);
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox))) {
            const char *filename = gtk_button_get_label(GTK_BUTTON(checkbox));
            total_files++;

            char *input_file = g_strdup_printf("%s/%s", current_dir, filename);
            char *output_file = g_strdup_printf("%s/%.*sMastered.%s", current_dir, 
                                                (int)(strlen(filename) - 4), filename, output_format);
            
            ThreadArgs* args = malloc(sizeof(ThreadArgs));
            strncpy(args->input_file, input_file, MAX_PATH - 1);
            args->input_file[MAX_PATH - 1] = '\0';
            strncpy(args->output_file, output_file, MAX_PATH - 1);
            args->output_file[MAX_PATH - 1] = '\0';
            args->vocal_mode = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vocal_checkbox));
            strncpy(args->output_format, output_format, 9);
            args->output_format[9] = '\0';

            pthread_create(&threads[thread_count], NULL, process_file_thread, args);
            thread_count++;

            g_free(input_file);
            g_free(output_file);

            if (thread_count == MAX_THREADS) {
                for (int i = 0; i < MAX_THREADS; i++) {
                    pthread_join(threads[i], NULL);
                }
                thread_count = 0;
                double progress = (double)processed_files / total_files;
                update_progress("Processing...", 0.5 + progress * 0.5);
            }
        }
    }

    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    g_list_free(children);

    update_progress("Processing completed", 1.0);
    update_file_list();
}

void* process_file_thread(void* arg) {
    ThreadArgs* args = (ThreadArgs*)arg;
    master_audio_file(args->input_file, args->output_file, args->vocal_mode, args->output_format);
    free(arg);
    return NULL;
}

void update_progress(const char* message, double fraction) {
    gtk_label_set_text(GTK_LABEL(status_label), message);

    while (gtk_events_pending()) {
        gtk_main_iteration();
    }
}

int check_ffmpeg_installed(void) {
    return system("ffmpeg -version > /dev/null 2>&1") == 0;
}

void master_audio_file(const char* input_file, const char* output_file, int vocal_mode, const char* output_format) {
    update_progress("Starting processing...", 0.0);
    char filter_complex[COMMAND_SIZE / 2];
    char vocal_filters[COMMAND_SIZE / 4] = "";

    snprintf(filter_complex, COMMAND_SIZE / 2,
        "aformat=channel_layouts=stereo:sample_rates=48000,"
        "highpass=f=20,lowpass=f=20000,"
        "afftdn=nr=10:nf=-25,"
        "compand=attacks=0:points=-80/-900|-45/-15|-27/-9|-15/-5|-5/-2|0/-1|20/0,"
        "equalizer=f=60:t=q:w=1.5:g=1,"
        "equalizer=f=120:t=q:w=1:g=-1,"
        "equalizer=f=1000:t=q:w=1.5:g=-1,"
        "equalizer=f=4000:t=q:w=1:g=2,"
        "equalizer=f=6000:t=q:w=1:g=1.5,"
        "equalizer=f=8000:t=q:w=1:g=1,"
        "equalizer=f=12000:t=q:w=1.5:g=1,"
        "stereotools=mlev=1:slev=1.2:sbal=0.2:phase=0:mode=lr>lr,"
        "loudnorm=I=-14:TP=-1:LRA=9,"
        "alimiter=level_in=1:level_out=1:limit=0.95:attack=5:release=30,"
        "volume=1.1,pan=stereo|c0=c0|c1=c1"
    );

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(reverb_checkbox))) {
        double delay = gtk_range_get_value(GTK_RANGE(reverb_delay_scale));
        double decay = gtk_range_get_value(GTK_RANGE(reverb_decay_scale));
        char reverb_filter[100];
        snprintf(reverb_filter, sizeof(reverb_filter), 
                ",aecho=0.8:0.5:%d|%d|%d:%.1f|%.1f|%.1f",
                (int)delay, (int)(delay*1.5), (int)(delay*2),
                decay, decay*0.8, decay*0.6);
        strncat(filter_complex, reverb_filter, COMMAND_SIZE / 2 - strlen(filter_complex) - 1);
    }
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(bass_booster_checkbox))) {
        strncat(filter_complex, ",equalizer=f=100:t=q:w=1:g=5", COMMAND_SIZE / 2 - strlen(filter_complex) - 1);
    }
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wet_checkbox))) {
        strncat(filter_complex, ",asplit[dry][wet];"
                                "[wet]aecho=0.8:0.88:60:0.4[wet];"
                                "[dry][wet]amix=inputs=2:weights=0.7 0.3", 
                COMMAND_SIZE / 2 - strlen(filter_complex) - 1);
    }

    if (vocal_mode) {
        snprintf(vocal_filters, COMMAND_SIZE / 4,
            ",highpass=f=80,lowpass=f=12000,"
            "equalizer=f=200:width_type=o:width=1:g=-3,"
            "equalizer=f=1800:width_type=o:width=1:g=2,"
            "equalizer=f=4000:width_type=o:width=1:g=3,"
            "equalizer=f=8000:width_type=o:width=1:g=1.5,"
            "compand=attacks=0.02:decays=0.1:points=-80/-80|-45/-25|-20/-12|-10/-8|-5/-5|0/-4:soft-knee=6:gain=2,"
            "acompressor=threshold=-12dB:ratio=3:attack=10:release=100:makeup=2:knee=5,"
            "volume=1.5"
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

    snprintf(ffmpeg_command, COMMAND_SIZE / 4, "ffmpeg -i \"%s\" -threads 0 -filter_complex '", input_file);
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
    fprintf(log_file, "Executing FFmpeg command:\n%s\n", command);
    FILE* fp = popen(command, "r");
    if (!fp) {
        fprintf(stderr, "Error executing FFmpeg command for %s\n", input_file);
        free(command);
        return;
    }

    char buffer[1024];
    double duration = 0;
    double current_time = 0;
    time_t start_time = time(NULL);
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        fprintf(log_file, "%s", buffer);
        
        if (strstr(buffer, "Duration:") && duration == 0) {
            int hours, minutes;
            float seconds;
            sscanf(buffer, "  Duration: %d:%d:%f", &hours, &minutes, &seconds);
            duration = hours * 3600 + minutes * 60 + seconds;
        }
        if (strstr(buffer, "time=")) {
            int hours, minutes;
            float seconds;
            sscanf(strstr(buffer, "time="), "time=%d:%d:%f", &hours, &minutes, &seconds);
            current_time = hours * 3600 + minutes * 60 + seconds;
            double file_progress = current_time / duration;
            double overall_progress = (processed_files + file_progress) / total_files;
            char progress_message[256];
            snprintf(progress_message, sizeof(progress_message), 
            "Processing: %.1f%% (File: %.1f%%)", 
            overall_progress * 100, file_progress * 100);
            update_progress(progress_message, 0.0);
        }
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
    update_progress("Processing...", 0.0);
    pthread_mutex_unlock(&mutex);
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