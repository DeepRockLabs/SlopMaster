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
#include <gst/gst.h>
#include <time.h>
#include <math.h>
#include <sndfile.h>

#define MAX_PATH 4096
#define COMMAND_SIZE 524288
#define MAX_THREADS 4
#define COLOR_BACKGROUND "#1D1E2C"
#define COLOR_PRIMARY "#F17E8A"
#define COLOR_SECONDARY "#FFBDC1"
#define COLOR_ACCENT "#4A6FA5"
#define COLOR_TEXT "#FFEBEE"
#define SAMPLE_RATE 48000
#define CHANNELS 2
#define MAX_WAVEFORM_POINTS 1000

typedef struct {
    char input_file[MAX_PATH];
    char output_file[MAX_PATH];
    int vocal_mode;
    char output_format[10];
} ThreadArgs;

FILE* log_file = NULL;
int total_files = 0;
int processed_files = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

GtkWidget *window, *master_button, *vocal_checkbox, *file_list, *reverb_checkbox;
GtkWidget *bass_booster_checkbox, *wet_checkbox, *reverb_delay_scale, *reverb_decay_scale;
GtkWidget *format_combo, *stereo_width_scale, *multiband_frame;
GtkWidget *low_threshold, *low_ratio, *mid_threshold, *mid_ratio, *high_threshold, *high_ratio;
GtkWidget *crossover_low, *crossover_high, *right_panel, *progress_bar;
GtkWidget *waveform_drawing_area, *original_file_chooser, *processed_file_chooser;
GtkWidget *time_label, *seek_bar;
GtkWidget *volume_adjustment_scale;

GstElement *playbin = NULL;
gint64 current_position = 0;
gboolean is_playing_original = TRUE;
gboolean is_audio_playing = FALSE;
GThread *processing_thread;
GMutex progress_mutex;
GCond progress_cond;
gdouble current_progress = 0.0;
gboolean processing_active = FALSE;
GstState current_state = GST_STATE_NULL;
gint64 shared_position = 0;
gboolean position_reset_needed = FALSE;
guint seek_bar_update_id = 0;

char current_dir[MAX_PATH] = ".";
char output_format[10] = "wav";
char *original_file_path = NULL;
char *processed_file_path = NULL;
float *waveform_data = NULL;
int waveform_size = 0;
gdouble waveform_color[3] = {0.0, 0.8, 0.0};
double volume_adjustment_db = 0.0;
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
void apply_theme(void);
void on_reverb_toggled(GtkToggleButton *button, gpointer user_data);
gboolean update_progress_bar(gpointer user_data);
gpointer process_audio_files_thread(gpointer data);
void on_play_original(GtkWidget *widget, gpointer data);
void on_play_processed(GtkWidget *widget, gpointer data);
void on_stop_playback(GtkWidget *widget, gpointer data);
gboolean draw_waveform(GtkWidget *widget, cairo_t *cr, gpointer data);
void init_audio(void);
void cleanup_audio(void);
void play_audio(const char *filename, gboolean is_original);
void stop_audio(void);
void update_play_buttons_sensitivity(void);
void cleanup_file_paths(void);
void init_concurrent_processing(void);
void cleanup_concurrent_processing(void);
int parse_arguments(int argc, char *argv[]);
gboolean update_shared_position(gpointer user_data);
void cleanup_waveform(void);
void update_waveform(const char *filename);
void on_file_chosen(GtkFileChooserButton *chooser_button, gpointer user_data);
static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data);
void format_time(gint64 duration, gchar *str, gsize str_size);
gboolean update_seek_bar(gpointer data);
void on_seek_bar_value_changed(GtkRange *range, gpointer user_data);
void on_open_folder_clicked(GtkWidget *widget, gpointer data);
void on_volume_adjustment_changed(GtkRange *range, gpointer user_data);


int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    gst_init(&argc, &argv);

    if (parse_arguments(argc, argv) != 0) {
        return 1;
    }

    g_set_prgname("SlopMaster");
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE, NULL);
    apply_theme();

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

    init_audio();
    init_concurrent_processing();
    create_gui();
    gtk_main();
    cleanup_waveform();
    cleanup_audio();
    cleanup_concurrent_processing();
    cleanup_file_paths();
    fclose(log_file);
    return 0;
}

void create_gui(void) {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "SlopMaster");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 700);
    gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &(GdkGeometry){.min_width = 800, .min_height = 600}, GDK_HINT_MIN_SIZE);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_window_maximize(GTK_WINDOW(window));

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), main_box);

    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "SlopMaster");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), "Audio Mastering Tool");
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(main_box), scrolled_window, TRUE, TRUE, 0);

    GtkWidget *content_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_margin_start(content_box, 20);
    gtk_widget_set_margin_end(content_box, 20);
    gtk_widget_set_margin_top(content_box, 20);
    gtk_widget_set_margin_bottom(content_box, 20);
    gtk_container_add(GTK_CONTAINER(scrolled_window), content_box);

    GtkWidget *left_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 15);
    gtk_box_pack_start(GTK_BOX(content_box), left_panel, FALSE, FALSE, 0);

    GtkWidget *open_folder_button = gtk_button_new_with_label("Open Folder");
    gtk_widget_set_size_request(open_folder_button, 200, 40);
    g_signal_connect(open_folder_button, "clicked", G_CALLBACK(on_open_folder_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(left_panel), open_folder_button, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(open_folder_button, "Choose a folder containing audio files");

    master_button = gtk_button_new_with_label("Master");
    gtk_widget_set_name(master_button, "master-button");
    gtk_widget_set_size_request(master_button, 200, 50);
    g_signal_connect(master_button, "clicked", G_CALLBACK(on_master_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(left_panel), master_button, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(master_button, "Start the mastering process");

    GtkWidget *options_frame = gtk_frame_new("Options");
    gtk_box_pack_start(GTK_BOX(left_panel), options_frame, FALSE, FALSE, 0);

    GtkWidget *options_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(options_frame), options_box);
    gtk_container_set_border_width(GTK_CONTAINER(options_box), 10);

    GtkWidget *volume_label = gtk_label_new("Volume Adjustment (dB):");
    gtk_box_pack_start(GTK_BOX(options_box), volume_label, FALSE, FALSE, 0);

    volume_adjustment_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -20.0, 20.0, 0.1);
    gtk_range_set_value(GTK_RANGE(volume_adjustment_scale), 0.0);
    gtk_widget_set_size_request(volume_adjustment_scale, 200, -1);
    gtk_box_pack_start(GTK_BOX(options_box), volume_adjustment_scale, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(volume_adjustment_scale, "Adjust the overall volume of the mastered song");

    g_signal_connect(volume_adjustment_scale, "value-changed", G_CALLBACK(on_volume_adjustment_changed), NULL);


    vocal_checkbox = gtk_check_button_new_with_label("Vocals");
    gtk_box_pack_start(GTK_BOX(options_box), vocal_checkbox, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(vocal_checkbox, "Enable for vocal-focused processing");

    reverb_checkbox = gtk_check_button_new_with_label("Reverb");
    gtk_widget_set_name(reverb_checkbox, "reverb-checkbox");
    gtk_box_pack_start(GTK_BOX(options_box), reverb_checkbox, FALSE, FALSE, 0);
    g_signal_connect(reverb_checkbox, "toggled", G_CALLBACK(on_reverb_toggled), NULL);
    gtk_widget_set_tooltip_text(reverb_checkbox, "Add reverb effect to the audio");

    GtkWidget *reverb_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(options_box), reverb_box, FALSE, FALSE, 0);
    gtk_widget_set_margin_start(reverb_box, 20);

    GtkWidget *delay_label = gtk_label_new("Reverb Delay");
    gtk_widget_set_halign(delay_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(reverb_box), delay_label, FALSE, FALSE, 0);

    reverb_delay_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 20, 100, 1);
    gtk_widget_set_size_request(reverb_delay_scale, 180, -1);
    gtk_range_set_value(GTK_RANGE(reverb_delay_scale), 60);
    gtk_box_pack_start(GTK_BOX(reverb_box), reverb_delay_scale, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(reverb_delay_scale, "Adjust the delay time of the reverb");

    GtkWidget *decay_label = gtk_label_new("Reverb Decay");
    gtk_widget_set_halign(decay_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(reverb_box), decay_label, FALSE, FALSE, 0);

    reverb_decay_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.1, 0.9, 0.1);
    gtk_widget_set_size_request(reverb_decay_scale, 180, -1);
    gtk_range_set_value(GTK_RANGE(reverb_decay_scale), 0.5);
    gtk_box_pack_start(GTK_BOX(reverb_box), reverb_decay_scale, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(reverb_decay_scale, "Adjust the decay time of the reverb");

    gtk_widget_set_sensitive(reverb_delay_scale, FALSE);
    gtk_widget_set_sensitive(reverb_decay_scale, FALSE);

    bass_booster_checkbox = gtk_check_button_new_with_label("Bass Booster");
    gtk_box_pack_start(GTK_BOX(options_box), bass_booster_checkbox, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(bass_booster_checkbox, "Enhance the low frequencies");

    wet_checkbox = gtk_check_button_new_with_label("Wet");
    gtk_widget_set_name(wet_checkbox, "wet-checkbox");
    gtk_box_pack_start(GTK_BOX(options_box), wet_checkbox, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(wet_checkbox, "Adjust the wet/dry mix of effects");

    GtkWidget *format_label = gtk_label_new("Output Format:");
    gtk_box_pack_start(GTK_BOX(options_box), format_label, FALSE, FALSE, 0);
    format_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format_combo), "WAV");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format_combo), "FLAC");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(format_combo), "MP3");
    gtk_combo_box_set_active(GTK_COMBO_BOX(format_combo), 0);
    gtk_box_pack_start(GTK_BOX(options_box), format_combo, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(format_combo, "Select the output file format");

    GtkWidget *stereo_width_label = gtk_label_new("Stereo Width:");
    gtk_box_pack_start(GTK_BOX(options_box), stereo_width_label, FALSE, FALSE, 0);
    stereo_width_scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 200, 1);
    gtk_range_set_value(GTK_RANGE(stereo_width_scale), 100);
    gtk_box_pack_start(GTK_BOX(options_box), stereo_width_scale, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(stereo_width_scale, "Adjust the stereo image width");

    GtkWidget *advanced_expander = gtk_expander_new("Advanced Options");
    gtk_box_pack_start(GTK_BOX(options_box), advanced_expander, FALSE, FALSE, 0);

    multiband_frame = gtk_frame_new("Multi-band Compressor");
    gtk_container_add(GTK_CONTAINER(advanced_expander), multiband_frame);
    GtkWidget *multiband_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(multiband_frame), multiband_box);

    GtkWidget *low_label = gtk_label_new("Low Band");
    gtk_box_pack_start(GTK_BOX(multiband_box), low_label, FALSE, FALSE, 0);
    low_threshold = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -60, 0, 1);
    gtk_range_set_value(GTK_RANGE(low_threshold), -12);
    gtk_box_pack_start(GTK_BOX(multiband_box), low_threshold, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(low_threshold, "Set the threshold for the low frequency band");
    low_ratio = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1, 20, 0.1);
    gtk_range_set_value(GTK_RANGE(low_ratio), 4);
    gtk_box_pack_start(GTK_BOX(multiband_box), low_ratio, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(low_ratio, "Set the compression ratio for the low frequency band");

    GtkWidget *mid_label = gtk_label_new("Mid Band");
    gtk_box_pack_start(GTK_BOX(multiband_box), mid_label, FALSE, FALSE, 0);
    mid_threshold = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -60, 0, 1);
    gtk_range_set_value(GTK_RANGE(mid_threshold), -12);
    gtk_box_pack_start(GTK_BOX(multiband_box), mid_threshold, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(mid_threshold, "Set the threshold for the mid frequency band");
    mid_ratio = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1, 20, 0.1);
    gtk_range_set_value(GTK_RANGE(mid_ratio), 4);
    gtk_box_pack_start(GTK_BOX(multiband_box), mid_ratio, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(mid_ratio, "Set the compression ratio for the mid frequency band");

    GtkWidget *high_label = gtk_label_new("High Band");
    gtk_box_pack_start(GTK_BOX(multiband_box), high_label, FALSE, FALSE, 0);
    high_threshold = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -60, 0, 1);
    gtk_range_set_value(GTK_RANGE(high_threshold), -12);
    gtk_box_pack_start(GTK_BOX(multiband_box), high_threshold, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(high_threshold, "Set the threshold for the high frequency band");
    high_ratio = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1, 20, 0.1);
    gtk_range_set_value(GTK_RANGE(high_ratio), 4);
    gtk_box_pack_start(GTK_BOX(multiband_box), high_ratio, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(high_ratio, "Set the compression ratio for the high frequency band");

    GtkWidget *crossover_label = gtk_label_new("Crossover Frequencies");
    gtk_box_pack_start(GTK_BOX(multiband_box), crossover_label, FALSE, FALSE, 0);
    crossover_low = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 20, 1000, 10);
    gtk_range_set_value(GTK_RANGE(crossover_low), 200);
    gtk_box_pack_start(GTK_BOX(multiband_box), crossover_low, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(crossover_low, "Set the frequency where low band transitions to mid band");
    crossover_high = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 1000, 20000, 100);
    gtk_range_set_value(GTK_RANGE(crossover_high), 5000);
    gtk_box_pack_start(GTK_BOX(multiband_box), crossover_high, FALSE, FALSE, 0);
    gtk_widget_set_tooltip_text(crossover_high, "Set the frequency where mid band transitions to high band");

    right_panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_box_pack_start(GTK_BOX(content_box), right_panel, TRUE, TRUE, 0);

    GtkWidget *file_frame = gtk_frame_new("Audio Files");
    gtk_box_pack_start(GTK_BOX(right_panel), file_frame, TRUE, TRUE, 0);

    GtkWidget *file_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(file_frame), file_scroll);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(file_scroll), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(file_scroll, 400, -1);

    file_list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(file_scroll), file_list);

    GtkWidget *ab_frame = gtk_frame_new("Song Comparison");
    gtk_box_pack_start(GTK_BOX(right_panel), ab_frame, FALSE, FALSE, 0);

    GtkWidget *ab_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(ab_frame), ab_box);

    GtkWidget *chooser_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(ab_box), chooser_box, FALSE, FALSE, 0);

    GtkWidget *original_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(chooser_box), original_box, TRUE, TRUE, 0);

    GtkWidget *original_label = gtk_label_new("Original:");
    gtk_box_pack_start(GTK_BOX(original_box), original_label, FALSE, FALSE, 0);

    original_file_chooser = gtk_file_chooser_button_new("Choose Original File", GTK_FILE_CHOOSER_ACTION_OPEN);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(original_file_chooser), current_dir);
    gtk_widget_set_size_request(original_file_chooser, 200, -1);
    gtk_box_pack_start(GTK_BOX(original_box), original_file_chooser, TRUE, TRUE, 0);
    g_signal_connect(original_file_chooser, "file-set", G_CALLBACK(on_file_chosen), "original");

    GtkWidget *processed_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(chooser_box), processed_box, TRUE, TRUE, 0);

    GtkWidget *processed_label = gtk_label_new("Processed:");
    gtk_box_pack_start(GTK_BOX(processed_box), processed_label, FALSE, FALSE, 0);

    processed_file_chooser = gtk_file_chooser_button_new("Choose Processed File", GTK_FILE_CHOOSER_ACTION_OPEN);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(processed_file_chooser), current_dir);
    gtk_widget_set_size_request(processed_file_chooser, 200, -1);
    gtk_box_pack_start(GTK_BOX(processed_box), processed_file_chooser, TRUE, TRUE, 0);
    g_signal_connect(processed_file_chooser, "file-set", G_CALLBACK(on_file_chosen), "processed");

    GtkWidget *playback_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(ab_box), playback_box, FALSE, FALSE, 0);

    GtkWidget *a_button = gtk_button_new_with_label("Play Original (A)");
    gtk_box_pack_start(GTK_BOX(playback_box), a_button, TRUE, TRUE, 0);
    g_signal_connect(a_button, "clicked", G_CALLBACK(on_play_original), NULL);
    gtk_widget_set_tooltip_text(a_button, "Play the original unprocessed audio");

    GtkWidget *b_button = gtk_button_new_with_label("Play Processed (B)");
    gtk_box_pack_start(GTK_BOX(playback_box), b_button, TRUE, TRUE, 0);
    g_signal_connect(b_button, "clicked", G_CALLBACK(on_play_processed), NULL);
    gtk_widget_set_tooltip_text(b_button, "Play the processed audio");

    GtkWidget *stop_button = gtk_button_new_with_label("Stop");
    gtk_box_pack_start(GTK_BOX(playback_box), stop_button, TRUE, TRUE, 0);
    g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop_playback), NULL);
    gtk_widget_set_tooltip_text(stop_button, "Stop audio playback");

    seek_bar = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
    gtk_scale_set_draw_value(GTK_SCALE(seek_bar), FALSE);
    g_signal_connect(seek_bar, "value-changed", G_CALLBACK(on_seek_bar_value_changed), NULL);
    gtk_box_pack_start(GTK_BOX(ab_box), seek_bar, FALSE, FALSE, 0);

    time_label = gtk_label_new("0:00:00");
    gtk_box_pack_start(GTK_BOX(ab_box), time_label, FALSE, FALSE, 0);

    GtkWidget *waveform_frame = gtk_frame_new("Waveform");
    gtk_box_pack_start(GTK_BOX(right_panel), waveform_frame, FALSE, FALSE, 0);

    waveform_drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(waveform_drawing_area, -1, 150);
    gtk_container_add(GTK_CONTAINER(waveform_frame), waveform_drawing_area);
    g_signal_connect(G_OBJECT(waveform_drawing_area), "draw", G_CALLBACK(draw_waveform), NULL);

    progress_bar = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(progress_bar), TRUE);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), "Ready");
    gtk_box_pack_end(GTK_BOX(main_box), progress_bar, FALSE, FALSE, 10);

    update_file_list();
    apply_theme();
    gtk_widget_show_all(window);
}

void apply_theme(void) {
    GtkCssProvider *provider = gtk_css_provider_new();
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);

    gtk_style_context_add_provider_for_screen(screen,
                                              GTK_STYLE_PROVIDER(provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    const char *css = 
        "window { background-color: " COLOR_BACKGROUND "; }"
        "headerbar { background-color: " COLOR_ACCENT "; color: " COLOR_TEXT "; }"
        "headerbar label { color: " COLOR_TEXT "; font-weight: bold; }"
        "button { background-image: none; background-color: " COLOR_PRIMARY "; color: " COLOR_BACKGROUND "; border: none; border-radius: 5px; padding: 5px 10px; font-weight: bold; }"
        "button:hover { background-color: " COLOR_SECONDARY "; color: " COLOR_BACKGROUND "; }"
        "checkbutton { color: " COLOR_TEXT "; }"
        "checkbutton:checked { color: " COLOR_PRIMARY "; }"
        "label { color: " COLOR_TEXT "; }"
        "frame { border-radius: 5px; border: 1px solid " COLOR_ACCENT "; }"
        "frame > label { font-weight: bold; padding: 5px; }"
        "scrollbar slider { background-color: " COLOR_ACCENT "; }"
        "scrollbar slider:hover { background-color: " COLOR_PRIMARY "; }"
        "scale trough { background-color: " COLOR_ACCENT "; }"
        "scale slider { background-color: " COLOR_PRIMARY "; border-radius: 50%; }"
        "scale slider:hover { background-color: " COLOR_SECONDARY "; }"
        "scale trough highlight { background-color: " COLOR_PRIMARY "; }"
        "progressbar trough { background-color: " COLOR_BACKGROUND "; }"
        "progressbar progress { background-color: " COLOR_PRIMARY "; }"
        "button#open-folder-button { padding: 2px 5px; font-size: 0.9em; }";

    gtk_css_provider_load_from_data(provider, css, -1, NULL);
    g_object_unref(provider);
}

void format_time(gint64 duration, gchar *str, gsize str_size) {
    gint64 hours, minutes, seconds;

    hours = duration / (60 * 60 * GST_SECOND);
    minutes = (duration / (60 * GST_SECOND)) % 60;
    seconds = (duration / GST_SECOND) % 60;

    g_snprintf(str, str_size, "%01" G_GINT64_FORMAT ":%02" G_GINT64_FORMAT ":%02" G_GINT64_FORMAT,
               hours, minutes, seconds);
}

gboolean update_seek_bar(gpointer data) {
    static int update_counter = 0;
    if (!is_audio_playing) return G_SOURCE_CONTINUE;

    gint64 duration;
    if (gst_element_query_position(playbin, GST_FORMAT_TIME, &shared_position) &&
        gst_element_query_duration(playbin, GST_FORMAT_TIME, &duration)) {
        gtk_range_set_range(GTK_RANGE(seek_bar), 0, duration / GST_SECOND);
        gtk_range_set_value(GTK_RANGE(seek_bar), shared_position / GST_SECOND);

        gchar time_str[32];
        format_time(shared_position, time_str, sizeof(time_str));
        gtk_label_set_text(GTK_LABEL(time_label), time_str);
        
        update_counter++;
    }

    return G_SOURCE_CONTINUE;
}

void on_seek_bar_value_changed(GtkRange *range, gpointer user_data) {
    if (!is_audio_playing) return;

    gdouble value = gtk_range_get_value(range);
    gint64 position = (gint64)(value * GST_SECOND);

    if (!gst_element_seek_simple(playbin, GST_FORMAT_TIME,
                                 GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT,
                                 position)) {
        g_print("Seek failed!\n");
    } else {
        shared_position = position;
    }
}

void init_audio(void) {
    playbin = gst_element_factory_make("playbin", "playbin");
    if (!playbin) {
        g_printerr("Failed to create playbin element\n");
    }

    GstBus *bus = gst_element_get_bus(playbin);
    gst_bus_add_watch(bus, (GstBusFunc)bus_call, NULL);
    gst_object_unref(bus);
}

static gboolean bus_call(GstBus *bus, GstMessage *msg, gpointer data) {
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_EOS:
            g_print("End of stream\n");
            stop_audio();
            break;
        case GST_MESSAGE_ERROR: {
            gchar *debug;
            GError *error;
            gst_message_parse_error(msg, &error, &debug);
            g_free(debug);
            g_printerr("Error: %s\n", error->message);
            g_error_free(error);
            break;
        }
        default:
            break;
    }
    return TRUE;
}

void cleanup_audio(void) {
    if (playbin) {
        gst_element_set_state(playbin, GST_STATE_NULL);
        gst_object_unref(playbin);
    }
}

void update_current_directory(const char* file_path) {
    char* dir_path = g_path_get_dirname(file_path);
    strncpy(current_dir, dir_path, sizeof(current_dir) - 1);
    current_dir[sizeof(current_dir) - 1] = '\0';
    g_free(dir_path);
}

void play_audio(const char *filename, gboolean is_original) {
    if (!playbin) return;

    g_print("Entering play_audio function\n");

    if (is_audio_playing) {
        stop_audio();

        g_usleep(100000);
    }

    gchar *uri = gst_filename_to_uri(filename, NULL);
    g_object_set(G_OBJECT(playbin), "uri", uri, NULL);
    g_print("Set new URI: %s\n", uri);

    if (position_reset_needed) {
        g_print("Resetting position\n");
        shared_position = 0;
        position_reset_needed = FALSE;
    }

    gst_element_set_state(playbin, GST_STATE_PAUSED);

    GstState state;
    GstStateChangeReturn ret = gst_element_get_state(playbin, &state, NULL, GST_CLOCK_TIME_NONE);
    if (ret == GST_STATE_CHANGE_SUCCESS) {
        g_print("Successfully changed state to PAUSED\n");
    } else {
        g_print("Failed to change state to PAUSED\n");
    }

    if (!gst_element_seek_simple(playbin, GST_FORMAT_TIME,
                                 GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
                                 shared_position)) {
        g_print("Seek failed!\n");
    } else {
        g_print("Seek successful\n");
    }

    gst_element_set_state(playbin, GST_STATE_PLAYING);
    g_print("Set playbin state to PLAYING\n");

    is_playing_original = is_original;
    is_audio_playing = TRUE;
    g_free(uri);

    update_waveform(filename);

    if (seek_bar_update_id == 0) {
        seek_bar_update_id = g_timeout_add(200, update_seek_bar, NULL);
        g_print("Started seek bar update timer\n");
    }

    g_print("Exiting play_audio function\n");
}

void stop_audio(void) {
    if (playbin && is_audio_playing) {
        g_print("Stopping audio playback\n");
        
        gst_element_set_state(playbin, GST_STATE_PAUSED);

        GstState state;
        GstStateChangeReturn ret = gst_element_get_state(playbin, &state, NULL, GST_CLOCK_TIME_NONE);
        if (ret == GST_STATE_CHANGE_SUCCESS) {
            g_print("Successfully changed state to PAUSED\n");
        } else {
            g_print("Failed to change state to PAUSED\n");
        }

        gst_element_set_state(playbin, GST_STATE_NULL);
        
        is_audio_playing = FALSE;
        position_reset_needed = TRUE;
        shared_position = 0;

        if (seek_bar_update_id != 0) {
            g_source_remove(seek_bar_update_id);
            seek_bar_update_id = 0;
        }

        gtk_range_set_value(GTK_RANGE(seek_bar), 0);
        gtk_label_set_text(GTK_LABEL(time_label), "0:00:00");
        
        g_print("Audio playback stopped\n");
    }
}

void on_open_folder_clicked(GtkWidget *widget, gpointer data) {
    GtkWidget *dialog;
    GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
    gint res;

    dialog = gtk_file_chooser_dialog_new("Select Folder",
                                         GTK_WINDOW(window),
                                         action,
                                         "_Cancel",
                                         GTK_RESPONSE_CANCEL,
                                         "_Open",
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);

    res = gtk_dialog_run(GTK_DIALOG(dialog));
    if (res == GTK_RESPONSE_ACCEPT) {
        char *folder_path;
        GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
        folder_path = gtk_file_chooser_get_filename(chooser);
        
        if (folder_path) {
            strncpy(current_dir, folder_path, sizeof(current_dir) - 1);
            current_dir[sizeof(current_dir) - 1] = '\0';
            g_free(folder_path);
            update_file_list();
        }
    }

    gtk_widget_destroy(dialog);
}

void on_file_chosen(GtkFileChooserButton *chooser_button, gpointer user_data) {
    const char *file_path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(chooser_button));
    if (strcmp(user_data, "original") == 0) {
        g_free(original_file_path);
        original_file_path = g_strdup(file_path);
    } else if (strcmp(user_data, "processed") == 0) {
        g_free(processed_file_path);
        processed_file_path = g_strdup(file_path);
    }
    update_current_directory(file_path);
    update_play_buttons_sensitivity();
    position_reset_needed = TRUE;
}

void update_play_buttons_sensitivity(void) {
    GtkWidget *playback_box = gtk_widget_get_parent(original_file_chooser);
    playback_box = gtk_widget_get_parent(playback_box);
    playback_box = gtk_widget_get_parent(playback_box);

    GList *children = gtk_container_get_children(GTK_CONTAINER(playback_box));
    GtkWidget *a_button = GTK_WIDGET(g_list_nth_data(children, 0));
    GtkWidget *b_button = GTK_WIDGET(g_list_nth_data(children, 1));

    gtk_widget_set_sensitive(a_button, original_file_path != NULL);
    gtk_widget_set_sensitive(b_button, processed_file_path != NULL);

    g_list_free(children);
}

void on_play_original(GtkWidget *widget, gpointer data) {
    if (original_file_path) {
        g_print("Playing original audio: %s\n", original_file_path);
        play_audio(original_file_path, TRUE);
    } else {
        g_print("No original file selected\n");
    }
}

void on_play_processed(GtkWidget *widget, gpointer data) {
    if (processed_file_path) {
        g_print("Playing processed audio: %s\n", processed_file_path);
        play_audio(processed_file_path, FALSE);
    } else {
        g_print("No processed file selected\n");
    }
}

void on_stop_playback(GtkWidget *widget, gpointer data) {
    g_print("Stopping audio playback\n");
    stop_audio();
}

gboolean update_shared_position(gpointer user_data) {
    if (is_audio_playing) {
        gst_element_query_position(playbin, GST_FORMAT_TIME, &shared_position);
    }
    return G_SOURCE_CONTINUE;
}

void cleanup_file_paths(void) {
    g_free(original_file_path);
    g_free(processed_file_path);
}

void init_concurrent_processing(void) {
    g_mutex_init(&progress_mutex);
    g_cond_init(&progress_cond);
}

void cleanup_concurrent_processing(void) {
    g_mutex_clear(&progress_mutex);
    g_cond_clear(&progress_cond);
}

void on_reverb_toggled(GtkToggleButton *button, gpointer user_data) {
    gboolean active = gtk_toggle_button_get_active(button);
    gtk_widget_set_sensitive(reverb_delay_scale, active);
    gtk_widget_set_sensitive(reverb_decay_scale, active);
}

void on_volume_adjustment_changed(GtkRange *range, gpointer user_data) {
    volume_adjustment_db = gtk_range_get_value(range);
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
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 0.0);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), "Processing...");
    total_files = 0;
    processed_files = 0;
    process_audio_files();
}

void process_audio_files(void) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(file_list));

    total_files = 0;
    processed_files = 0;
    current_progress = 0.0;
    processing_active = TRUE;

    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
        GtkWidget *checkbox = GTK_WIDGET(iter->data);
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox))) {
            total_files++;
        }
    }

    processing_thread = g_thread_new("audio_processing", process_audio_files_thread, children);

    g_timeout_add(100, update_progress_bar, NULL);
}

gpointer process_audio_files_thread(gpointer data) {
    GList *children = (GList *)data;

    for (GList *iter = children; iter != NULL; iter = g_list_next(iter)) {
        GtkWidget *checkbox = GTK_WIDGET(iter->data);
        if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox))) {
            const char *filename = gtk_button_get_label(GTK_BUTTON(checkbox));

            char *input_file = g_strdup_printf("%s/%s", current_dir, filename);
            char *output_file = g_strdup_printf("%s/%.*sMastered.%s", current_dir, 
                                                (int)(strlen(filename) - 4), filename, output_format);

            master_audio_file(input_file, output_file, 
                              gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(vocal_checkbox)), 
                              output_format);

            g_free(input_file);
            g_free(output_file);

            g_mutex_lock(&progress_mutex);
            processed_files++;
            current_progress = (gdouble)processed_files / total_files;
            g_cond_signal(&progress_cond);
            g_mutex_unlock(&progress_mutex);
        }
    }

    g_mutex_lock(&progress_mutex);
    processing_active = FALSE;
    g_cond_signal(&progress_cond);
    g_mutex_unlock(&progress_mutex);

    g_list_free(children);
    return NULL;
}

gboolean update_progress_bar(gpointer user_data) {
    g_mutex_lock(&progress_mutex);

    if (!processing_active && current_progress >= 1.0) {
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), 1.0);
        gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), "Processing complete");
        g_mutex_unlock(&progress_mutex);

        g_thread_join(processing_thread);

        update_file_list();
        return G_SOURCE_REMOVE;
    }

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), current_progress);
    char progress_text[32];
    snprintf(progress_text, sizeof(progress_text), "Processing: %.1f%%", current_progress * 100);
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), progress_text);

    g_mutex_unlock(&progress_mutex);
    return G_SOURCE_CONTINUE;
}

void master_audio_file(const char* input_file, const char* output_file, int vocal_mode, const char* output_format) {
    char filter_complex[COMMAND_SIZE / 2];
    char vocal_filters[COMMAND_SIZE / 4] = "";

    double stereo_width = gtk_range_get_value(GTK_RANGE(stereo_width_scale)) / 100.0;

    double low_thresh = gtk_range_get_value(GTK_RANGE(low_threshold));
    double low_comp_ratio = gtk_range_get_value(GTK_RANGE(low_ratio));
    double mid_thresh = gtk_range_get_value(GTK_RANGE(mid_threshold));
    double mid_comp_ratio = gtk_range_get_value(GTK_RANGE(mid_ratio));
    double high_thresh = gtk_range_get_value(GTK_RANGE(high_threshold));
    double high_comp_ratio = gtk_range_get_value(GTK_RANGE(high_ratio));
    double cross_low = gtk_range_get_value(GTK_RANGE(crossover_low));
    double cross_high = gtk_range_get_value(GTK_RANGE(crossover_high));

    snprintf(filter_complex, COMMAND_SIZE / 2,
        "aformat=channel_layouts=stereo:sample_rates=48000,"
        "highpass=f=20,lowpass=f=20000,"
        "afftdn=nr=10:nf=-25,"
        "compand=attacks=0.005:decays=0.1:points=-80/-80|-60/-40|-40/-20|-20/-10|-10/-5|0/0:soft-knee=6,"
        "equalizer=f=60:t=q:w=1.5:g=1,"
        "equalizer=f=120:t=q:w=1:g=-1,"
        "equalizer=f=1000:t=q:w=1.5:g=-1,"
        "equalizer=f=4000:t=q:w=1:g=2,"
        "equalizer=f=6000:t=q:w=1:g=1.5,"
        "equalizer=f=8000:t=q:w=1:g=1,"
        "equalizer=f=12000:t=q:w=1.5:g=1,"
        "stereotools=mlev=1:slev=%.2f:sbal=0:phase=0:mode=lr>lr,"
        "asplit=3[low][mid][high];"
        "[low]lowpass=f=%.1f,compand=attacks=0.01:decays=0.1:points=-80/-80|%.1f/%.1f|0/0:soft-knee=6:gain=1[clow];"
        "[mid]bandpass=f=%.1f:width_type=h:w=%.1f,compand=attacks=0.01:decays=0.1:points=-80/-80|%.1f/%.1f|0/0:soft-knee=6:gain=1[cmid];"
        "[high]highpass=f=%.1f,compand=attacks=0.01:decays=0.1:points=-80/-80|%.1f/%.1f|0/0:soft-knee=6:gain=1[chigh];"
        "[clow][cmid][chigh]amix=inputs=3:weights=1 1 1,"
        "loudnorm=I=-14:TP=-1:LRA=11,"
        "alimiter=level_in=0.9:level_out=0.9:limit=0.95:attack=5:release=50,"
        "volume=0.9,pan=stereo|c0=c0|c1=c1",
        stereo_width, cross_low, low_thresh, low_thresh/low_comp_ratio,
        (cross_low + cross_high) / 2, cross_high - cross_low,
        mid_thresh, mid_thresh/mid_comp_ratio,
        cross_high, high_thresh, high_thresh/high_comp_ratio
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

    char volume_adjustment[50];
    snprintf(volume_adjustment, sizeof(volume_adjustment), ",volume=%.1fdB", volume_adjustment_db);
    strncat(filter_complex, volume_adjustment, COMMAND_SIZE / 2 - strlen(filter_complex) - 1);

    char* command = malloc(COMMAND_SIZE);
    if (!command) {
        fprintf(stderr, "Memory allocation failed for command\n");
        return;
    }

    char ffmpeg_command[COMMAND_SIZE / 4];
    char output_options[COMMAND_SIZE / 4];

    snprintf(ffmpeg_command, COMMAND_SIZE / 4, "ffmpeg -hwaccel auto -i \"%s\" -threads 0 -filter_complex '", input_file);

    const char* selected_format = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(format_combo));
    snprintf(output_options, COMMAND_SIZE / 4, "' -ar 48000 -c:a %s \"%s\" -y", 
             strcmp(selected_format, "WAV") == 0 ? "pcm_s24le" : 
             strcmp(selected_format, "FLAC") == 0 ? "flac" : "libmp3lame", 
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

    char buffer[8192];
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
}

int check_ffmpeg_installed(void) {
    return system("ffmpeg -version > /dev/null 2>&1") == 0;
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

gboolean draw_waveform(GtkWidget *widget, cairo_t *cr, gpointer data) {
    guint width, height;
    GtkStyleContext *context;

    context = gtk_widget_get_style_context(widget);
    width = gtk_widget_get_allocated_width(widget);
    height = gtk_widget_get_allocated_height(widget);

    gtk_render_background(context, cr, 0, 0, width, height);

    if (waveform_data && waveform_size > 0) {
        cairo_set_source_rgb(cr, waveform_color[0], waveform_color[1], waveform_color[2]);
        cairo_set_line_width(cr, 1);

        double x_scale = (double)width / waveform_size;
        double y_scale = height / 2;

        cairo_move_to(cr, 0, height / 2);
        for (int i = 0; i < waveform_size; i++) {
            double x = i * x_scale;
            double y = height / 2 - waveform_data[i] * y_scale;
            cairo_line_to(cr, x, y);
        }

        cairo_stroke(cr);
    }

    return FALSE;
}

void update_waveform(const char *filename) {
    SNDFILE *file;
    SF_INFO sfinfo;

    memset(&sfinfo, 0, sizeof(sfinfo));
    file = sf_open(filename, SFM_READ, &sfinfo);
    if (!file) {
        fprintf(stderr, "Error opening file: %s\n", sf_strerror(file));
        return;
    }

    float *buffer = malloc(sfinfo.frames * sfinfo.channels * sizeof(float));
    if (!buffer) {
        fprintf(stderr, "Memory allocation error\n");
        sf_close(file);
        return;
    }

    sf_count_t count = sf_read_float(file, buffer, sfinfo.frames * sfinfo.channels);

    sf_close(file);

    if (waveform_data) {
        free(waveform_data);
    }
    waveform_data = malloc(MAX_WAVEFORM_POINTS * sizeof(float));
    if (!waveform_data) {
        fprintf(stderr, "Memory allocation error for waveform data\n");
        free(buffer);
        return;
    }

    waveform_size = MAX_WAVEFORM_POINTS;
    int samples_per_point = (sfinfo.frames * sfinfo.channels) / MAX_WAVEFORM_POINTS;

    for (int i = 0; i < MAX_WAVEFORM_POINTS; i++) {
        float max = 0;
        for (int j = 0; j < samples_per_point; j++) {
            int index = i * samples_per_point + j;
            if (index < count) {
                float abs_sample = fabs(buffer[index]);
                if (abs_sample > max) {
                    max = abs_sample;
                }
            }
        }
        waveform_data[i] = max;
    }

    free(buffer);

    if (strcmp(filename, original_file_path) == 0) {
        waveform_color[0] = 0.0;
        waveform_color[1] = 0.8;
        waveform_color[2] = 0.0;
    } else {
        waveform_color[0] = 0.8;
        waveform_color[1] = 0.0;
        waveform_color[2] = 0.8;
    }

    gtk_widget_queue_draw(waveform_drawing_area);
}

void cleanup_waveform() {
    if (waveform_data) {
        free(waveform_data);
        waveform_data = NULL;
    }
}

void *process_file_thread(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    master_audio_file(args->input_file, args->output_file, args->vocal_mode, args->output_format);
    free(arg);
    return NULL;
}

void update_progress(const char *message, double fraction) {
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(progress_bar), message);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), fraction);

    while (gtk_events_pending()) {
        gtk_main_iteration();
    }
}

int parse_arguments(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "i:o:vf:nh")) != -1) {
        switch (opt) {
            case 'i':
                strncpy(current_dir, optarg, sizeof(current_dir) - 1);
                break;
            case 'o':
                break;
            case 'v':
                break;
            case 'f':
                strncpy(output_format, optarg, sizeof(output_format) - 1);
                break;
            case 'n':
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                fprintf(stderr, "Unknown option: %c\n", opt);
                print_usage(argv[0]);
                return 1;
        }
    }
    return 0;
}



