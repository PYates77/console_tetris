#include <curses.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

// TODO
// color schemes for levels
// score
// friendly game over
// border for game board and next box
// delay and/or animation for clearing rows
// don't use so many global variables
// better push down handling
// some abiguity using "rows" some places and "lines" other places
// decouple lines cleared and level so you can start on whatever level you want
// ugly global variables (double-buffer screen?)
// fix awkward attron/attroff clearing of minos
// clean up screen refresh triggers
// speed up screen refresh by not redrawing unchanged pixels

#define MINOS_IN_TETROMINO 4

#define BOARD_HEIGHT 20
#define BOARD_WIDTH  10

#define NEXT_BOX_X 15
#define NEXT_BOX_Y 5
#define NEXT_BOX_HEIGHT 5
#define NEXT_BOX_WIDTH 5
#define NEXT_BOX_OFFSET 2

// generic array sizeof
#define SIZE(x) sizeof(x)/sizeof(x[0])

// Helper macro to address a mino in the board[] array
#define BOARD(x,y) board[(y)*BOARD_WIDTH+(x)]

// Helper boolean
#define WITHIN_BOARD(x,y) (x>=0 && x<BOARD_WIDTH && y>=0 && y<BOARD_HEIGHT)

// The last curses color pair will be our board empty color
#define EMPTY_COLOR COLOR_PAIRS-1

// multiple keys can be pressed at once
struct keymask {
    bool left;
    bool right;
    bool down;
    bool rot_l;
    bool rot_r;
};

// The game board will be an array of minos
enum mino {
    MINO_NONE,
    MINO_I,
    MINO_J,
    MINO_L,
    MINO_T,
    MINO_S,
    MINO_Z,
    MINO_O,
};

// Maps to enum mino
int color_map[] = {
    COLOR_BLACK,
    COLOR_WHITE,
    COLOR_BLUE,
    COLOR_MAGENTA,
    COLOR_GREEN,
    COLOR_YELLOW,
    COLOR_CYAN,
    COLOR_WHITE,
};

// number of milliseconds to wait between dropping the active tetromino down
// for each level up to level 29
int drop_delay_levels[] = {
// 0 - 9
800, 717, 633, 550, 467, 383, 300, 217, 133, 100,
// 10 - 12
83, 83, 83,
// 13 - 15
67, 67, 67,
// 16 - 18
50, 50, 50,
// 19 - 28
33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
// 29+
17
};

// game board will reflect all the locked (not falling) minos on screen
enum mino board[BOARD_HEIGHT*BOARD_WIDTH];
struct tetromino current_tetromino;
unsigned int next_tetromino;
bool game_active = true;

// only redraw the screen when something changes
bool screen_refresh = true;

// Manually starting at level 18, // TODO this is junk fix this
int drop_delay_ms = 50;
int lines_cleared = 180;
int level = 18;

// which rows on the board are cleared and need to be removed
// -1 uninitialized
// the maximum cleared rows is number of minos in a tetromino (4)
int cleared_rows[MINOS_IN_TETROMINO];

// generic vec2 struct
struct coord {
    int x;
    int y;
};

// tetrominos are stored as a list of mino offsets from the origin
struct offsets {
    struct coord coords[MINOS_IN_TETROMINO];
};

// each tetromino has a set of rotation states
// each rotation state has its own set of offsets
struct info {
    enum mino type;
    unsigned int num_rotations;
    struct offsets rotation[MINOS_IN_TETROMINO];
};

// Stores all information about the currently falling tetromino
// Each tetromino has an origin indicating its location on the board
// and a list of offsets which indicate where each mino in the
// tetromino should be relative to the offset
// each tetromino has a different set of offsets based on its rotation state
// some tetrominos have only 2 rotation states whereas most have 4
struct tetromino {
    struct coord origin;
    unsigned int rotation_state;
    struct info *info;
};

// each tetromino has a list of offsets for each possible rotation state
// some tetrominos only have 2 rotation states
// some have 4 rotation states
// (we are emulating NES tetris here)
// (https://harddrop.com/wiki/Nintendo_Rotation_System)
// For the drawings, I'm denoting the origin as either a small x or an o
// depending on whether the origin has a mino in it

// I Tetromino
// XxXX
// or
//  oX
//   X
//   X
//   X

struct info tetromino_i = {
    .type = MINO_I,
    .num_rotations = 2,
    .rotation[0] = {
        .coords[0] = { -1, 0 },
        .coords[1] = { 0,  0 },
        .coords[2] = { 1, 0 },
        .coords[3] = { 2, 0 },
    },
    .rotation[1] = {
        .coords[0] = { 1, 0 },
        .coords[1] = { 1, -1 },
        .coords[2] = { 1, -2 },
        .coords[3] = { 1, -3 },
    },
};

struct info tetromino_o = {
    .type = MINO_O,
    .num_rotations = 1,
    .rotation[0] = {
        .coords[0] = { 0, 0},
        .coords[1] = { 1, 0},
        .coords[2] = { 0, -1},
        .coords[3] = { 1, -1},
    }
};

struct info tetromino_t = {
    .type = MINO_T,
    .num_rotations = 4,
    .rotation[0] = {
        .coords[0] = { -1, 0},
        .coords[1] = { 0, 0},
        .coords[2] = { 1, 0},
        .coords[3] = { 0, -1},
    },
    .rotation[1] = {
        .coords[0] = { 0, 1},
        .coords[1] = { -1,0},
        .coords[2] = { 0, 0},
        .coords[3] = { 0, -1},
    },
    .rotation[2] = {
        .coords[0] = { -1, 0},
        .coords[1] = { 0, 0},
        .coords[2] = { 1, 0},
        .coords[3] = { 0, 1},
    },
    .rotation[3] = {
        .coords[0] = { 1, 0},
        .coords[1] = { 0, -1},
        .coords[2] = { 0, 0},
        .coords[3] = { 0, 1},
    },
};

struct info tetromino_l = {
    .type = MINO_L,
    .num_rotations = 4,
    .rotation[0] = {
        .coords[0] = { -1, 0},
        .coords[1] = { 0, 0},
        .coords[2] = { 1, 0},
        .coords[3] = { -1, -1},
    },
    .rotation[1] = {
        .coords[0] = { -1, 1},
        .coords[1] = { 0, 1},
        .coords[2] = { 0, 0},
        .coords[3] = { 0, -1},
    },
    .rotation[2] = {
        .coords[0] = { -1, 0},
        .coords[1] = { 0, 0},
        .coords[2] = { 1, 0},
        .coords[3] = { 1, 1},
    },
    .rotation[3] = {
        .coords[0] = { 0, 1},
        .coords[1] = { 0, 0},
        .coords[2] = { 0, -1},
        .coords[3] = { 1, -1},
    },
};

struct info tetromino_j = {
    .type = MINO_L,
    .num_rotations = 4,
    .rotation[0] = {
        .coords[0] = { -1, 0},
        .coords[1] = { 0, 0},
        .coords[2] = { 1, 0},
        .coords[3] = { 1, -1},
    },
    .rotation[1] = {
        .coords[0] = { -1, -1},
        .coords[1] = { 0, 1},
        .coords[2] = { 0, 0},
        .coords[3] = { 0, -1},
    },
    .rotation[2] = {
        .coords[0] = { -1, 0},
        .coords[1] = { 0, 0},
        .coords[2] = { 1, 0},
        .coords[3] = { -1, 1},
    },
    .rotation[3] = {
        .coords[0] = { 0, 1},
        .coords[1] = { 0, 0},
        .coords[2] = { 0, -1},
        .coords[3] = { 1, 1},
    },
};

struct info tetromino_s = {
    .type = MINO_S,
    .num_rotations = 2,
    .rotation[0] = {
        .coords[0] = { 0, 0},
        .coords[1] = { 1, 0},
        .coords[2] = { -1, -1},
        .coords[3] = { 0, -1},
    },
    .rotation[1] = {
        .coords[0] = { 0, 1},
        .coords[1] = { 0, 0},
        .coords[2] = { 1, 0},
        .coords[3] = { 1, -1},
    },
};

struct info tetromino_z = {
    .type = MINO_Z,
    .num_rotations = 2,
    .rotation[0] = {
        .coords[0] = { -1, 0},
        .coords[1] = { 0, 0},
        .coords[2] = { 0, -1},
        .coords[3] = { 1, -1},
    },
    .rotation[1] = {
        .coords[0] = { 0, 0},
        .coords[1] = { 0, -1},
        .coords[2] = { 1, 1},
        .coords[3] = { 1, 0},
    },
};

// Array of pointers to the tetromino offset structs
// this makes it easy to randomly pick a tetromino
struct info *tetromino_types[] = {
    &tetromino_i,
    &tetromino_o,
    &tetromino_t,
    &tetromino_l,
    &tetromino_j,
    &tetromino_z,
    &tetromino_s,
};

void update_level_and_speed()
{
    level = lines_cleared / 10;
    if (level < SIZE(drop_delay_levels)) {
        drop_delay_ms = drop_delay_levels[level];
    } else {
        drop_delay_ms = drop_delay_levels[SIZE(drop_delay_levels)-1];
    }
}

// searches for completed rows on the game board
// clears all minos from these rows
// for animation reasons, don't drop the rows down yet
int clear_completed_rows()
{
    int i;
    int num_cleared = 0;
    for (i=0; i<SIZE(cleared_rows); i++) {
        cleared_rows[i] = -1;
    }
    i=0;
    for (int y=0; y<BOARD_HEIGHT; y++) {
        bool row_complete = true;
        for (int x=0; x<BOARD_WIDTH; x++) {
            if (BOARD(x,y) == MINO_NONE) {
                row_complete = false;
                break;
            }
        }
        if (row_complete) {
	    num_cleared++;
            lines_cleared++;
            update_level_and_speed();
            cleared_rows[i++] = y;
            for (int x=0; x<BOARD_WIDTH; x++) {
                BOARD(x,y) = MINO_NONE;
            }
        }
    }
    return num_cleared;
}

// take the row at y and copy it into the row n below
void drop_row_n(int y, int n)
{
    for (int x=0; x<BOARD_WIDTH; x++) {
        BOARD(x,y-n) = BOARD(x,y);
    }
}


// Use the cleared_rows array from clear_completed_rows()
// to move rows downwards to fill cleared rows
// clared_rows must be guaranteed to be in bottom-up order
void drop_rows()
{
    int i=0;
    // negative number in cleared_rows array means no more rows to clear
    while (i<SIZE(cleared_rows) && cleared_rows[i] >= 0 && cleared_rows[i]+1<=BOARD_HEIGHT) {
        // if this is the last row to clear
        // drop lines all the way up to the top of the screen
        int z;
        if ((i+1 >= SIZE(cleared_rows)) || cleared_rows[i+1] < 0) {
            z = BOARD_HEIGHT;
        } else {
            z = cleared_rows[i+1];
        }

        for (int y=cleared_rows[i]+1; y<z; y++) {
            // If this is the first index in the cleared_rows array
            // drop all rows above down 1 until the next cleared row
            // if this is the second cleared row, drop down 2...
            drop_row_n(y, i+1);
            screen_refresh = true;
        }
        i++;
    }
}

#define SCREEN_X(x) (2*x+1)
#define SCREEN_Y(y) (BOARD_HEIGHT - y + 1)

// convert the game coords to screen coords and draw a mino
void draw_mino(unsigned int x, unsigned int y, enum mino m, bool nextbox) {
    if (WITHIN_BOARD(x,y) || nextbox) {
        attron(COLOR_PAIR(m));
        mvaddch(SCREEN_Y(y),SCREEN_X(x), '[');
        mvaddch(SCREEN_Y(y),SCREEN_X(x)+1, ']');
    }
}

// draw all minos on the board which are not currently falling
void draw_board()
{
    for (int y=0; y<BOARD_HEIGHT; y++) {
        for(int x=0; x<BOARD_WIDTH; x++) {
            if (BOARD(x,y) > 0) {
                draw_mino(x,y, BOARD(x,y), false);
            } else {
		// TODO: break this out into a function for this?
                attron(COLOR_PAIR(EMPTY_COLOR));
                mvaddch(SCREEN_Y(y),SCREEN_X(x), ' ');
                mvaddch(SCREEN_Y(y),SCREEN_X(x)+1, ' ');
            }
        }
    }
}

void draw_tetromino()
{
    struct tetromino *c = &current_tetromino;
    for (int i=0; i<MINOS_IN_TETROMINO; i++) {
        int x = c->origin.x + c->info->rotation[c->rotation_state].coords[i].x;
        int y = c->origin.y + c->info->rotation[c->rotation_state].coords[i].y;
        draw_mino(x, y, c->info->type, false);
    }
}

void draw_next_tetromino(bool clear)
{
    struct tetromino n;
    n.info = tetromino_types[next_tetromino];
    n.rotation_state = 0;
    n.origin.x = NEXT_BOX_X;
    n.origin.y = NEXT_BOX_Y;

    for (int i=0; i<MINOS_IN_TETROMINO; i++) {
        int x = n.origin.x + n.info->rotation[n.rotation_state].coords[i].x;
        int y = n.origin.y + n.info->rotation[n.rotation_state].coords[i].y;
	if (clear) {
	    attron(COLOR_PAIR(EMPTY_COLOR));
	    mvaddch(SCREEN_Y(y),SCREEN_X(x), ' ');
	    mvaddch(SCREEN_Y(y),SCREEN_X(x)+1, ' ');
        } else {
            draw_mino(x, y, n.info->type, true);
	}
    }
}

void new_active_tetromino() {
    // set current tetromino to next tetromino
    //printf("Choose tetromino %d\n", chosen_tetromino);
    current_tetromino.info = tetromino_types[next_tetromino];

    current_tetromino.origin.x = BOARD_WIDTH/2;
    current_tetromino.origin.y = BOARD_HEIGHT-1;

    current_tetromino.rotation_state = 0;

    // select a random tetromino type
    draw_next_tetromino(true);
    next_tetromino = rand() % SIZE(tetromino_types);
    draw_next_tetromino(false);
}

void tetris_setup()
{
    //printf("Filling board minos\n");
    for (int y=0; y<BOARD_HEIGHT; y++) {
        for (int x=0; x<BOARD_WIDTH; x++) {
            BOARD(x,y) = MINO_NONE;
        }
    }

    //printf("Making falling tetromino\n");
    next_tetromino = rand() % SIZE(tetromino_types);
    new_active_tetromino();
}



void curses_setup() {
    initscr();
    start_color();
    use_default_colors(); noecho();
    keypad(stdscr,TRUE);
    curs_set(0);
    nodelay(stdscr, TRUE);
    leaveok(stdscr, TRUE);
    scrollok(stdscr, FALSE);

    init_pair(MINO_I, color_map[MINO_I], color_map[MINO_I]);
    init_pair(MINO_J, color_map[MINO_J], color_map[MINO_J]);
    init_pair(MINO_L, color_map[MINO_L], color_map[MINO_L]);
    init_pair(MINO_T, color_map[MINO_T], color_map[MINO_T]);
    init_pair(MINO_S, color_map[MINO_S], color_map[MINO_S]);
    init_pair(MINO_Z, color_map[MINO_Z], color_map[MINO_Z]);
    init_pair(MINO_O, color_map[MINO_O], color_map[MINO_O]);
    init_pair(EMPTY_COLOR, COLOR_WHITE, COLOR_BLACK);
}

void get_user_input(struct keymask *keys) {
    int ch;
    do {
        ch = getch();
        switch(ch) {
            case KEY_RIGHT:
            case 'l':
                keys->right = true;
                break;
            case KEY_LEFT:
            case 'h':
                keys->left = true;
                break;
            case KEY_UP:
            case 'k':
                break;
            case KEY_DOWN:
            case 'j':
                keys->down = true;
                break;
            case 'z':
                keys->rot_l = true;
                break;
            case 'x':
                keys->rot_r = true;
                break;
            case 'n':
                //new_game = true;
                break;
            case 'q':
                game_active = false;
                break;
        }
    } while (ch != -1);
}

// if the current tetromino intersects any mino on the board
// or if the current tetromino is to the left, right, or below the board
// (active tetromino above the board is okay unless it locks there
// in which case game over)
bool tetromino_intersects(struct tetromino *t) {
    // iterate across 4 minos of current tetromino
    struct offsets *o = &t->info->rotation[t->rotation_state];
    for (int i=0; i<MINOS_IN_TETROMINO; i++) {
        int x = t->origin.x + o->coords[i].x;
        int y = t->origin.y + o->coords[i].y;

        //if (!WITHIN_BOARD(x,y)) printf("outside board. x=%d, y=%d\n", x, y);

        if (!WITHIN_BOARD(x,y) || BOARD(x,y) != MINO_NONE) {
            return true;
        }
    }

    return false;
}

// add the tetromono's minos to the game board
// if the tetromino is off the top of the screen, game over
void tetromino_to_board(struct tetromino *t) {
    // iterate across 4 minos of current tetromino
    struct offsets *o = &t->info->rotation[t->rotation_state];
    for (int i=0; i<MINOS_IN_TETROMINO; i++) {
        int x = t->origin.x + o->coords[i].x;
        int y = t->origin.y + o->coords[i].y;
        if (y >= BOARD_HEIGHT || BOARD(x,y) != MINO_NONE) {
            printf("\r\nGame Over\n\r"); // TODO fix this
            game_active = false;
        }

        if (WITHIN_BOARD(x,y)) {
            BOARD(x,y) = t->info->type;
        }
    }

    new_active_tetromino();
    screen_refresh = true;
}

void drop_tetromino()
{
    // make a copy of the active tetromino
    struct tetromino t = current_tetromino;
    // drop the hypothetical tetromino down 1 block
    t.origin.y--;
    if (tetromino_intersects(&t)) {
        // If tetromino cannot drop any further, add it to the board
        tetromino_to_board(&current_tetromino);
    } else {
        current_tetromino.origin.y--;
        screen_refresh = true;
    }
}

void move_tetromino_r()
{
    // make a copy of the active tetromino
    struct tetromino t = current_tetromino;
    // move the hypothetical tetromino and check for collisions
    t.origin.x++;
    if (!tetromino_intersects(&t)) {
        current_tetromino.origin.x++;
        screen_refresh = true;
    }

}

void move_tetromino_l()
{
    // make a copy of the active tetromino
    struct tetromino t = current_tetromino;
    // move the hypothetical tetromino and check for collisions
    t.origin.x--;
    if (!tetromino_intersects(&t)) {
        current_tetromino.origin.x--;
        screen_refresh = true;
    }
}

void rotate_tetromino_r()
{
    // make a copy of the active tetromino
    struct tetromino t = current_tetromino;
    // move the hypothetical tetromino and check for collisions
    t.rotation_state = (t.rotation_state + 1) % t.info->num_rotations;
    if (!tetromino_intersects(&t)) {
        current_tetromino.rotation_state = t.rotation_state;
        screen_refresh = true;
    }
}

void rotate_tetromino_l()
{
    // make a copy of the active tetromino
    struct tetromino t = current_tetromino;
    // move the hypothetical tetromino and check for collisions
    t.rotation_state = (t.rotation_state - 1) % t.info->num_rotations;
    if (!tetromino_intersects(&t)) {
        current_tetromino.rotation_state = t.rotation_state;
        screen_refresh = true;
    }
}

void process_user_input(struct keymask *prev_keys)
{
    struct keymask keys = {0};
    get_user_input(&keys);
    if (keys.right) {
        if(prev_keys->right) {
            // right held
        } else {
            // right pressed
            move_tetromino_r();
        }
    }

    if (keys.left) {
        if(prev_keys->left) {
            // left held
        } else {
            // left pressed
            move_tetromino_l();
        }
    }

    if (keys.rot_l) {
        if(prev_keys->left) {
            // key held
        } else {
            // key pressed
            rotate_tetromino_l();
        }
    }

    if (keys.rot_r) {
        if(prev_keys->left) {
            // key held
        } else {
            // key pressed
            rotate_tetromino_r();
        }
    }

    if (keys.down) {
        // TODO: fix
        drop_delay_ms = 100;
    } else {
        drop_delay_ms = 300;
    }

    // copy new keys to old key struct
    *prev_keys = keys;
}

long get_millis() {
    struct timespec ts;
    int rc = clock_gettime(CLOCK_MONOTONIC, &ts);
    if (rc) {
        printf("Clock read error\n");
    }
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

int main()
{
    srand(time(NULL));
    curses_setup();
    tetris_setup();

    int millis = get_millis();
    int last_drop_ms = millis;
    struct keymask keys = {0};
    while (game_active) {
        millis = get_millis();
        if (screen_refresh) {
            draw_board();
            draw_tetromino();
            refresh();
            screen_refresh = false;
        }
        int num_cleared = clear_completed_rows();
	if (num_cleared > 0) {
	    drop_rows();
	}
        process_user_input(&keys);
        if (millis - last_drop_ms > drop_delay_ms) {
            drop_tetromino();
            last_drop_ms = millis;
            screen_refresh = true;
        }
    }

    sleep(1);
    endwin();
    return 0;
}
