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

#define LOGO_MIN_COLS 80 /* fewer, and the head loses its eyes */
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
#define LOGO_SETTINGS {0.3, 2, 1, 96, 0.6}
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
    {39.5, 54.5, 42.125, 54.5},
    {42.125, 54.5, 44, 51.875},
    {42.125, 54.5, 44, 57.125},
};

/* The rattle: three segments of three rows above a row of floor over the tail's end, each 0.85 the width of the one
 * below, centred where these columns are at rest */
static const double logo_rattle_rest[3] = {40.66, 42.94, 46.84};

static char logo_rows[LOGO_MAX_ROWS][LOGO_MAX_COLS];
static int logo_cols; /* this frame's size */
static int logo_height;
static double logo_scale; /* columns of this frame per column of the model */

/* floor, round, sqrt (exact to 400, more than the snake needs, and too large past it) and sin without libm */
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

/* How thick the snake's lines are drawn past their geometry, in this frame's columns: a column of the model's at the
 * model's own width, and more at higher resolutions, since the floor's lines cross a row in as many columns at every
 * resolution and an outline no heavier than they are dissolves into them */
static double logo_thin(void) { return logo_scale * logo_scale; }

/* The model's row at the middle of row y, and the row or column of the model in this frame */
static double logo_row(int y) { return y / logo_scale; }

static int logo_y(double row) { return logo_round(row * logo_scale); }

#define LOGO_CENTER 57.0 /* the model's column between the eyes, drawn at the view's middle (48 of 96) */

static double logo_x(double col) { return (col - LOGO_CENTER + 48) * logo_scale; }

#define LOGO_LENGTH 1.18 /* how much longer the snake is drawn than the logo's, from the tail's end down */
#define LOGO_SLIM 0.7    /* and how much slimmer its body: drawn as long and wide as the logo's, it looked blobby */

/* The row the snake's model row r is drawn on: stretched down from the tail's end */
static double logo_long(double r) { return r < LOGO_TAIL ? r : LOGO_TAIL + (r - LOGO_TAIL) * LOGO_LENGTH; }

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

#define LOGO_NOD 1.15 /* columns the head nods each way at the default sway: 3 px at the site's 250 px */
#ifndef LOGO_TIP
#define LOGO_TIP 0.65 /* how much less the bends move toward the tail's end, which is far off and thin */
#endif
#ifndef LOGO_START
#define LOGO_START (LOGO_PI / 2) /* the phase it starts at: a quarter of a sway on, halfway to its mirror image */
#endif

/* The radians the rattle's swing lags by per row away from the neck */
static double logo_wave(void) { return logo_set.waves * 2 * LOGO_PI / (LOGO_NECK - LOGO_TAIL); }

/* The model's column of side 0 (the left) or 1 (the right) of the snake as drawn, at row r between two rows */
static double logo_drawn(int side, double r) {
    int below = (int)r; /* r is never below 0, so this is its floor */
    int above = below + 1 < LOGO_CHIN ? below + 1 : below;
    double t = r - below;

    return (1 - t) * logo_snake[below - LOGO_TAIL][side] + t * logo_snake[above - LOGO_TAIL][side];
}

#define LOGO_BEND 6.0      /* columns a knee reaches from the line between the tail's end and the neck, by the neck */
#define LOGO_BEND_TAIL 4.0 /* and by the tail, which is further off */

/* How many half waves the body bends in: 3 knees at the default, from 1 with no waves to 7 with the most, the
 * first to the right below the neck */
static double logo_knees(void) { return 1 + 2 * logo_set.waves; }

/* How far the head nods from its place in the view's middle at phase p */
static double logo_nod(double p) {
    double reach = logo_set.sway / 2;

    return LOGO_NOD * (reach < 1 ? reach : 1) * logo_sin(p);
}

/* The middle of the snake at the model's row (held between the tail's end and the chin) at phase p, as a snake
 * crawls: its knees run along the body from the neck toward the tail, so that half a sway on it is its mirror image.
 * The sway knob takes it from still (0) through all the way into the mirror image (the default) to a wider swing, the
 * waves knob from one bend to seven, each the less deep the more there are so that the body never lies flat. The turn
 * eases in below the neck, so the head stays in the middle, and the knees move less toward the tail's end, which is
 * far off: moved as far as near ones, it lay flat */
static double logo_middle(double row, double p) {
    double r = row < LOGO_TAIL ? LOGO_TAIL : row > LOGO_CHIN - 1 ? LOGO_CHIN - 1 : row;
    double u = (LOGO_NECK - r) / (LOGO_NECK - LOGO_TAIL);
    double reach = logo_set.sway / 2;
    double mix = reach < 1 ? reach : 1;
    double grow = reach > 1 ? 1 + (reach - 1) * 0.15 : 1;
    double knees = logo_knees();
    double rise = u / 0.25; /* the knees fade out toward the neck, so the body meets the head where the head is */
    double deep = (LOGO_BEND + (LOGO_BEND_TAIL - LOGO_BEND) * u) * (knees > 3 ? 3 / knees : 1) *
                  (rise > 1 ? 1 : rise * rise * (3 - 2 * rise));
    double near = u / 0.3;
    double ease = near > 1 ? 1 : near * near * (3 - 2 * near);
    double far = (u - 0.75) / 0.25;
    double tip = 1 - LOGO_TIP * (far < 0 ? 0 : far * far * (3 - 2 * far));
    double tail = (logo_drawn(0, LOGO_TAIL) + logo_drawn(1, LOGO_TAIL)) / 2;
    double neck = (logo_drawn(0, LOGO_NECK) + logo_drawn(1, LOGO_NECK)) / 2;
    double line = neck + (tail - neck) * u; /* the straight line the knees are measured from */
    double at = knees * LOGO_PI * u;
    double bend = deep * logo_sin(at); /* at rest */
    double turned = deep * (logo_sin(at + p) * tip + logo_sin(at) * logo_sin(p + LOGO_PI / 2) * (1 - tip)); /* moving */

    if (r >= LOGO_NECK)
        return (logo_drawn(0, r) + logo_drawn(1, r)) / 2 + logo_nod(p);
    return line + grow * (bend + mix * ease * (turned - bend)) + (1 - u) * logo_nod(p);
}

/* The middles of the rattle's segments at phase p, in the model's columns, after the tail's end (cols[0]): each
 * hangs from the one below it as the tail moves it and swings a little after it */
static void logo_rattle_cols(double p, double cols[4]) {
    double moved = logo_middle(LOGO_TAIL, p) - (logo_drawn(0, LOGO_TAIL) + logo_drawn(1, LOGO_TAIL)) / 2;
    int k;

    cols[0] = logo_middle(LOGO_TAIL, p);
    for (k = 0; k < 3; k++) {
        moved += logo_set.rattle * logo_sin(p - (LOGO_NECK - LOGO_TAIL + 3 * (k + 1)) * logo_wave());
        cols[k + 1] = logo_rattle_rest[k] + moved;
    }
}

#define LOGO_EDGE 1.3   /* the outline's thickness, in the model's columns at its own width */
#define LOGO_MARGIN 1.0 /* the dark margin's past it */

/* Half the snake's thickness across its path at the model's row r: LOGO_SLIM of half the logo's width there, so the
 * body thins toward the tail as the logo's does */
static double logo_half(double r) {
    if (r > LOGO_NECK - 3) /* the neck widens to the head's width as the logo's does */
        return (LOGO_SLIM + (1 - LOGO_SLIM) * (r - (LOGO_NECK - 3)) / 3) * (logo_drawn(1, r) - logo_drawn(0, r)) / 2;
    return LOGO_SLIM * (logo_drawn(1, r) - logo_drawn(0, r)) / 2;
}

#define LOGO_STEP 0.25                                    /* rows between two points of the snake's line */
#define LOGO_POINTS (4 * (LOGO_NECK - LOGO_TAIL + 3) + 1) /* from the first rattle segment's middle to the neck */

/* The snake, drawn by its distance from its middle line, from the first rattle segment's middle (the tail tapering
 * to it) to the chin: dark within its thickness, then an outline of even thickness all round, then a dark margin that
 * sets it apart from the floor's lines (not on the solid horizon, which the tail cuts into without one). Drawn this
 * way the outline is one unbroken band at every slant, phase and resolution: drawn row by row, it came apart into
 * staggered strokes where the body slanted or kinked */
static void logo_draw_snake(double p) {
    double col[LOGO_POINTS];
    double row[LOGO_POINTS];
    double half[LOGO_POINTS];
    double rattle[4];
    /* the outline at least 2.2 of this frame's columns across, which is more than one row up and down (a row is two
     * columns tall): thinner, it fell apart into single glyphs where the body slants */
    double edge =
        LOGO_EDGE / logo_sqrt(logo_scale) > 2.2 / logo_scale ? LOGO_EDGE / logo_sqrt(logo_scale) : 2.2 / logo_scale;
    /* and as wide across a row wherever the body runs (3 of this frame's columns at least): measured across the body
     * alone, it was two glyphs where the body runs straight down and four where it slants */
    double across = 3.0 / logo_scale > edge ? 3.0 / logo_scale : edge;
    double margin =
        LOGO_MARGIN / logo_sqrt(logo_scale) > 1.2 / logo_scale ? LOGO_MARGIN / logo_sqrt(logo_scale) : 1.2 / logo_scale;
    double down = logo_long(LOGO_NECK) - LOGO_NECK; /* how far the head is drawn below the logo's */
    int i;
    int y;
    int e;

    logo_rattle_cols(p, rattle);
    for (i = 0; i < LOGO_POINTS; i++) {
        double t = (i * LOGO_STEP) / 3; /* 0 at the first segment's middle, 1 at the tail's end */

        double r = LOGO_TAIL - 3 + i * LOGO_STEP; /* the model's row */

        row[i] = logo_long(r);
        col[i] = t < 1 ? rattle[1] + (rattle[0] - rattle[1]) * t : logo_middle(r, p);
        half[i] = (t < 1 ? t : 1) * logo_half(r < LOGO_TAIL ? LOGO_TAIL : r);
    }

    for (y = logo_y(LOGO_TAIL - 3) - logo_y(2); y <= logo_y(logo_long(LOGO_NECK + 2)); y++) {
        double m = logo_row(y);
        double lo = 1e9;
        double hi = -1e9;
        int x;

        for (i = 0; i < LOGO_POINTS; i++) {
            double reach = half[i] + across + margin;

            if ((row[i] - m) * 2 < reach && (m - row[i]) * 2 < reach) {
                lo = col[i] - reach < lo ? col[i] - reach : lo;
                hi = col[i] + reach > hi ? col[i] + reach : hi;
            }
        }
        for (x = logo_round(logo_x(lo)); lo < hi && x <= logo_round(logo_x(hi)); x++) {
            double c = x / logo_scale + LOGO_CENTER - 48; /* the model's column of this cell */
            double d = 1e9;
            double band = edge; /* the outline's thickness across the nearest stretch of the body */

            for (i = 0; i + 1 < LOGO_POINTS; i++) {
                double ax = col[i];
                double ay = row[i] * 2;
                double bx = col[i + 1] - ax;
                double by = (row[i + 1] - row[i]) * 2;
                double t = ((c - ax) * bx + (m * 2 - ay) * by) / (bx * bx + by * by);
                double dx;
                double dy;
                double here;

                t = t < 0 ? 0 : t > 1 ? 1 : t;
                dx = c - ax - t * bx;
                dy = m * 2 - ay - t * by;
                here = logo_sqrt(dx * dx + dy * dy) - (half[i] + (half[i + 1] - half[i]) * t);
                if (here < d) {
                    double upright = (by < 0 ? -by : by) / logo_sqrt(bx * bx + by * by); /* 1 where it runs down */

                    d = here;
                    band = across * upright > edge ? across * upright : edge;
                }
            }
            if (x < 0 || x >= logo_cols || y < 0 || y >= logo_height)
                continue;
            if (d < 0)
                logo_rows[y][x] = ' ';
            else if (d < band)
                logo_rows[y][x] = 'c';
            else if (d < band + margin && y > logo_y(LOGO_HORIZON))
                logo_rows[y][x] = ' ';
        }
    }

    for (y = logo_y(LOGO_NECK - 1 + down); y < logo_y(LOGO_CHIN + down); y++) { /* the head as the logo draws it */
        double r = logo_row(y) - down;
        double up = r - 0.5 / logo_scale < LOGO_NECK - 1 ? LOGO_NECK - 1 : r - 0.5 / logo_scale;
        double on = r + 0.5 / logo_scale > LOGO_CHIN - 1 ? LOGO_CHIN - 1 : r + 0.5 / logo_scale;
        double left = logo_x(logo_drawn(0, r) + logo_nod(p));
        double right = logo_x(logo_drawn(1, r) + logo_nod(p));
        /* each side's edge reaches from where the side is half a row up to half a row down, so it runs on unbroken
         * where the jaw slants */
        double outer0 =
            logo_x((logo_drawn(0, up) < logo_drawn(0, on) ? logo_drawn(0, up) : logo_drawn(0, on)) + logo_nod(p));
        double outer1 =
            logo_x((logo_drawn(1, up) > logo_drawn(1, on) ? logo_drawn(1, up) : logo_drawn(1, on)) + logo_nod(p));
        double inner0 =
            logo_x((logo_drawn(0, up) > logo_drawn(0, on) ? logo_drawn(0, up) : logo_drawn(0, on)) + logo_nod(p));
        double inner1 =
            logo_x((logo_drawn(1, up) < logo_drawn(1, on) ? logo_drawn(1, up) : logo_drawn(1, on)) + logo_nod(p));

        logo_fill(y, outer0 - (edge + margin) * logo_scale - 1, outer1 + (edge + margin) * logo_scale + 1, ' ');
        logo_fill(y, outer0 - edge * logo_scale - 1, inner0 - 1, 'c'); /* as heavy as the body's, which slants */
        logo_fill(y, inner1 + 1, outer1 + edge * logo_scale + 1, 'c');
        logo_fill(y, left, right, ' ');
    }
    for (y = logo_y(LOGO_CHIN + down); y < logo_y(LOGO_CHIN + 1 + down); y++) /* the floor closed under the chin */
        logo_fill(y, logo_x(logo_drawn(0, LOGO_CHIN - 1) + logo_nod(p)) - edge * logo_scale,
                  logo_x(logo_drawn(1, LOGO_CHIN - 1) + logo_nod(p)) + edge * logo_scale, 'c');
    for (e = 0; e < 2; e++) {
        double eye = logo_eyes[e] + logo_nod(p);

        logo_fill(logo_y(LOGO_EYES_ROW + down), logo_x(eye - 0.4), logo_x(eye + 0.4), 'c');
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
        double down = logo_long(LOGO_CHIN) - LOGO_CHIN; /* the tongue hangs from the chin where it is drawn */
        double part[4];

        part[0] = logo_tongue[i][0] + down;
        part[1] = logo_tongue[i][1];
        part[2] = logo_tongue[i][2] + down;
        part[3] = logo_tongue[i][3];

        for (y = logo_y(part[0]); y < logo_y(part[2]); y++) {
            double from = logo_row(y) - half < part[0] ? part[0] : logo_row(y) - half;
            double to = logo_row(y) + half > part[2] ? part[2] : logo_row(y) + half;
            double a = part[1] + (part[3] - part[1]) * (from - part[0]) / (part[2] - part[0]);
            double b = part[1] + (part[3] - part[1]) * (to - part[0]) / (part[2] - part[0]);
            double moved = logo_nod(p) + flick * (to - part[0]) / (part[2] - part[0]);
            double left = logo_x((a < b ? a : b) + moved);
            double right = logo_x((a < b ? b : a) + moved);

            right = right - left < logo_thin() ? left + logo_thin() : right;
            logo_fill(y, left - logo_thin(), right + logo_thin(), ' ');
            logo_fill(y, left, right, 'c');
        }
    }
}

/* The rattle: cut out of the horizon, each segment swinging from the one below it, the first from the tail's end,
 * which tapers into it (logo_draw_snake draws the taper) */
static void logo_draw_rattle(double p) {
    double rows[4] = {LOGO_TAIL, LOGO_TAIL - 3, LOGO_TAIL - 6, LOGO_TAIL - 9};
    double cols[4];
    double scale = 1.05;
    int k;
    int y;

    logo_rattle_cols(p, cols);
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
static int logo_lit(int y, int x) {
    return y >= 0 && y < logo_height && x >= 0 && x < logo_cols && logo_rows[y][x] == 'c';
}

/* Row y's column x as shown: a 'c' with no other 'c' around it (the eyes aside) and a ' ' with a 'c' on all four
 * sides are where two drawn shapes meet a column apart, and at the site's size they read as broken pixels, so the
 * one is cleared and the other filled */
static char logo_shown(int y, int x) {
    int around = 0;
    int dy;
    int dx;

    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            around += (dy || dx) && logo_lit(y + dy, x + dx);
    if (logo_rows[y][x] == 'c' && around == 0 && y != logo_y(LOGO_EYES_ROW + logo_long(LOGO_NECK) - LOGO_NECK))
        return ' ';
    if (logo_rows[y][x] == ' ' && logo_lit(y, x - 1) && logo_lit(y, x + 1) && logo_lit(y - 1, x) && logo_lit(y + 1, x))
        return 'c';
    return logo_rows[y][x];
}

int logo_frame(double t) {
    static double p = LOGO_START; /* the phase, carried on at the current speed: a change of speed goes on from here */
    static double last_t;         /* the time of the frame before */
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
            char ch = logo_shown(y, x);

            changed |= out[x] != ch;
            out[x] = ch;
        }
        changed |= out[logo_cols] != (y + 1 < logo_height ? '\n' : '\0');
        out[logo_cols] = y + 1 < logo_height ? '\n' : '\0';
    }
    return changed;
}

#ifdef LOGO_MAIN
/* Plays the snake in a terminal, at most as wide and tall as it, until Ctrl-C. Given a directory, it writes one sway there
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
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>
#endif

/* The most columns a frame may have to fit the terminal, with a row to spare so it never scrolls, and in *rows_out
 * the rows it has room for; 0 if unknown. Drawn wider or taller, its lines wrap and each frame scrolls on below the
 * last instead of over it */
static int logo_fit(int *rows_out) {
    int cols = 0;
    int rows = 0;
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO info;

    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info)) {
        cols = info.srWindow.Right - info.srWindow.Left + 1;
        rows = info.srWindow.Bottom - info.srWindow.Top + 1;
    }
#else
    struct winsize size;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0) {
        cols = size.ws_col;
        rows = size.ws_row;
    }
#endif
    *rows_out = rows - 1;
    if (cols <= 0 || rows <= 1)
        return 0;
    return cols < (rows - 1) * 96 / 51 ? cols : (rows - 1) * 96 / 51;
}

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
        int room = 0;
        int fit = logo_fit(&room); /* every frame, so a resized terminal is followed */
        char *end = logo_out;

        if (fit > 0 && logo_set.columns > fit)
            logo_set.columns = fit;
        logo_frame(f * 1000.0 / 30);
        while (room > 0 && (end = strchr(end, '\n')) != NULL && --room > 0)
            end++;
        if (end != NULL && room == 0 && fit > 0) /* narrower than the smallest frame is tall: its top only */
            *end = '\0';
        printf("\033[H\033[32m%s\033[0m", logo_out);
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
