/* The snakepath logo as a toy: a rattlesnake that is also a path, swaying over a floor in perspective.

   The logo is drawn in 'c' and ' ', each row two character widths tall, at any width from LOGO_MIN_COLS to
   LOGO_MAX_COLS columns. Each frame is drawn whole from a model, so every one is a complete picture at every size:
   the floor (a horizon, floor lines across it and lines fanning out from a vanishing point), the snake as a dark path
   with an edge on each side that runs unbroken from row to row, from the cap over its tail's end to its chin, its eyes
   and forked tongue, and the rattle hanging off its tail. The model is measured in the columns and rows of the logo at
   96 columns. The body slides over the floor in a wave that grows toward the tail, the head moves with the neck, the
   tongue flicks, and each rattle segment swings a little after the one below it.

   The site runs it as WebAssembly, freestanding (no libc, so no libm either):
     clang -std=c99 --target=wasm32 -mbulk-memory -O2 -nostdlib -Wl,--no-entry \
       -Wl,--export=logo_frame,--export=logo_set,--export=logo_out -o logo.wasm logo.c
   and ./nob snake makes it snake.c, the site's export: with LOGO_MAIN, a program that plays it in a terminal, with
   the snakepath it uses in place of the #include. */

#define LOGO_MIN_COLS 96 /* fewer, and the snake's outline dissolves into the floor at the site's size */
#define LOGO_MAX_COLS 192
#define LOGO_MAX_ROWS 102 /* 51 rows for each 96 columns */
#define LOGO_PI 3.14159265358979323846

typedef struct {
    double speed;   /* sways a second */
    double sway;    /* columns the tail swings each way (the tongue flicks half as far) */
    double waves;   /* waves along the body at once */
    double columns; /* the logo's width in characters */
    double rattle;  /* columns each rattle segment swings from the one below */
} LogoSettings;     /* at rest: no sway or rattle */

/* What the site's JavaScript uses: it writes logo_set, calls logo_frame and reads logo_out */
extern LogoSettings logo_set;
extern char logo_out[LOGO_MAX_ROWS * (LOGO_MAX_COLS + 1)];
int logo_frame(double t);

#ifndef LOGO_SETTINGS
#define LOGO_SETTINGS {0.3, 2, 0.7, 112, 0.6}
#endif
LogoSettings logo_set = LOGO_SETTINGS;
char logo_out[LOGO_MAX_ROWS * (LOGO_MAX_COLS + 1)]; /* the rows joined by '\n' */

/* The floor: solid to the horizon, lines across it on these rows, and between them lines fanning out from a
 * vanishing point above the top, half a step either side of it, so that none stands upright (one line through it
 * read as a column) and the floor is patterned right up to the snake, which reads by its dark against it */
#define LOGO_HORIZON 11
static const unsigned char logo_floor_lines[] = {15, 19, 24, 30, 36, 43, 50};
#define LOGO_VANISH_COL 61.5
#define LOGO_VANISH_ROW (-2.4)
#define LOGO_FAN 0.345 /* columns between two fan lines per row away from the vanishing point */

/* The snake from the tail's end to above its chin, by the dark columns of each row: the body to the neck, then the
 * head, with its eyes on LOGO_EYES_ROW */
#define LOGO_TAIL 13
#define LOGO_NECK 31
#define LOGO_CHIN 39
static const unsigned char logo_snake[LOGO_CHIN - LOGO_TAIL][2] = {
    {37, 41}, {34, 39}, {32, 37}, {31, 37}, {31, 38}, {32, 40}, {33, 43}, {35, 46}, {38, 49},
    {40, 52}, {43, 55}, {46, 58}, {48, 60}, {51, 62}, {52, 64}, {54, 65}, {55, 66}, {53, 68},
    {52, 68}, {50, 68}, {49, 67}, {48, 67}, {47, 66}, {47, 64}, {48, 61}, {51, 57},
};
#define LOGO_EYES_ROW 34
static const double logo_eyes[2] = {52, 62};

/* The tongue: from under the chin to where it forks, then to each tip, as row and column */
static const double logo_tongue[3][4] = {
    {39.5, 54.5, 43, 54.5},
    {43, 54.5, 45.5, 51},
    {43, 54.5, 45.5, 58.5},
};

/* The rattle: three segments of three rows above a row of floor over the tail's end, each 0.85 the width of the one
 * below, centred where these columns are at rest */
static const double logo_rattle_rest[3] = {40.66, 42.94, 46.84};

static char logo_rows[LOGO_MAX_ROWS][LOGO_MAX_COLS];
static int logo_cols; /* this frame's size */
static int logo_height;
static double logo_scale; /* columns of this frame per column of the model */

/* floor, round, sqrt (for 0 <= v <= 1) and sin without libm */
static double logo_floor_of(double v) {
    double t = (double)(long)v;

    return t > v ? t - 1 : t;
}

static int logo_round(double v) {
    double down = logo_floor_of(v + 0.5);

    return (int)down;
}

static double logo_sqrt(double v) {
    double x = (v + 1) / 2;
    int i;

    for (i = 0; i < 12 && v > 0; i++)
        x = (x + v / x) / 2;
    return v > 0 ? x : 0;
}

static double logo_sin(double x) {
    double x2;

    x -= 2 * LOGO_PI * logo_floor_of((x + LOGO_PI) / (2 * LOGO_PI));
    if (x > LOGO_PI / 2)
        x = LOGO_PI - x;
    if (x < -LOGO_PI / 2)
        x = -LOGO_PI - x;
    x2 = x * x;
    return x * (1 - x2 / 6 * (1 - x2 / 20 * (1 - x2 / 42 * (1 - x2 / 72 * (1 - x2 / 110)))));
}

/* Row y's columns from..to (this frame's, in either order, rounded) set to ch */
static void logo_fill(int y, double from, double to, char ch) {
    int c = logo_round(from < to ? from : to);
    int last = logo_round(from < to ? to : from);

    for (c = c < 0 ? 0 : c; y >= 0 && y < logo_height && c <= last && c < logo_cols; c++)
        logo_rows[y][c] = ch;
}

/* How thick the snake's lines are drawn past their geometry, in this frame's columns: a column of the model's, so
 * that they keep their weight against the floor at every resolution */
static double logo_thin(void) { return logo_scale; }

/* The model's row at the middle of row y, and the row or column of the model in this frame */
static double logo_row(int y) { return y / logo_scale; }

static int logo_y(double row) { return logo_round(row * logo_scale); }

static double logo_x(double col) { return col * logo_scale; }

static int logo_floor_line(int y) {
    unsigned i;

    for (i = 0; i < sizeof logo_floor_lines; i++)
        if (logo_y(logo_floor_lines[i]) == y)
            return 1;
    return y <= logo_y(LOGO_HORIZON);
}

/* The fan line k's column at the model's row */
static double logo_fan(int k, double row) { return LOGO_VANISH_COL + (k + 0.5) * LOGO_FAN * (row - LOGO_VANISH_ROW); }

static void logo_draw_floor(void) {
    double half = 0.5 / logo_scale; /* half a row of this frame, in the model's rows */
    int y;
    int k;

    for (y = 0; y < logo_height; y++) {
        logo_fill(y, 0, logo_cols - 1, logo_floor_line(y) ? 'c' : ' ');
        for (k = -30; k < 30 && !logo_floor_line(y); k++)
            logo_fill(y, logo_x(logo_fan(k, logo_row(y) - half)), logo_x(logo_fan(k, logo_row(y) + half)), 'c');
    }
}

#define LOGO_SHEAR 2.2 /* the most the sway may move one row of the body against the next, in columns */

/* The radians the sway lags by per row away from the neck */
static double logo_wave(void) { return logo_set.waves * 2 * LOGO_PI / (LOGO_NECK - LOGO_TAIL); }

/* How far the snake is from its place at the model's row (from the neck on, the head; past the tail's
 * end, the rattle) at phase p: the sway grows toward the tail, and the head takes a part of it that grows with the waves (with
 * none, the body swings around a still head) */
static double logo_offset(double row, double p) {
    double from_neck = row < LOGO_NECK ? LOGO_NECK - row : 0;
    double u = from_neck / (LOGO_NECK - LOGO_TAIL);
    double head = logo_set.waves / (1 + logo_set.waves);

    double sway = logo_set.sway;

    /* no more than LOGO_SHEAR columns between two rows, or the body comes apart into bars: the more waves, the less
     * it can swing */
    if (sway * logo_wave() > LOGO_SHEAR)
        sway = LOGO_SHEAR / logo_wave();
    u = u > 1 ? 1 : u;
    return sway * (head + (1 - head) * u * logo_sqrt(u)) * logo_sin(p - from_neck * logo_wave());
}

/* A side of the snake at the model's row, in this frame's columns: side 0 the left, 1 the right, interpolated */
static double logo_side(int side, double row, double p) {
    double r = row < LOGO_TAIL ? LOGO_TAIL : row > LOGO_CHIN - 1 ? LOGO_CHIN - 1 : row;
    int below = (int)r; /* r is never below 0, so this is its floor */
    int above = below + 1 < LOGO_CHIN ? below + 1 : below;
    double t = r - below;
    double col = (1 - t) * logo_snake[below - LOGO_TAIL][side] + t * logo_snake[above - LOGO_TAIL][side];

    return logo_x(col + logo_offset(row, p));
}

/* An edge outside a side of the snake on row y, as the floor's lines are drawn: from where the side is halfway to
 * the row above to where it is halfway to the row below, and a column more, so the edges run on unbroken; and past
 * it a dark margin as wide, so the snake's outline stands apart from the floor's lines */
static void logo_draw_edge(int side, int y, double p) {
    double half = 0.5 / logo_scale;
    double above = logo_side(side, logo_row(y) - half, p);
    double below = logo_side(side, logo_row(y) + half, p);
    double near = side ? (above < below ? above : below) : (above > below ? above : below);
    double far = side ? (above > below ? above : below) : (above < below ? above : below);
    double out = side ? 1 : -1;

    logo_fill(y, far + (1 + logo_thin()) * out, far + (1 + 2 * logo_thin()) * out, ' '); /* a dark margin outside */
    logo_fill(y, near + out, far + (1 + logo_thin()) * out, 'c');
}

/* The snake: dark between its sides, with an edge outside each on a row between floor lines, the floor closed under
 * the chin, and the eyes (the tail's end tapers into the rattle, which logo_draw_rattle draws) */
static void logo_draw_snake(double p) {
    int tail = logo_y(LOGO_TAIL);
    int chin = logo_y(LOGO_CHIN);
    int y;
    int e;

    for (y = tail; y < chin; y++) {
        if (!logo_floor_line(y)) {
            logo_draw_edge(0, y, p);
            logo_draw_edge(1, y, p);
        }
        logo_fill(y, logo_side(0, logo_row(y), p), logo_side(1, logo_row(y), p), ' ');
    }
    for (y = chin; y < logo_y(LOGO_CHIN + 1); y++)
        logo_fill(y, logo_side(0, LOGO_CHIN, p) - logo_thin(), logo_side(1, LOGO_CHIN, p) + logo_thin(), 'c');
    for (e = 0; e < 2; e++) {
        double col = logo_eyes[e] + logo_offset(LOGO_EYES_ROW, p);

        logo_fill(logo_y(LOGO_EYES_ROW), logo_x(col - 0.4), logo_x(col + 0.4), 'c');
    }
}

/* The tongue, flicking three times a sway, more toward its tips: each part as a line two columns wide at least,
 * with a dark column either side where it crosses the floor */
static void logo_draw_tongue(double p) {
    double half = 0.5 / logo_scale;
    double flick = logo_set.sway / 2 * logo_sin(3 * p);
    int y;
    unsigned i;

    for (i = 0; i < sizeof logo_tongue / sizeof logo_tongue[0]; i++) {
        const double *part = logo_tongue[i];

        for (y = logo_y(part[0]); y < logo_y(part[2]); y++) {
            double from = logo_row(y) - half < part[0] ? part[0] : logo_row(y) - half;
            double to = logo_row(y) + half > part[2] ? part[2] : logo_row(y) + half;
            double a = part[1] + (part[3] - part[1]) * (from - part[0]) / (part[2] - part[0]);
            double b = part[1] + (part[3] - part[1]) * (to - part[0]) / (part[2] - part[0]);
            double moved = logo_offset(LOGO_CHIN, p) + flick * (to - LOGO_CHIN) / (part[2] - LOGO_CHIN);
            double left = logo_x((a < b ? a : b) + moved);
            double right = logo_x((a < b ? b : a) + moved);

            right = right - left < logo_thin() ? left + logo_thin() : right;
            logo_fill(y, left - logo_thin(), right + logo_thin(), ' ');
            logo_fill(y, left, right, 'c');
        }
    }
}

/* The rattle: cut out of the horizon, each segment swinging from the one below it, the first from the tail's end;
 * the tail tapers into it, its dark narrowing row by row from the tail's width to nothing at the first segment's
 * middle, so the body runs into the rattle without a wall between them or a stray glyph at any resolution */
static void logo_draw_rattle(double p) {
    double rows[4] = {LOGO_TAIL, LOGO_TAIL - 3, LOGO_TAIL - 6, LOGO_TAIL - 9};
    double cols[4];
    double left = logo_side(0, LOGO_TAIL, p);
    double right = logo_side(1, LOGO_TAIL, p);
    double moved = logo_offset(LOGO_TAIL, p);
    double scale = 1.05;
    int k;
    int y;

    cols[0] = (left + right) / 2 / logo_scale;
    for (k = 0; k < 3; k++) {
        moved += logo_set.rattle * logo_sin(p - (LOGO_NECK - LOGO_TAIL + 3 * (k + 1)) * logo_wave());
        cols[k + 1] = logo_rattle_rest[k] + moved;
    }

    for (y = logo_y(rows[1]); y < logo_y(LOGO_TAIL); y++) {
        double t = (logo_row(y) - rows[1]) / (rows[0] - rows[1]); /* 0 at the segment's middle, 1 at the tail's end */
        double spine = logo_x(cols[1] + (cols[0] - cols[1]) * t);
        double half = (right - left) / 2 * t;

        logo_fill(y, spine - half, spine + half, ' ');
    }

    for (k = 0; k < 3; k++, scale *= 0.85) {
        double middle = rows[k + 1];

        for (y = logo_y(middle - 1); y <= logo_y(middle + 1); y++) {
            double off = logo_row(y) - middle;
            double half = (2 - 1.5 * (off < 0 ? -off : off)) * scale; /* widest in the middle */

            logo_fill(y, logo_x(cols[k + 1] - half), logo_x(cols[k + 1] + half), ' ');
        }
    }
}

/* Fills logo_out for the time t in milliseconds; returns 1 if it changed */
int logo_frame(double t) {
    static double p;      /* the phase, carried on at the current speed: a change of speed goes on from here */
    static double last_t; /* the time of the frame before */
    double cols = logo_set.columns;
    int changed = 0;
    int y;

    p += 2 * LOGO_PI * logo_set.speed * (t - last_t) / 1000;
    last_t = t;
    logo_cols = logo_round(cols < LOGO_MIN_COLS ? LOGO_MIN_COLS : cols > LOGO_MAX_COLS ? LOGO_MAX_COLS : cols);
    logo_scale = logo_cols / 96.0;
    logo_height = logo_round(51 * logo_scale);
    logo_draw_floor();
    logo_draw_snake(p);
    logo_draw_tongue(p);
    logo_draw_rattle(p);

    for (y = 0; y < logo_height; y++) {
        char *out = logo_out + y * (logo_cols + 1);
        int x;

        for (x = 0; x < logo_cols; x++) {
            changed |= out[x] != logo_rows[y][x];
            out[x] = logo_rows[y][x];
        }
        changed |= out[logo_cols] != (y + 1 < logo_height ? '\n' : '\0');
        out[logo_cols] = y + 1 < logo_height ? '\n' : '\0';
    }
    return changed;
}

#ifdef LOGO_MAIN
/* Plays the snake in a terminal at least 96 columns wide, until Ctrl-C. Given a directory, it writes one sway there
 * instead, a file per frame, through snakepath. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L /* nanosleep */
#endif
#define SP_PATH_MAX 4096
#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"
#ifndef SNAKEPATH_H /* in snake.c, the trimmed header in its place */
#error "snakepath.h is missing"
#endif
#include <stdio.h>
#ifndef _WIN32
#include <time.h>
#endif

int main(int argc, char **argv) {
    int frames = (int)(30 / logo_set.speed);
    int f;

    if (argc > 1) {
        SpPath dir = sp_path(argv[1]);
        SpMkdirOptions options = {true, true, SP_MODE_DIR};
        SpError err = sp_mkdir(&dir, SP_MODE_DIR, options);

        for (f = 0; err == SP_OK && f < frames; f++) {
            char name[32];
            SpPath file;

            snprintf(name, sizeof name, "frame_%03d.txt", f);
            file = sp_join_one(&dir, name);
            logo_frame(f * 1000.0 / 30);
            err = sp_write_file(&file, logo_out, strlen(logo_out)).error;
        }
        if (err != SP_OK)
            fprintf(stderr, "%s: %s\n", argv[1], sp_error_str(err));
        return err != SP_OK;
    }
    printf("\033[2J");
    for (f = 0;; f++) {
        logo_frame(f * 1000.0 / 30);
        printf("\033[H\033[32m%s\033[0m\n", logo_out);
        fflush(stdout);
#ifdef _WIN32
        Sleep(1000 / 30);
#else
        {
            struct timespec frame = {0, 1000000000L / 30};

            nanosleep(&frame, NULL);
        }
#endif
    }
}
#endif
