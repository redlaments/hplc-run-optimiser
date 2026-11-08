#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <sqlite3.h>

#include "lib/file.h"
#include "lib/csv.h"
#include "lib/metadata.h"
#include "logic.h"
#include "run-db.h"
#include "query.h"
#include "macro.h"
#include "util.h"
#include "hplc-types.h"

#define MAX_DASH_ROWS 16
#define DASH_TABLE_COLUMNS 2
#define BENCH_TABLE_ROWS 4
#define BENCH_TABLE_COLS 2

// Global Active Mode State (Defaults to ISO)
static enum hplc_run_type selected_mode = ISO;
static int active_dash_rows = 12;

char *dash_table[MAX_DASH_ROWS][DASH_TABLE_COLUMNS];
char *bench_table[BENCH_TABLE_ROWS][BENCH_TABLE_COLS];
char *bench_head[BENCH_TABLE_COLS] = {
    "Parameter",
    "Value"
};
struct benchmarks bench;
bool screen = false;

static const char *const TABLE_HEADERS[DASH_TABLE_COLUMNS] = {
    "Variable", "Parameter"
};

#define NUM_BLUEPRINT_PARTS 7

typedef struct {
    const char *id;
    const char *name;
    const char *description;
    const char *image_path;
    GdkRectangle bounds;
} BlueprintPart;

static const BlueprintPart MACHINE_PARTS[NUM_BLUEPRINT_PARTS] = {
    {
        "Solvent Bottles",
        "Solvent Bottles",
        "The solvent bottles store the various solvents needed in a pure form. "
        "Solvents are mixed in the required proportions by the HPLC apparatus"
        "\n\nThis module affects the solvent parameter. The solvent is a chemical parameter and as such will influence the selectivity and efficiency of the chromatograms when changed",
        "test.jpg",
        {40, 40, 130, 80}
    },
    {
        "Pumps",
        "Pumps",
        "The pump generates the pressure for pushing the liquid through the "
        "column. There are at least 2 pump heads in each pump. This is as"
        " 1 head cannot generate a constant flow rate "
        "\n\nThis module affects the flow rate, a mechanical parameter, This will influence the efficiency but not the selectivity of chromatograms when changed"
        "\n\nThis module affects the %B, a chemical parameter. Changing this will inflence the selectivity of compounds in chromatograms.",
        "radiator.png",
        {40, 120, 130, 80}
    },
    {
        "Column",
        "Column",
        "The place where chromatography occurs"
        "\n\nThis module affects the column ligand, a chemical parameter. Changing this will affect the selectivity of compounds in chromatograms",
        "mcu.png",
        {40, 200, 120, 80}
    },
    {
        "Column Oven",
        "Column Oven",
        "Heats the column so the column can function at a consistant temperature"
        "\n\nThis module affect temperature, both a mechanical and chemical paramter. Changing this will affect both the selectivity of compounds and efficiency in chromatograms",
        "mcu.png",
        {40, 280, 120, 80}
    },
    {
        "Detector",
        "Detector",
        "Detects the presence of compounds in the eluted solvent. Typically "
        "via UV-Vis spectroscopy. Some systems are fitted with mass "
        "spectroscopy or other means of measurement",
        "mcu.png",
        {40, 360, 120, 80}
    },
    {
        "Degasser",
        "Degasser",
        "The degasser removes dissolved gas from the solvents. The amount of "
        "gas a liquid can dissolve changes at the pressure changes. As gases "
        "are compressable and liquids are not, this can cause issues with "
        "pressure generation at the pump head",
        "mcu.png",
        {40, 440, 120, 80}
    },
    {
        "Injector",
        "Injector",
        "The injector is where samples are loaded before they are sent into "
        "the instrument. Due to the design of the injector interface, the time"
        "at which the sample is loaded into the instrument can be seen on the "
        "chromatogram",
        "mcu.png",
        {40, 520, 120, 80}
    }
};

#define DB_TABLE_COLS 12

static const char *const DB_HEADERS[DB_TABLE_COLS] = {
    "Primary Key","Solvent","pH","Run Type","Flow Rate","Injection Volume","Injection Type","Sample Concentration","Column Type","Dimensions","Temperature","Wavelength"
};

// UI State & Context Structure
typedef struct {
    GtkWidget *window;
    GtkWidget *table_grid_container;
    GtkWidget *part_title_label;
    GtkWidget *part_desc_label;
    GtkWidget *part_image;
    GtkWidget *entry;
    GtkWidget *drawing_area;
    GtkWidget *db_grid_container;
    const BlueprintPart *active_part;
    GtkWidget *table_labels[MAX_DASH_ROWS][DASH_TABLE_COLUMNS];

    GtkWidget *config_tab_lbl[BENCH_TABLE_ROWS][BENCH_TABLE_COLS];
    GtkWidget *config_entry;
    GtkTextBuffer *config_console_buffer;
    GtkWidget     *config_console_view;

    GtkTextBuffer *console_buffer;
    GtkWidget     *console_view;
} AppContext;

AppContext app = {0};

// Forward Declarations
void hplc_column_brand(struct hplc_column *clmn);
enum selectivity_action multi_menu(void *adata, void **out_arg);
void update_metadata(struct hplc_run_metadata *m, enum selectivity_action action, void *adata);
void update_bench(struct benchmarks *ben);
void print_data(const char *restrict format, ...);
void print_error(char *format, ...);
void update_screen1_table_data(AppContext *app, const char *new_matrix[MAX_DASH_ROWS][DASH_TABLE_COLUMNS]);
void update_config_screen_table_data(AppContext *app, const char *new_matrix[BENCH_TABLE_ROWS][BENCH_TABLE_COLS]);
void rebuild_dash_table_ui(AppContext *app);

void init_dash()
{
    if (selected_mode == ISO) {
        active_dash_rows = 12;
        char *name[12][DASH_TABLE_COLUMNS] = {
            {"Solvent", ""},
            {"pH", ""},
            {"Run Type", "Isocratic"},
            {"%B", ""},
            {"Flow Rate (mL/s)", ""},
            {"Injection volume (microliters)", ""},
            {"Injection type", ""},
            {"Sample Concentration", ""},
            {"Column phase and brand", ""},
            {"Column measurements", ""},
            {"Temperature (C)", ""},
            {"Detector Wavelength (nm)", ""}
        };
        memcpy(dash_table, name, sizeof(name));
    } else {
        active_dash_rows = 14;
        char *name[14][DASH_TABLE_COLUMNS] = {
            {"Solvent", ""},
            {"pH", ""},
            {"Run Type", "Gradient"},
            {"%B Start", ""},
            {"%B End", ""},
            {"Gradient Time (tG min)", ""},
            {"Flow Rate (mL/s)", ""},
            {"Injection volume (microliters)", ""},
            {"Injection type", ""},
            {"Sample Concentration", ""},
            {"Column phase and brand", ""},
            {"Column measurements", ""},
            {"Temperature (C)", ""},
            {"Detector Wavelength (nm)", ""}
        };
        memcpy(dash_table, name, sizeof(name));
    }
}

void init_bench()
{
    char *name[BENCH_TABLE_ROWS][BENCH_TABLE_COLS] = {
        {"Peaks", "7"},
        {"Resolution", "1.4"},
        {"Runtime (mins)", "15"},
        {"Backpressure (psi)", "4000"}
    };
    memcpy(bench_table, name, sizeof(name));
    bench.peaks=7;
    bench.resolution=hplc_strtosi("1.4",NULL);
    bench.runtime=hplc_scale_int(15);
    bench.backpressure=4000;
}

// Backend Execution Logic
int add_run(char *input)
{
    FILE *f;
    struct hplc_data hdata;
    enum selectivity_action action;
    void *adata = NULL;
    char *new_pk=NULL;

    f = file_open(input, &hdata.data);
    if (!f) {
        print_error("file_open failed check your file_name adheres to the naming standard!\n");
        return 0;
    }
    if (hplc_column_phase(&hdata.data) < 0) {
        print_error("Column phase %s is invalid\n", hdata.data.column.clmn_phase);
        hplc_metadata_destroy(&hdata.data);
        fclose(f);
        return 0;
    }
    if (!csv_parse_file(f, &hdata)) {
        print_error("csv parsing failed\n");
        hplc_metadata_destroy(&hdata.data);
        fclose(f);
        return 0;
    }
    if (!hplc_run_check(input))
        hplc_run_add(&hdata.data, input);

    // Enforce active run mode inside union
    hdata.data.run.type = selected_mode;

    // Dispatch logic check based on run type
    if (hdata.data.run.type == GRA) {
        action = gra_logic(&hdata, &bench, &adata);
    } else {
        action = logic(&hdata, &bench, &adata);
    }

    if (action == MULTI ){
        void *selected_arg = NULL;
        void *options_array = adata;
        action = multi_menu(options_array, &selected_arg);
        free(options_array);
        adata = selected_arg;

	switch (action) {
/**/	    case CDFLOW:
		hdata.data.flow_rate-=*(hplc_sint*)adata;
		break;
/**/	    case CIFLOW:
		hdata.data.flow_rate+=*(hplc_sint*)adata;
		break;
/**/	    case SUCCESS:
		update_metadata(&hdata.data, action, adata);
		if (adata) free(adata);
		hplc_destroy(&hdata);
		fclose(f);
		return 1;
/**/	    case SPP:
		print_data("Changed SPP\n");
		hplc_column_brand(&hdata.data.column);
		break;
/**/	    case CDPS:
		hdata.data.column.clmn_bead_size = *(hplc_sint *)adata;
		hplc_column_brand(&hdata.data.column);
		break;
/**/	    case CCL:
		hdata.data.column.clmn_length = *(hplc_sint *)adata;
		hplc_column_brand(&hdata.data.column);
		break;
	    default:
		print_error("Option not recognised:%s\n", resolve_action_name(action));
		if (adata) free(adata);
		hplc_destroy(&hdata);
		fclose(f);
		return 0;
	    }
    }
    else{
	    switch (action) {
	    case SUCCESS:
		update_metadata(&hdata.data, action, adata);
	    case ERROR:
		free(adata);
		hplc_destroy(&hdata);
		fclose(f);
		return 1;
	    case SPP:
		print_data("Changed SPP\n");
		hplc_column_brand(&hdata.data.column);
		break;
	    case CLIG:
		hplc_column_brand(&hdata.data.column);
		break;
	    case CDPS:
		hplc_column_brand(&hdata.data.column);
		break;
	    case CCL:
		hplc_column_brand(&hdata.data.column);
		break;
	    default:
		break;
	    }
    }

    if (!hplc_run_meta_check(&hdata.data, &new_pk)) {
        update_metadata(&hdata.data, action, adata);
    } else {
    	char*a_tmp=action_str(action,adata);
	print_data("%s:Found %s matching change, changing that\n",a_tmp,new_pk);
	free(a_tmp);

        add_run(new_pk);
        free(new_pk);
    }

    free(adata);
    hplc_destroy(&hdata);
    fclose(f);

    return 1;
}

// C Backend Helpers
void c_backend_process_command(const char *input_text) {
    printf("[C Backend] Received text command: \"%s\"\n", input_text);
    add_run((char *)input_text);
}

void c_backend_on_part_hovered(const BlueprintPart *part) {
    if (part) {
        printf("[C Backend] Hovering part ID: %s | Name: %s\n", part->id, part->name);
    } else {
        printf("[C Backend] Pointer left active blueprint area.\n");
    }
}

// Callbacks & Logging
void gui_log(AppContext *app_ptr, const char *text, bool is_error) {
    GtkTextBuffer *l_buf;
    GtkWidget *l_view;
    GtkTextIter end;

    if (!app_ptr)
        return;
    if (!screen) {
        l_buf = app_ptr->console_buffer;
        l_view = app_ptr->console_view;
    } else {
        l_buf = app_ptr->config_console_buffer;
        l_view = app_ptr->config_console_view;
    }
    if (!l_buf || !l_view)
        return;

    gtk_text_buffer_get_end_iter(l_buf, &end);

    const char *tag_name = is_error ? "stderr_tag" : "stdout_tag";
    gtk_text_buffer_insert_with_tags_by_name(l_buf, &end, text, -1, tag_name, NULL);

    GtkTextMark *mark = gtk_text_buffer_get_insert(l_buf);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(l_view), mark, 0.0, FALSE, 0.0, 1.0);
}

void print_data(const char *restrict format, ...)
{
    va_list ap,ap_copy;
    char *tmp;
    size_t size;

    va_start(ap, format);
    va_copy(ap_copy,ap);

    size = vsnprintf(NULL, 0, format, ap);
    va_end(ap);

    tmp = malloc(sizeof(char) * ++size);
    vsnprintf(tmp, size, format, ap_copy);
    va_end(ap_copy);

    gui_log(&app, tmp, false);
    free(tmp);
}

void print_error(char *format, ...)
{
    va_list ap,ap_copy;
    char *tmp;
    size_t size;

    va_start(ap, format);
    va_copy(ap_copy,ap);

    size = vsnprintf(NULL, 0, format, ap);
    va_end(ap);

    tmp = malloc(sizeof(char) * ++size);
    vsnprintf(tmp, size, format, ap_copy);
    va_end(ap_copy);

    gui_log(&app, tmp, true);
    free(tmp);
}

void config_help()
{
    print_data("Please input the config as follows:"
               "{peaks} {resolution} {runtime} {backpressure}.\n"
               "For example, enter \"7 1.4 15 4000\" to set config to desiring "
               "7 peaks, a resolution of 1.4, a max runtime of 15 mins and "
               "a max backpressure under 4000 psi\n");
}

static void on_command_submitted(GtkButton *button, gpointer user_data) {
    AppContext *app_ptr = (AppContext *)user_data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(app_ptr->entry));

    screen = false;
    c_backend_process_command(text);
    gtk_editable_set_text(GTK_EDITABLE(app_ptr->entry), "");
}

static void handle_config(GtkButton *button, gpointer user_data) {
    AppContext *app_ptr = (AppContext *)user_data;
    const char *text;
    int argc, argv_size = 4;
    char *tmp, *argv[argv_size];

    screen = true;
    text = gtk_editable_get_text(GTK_EDITABLE(app_ptr->config_entry));
    tmp = strdup(text);
    argc = tokenize_str1(tmp, argv, argv_size, ' ');
    if (argc != 4) {
        config_help();
    } else {
        bench.peaks = strtol(argv[0], NULL, 10);
        bench.resolution = hplc_strtosi(argv[1], NULL);
        bench.runtime = hplc_strtosi(argv[2], NULL);
        bench.backpressure = strtol(argv[3], NULL, 10);
        update_bench(&bench);
    }
    free(tmp);
    gtk_editable_set_text(GTK_EDITABLE(app_ptr->config_entry), "");
}

static void on_blueprint_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    AppContext *app_ptr = (AppContext *)user_data;

    cairo_set_source_rgb(cr, 0.08, 0.12, 0.20);
    cairo_paint(cr);

    cairo_set_source_rgba(cr, 0.20, 0.35, 0.50, 0.35);
    cairo_set_line_width(cr, 1.0);
    for (int x = 0; x < width; x += 20) {
        cairo_move_to(cr, x, 0);
        cairo_line_to(cr, x, height);
    }
    for (int y = 0; y < height; y += 20) {
        cairo_move_to(cr, 0, y);
        cairo_line_to(cr, width, y);
    }
    cairo_stroke(cr);

    for (int i = 0; i < NUM_BLUEPRINT_PARTS; i++) {
        const BlueprintPart *p = &MACHINE_PARTS[i];
        bool is_hovered = (app_ptr->active_part == p);

        if (is_hovered) {
            cairo_set_source_rgb(cr, 0.20, 0.85, 1.0);
            cairo_set_line_width(cr, 2.5);
        } else {
            cairo_set_source_rgb(cr, 0.40, 0.60, 0.80);
            cairo_set_line_width(cr, 1.5);
        }

        cairo_rectangle(cr, p->bounds.x, p->bounds.y, p->bounds.width, p->bounds.height);
        cairo_stroke_preserve(cr);

        if (is_hovered) {
            cairo_set_source_rgba(cr, 0.20, 0.85, 1.0, 0.25);
        } else {
            cairo_set_source_rgba(cr, 0.10, 0.30, 0.50, 0.30);
        }
        cairo_fill(cr);

        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 10.0);
        cairo_move_to(cr, p->bounds.x + 8, p->bounds.y + 20);
        cairo_show_text(cr, p->id);
    }
}

static void on_blueprint_motion(GtkEventControllerMotion *controller, double x, double y, gpointer user_data) {
    AppContext *app_ptr = (AppContext *)user_data;

    const BlueprintPart *hovered = NULL;
    for (int i = 0; i < NUM_BLUEPRINT_PARTS; i++) {
        const BlueprintPart *p = &MACHINE_PARTS[i];
        if (x >= p->bounds.x && x <= (p->bounds.x + p->bounds.width) &&
            y >= p->bounds.y && y <= (p->bounds.y + p->bounds.height)) {
            hovered = p;
            break;
        }
    }

    if (hovered != app_ptr->active_part) {
        app_ptr->active_part = hovered;
        c_backend_on_part_hovered(hovered);

        if (hovered) {
            gtk_label_set_text(GTK_LABEL(app_ptr->part_title_label), hovered->name);
            gtk_label_set_text(GTK_LABEL(app_ptr->part_desc_label), hovered->description);
        } else {
            gtk_label_set_text(GTK_LABEL(app_ptr->part_title_label), "Machine Blueprint");
            gtk_label_set_text(GTK_LABEL(app_ptr->part_desc_label), "Hover mouse cursor over a component region to inspect details and images.");
            gtk_image_set_from_icon_name(GTK_IMAGE(app_ptr->part_image), "dialog-information");
        }

        gtk_widget_queue_draw(app_ptr->drawing_area);
    }
}

static void populate_db_grid(AppContext *app_ptr) {
    GtkWidget *old_grid = gtk_widget_get_first_child(app_ptr->db_grid_container);
    if (old_grid) {
        gtk_box_remove(GTK_BOX(app_ptr->db_grid_container), old_grid);
    }

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 24);
    gtk_widget_set_vexpand(grid, TRUE);
    gtk_widget_set_hexpand(grid, TRUE);

    for (int col = 0; col < DB_TABLE_COLS; col++) {
        char header_markup[128];
        snprintf(header_markup, sizeof(header_markup), "<b><u>%s</u></b>", DB_HEADERS[col]);
        GtkWidget *lbl = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(lbl), header_markup);
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), lbl, col, 0, 1, 1);
    }

    sqlite3_stmt *stmt = hplc_run_table_prepare();
    if (stmt) {
        char **pdata = NULL;
        int row_idx = 0;

        while (hplc_run_table_row(stmt, &pdata)) {
            if (pdata) {
                for (int col = 0; col < DB_TABLE_COLS; col++) {
                    const char *cell_text = pdata[col] ? pdata[col] : "";
                    GtkWidget *lbl = gtk_label_new(cell_text);
                    gtk_widget_set_halign(lbl, GTK_ALIGN_START);
                    gtk_grid_attach(GTK_GRID(grid), lbl, col, row_idx + 1, 1, 1);

                    if (pdata[col]) {
                        free(pdata[col]);
                    }
                }
                free(pdata);
                pdata = NULL;
            }
            row_idx++;
        }
        hplc_run_table_row_destroy(stmt);
    }

    gtk_box_append(GTK_BOX(app_ptr->db_grid_container), grid);
    gtk_widget_queue_resize(app_ptr->db_grid_container);
}

static void on_refresh_db_clicked(GtkButton *button, gpointer user_data) {
    AppContext *app_ptr = (AppContext *)user_data;
    populate_db_grid(app_ptr);
}

static void db_run_delete(GtkButton *button, gpointer user_data) {
    AppContext *app_ptr = (AppContext *)user_data;
    hplc_run_delete();
    on_refresh_db_clicked(button, user_data);
}

void update_config_screen_table_data(AppContext *app_ptr,
    const char *new_matrix[BENCH_TABLE_ROWS][BENCH_TABLE_COLS])
{
    for (int row = 0; row < BENCH_TABLE_ROWS; row++) {
        for (int col = 0; col < BENCH_TABLE_COLS; col++) {
            if (app_ptr->config_tab_lbl[row][col] != NULL) {
                gtk_label_set_markup(
                    GTK_LABEL(app_ptr->config_tab_lbl[row][col]),
                    new_matrix[row][col] ? new_matrix[row][col] : ""
                );
            }
        }
    }
}

void update_screen1_table_data(AppContext *app_ptr, const char *new_matrix[MAX_DASH_ROWS][DASH_TABLE_COLUMNS]) {
    for (int row = 0; row < active_dash_rows; row++) {
        for (int col = 0; col < DASH_TABLE_COLUMNS; col++) {
            if (app_ptr->table_labels[row][col] != NULL) {
                gtk_label_set_markup(
                    GTK_LABEL(app_ptr->table_labels[row][col]),
                    new_matrix[row][col] ? new_matrix[row][col] : ""
                );
            }
        }
    }
}

void rebuild_dash_table_ui(AppContext *app_ptr) {
    if (!app_ptr->table_grid_container) return;

    GtkWidget *old_grid = gtk_widget_get_first_child(app_ptr->table_grid_container);
    if (old_grid) {
        gtk_box_remove(GTK_BOX(app_ptr->table_grid_container), old_grid);
    }

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 24);

    for (int col = 0; col < DASH_TABLE_COLUMNS; col++) {
        char header_markup[128];
        snprintf(header_markup, sizeof(header_markup), "<b><u>%s</u></b>", TABLE_HEADERS[col]);
        GtkWidget *lbl = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(lbl), header_markup);
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), lbl, col, 0, 1, 1);
    }

    for (int row = 0; row < active_dash_rows; row++) {
        for (int col = 0; col < DASH_TABLE_COLUMNS; col++) {
            GtkWidget *lbl = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(lbl), dash_table[row][col]);
            gtk_widget_set_halign(lbl, GTK_ALIGN_START);
            gtk_grid_attach(GTK_GRID(grid), lbl, col, row + 1, 1, 1);

            app_ptr->table_labels[row][col] = lbl;
        }
    }

    gtk_box_append(GTK_BOX(app_ptr->table_grid_container), grid);
}

// UI Building Functions
static GtkWidget *build_table_screen(AppContext *app_ptr) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b>MonasHPLC Run Parameters</b>");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), title);

    app_ptr->table_grid_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(vbox), app_ptr->table_grid_container);
    rebuild_dash_table_ui(app_ptr);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(sep, 8);
    gtk_widget_set_margin_bottom(sep, 8);
    gtk_box_append(GTK_BOX(vbox), sep);

    GtkWidget *entry_label = gtk_label_new("Execute Backend Command:");
    gtk_widget_set_halign(entry_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), entry_label);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    app_ptr->entry = gtk_entry_new();
    char *tmp, *dir, *help = "Enter Primary Key e.g. GR19-16";
    int buf_size;
    dir = current_dir();
    buf_size = snprintf(NULL, 0, "%s, Ensure csv file is in current dir:%s", help, dir);
    tmp = malloc(sizeof(char) * ++buf_size);
    snprintf(tmp, buf_size, "%s, Ensure csv file is in current dir:%s", help, dir);
    tmp[buf_size - 1] = '\0';
    gtk_entry_set_placeholder_text(GTK_ENTRY(app_ptr->entry), tmp);
    free(dir);
    free(tmp);

    gtk_widget_set_hexpand(app_ptr->entry, TRUE);
    g_signal_connect(app_ptr->entry, "activate", G_CALLBACK(on_command_submitted), app_ptr);
    gtk_box_append(GTK_BOX(hbox), app_ptr->entry);

    GtkWidget *btn = gtk_button_new_with_label("Send");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_command_submitted), app_ptr);
    gtk_box_append(GTK_BOX(hbox), btn);
    gtk_box_append(GTK_BOX(vbox), hbox);

    GtkWidget *console_label = gtk_label_new("Terminal / Execution Output:");
    gtk_widget_set_halign(console_label, GTK_ALIGN_START);
    gtk_widget_set_margin_top(console_label, 8);
    gtk_box_append(GTK_BOX(vbox), console_label);

    GtkWidget *scrolled_win = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled_win, TRUE);
    gtk_widget_set_hexpand(scrolled_win, TRUE);
    gtk_widget_set_size_request(scrolled_win, -1, 120);

    app_ptr->console_buffer = gtk_text_buffer_new(NULL);
    app_ptr->console_view = gtk_text_view_new_with_buffer(app_ptr->console_buffer);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app_ptr->console_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(app_ptr->console_view), TRUE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(app_ptr->console_view), 8);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(app_ptr->console_view), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(app_ptr->console_view), 8);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(app_ptr->console_view), 8);

    gtk_text_buffer_create_tag(app_ptr->console_buffer, "stdout_tag", "foreground", "#000000", NULL);
    gtk_text_buffer_create_tag(app_ptr->console_buffer, "stderr_tag", "foreground", "#FF3333", NULL);

    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_win), app_ptr->console_view);
    gtk_box_append(GTK_BOX(vbox), scrolled_win);

    print_data("Example: GR19-16 (primary key) represents file GR19-16-2-ACN-3-ISO-38-1.5-20u-CP-0.1-C18P-150x4.6x5-40-254.csv in folder\n");
    return vbox;
}

static GtkWidget *build_blueprint_screen(AppContext *app_ptr) {
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_margin_start(hbox, 16);
    gtk_widget_set_margin_end(hbox, 16);
    gtk_widget_set_margin_top(hbox, 16);
    gtk_widget_set_margin_bottom(hbox, 16);

    app_ptr->drawing_area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(app_ptr->drawing_area), 350);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(app_ptr->drawing_area), 260);
    gtk_widget_set_hexpand(app_ptr->drawing_area, TRUE);
    gtk_widget_set_vexpand(app_ptr->drawing_area, TRUE);

    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(app_ptr->drawing_area), on_blueprint_draw, app_ptr, NULL);

    GtkEventController *motion_controller = gtk_event_controller_motion_new();
    g_signal_connect(motion_controller, "motion", G_CALLBACK(on_blueprint_motion), app_ptr);
    gtk_widget_add_controller(app_ptr->drawing_area, motion_controller);

    gtk_box_append(GTK_BOX(hbox), app_ptr->drawing_area);

    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_size_request(panel, 220, -1);

    app_ptr->part_title_label = gtk_label_new("Machine Blueprint");
    PangoAttrList *attr_list = pango_attr_list_new();
    pango_attr_list_insert(attr_list, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    pango_attr_list_insert(attr_list, pango_attr_scale_new(1.2));
    gtk_label_set_attributes(GTK_LABEL(app_ptr->part_title_label), attr_list);
    pango_attr_list_unref(attr_list);
    gtk_widget_set_halign(app_ptr->part_title_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(panel), app_ptr->part_title_label);

    app_ptr->part_image = gtk_image_new_from_icon_name("dialog-information");
    gtk_image_set_pixel_size(GTK_IMAGE(app_ptr->part_image), 96);
    gtk_box_append(GTK_BOX(panel), app_ptr->part_image);

    app_ptr->part_desc_label = gtk_label_new("Hover mouse cursor over a component region to inspect details and images.");
    gtk_label_set_wrap(GTK_LABEL(app_ptr->part_desc_label), TRUE);
    gtk_widget_set_halign(app_ptr->part_desc_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(panel), app_ptr->part_desc_label);

    gtk_box_append(GTK_BOX(hbox), panel);

    return hbox;
}

static GtkWidget *build_db_screen(AppContext *app_ptr) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);

    GtkWidget *top_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b>MonasHPLC Past Runs (local db)</b>");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_widget_set_hexpand(title, TRUE);
    gtk_box_append(GTK_BOX(top_bar), title);

    GtkWidget *fresh_btn = gtk_button_new_with_label("Delete all runs");
    g_signal_connect(fresh_btn, "clicked", G_CALLBACK(db_run_delete), app_ptr);
    gtk_box_append(GTK_BOX(top_bar), fresh_btn);

    GtkWidget *refresh_btn = gtk_button_new_with_label("Fetch / Query DB");
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_refresh_db_clicked), app_ptr);
    gtk_box_append(GTK_BOX(top_bar), refresh_btn);

    gtk_box_append(GTK_BOX(vbox), top_bar);

    GtkWidget *scrolled_win = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled_win, TRUE);
    gtk_widget_set_hexpand(scrolled_win, TRUE);

    app_ptr->db_grid_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_win), app_ptr->db_grid_container);

    gtk_box_append(GTK_BOX(vbox), scrolled_win);

    populate_db_grid(app_ptr);

    return vbox;
}

static GtkWidget *build_run_config_screen(AppContext *app_ptr) {
    GtkWidget *vbox;
    GtkWidget *title;
    GtkWidget *grid;
    GtkWidget *sep;
    GtkWidget *entry_label;
    GtkWidget *hbox;
    GtkWidget *btn;
    GtkWidget *console_lbl;
    GtkWidget *scrolled_win;
    char *help = "Enter config: <i>desired peaks</i> "
                 "<i>desired resolution</i> <i>desired runtime (min)</i> "
                 "<i>desired backpressure (psi)</i>";

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);
    title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b>MonasHPLC desired Run Attributes</b>");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), title);

    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 24);

    for (int col = 0; col < BENCH_TABLE_COLS; col++) {
        char header_markup[128];
        GtkWidget *lbl;

        snprintf(header_markup, sizeof(header_markup), "<b><u>%s</u></b>", bench_head[col]);
        lbl = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(lbl), header_markup);
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), lbl, col, 0, 1, 1);
    }

    for (int row = 0; row < BENCH_TABLE_ROWS; row++) {
        for (int col = 0; col < BENCH_TABLE_COLS; col++) {
            GtkWidget *lbl;

            lbl = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(lbl), bench_table[row][col]);
            gtk_widget_set_halign(lbl, GTK_ALIGN_START);
            gtk_grid_attach(GTK_GRID(grid), lbl, col, row + 1, 1, 1);

            app_ptr->config_tab_lbl[row][col] = lbl;
        }
    }

    gtk_box_append(GTK_BOX(vbox), grid);

    sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(sep, 8);
    gtk_widget_set_margin_bottom(sep, 8);
    gtk_box_append(GTK_BOX(vbox), sep);

    entry_label = gtk_label_new("Execute config parameters");
    gtk_widget_set_halign(entry_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), entry_label);
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    app_ptr->config_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(app_ptr->config_entry), help);
    gtk_widget_set_hexpand(app_ptr->config_entry, TRUE);
    g_signal_connect(app_ptr->config_entry, "activate", G_CALLBACK(handle_config), app_ptr);
    gtk_box_append(GTK_BOX(hbox), app_ptr->config_entry);

    btn = gtk_button_new_with_label("Send");
    g_signal_connect(btn, "clicked", G_CALLBACK(handle_config), app_ptr);
    gtk_box_append(GTK_BOX(hbox), btn);
    gtk_box_append(GTK_BOX(vbox), hbox);

    console_lbl = gtk_label_new("Output");
    gtk_widget_set_halign(console_lbl, GTK_ALIGN_START);
    gtk_widget_set_margin_top(console_lbl, 8);
    gtk_box_append(GTK_BOX(vbox), console_lbl);

    scrolled_win = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled_win, TRUE);
    gtk_widget_set_hexpand(scrolled_win, TRUE);
    gtk_widget_set_size_request(scrolled_win, -1, 120);

    app_ptr->config_console_buffer = gtk_text_buffer_new(NULL);
    app_ptr->config_console_view = gtk_text_view_new_with_buffer(app_ptr->config_console_buffer);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(app_ptr->config_console_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(app_ptr->config_console_view), TRUE);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(app_ptr->config_console_view), 8);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(app_ptr->config_console_view), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(app_ptr->config_console_view), 8);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(app_ptr->config_console_view), 8);

    gtk_text_buffer_create_tag(app_ptr->config_console_buffer, "stdout_tag", "foreground", "#000000", NULL);
    gtk_text_buffer_create_tag(app_ptr->config_console_buffer, "stderr_tag", "foreground", "#FF3333", NULL);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_win), app_ptr->config_console_view);
    gtk_box_append(GTK_BOX(vbox), scrolled_win);

    return vbox;
}

// Modal Multi-Menu Selection Dialog
typedef struct {
    GMainLoop *loop;
    enum selectivity_action action;
    void *arg;
    bool selected;
} MultiMenuContext;

static void on_option_button_clicked(GtkButton *button, gpointer user_data) {
    GtkWidget *window = GTK_WIDGET(user_data);
    MultiMenuContext *ctx = (MultiMenuContext *)g_object_get_data(G_OBJECT(button), "menu_ctx");
    const struct action_arg *choice = (const struct action_arg *)g_object_get_data(G_OBJECT(button), "choice_str");

    if (choice) {
        ctx->action = choice->action;
        ctx->arg = choice->arg;
        ctx->selected = true;
    }

    gtk_window_destroy(GTK_WINDOW(window));
}

static void on_window_destroy(GtkWidget *widget, gpointer user_data) {
    MultiMenuContext *ctx = (MultiMenuContext *)user_data;
    if (ctx->loop && g_main_loop_is_running(ctx->loop)) {
        g_main_loop_quit(ctx->loop);
    }
}

enum selectivity_action multi_menu(void *adata, void **out_arg) {
    struct action_arg *options = (struct action_arg *)adata;
    if (!options) {
        if (out_arg) *out_arg = NULL;
        return 0;
    }

    MultiMenuContext ctx = {
        .loop = g_main_loop_new(NULL, FALSE),
        .action = 0,
        .arg = NULL,
        .selected = false
    };

    GtkWidget *window = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(window), "Select Action");
    gtk_window_set_modal(GTK_WINDOW(window), TRUE);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);

    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), &ctx);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    GtkWidget *label = gtk_label_new("Multiple actions available. Please select one:");
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), label);

    for (int i = 0; options[i].action != END; i++) {
        char *tmp = action_arg_str(options[i]);
        GtkWidget *btn = gtk_button_new_with_label(tmp);

        g_object_set_data(G_OBJECT(btn), "menu_ctx", &ctx);
        g_object_set_data(G_OBJECT(btn), "choice_str", &options[i]);

        g_signal_connect(btn, "clicked", G_CALLBACK(on_option_button_clicked), window);
        gtk_box_append(GTK_BOX(vbox), btn);
        free(tmp);
    }

    gtk_window_present(GTK_WINDOW(window));

    g_main_loop_run(ctx.loop);
    g_main_loop_unref(ctx.loop);

    // Free unselected option payloads
    for (int i = 0; options[i].action != END; i++) {
        if (options[i].arg != ctx.arg) {
            free(options[i].arg);
        }
    }

    if (out_arg) {
        *out_arg = ctx.arg;
    }

    return ctx.action;
}
void update_metadata(struct hplc_run_metadata *m, enum selectivity_action action, void *adata)
{
    char tmp_brand[100];
    char tmp_dims[100];
    char tmp_wavelength[32];

    char *colors[] = {
        "<span foreground=\"lime\">%s</span>",
        "<span foreground=\"cyan\">%s</span>",
        "<span foreground=\"orange\">%s</span>"
    };
    int index = -1, color = 0, start = 0, end = -1;
    
    char*a_tmp=action_str(action,adata);
    print_data("%s\n",a_tmp);
    free(a_tmp);

    if (action == SUCCESS) {
        dash_table[0][1] = "Success";
        for (int i = 1; i < active_dash_rows; i++) {
            dash_table[i][1] = "";
        }
        update_screen1_table_data(&app, (const char * (*)[DASH_TABLE_COLUMNS])dash_table);
        return;
    }

    switch (action) {
    case CITEMP:
    case CDTEMP:
        index = (m->run.type == GRA ? 12 : 10);
        color = 2;
        end = -1;
        break;
    case CSOL:
        index = 0;
        color = 0;
        end = -1;
        break;
    case CIB:
    case CDB:
        if (m->run.type == ISO) {
            index = 3;
        } else {
            index = 3;
        }
        color = 0;
        end = -1;
        break;
    case CMTG:
    case CDVTG:
        if (m->run.type == GRA) {
            index = 5;
            color = 0;
            end = -1;
        }
        break;
    case CDFLOW:
    case CIFLOW:
    case CMFLOW:
        index = (m->run.type == GRA ? 6 : 4);
        color = 1;
        end = -1;
        break;
    case SPP:
        color = 1;
        print_data("Changed SPP\n");
        hplc_column_brand(&m->column);
        index = (m->run.type == GRA ? 10 : 8);
        break;
    case CLIG:
        hplc_column_brand(&m->column);
        color = 0;
        index = (m->run.type == GRA ? 10 : 8);
        break;
    case CDPS:
        hplc_column_brand(&m->column);
        color = 1;
        index = (m->run.type == GRA ? 11 : 9);
        break;
    case CCL:
        hplc_column_brand(&m->column);
        color = 1;
        index = (m->run.type == GRA ? 11 : 9);
        break;
    default:
        print_error("Option not recognised:%s\n", resolve_action_name(action));
        break;
    }

    char *str_ph   = hplc_sint_str(m->pH);
    char *str_fr   = hplc_sint_str(m->flow_rate);
    char *str_sc   = hplc_sint_str(m->sample_conc);
    char *str_temp;
    if(m->temperature==AMB)
    	str_temp=strdup("amb");
    else
    	str_temp= hplc_sint_str(m->temperature);
    char *str_brand= hplc_clmn_brand_str(m->column.brand);

    char *s_len  = hplc_sint_str(m->column.clmn_length);
    char *s_id   = hplc_sint_str(m->column.clmn_id);
    char *s_bead = hplc_sint_str(m->column.clmn_bead_size);

    snprintf(tmp_brand, sizeof(tmp_brand), "%s %s", m->column.clmn_phase ? m->column.clmn_phase : "", str_brand ? str_brand : "");
    snprintf(tmp_dims, sizeof(tmp_dims), "%sx%sx%s", s_len ? s_len : "", s_id ? s_id : "", s_bead ? s_bead : "");
    snprintf(tmp_wavelength, sizeof(tmp_wavelength), "%d", m->wavelength);

    free(s_len);
    free(s_id);
    free(s_bead);

    char *str_sp = NULL;
    char *str_b_start = NULL;
    char *str_b_end = NULL;
    char *str_tg = NULL;

    if (m->run.type == ISO) {
        str_sp = hplc_sint_str(m->run.iso.slvnt_percent);

        dash_table[0][1]  = m->slvnt;
        dash_table[1][1]  = str_ph;
        dash_table[2][1]  = "Isocratic";
        dash_table[3][1]  = str_sp;
        dash_table[4][1]  = str_fr;
        dash_table[5][1]  = m->njctn_volume;
        dash_table[6][1]  = m->njctn_type;
        dash_table[7][1]  = str_sc;
        dash_table[8][1]  = tmp_brand;
        dash_table[9][1]  = tmp_dims;
        dash_table[10][1] = str_temp;
        dash_table[11][1] = tmp_wavelength;
    } else {
        str_b_start = hplc_sint_str(m->run.gra.slvnt_start);
        str_b_end   = hplc_sint_str(m->run.gra.slvnt_end);
        str_tg      = hplc_sint_str(m->run.gra.tg);

        dash_table[0][1]  = m->slvnt;
        dash_table[1][1]  = str_ph;
        dash_table[2][1]  = "Gradient";
        dash_table[3][1]  = str_b_start;
        dash_table[4][1]  = str_b_end;
        dash_table[5][1]  = str_tg;
        dash_table[6][1]  = str_fr;
        dash_table[7][1]  = m->njctn_volume;
        dash_table[8][1]  = m->njctn_type;
        dash_table[9][1]  = str_sc;
        dash_table[10][1] = tmp_brand;
        dash_table[11][1] = tmp_dims;
        dash_table[12][1] = str_temp;
        dash_table[13][1] = tmp_wavelength;
    }

    if (index < 0 || index >= active_dash_rows) {
        update_screen1_table_data(&app, (const char * (*)[DASH_TABLE_COLUMNS])dash_table);
    } else {
        char *original = dash_table[index][1];

        if (end == -1) {
            char *markup = g_markup_printf_escaped(colors[color], original ? original : "");
            dash_table[index][1] = markup;
            update_screen1_table_data(&app, (const char * (*)[DASH_TABLE_COLUMNS])dash_table);
            g_free(markup);
        } else {
            size_t len = original ? strlen(original) : 0;
            if (start >= 0 && (size_t)start < len && end > start && (size_t)end <= len) {
                char prefix[100] = {0};
                char target[100] = {0};
                char suffix[100] = {0};
                char full_markup[256];

                strncpy(prefix, original, start);
                prefix[start] = '\0';

                strncpy(target, original + start, end - start);
                target[end - start] = '\0';

                strncpy(suffix, original + end, sizeof(suffix) - 1);
                suffix[sizeof(suffix) - 1] = '\0';

                char *escaped_target = g_markup_printf_escaped(colors[color], target);
                snprintf(full_markup, sizeof(full_markup), "%s%s%s", prefix, escaped_target, suffix);

                dash_table[index][1] = full_markup;
                update_screen1_table_data(&app, (const char * (*)[DASH_TABLE_COLUMNS])dash_table);
                g_free(escaped_target);
            } else {
                update_screen1_table_data(&app, (const char * (*)[DASH_TABLE_COLUMNS])dash_table);
            }
        }
        dash_table[index][1] = original;
    }

    // Deferred memory freeing AFTER rendering UI table markup
    free(str_ph);
    free(str_fr);
    free(str_sc);
    free(str_temp);
    free(str_brand);
    free(str_sp);
    free(str_b_start);
    free(str_b_end);
    free(str_tg);
}
void update_bench(struct benchmarks *ben)
{
    char tmp[2][50], *tmp1[2];

    snprintf(tmp[0], sizeof(tmp[0]), "%d", ben->peaks);
    tmp1[0] = hplc_sint_str(ben->resolution);
    tmp1[1] = hplc_sint_str(ben->runtime);
    snprintf(tmp[1], sizeof(tmp[1]), "%d", ben->backpressure);

    bench_table[0][1] = tmp[0];
    bench_table[1][1] = tmp1[0];
    bench_table[2][1] = tmp1[1];
    bench_table[3][1] = tmp[1];

    update_config_screen_table_data(&app, (const char * (*)[BENCH_TABLE_COLS])bench_table);
    free(tmp1[0]);
    free(tmp1[1]);
}

// Mode Selection Dialog Callbacks
static void on_mode_selected(GtkButton *btn, gpointer user_data) {
    enum hplc_run_type mode = (enum hplc_run_type)GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "mode_enum"));
    selected_mode = mode;

    GtkWidget *dialog = GTK_WIDGET(user_data);
    init_dash();
    rebuild_dash_table_ui(&app);
    gtk_window_destroy(GTK_WINDOW(dialog));
}

static void show_mode_selection_dialog(GtkWindow *parent) {
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Select HPLC Run Mode");
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_widget_set_margin_start(vbox, 24);
    gtk_widget_set_margin_end(vbox, 24);
    gtk_widget_set_margin_top(vbox, 24);
    gtk_widget_set_margin_bottom(vbox, 24);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);

    GtkWidget *lbl = gtk_label_new("Please select the HPLC operating mode to proceed:");
    gtk_box_append(GTK_BOX(vbox), lbl);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(hbox, GTK_ALIGN_CENTER);

    GtkWidget *btn_iso = gtk_button_new_with_label("Isocratic Mode (ISO)");
    g_object_set_data(G_OBJECT(btn_iso), "mode_enum", GINT_TO_POINTER(ISO));
    g_signal_connect(btn_iso, "clicked", G_CALLBACK(on_mode_selected), dialog);
    gtk_box_append(GTK_BOX(hbox), btn_iso);

    GtkWidget *btn_gra = gtk_button_new_with_label("Gradient Mode (GRA)");
    g_object_set_data(G_OBJECT(btn_gra), "mode_enum", GINT_TO_POINTER(GRA));
    g_signal_connect(btn_gra, "clicked", G_CALLBACK(on_mode_selected), dialog);
    gtk_box_append(GTK_BOX(hbox), btn_gra);

    gtk_box_append(GTK_BOX(vbox), hbox);
    gtk_window_present(GTK_WINDOW(dialog));
}

// App Initialization & Main
static void activate(GtkApplication *app_ptr, gpointer user_data) {
    AppContext *app_ptr_ctx = (AppContext *)user_data;

    GtkWidget *window = gtk_application_window_new(app_ptr);
    app_ptr_ctx->window = window;
    gtk_window_set_title(GTK_WINDOW(window), "Engineering Workstation UI (GTK 4)");
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 400);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(window), vbox);

    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(stack), 200);

    GtkWidget *stack_switcher = gtk_stack_switcher_new();
    gtk_stack_switcher_set_stack(GTK_STACK_SWITCHER(stack_switcher), GTK_STACK(stack));
    gtk_widget_set_halign(stack_switcher, GTK_ALIGN_CENTER);

    gtk_widget_set_margin_top(stack_switcher, 6);
    gtk_widget_set_margin_bottom(stack_switcher, 6);

    gtk_box_append(GTK_BOX(vbox), stack_switcher);
    gtk_box_append(GTK_BOX(vbox), stack);

    GtkWidget *table_screen = build_table_screen(app_ptr_ctx);
    gtk_stack_add_titled(GTK_STACK(stack), table_screen, "table_view", "Run Parameters");

    GtkWidget *blueprint_screen = build_blueprint_screen(app_ptr_ctx);
    gtk_stack_add_titled(GTK_STACK(stack), blueprint_screen, "blueprint_view", "Blueprint Inspector");

    GtkWidget *db_screen = build_db_screen(app_ptr_ctx);
    gtk_stack_add_titled(GTK_STACK(stack), db_screen, "db_view", "Past Runs");

    GtkWidget *run_config_screen = build_run_config_screen(app_ptr_ctx);
    gtk_stack_add_titled(GTK_STACK(stack), run_config_screen, "run_config_view", "Config");

    gtk_window_present(GTK_WINDOW(window));

    show_mode_selection_dialog(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    init_dash();
    init_bench();

    if (!hplc_query_init())
        return 0;

    GtkApplication *gtk_app = gtk_application_new("edu.monash.hplc", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(activate), &app);

    int status = g_application_run(G_APPLICATION(gtk_app), argc, argv);
    g_object_unref(gtk_app);

    hplc_query_destroy();
    return status;
}
