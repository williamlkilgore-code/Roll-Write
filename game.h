/*
 * Procedural Dungeon Crawler - Game Header
 * Port from Python to C for Windows/Wine compatibility
 */

#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include <stdbool.h>

/* ==========================================================================
 * CONSTANTS
 * ========================================================================== */

#define GRID_WIDTH      25
#define GRID_HEIGHT     20
#define MAX_FLOORS      100
#define MAX_ROOMS       10
#define MAX_PORTALS     10
#define MAX_INVENTORY   20
#define MAX_HP          32
#define STARTING_HP     10

/* ==========================================================================
 * ENUMS
 * ========================================================================== */

typedef enum {
    TILE_EMPTY = 0,
    TILE_WALL,
    TILE_START,
    TILE_STAIRS,
    TILE_COIN,
    TILE_CHEST,
    TILE_HEART,
    TILE_ENEMY,
    TILE_WEB,
    TILE_KEY,
    TILE_LOCKED_DOOR,
    TILE_PORTAL
} TileType;

typedef enum {
    DIR_N = 0,  /* North (0, -1) */
    DIR_S,      /* South (0, 1) */
    DIR_E,      /* East (1, 0) */
    DIR_W,      /* West (-1, 0) */
    DIR_NE,     /* Northeast (1, -1) */
    DIR_NW,     /* Northwest (-1, -1) */
    DIR_SE,     /* Southeast (1, 1) */
    DIR_SW,     /* Southwest (-1, 1) */
    DIR_COUNT
} Direction;

typedef enum {
    DIFF_EASY = 2,
    DIFF_NORMAL = 4,
    DIFF_HARD = 8,
    DIFF_DEMONIC = 10
} Difficulty;

typedef enum {
    ITEM_COMPASS = 0,       /* Compass of True North - ignore parity */
    ITEM_ANCHOR,            /* Anchor Stone - stop early */
    ITEM_SMOKE_BOMB,        /* Smoke Bomb - negate enemy damage */
    ITEM_LOADED_DICE,       /* Loaded Dice - choose roll */
    ITEM_LUCKY_CHARM,       /* Lucky Charm - adjust roll +/-1 */
    ITEM_LOCKPICK,          /* Lockpick - open door without key */
    ITEM_PARITY_FLIP,       /* Parity Flip - swap odd/even */
    ITEM_COUNT
} ItemType;

typedef enum {
    MOVE_SUCCESS = 0,
    MOVE_BLOCKED,
    MOVE_WEB_STOPPED,
    MOVE_PLAYER_DIED,
    MOVE_REACHED_STAIRS
} MoveResult;

/* ==========================================================================
 * DATA STRUCTURES
 * ========================================================================== */

/* Deterministic PRNG using xorshift64 */
typedef struct {
    uint64_t state;
} PRNG;

/* Grid cell */
typedef struct {
    TileType tile;
    int portal_group;  /* -1 if not a portal */
} GridCell;

/* Room structure */
typedef struct {
    int x, y;           /* Top-left interior */
    int width, height;  /* Interior dimensions */
} Room;

/* Portal group */
typedef struct {
    int positions[4][2];  /* Up to 4 portals per group */
    int count;
} PortalGroup;

/* Level structure */
typedef struct {
    int floor_number;
    GridCell grid[GRID_HEIGHT][GRID_WIDTH];
    int start_x, start_y;
    int stairs_x, stairs_y;
    Room rooms[MAX_ROOMS];
    int room_count;
    PortalGroup portal_groups[MAX_PORTALS];
    int portal_group_count;
    uint64_t level_seed;
} Level;

/* Player state */
typedef struct {
    int hp;
    int max_hp;
    int coins;
    int keys;
    int x, y;
    ItemType inventory[MAX_INVENTORY];
    int inventory_count;

    /* Status flags */
    bool half_next_roll;
    bool portal_used_this_turn;
    bool compass_active;
    bool parity_flipped;
    int loaded_dice_value;  /* 0 = not set */

    /* Path tracking for backtrack prevention */
    int path_this_turn[100][2];
    int path_length;
} Player;

/* Shop state */
typedef struct {
    int floor_number;
    ItemType inventory[4];
    bool purchased[4];
    uint64_t shop_seed;
} Shop;

/* Main game state */
typedef struct {
    uint64_t master_seed;
    Difficulty difficulty;
    int current_floor;
    int turn_number;

    Player player;
    Level *current_level;
    Shop *current_shop;

    int shop_floors[20];
    int shop_floor_count;

    bool game_over;
    bool victory;

    PRNG turn_rng;
} Game;

/* Movement outcome */
typedef struct {
    MoveResult result;
    int new_x, new_y;
    int damage_taken;
    int coins_gained;
    int coins_lost;
    int hp_healed;
    int keys_gained;
    int keys_used;
    bool portal_teleported;
    bool smoke_bomb_used;
} MoveOutcome;

/* ==========================================================================
 * DIRECTION HELPERS
 * ========================================================================== */

static const int DIR_DX[DIR_COUNT] = { 0, 0, 1, -1, 1, -1, 1, -1 };
static const int DIR_DY[DIR_COUNT] = { -1, 1, 0, 0, -1, -1, 1, 1 };

static inline bool dir_is_orthogonal(Direction d) {
    return d == DIR_N || d == DIR_S || d == DIR_E || d == DIR_W;
}

static inline bool dir_is_diagonal(Direction d) {
    return d == DIR_NE || d == DIR_NW || d == DIR_SE || d == DIR_SW;
}

static inline Direction dir_reverse(Direction d) {
    switch (d) {
        case DIR_N: return DIR_S;
        case DIR_S: return DIR_N;
        case DIR_E: return DIR_W;
        case DIR_W: return DIR_E;
        case DIR_NE: return DIR_SW;
        case DIR_NW: return DIR_SE;
        case DIR_SE: return DIR_NW;
        case DIR_SW: return DIR_NE;
        default: return d;
    }
}

/* ==========================================================================
 * ITEM INFO
 * ========================================================================== */

static const char *ITEM_NAMES[ITEM_COUNT] = {
    "Compass of True North",
    "Anchor Stone",
    "Smoke Bomb",
    "Loaded Dice",
    "Lucky Charm",
    "Lockpick",
    "Parity Flip"
};

static const int ITEM_COSTS[ITEM_COUNT] = {
    8,   /* Compass */
    5,   /* Anchor */
    10,  /* Smoke Bomb */
    15,  /* Loaded Dice */
    6,   /* Lucky Charm */
    7,   /* Lockpick */
    6    /* Parity Flip */
};

/* ==========================================================================
 * FUNCTION DECLARATIONS
 * ========================================================================== */

/* PRNG functions */
void prng_seed(PRNG *rng, uint64_t seed);
uint64_t prng_next(PRNG *rng);
int prng_randint(PRNG *rng, int min, int max);
int prng_d6(PRNG *rng);
void prng_shuffle_int(PRNG *rng, int *arr, int count);

/* Hash function for deterministic seeding */
uint64_t hash_seed(uint64_t base, const char *key, int extra);

/* Level generation */
Level *level_generate(uint64_t master_seed, int floor_number);
void level_free(Level *level);

/* Shop functions */
Shop *shop_generate(uint64_t master_seed, int floor_number);
void shop_free(Shop *shop);
void shop_get_floors(uint64_t master_seed, int *floors, int *count);

/* Game functions */
Game *game_create(uint64_t seed, Difficulty difficulty);
void game_free(Game *game);
void game_start(Game *game);
void game_advance_floor(Game *game, int floor);
bool game_is_shop_floor(Game *game);
int game_roll_die(Game *game);
void game_start_turn(Game *game);

/* Movement functions */
bool movement_get_allowed_directions(Game *game, int roll, bool *allowed);
bool movement_can_move_to(Game *game, int x, int y, Direction dir);
MoveOutcome movement_execute_step(Game *game, Direction dir);
bool movement_is_legal(Game *game, Direction dir, bool *allowed, bool allow_backtrack);

/* Shop operations */
bool game_buy_item(Game *game, int index);
void game_gamble(Game *game, bool high, bool *won, int *change);
void game_leave_shop(Game *game);

/* Item usage */
bool game_use_compass(Game *game);
bool game_use_loaded_dice(Game *game, int value);
bool game_use_parity_flip(Game *game);
int game_apply_lucky_charm(Game *game, int roll, int adjust);

/* Save/Load */
bool game_save(Game *game, const char *filepath);
Game *game_load(const char *filepath);

#endif /* GAME_H */
