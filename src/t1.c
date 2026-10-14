#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sqlite3.h>

#include "lib/file.h"
#include "lib/csv.h"
#include "lib/metadata.h"
#include "logic.h"
#include "run-db.h"
#include "query.h"

#define DASH_TABLE_ROWS 12
#define DASH_TABLE_COLUMNS 2

char *dash_table[DASH_TABLE_ROWS][DASH_TABLE_COLUMNS];

void init_dash()
{
    char *name[DASH_TABLE_ROWS][DASH_TABLE_COLUMNS] = {
        {"Solvent", "ACN"},
        {"pH", "3"},
        {"Run Type", "ISO"},
        {"%B", "100"},
        {"Flow Rate (mL/s)", "1.5"},
        {"Injection volume (microliters)", "20"},
        {"Injection type", "BLK"},
        {"Sample Concentration", "0.0"},
        {"Column phase and brand", "C18 Pinnacle"},
        {"Column measurements", "150x4.6x5"},
        {"Temperature (C)", "amb"},
        {"Detector Wavelength (nm)", "254"}
    };

    memcpy(dash_table, name, sizeof(name));
}

void update_metadata(struct hplc_run_metadata *m);

int add_run(char *input)
{
    FILE *f;
    struct hplc_data hdata;
    struct benchmarks bench = {7, 1.4, 15, 4000};
    enum selectivity_action action;
    void *adata = NULL;
    char*new_pk;

    f = file_open(input, &hdata.data);
    if (!f) {
        fprintf(stderr, "%s\n", strerror(errno));
        return 0;
    }
    if(hplc_column_phase(&hdata.data)<0){
    	fprintf(stderr,"Column phase %s is invalid\n",hdata.data.clmn_phase);
	hplc_metadata_destroy(&hdata.data);
	fclose(f);
	return 0;
    }
    if (!csv_parse_file(f, &hdata)) {
        fprintf(stderr, "csv parsing failed\n");
        hplc_metadata_destroy(&hdata.data);
        fclose(f);
        return 0;
    }
    if(!hplc_run_check(input))
	    hplc_run_add(&hdata.data,input);
    action = logic(&hdata, &bench, &adata);

    switch (action) {
    case CITEMP:
    case CDTEMP:
        hdata.data.temperature = *(double *)adata;
        break;
    case CSOL:
        hdata.data.slvnt = (char *)adata;
        break;
    case CIB:
    case CDB:
        hdata.data.slvnt_percent = *(double *)adata;
        break;
    case CDFLOW:
    case CIFLOW:
        hdata.data.flow_rate = *(double *)adata;
        break;
    default:
        break;
    }
    if(!hplc_run_meta_check(&hdata.data,&new_pk))
	update_metadata(&hdata.data);
    else{
    	add_run(new_pk);
	free(new_pk);
    }
    free(adata);
    hplc_destroy(&hdata);
    fclose(f);

    return 1;
}

// ============================================================================
// COMPILE-TIME CONSTANTS & DATA DEFINITIONS
// ============================================================================
static const char *const TABLE_HEADERS[DASH_TABLE_COLUMNS] = {
    "Variable", "Parameter"
};

#define NUM_BLUEPRINT_PARTS 5

typedef struct {
    const char *id;
    const char *name;
    const char *description;
    const char *image_path;
    GdkRectangle bounds;
} BlueprintPart;

static const BlueprintPart MACHINE_PARTS[NUM_BLUEPRINT_PARTS] = {
    {
        "P-001",
        "Solvent Bottles",
        "Stores solvents",
        "test.jpg",
        {30, 40, 130, 90}
    },
    {
        "P-002",
        "Pumps",
        "Creates the pressure to pump the solvent through the column at a consistant, adjustable rate",
        "radiator.png",
        {190, 40, 110, 140}
    },
    {
        "P-003",
        "Column",
        "The place where chromatography occurs",
        "mcu.png",
        {40, 160, 120, 80}
    },
    {
        "P-004",
        "Column Oven",
        "Heats the column so the column can function at a consistant temperature",
        "mcu.png",
        {60, 160, 120, 80}
    },
    {
        "P-005",
        "Detector",
        "Detects the presence of compounds in the eluted solvent. Typically via UV-Vis spectroscopy. Some systems are fitted with mass spectroscopy or other means of measurement",
        "mcu.png",
        {80, 160, 120, 80}
    }
};

#define DB_TABLE_COLS 13

static const char *const DB_HEADERS[DB_TABLE_COLS] = {
    "Primary Key","Solvent","pH","Run Type","%B","Flow Rate","Injection Volume","Injection Type","Sample Concentration","Column Type","Dimensions","Temperature","Wavelength"
};

// ============================================================================
// UI STATE & CONTEXT STRUCTURE
// ============================================================================

typedef struct {
    GtkWidget *part_title_label;
    GtkWidget *part_desc_label;
    GtkWidget *part_image;
    GtkWidget *entry;
    GtkWidget *drawing_area;
    GtkWidget *db_grid_container; // Container for dynamic DB rows
    const BlueprintPart *active_part;
    GtkWidget *table_labels[DASH_TABLE_ROWS][DASH_TABLE_COLUMNS];
} AppContext;

AppContext app = {0};

// ============================================================================
// C BACKEND FUNCTIONS
// ============================================================================

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

// ============================================================================
// CALLBACKS & EVENT CONTROLLERS
// ============================================================================

static void on_command_submitted(GtkButton *button, gpointer user_data) {
    AppContext *app = (AppContext *)user_data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(app->entry));
    
    c_backend_process_command(text);
    gtk_editable_set_text(GTK_EDITABLE(app->entry), "");
}

static void on_blueprint_draw(GtkDrawingArea *drawing_area, cairo_t *cr, int width, int height, gpointer user_data) {
    AppContext *app = (AppContext *)user_data;

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
        bool is_hovered = (app->active_part == p);

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
    AppContext *app = (AppContext *)user_data;

    const BlueprintPart *hovered = NULL;
    for (int i = 0; i < NUM_BLUEPRINT_PARTS; i++) {
        const BlueprintPart *p = &MACHINE_PARTS[i];
        if (x >= p->bounds.x && x <= (p->bounds.x + p->bounds.width) &&
            y >= p->bounds.y && y <= (p->bounds.y + p->bounds.height)) {
            hovered = p;
            break;
        }
    }

    if (hovered != app->active_part) {
        app->active_part = hovered;
        c_backend_on_part_hovered(hovered);

        if (hovered) {
            gtk_label_set_text(GTK_LABEL(app->part_title_label), hovered->name);
            gtk_label_set_text(GTK_LABEL(app->part_desc_label), hovered->description);

            if (g_file_test(hovered->image_path, G_FILE_TEST_EXISTS)) {
                gtk_image_set_from_file(GTK_IMAGE(app->part_image), hovered->image_path);
            } else {
	    	fprintf(stderr,"image not found:%s\n",hovered->image_path);
                gtk_image_set_from_icon_name(GTK_IMAGE(app->part_image), "system-run");
            }
        } else {
            gtk_label_set_text(GTK_LABEL(app->part_title_label), "Machine Blueprint");
            gtk_label_set_text(GTK_LABEL(app->part_desc_label), "Hover mouse cursor over a component region to inspect details and images.");
            gtk_image_set_from_icon_name(GTK_IMAGE(app->part_image), "dialog-information");
        }

        gtk_widget_queue_draw(app->drawing_area);
    }
}

/**
 * Re-queries DB backend via hplc_run_table_prepare/hplc_run_table_row
 * and updates Screen 3 grid rows dynamically.
 */
static void populate_db_grid(AppContext *app) {
    // Clear old grid if exists
    GtkWidget *old_grid = gtk_widget_get_first_child(app->db_grid_container);
    if (old_grid) {
        gtk_box_remove(GTK_BOX(app->db_grid_container), old_grid);
    }

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 24);
    gtk_widget_set_vexpand(grid, TRUE);
    gtk_widget_set_hexpand(grid, TRUE);

    // 1. Attach Headers (Compile-time column count)
    for (int col = 0; col < DB_TABLE_COLS; col++) {
        char header_markup[128];
        snprintf(header_markup, sizeof(header_markup), "<b><u>%s</u></b>", DB_HEADERS[col]);
        GtkWidget *lbl = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(lbl), header_markup);
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), lbl, col, 0, 1, 1);
    }

    // 2. Prepare SQLite statement and fetch dynamic rows as needed to display
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

    gtk_box_append(GTK_BOX(app->db_grid_container), grid);
    gtk_widget_queue_resize(app->db_grid_container);
}

static void on_refresh_db_clicked(GtkButton *button, gpointer user_data) {
    AppContext *app = (AppContext *)user_data;
    populate_db_grid(app);
}

static void db_run_delete(GtkButton*button,gpointer user_data)
{
	AppContext*app=(AppContext*)user_data;
	hplc_run_delete();
	on_refresh_db_clicked(button,user_data);
}

// ============================================================================
// UI BUILDING FUNCTIONS
// ============================================================================

static GtkWidget *build_table_screen(AppContext *app) {
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);

    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<b>MonasHPLC Run Parameters</b>");
    gtk_widget_set_halign(title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), title);

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

    for (int row = 0; row < DASH_TABLE_ROWS; row++) {
        for (int col = 0; col < DASH_TABLE_COLUMNS; col++) {
            GtkWidget *lbl = gtk_label_new(dash_table[row][col]);
            gtk_widget_set_halign(lbl, GTK_ALIGN_START);
            gtk_grid_attach(GTK_GRID(grid), lbl, col, row + 1, 1, 1);

            app->table_labels[row][col] = lbl;
        }
    }

    gtk_box_append(GTK_BOX(vbox), grid);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(sep, 8);
    gtk_widget_set_margin_bottom(sep, 8);
    gtk_box_append(GTK_BOX(vbox), sep);

    GtkWidget *entry_label = gtk_label_new("Execute Backend Command:");
    gtk_widget_set_halign(entry_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), entry_label);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    
    app->entry = gtk_entry_new();
    char*tmp,*dir,*help="Enter Primary Key e.g. GR19-16";
    int buf_size;
    dir=current_dir();
    buf_size=snprintf(NULL,0,"%s, Ensure csv file is in current dir:%s",help,dir);
    tmp=malloc(sizeof(char)*++buf_size);
    snprintf(tmp,buf_size,"%s, Ensure csv file is in current dir:%s",help,dir);
    tmp[buf_size-1]='\0'; 
    gtk_entry_set_placeholder_text(GTK_ENTRY(app->entry),tmp);
    free(dir);
    free(tmp);

    gtk_widget_set_hexpand(app->entry, TRUE);
    g_signal_connect(app->entry, "activate", G_CALLBACK(on_command_submitted), app);
    gtk_box_append(GTK_BOX(hbox), app->entry);

    GtkWidget *btn = gtk_button_new_with_label("Send");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_command_submitted), app);
    gtk_box_append(GTK_BOX(hbox), btn);

    gtk_box_append(GTK_BOX(vbox), hbox);

    return vbox;
}

void update_screen1_table_data(AppContext *app, const char *new_matrix[DASH_TABLE_ROWS][DASH_TABLE_COLUMNS]) {
    for (int row = 0; row < DASH_TABLE_ROWS; row++) {
        for (int col = 0; col < DASH_TABLE_COLUMNS; col++) {
            if (app->table_labels[row][col] != NULL) {
                gtk_label_set_text(
                    GTK_LABEL(app->table_labels[row][col]),
                    new_matrix[row][col]
                );
            }
        }
    }
}

static GtkWidget *build_blueprint_screen(AppContext *app) {
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    gtk_widget_set_margin_start(hbox, 16);
    gtk_widget_set_margin_end(hbox, 16);
    gtk_widget_set_margin_top(hbox, 16);
    gtk_widget_set_margin_bottom(hbox, 16);

    app->drawing_area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(app->drawing_area), 350);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(app->drawing_area), 260);
    gtk_widget_set_hexpand(app->drawing_area, TRUE);
    gtk_widget_set_vexpand(app->drawing_area, TRUE);

    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(app->drawing_area), on_blueprint_draw, app, NULL);

    GtkEventController *motion_controller = gtk_event_controller_motion_new();
    g_signal_connect(motion_controller, "motion", G_CALLBACK(on_blueprint_motion), app);
    gtk_widget_add_controller(app->drawing_area, motion_controller);

    gtk_box_append(GTK_BOX(hbox), app->drawing_area);

    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_size_request(panel, 220, -1);

    app->part_title_label = gtk_label_new("Machine Blueprint");
    PangoAttrList *attr_list = pango_attr_list_new();
    pango_attr_list_insert(attr_list, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    pango_attr_list_insert(attr_list, pango_attr_scale_new(1.2));
    gtk_label_set_attributes(GTK_LABEL(app->part_title_label), attr_list);
    pango_attr_list_unref(attr_list);
    gtk_widget_set_halign(app->part_title_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(panel), app->part_title_label);

    app->part_image = gtk_image_new_from_icon_name("dialog-information");
    gtk_image_set_pixel_size(GTK_IMAGE(app->part_image), 96);
    gtk_box_append(GTK_BOX(panel), app->part_image);

    app->part_desc_label = gtk_label_new("Hover mouse cursor over a component region to inspect details and images.");
    gtk_label_set_wrap(GTK_LABEL(app->part_desc_label), TRUE);
    gtk_widget_set_halign(app->part_desc_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(panel), app->part_desc_label);

    gtk_box_append(GTK_BOX(hbox), panel);

    return hbox;
}

static GtkWidget *build_db_screen(AppContext *app) {
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
    g_signal_connect(fresh_btn, "clicked", G_CALLBACK(db_run_delete), app);
    gtk_box_append(GTK_BOX(top_bar), fresh_btn);
    
    GtkWidget *refresh_btn = gtk_button_new_with_label("Fetch / Query DB");
    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(on_refresh_db_clicked), app);
    gtk_box_append(GTK_BOX(top_bar), refresh_btn);

    gtk_box_append(GTK_BOX(vbox), top_bar);

    // Scrolled window container to handle scroll motions when rows exceed screen bounds
    GtkWidget *scrolled_win = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled_win, TRUE);
    gtk_widget_set_hexpand(scrolled_win, TRUE);

    app->db_grid_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_win), app->db_grid_container);

    gtk_box_append(GTK_BOX(vbox), scrolled_win);

    populate_db_grid(app);

    return vbox;
}

static void activate(GtkApplication *app_ptr, gpointer user_data) {
    AppContext *app = (AppContext *)user_data;

    GtkWidget *window = gtk_application_window_new(app_ptr);
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

    GtkWidget *table_screen = build_table_screen(app);
    gtk_stack_add_titled(GTK_STACK(stack), table_screen, "table_view", "Run Parameters");

    GtkWidget *blueprint_screen = build_blueprint_screen(app);
    gtk_stack_add_titled(GTK_STACK(stack), blueprint_screen, "blueprint_view", "Blueprint Inspector");

    GtkWidget *db_screen = build_db_screen(app);
    gtk_stack_add_titled(GTK_STACK(stack), db_screen, "db_view", "Past Runs");

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    init_dash();
    if(!hplc_query_init())
    	return 0;

    GtkApplication *gtk_app = gtk_application_new("com.engineering.station", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(gtk_app, "activate", G_CALLBACK(activate), &app);

    int status = g_application_run(G_APPLICATION(gtk_app), argc, argv);
    g_object_unref(gtk_app);

    hplc_query_destroy();
    return status;
}

void update_metadata(struct hplc_run_metadata *m)
{
    char tmp[9][50];

    dash_table[0][1] = m->slvnt;
    snprintf(tmp[0], sizeof(tmp[0]), "%f", m->pH);
    dash_table[1][1] = tmp[0];
    dash_table[2][1]=(char*)hplc_run_type_str(m->type);
    snprintf(tmp[1], sizeof(tmp[1]), "%f", m->slvnt_percent);
    dash_table[3][1] = tmp[1];
    snprintf(tmp[2], sizeof(tmp[2]), "%f", m->flow_rate);
    dash_table[4][1] = tmp[2];
    dash_table[5][1] = m->njctn_volume;
    dash_table[6][1] = m->njctn_type;
    snprintf(tmp[3], sizeof(tmp[3]), "%f", m->sample_conc);
    dash_table[7][1] = tmp[3];
    snprintf(tmp[4], sizeof(tmp[4]), "%s %s", m->clmn_phase, hplc_clmn_brand_str(m->brand));
    dash_table[8][1] = tmp[4];

    snprintf(tmp[5], sizeof(tmp[5]), "%.3fx%.2fx%.2f", m->clmn_length, m->clmn_id, m->clmn_bead_size);
    dash_table[9][1] = tmp[5];
    snprintf(tmp[6], sizeof(tmp[6]), "%.2f", m->temperature);
    dash_table[10][1] = tmp[6];
    snprintf(tmp[7], sizeof(tmp[7]), "%d", m->wavelength);
    dash_table[11][1] = tmp[7];

    update_screen1_table_data(&app, (const char * (*)[2])dash_table);
}
