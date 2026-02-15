/*
 * Procedural Dungeon Crawler - Core Game Logic
 * Port from Python to C for Windows/Wine compatibility
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "game.h"

/* ==========================================================================
 * PRNG IMPLEMENTATION (xorshift64)
 * ========================================================================== */

void prng_seed(PRNG *rng, uint64_t seed) {
    rng->state = seed ? seed : 1;
}

uint64_t prng_next(PRNG *rng) {
    uint64_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng->state = x;
    return x;
}

int prng_randint(PRNG *rng, int min, int max) {
    if (min > max) { int t = min; min = max; max = t; }
    uint64_t range = (uint64_t)(max - min + 1);
    return min + (int)(prng_next(rng) % range);
}

int prng_d6(PRNG *rng) {
    return prng_randint(rng, 1, 6);
}

void prng_shuffle_int(PRNG *rng, int *arr, int count) {
    for (int i = count - 1; i > 0; i--) {
        int j = prng_randint(rng, 0, i);
        int tmp = arr[i];
        arr[i] = arr[j];
        arr[j] = tmp;
    }
}

/* ==========================================================================
 * HASH FUNCTION FOR DETERMINISTIC SEEDING
 * ========================================================================== */

/* Simple FNV-1a hash */
uint64_t hash_seed(uint64_t base, const char *key, int extra) {
    uint64_t hash = 14695981039346656037ULL;

    /* Hash the base */
    for (int i = 0; i < 8; i++) {
        hash ^= (base >> (i * 8)) & 0xFF;
        hash *= 1099511628211ULL;
    }

    /* Hash the key string */
    if (key) {
        while (*key) {
            hash ^= (uint8_t)*key++;
            hash *= 1099511628211ULL;
        }
    }

    /* Hash the extra value */
    for (int i = 0; i < 4; i++) {
        hash ^= (extra >> (i * 8)) & 0xFF;
        hash *= 1099511628211ULL;
    }

    return hash;
}

/* Event-based d6 roll (deterministic based on event parameters) */
static int event_d6(uint64_t level_seed, const char *event, int x, int y) {
    uint64_t seed = hash_seed(level_seed, event, x * 1000 + y);
    PRNG rng;
    prng_seed(&rng, seed);
    return prng_d6(&rng);
}

/* ==========================================================================
 * LEVEL GENERATION
 * ========================================================================== */

static void generate_rooms(Level *level, PRNG *rng) {
    int max_attempts = 50;
    int target_rooms = prng_randint(rng, 4, 7);

    level->room_count = 0;

    for (int attempt = 0; attempt < max_attempts && level->room_count < target_rooms; attempt++) {
        /* Room interior size (min 3x3) */
        int room_w = prng_randint(rng, 3, 6);
        int room_h = prng_randint(rng, 3, 6);

        if (room_w > GRID_WIDTH - 4) room_w = GRID_WIDTH - 4;
        if (room_h > GRID_HEIGHT - 4) room_h = GRID_HEIGHT - 4;

        /* Position (leaving space for walls) */
        int room_x = prng_randint(rng, 2, GRID_WIDTH - room_w - 2);
        int room_y = prng_randint(rng, 2, GRID_HEIGHT - room_h - 2);

        /* Check for overlap */
        bool overlaps = false;
        for (int i = 0; i < level->room_count; i++) {
            Room *existing = &level->rooms[i];
            if (room_x - 2 < existing->x + existing->width &&
                room_x + room_w + 2 > existing->x &&
                room_y - 2 < existing->y + existing->height &&
                room_y + room_h + 2 > existing->y) {
                overlaps = true;
                break;
            }
        }

        if (!overlaps) {
            Room *room = &level->rooms[level->room_count++];
            room->x = room_x;
            room->y = room_y;
            room->width = room_w;
            room->height = room_h;

            /* Carve out room interior */
            for (int dy = 0; dy < room_h; dy++) {
                for (int dx = 0; dx < room_w; dx++) {
                    level->grid[room_y + dy][room_x + dx].tile = TILE_EMPTY;
                }
            }
        }
    }
}

static void carve_horizontal(Level *level, int x1, int x2, int y) {
    int start = x1 < x2 ? x1 : x2;
    int end = x1 < x2 ? x2 : x1;
    for (int x = start; x <= end; x++) {
        if (x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT) {
            level->grid[y][x].tile = TILE_EMPTY;
        }
    }
}

static void carve_vertical(Level *level, int y1, int y2, int x) {
    int start = y1 < y2 ? y1 : y2;
    int end = y1 < y2 ? y2 : y1;
    for (int y = start; y <= end; y++) {
        if (x >= 0 && x < GRID_WIDTH && y >= 0 && y < GRID_HEIGHT) {
            level->grid[y][x].tile = TILE_EMPTY;
        }
    }
}

static void connect_rooms(Level *level, PRNG *rng) {
    for (int i = 0; i < level->room_count - 1; i++) {
        Room *r1 = &level->rooms[i];
        Room *r2 = &level->rooms[i + 1];

        int x1 = r1->x + r1->width / 2;
        int y1 = r1->y + r1->height / 2;
        int x2 = r2->x + r2->width / 2;
        int y2 = r2->y + r2->height / 2;

        if (prng_randint(rng, 0, 1) == 0) {
            carve_horizontal(level, x1, x2, y1);
            carve_vertical(level, y1, y2, x2);
        } else {
            carve_vertical(level, y1, y2, x1);
            carve_horizontal(level, x1, x2, y2);
        }
    }
}

/* Check if position has valid moves for BOTH parities (orthogonal and diagonal) */
static bool position_has_all_moves(Level *level, int px, int py) {
    /* Check bounds - need 1 tile margin all around */
    if (px < 1 || px >= GRID_WIDTH - 1 || py < 1 || py >= GRID_HEIGHT - 1) {
        return false;
    }

    /* Check all 8 neighbors are passable (not walls) */
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            TileType tile = level->grid[py + dy][px + dx].tile;
            if (tile == TILE_WALL || tile == TILE_LOCKED_DOOR) {
                return false;
            }
        }
    }

    return true;
}

static void place_start_and_stairs(Level *level, PRNG *rng) {
    if (level->room_count >= 2) {
        Room *start_room = &level->rooms[0];
        Room *stairs_room = &level->rooms[level->room_count - 1];

        /* Try center first */
        int center_x = start_room->x + start_room->width / 2;
        int center_y = start_room->y + start_room->height / 2;

        if (position_has_all_moves(level, center_x, center_y)) {
            level->start_x = center_x;
            level->start_y = center_y;
        } else {
            /* Search for valid position within room, starting from center outward */
            bool found = false;
            for (int radius = 0; radius < 3 && !found; radius++) {
                for (int dy = -radius; dy <= radius && !found; dy++) {
                    for (int dx = -radius; dx <= radius && !found; dx++) {
                        int tx = center_x + dx;
                        int ty = center_y + dy;
                        /* Check if inside room bounds */
                        if (tx >= start_room->x && tx < start_room->x + start_room->width &&
                            ty >= start_room->y && ty < start_room->y + start_room->height) {
                            if (position_has_all_moves(level, tx, ty)) {
                                level->start_x = tx;
                                level->start_y = ty;
                                found = true;
                            }
                        }
                    }
                }
            }

            if (!found) {
                /* Fallback to center even if not perfect */
                level->start_x = center_x;
                level->start_y = center_y;
            }
        }

        /* Stairs can be anywhere in the room */
        level->stairs_x = stairs_room->x + prng_randint(rng, 0, stairs_room->width - 1);
        level->stairs_y = stairs_room->y + prng_randint(rng, 0, stairs_room->height - 1);
    } else {
        /* Fallback: find any empty tiles */
        level->start_x = GRID_WIDTH / 4;
        level->start_y = GRID_HEIGHT / 4;
        level->stairs_x = 3 * GRID_WIDTH / 4;
        level->stairs_y = 3 * GRID_HEIGHT / 4;
    }

    level->grid[level->start_y][level->start_x].tile = TILE_START;
    level->grid[level->stairs_y][level->stairs_x].tile = TILE_STAIRS;
}

static int place_locked_doors(Level *level, PRNG *rng) {
    int doors_placed = 0;

    for (int i = 0; i < level->room_count; i++) {
        if (prng_randint(rng, 0, 2) == 0) {  /* 1/3 chance */
            Room *room = &level->rooms[i];

            /* Find a potential door position (corridor opening) */
            /* Check all edges for corridor connections */
            for (int edge = 0; edge < 4 && doors_placed < 5; edge++) {
                int check_x, check_y;
                bool found = false;

                switch (edge) {
                    case 0: /* North edge */
                        for (int dx = 0; dx < room->width && !found; dx++) {
                            check_x = room->x + dx;
                            check_y = room->y - 1;
                            if (check_y >= 0 && level->grid[check_y][check_x].tile == TILE_EMPTY) {
                                /* Check for corridor characteristics */
                                bool wall_left = (check_x == 0 || level->grid[check_y][check_x-1].tile == TILE_WALL);
                                bool wall_right = (check_x == GRID_WIDTH-1 || level->grid[check_y][check_x+1].tile == TILE_WALL);
                                if (wall_left && wall_right) {
                                    level->grid[check_y][check_x].tile = TILE_LOCKED_DOOR;
                                    doors_placed++;
                                    found = true;
                                }
                            }
                        }
                        break;
                    /* Similar for other edges - simplified for brevity */
                }
                if (found) break;
            }
        }
    }

    return doors_placed;
}

static void place_content(Level *level, PRNG *rng, int num_keys) {
    /* Collect empty tiles (excluding start and stairs) */
    int empty_tiles[GRID_WIDTH * GRID_HEIGHT][2];
    int empty_count = 0;

    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            if (level->grid[y][x].tile == TILE_EMPTY) {
                if ((x != level->start_x || y != level->start_y) &&
                    (x != level->stairs_x || y != level->stairs_y)) {
                    empty_tiles[empty_count][0] = x;
                    empty_tiles[empty_count][1] = y;
                    empty_count++;
                }
            }
        }
    }

    if (empty_count == 0) return;

    /* Shuffle empty tiles */
    for (int i = empty_count - 1; i > 0; i--) {
        int j = prng_randint(rng, 0, i);
        int tx = empty_tiles[i][0], ty = empty_tiles[i][1];
        empty_tiles[i][0] = empty_tiles[j][0];
        empty_tiles[i][1] = empty_tiles[j][1];
        empty_tiles[j][0] = tx;
        empty_tiles[j][1] = ty;
    }

    int idx = 0;

    /* Place keys first */
    for (int i = 0; i < num_keys && idx < empty_count; i++) {
        int x = empty_tiles[idx][0], y = empty_tiles[idx][1];
        level->grid[y][x].tile = TILE_KEY;
        idx++;
    }

    /* Place other content */
    int num_coins = prng_randint(rng, 3, 6);
    int num_chests = prng_randint(rng, 1, 2);
    int num_enemies = prng_randint(rng, 2, 4);
    int num_hearts = prng_randint(rng, 1, 2);
    int num_webs = prng_randint(rng, 1, 3);

    for (int i = 0; i < num_coins && idx < empty_count; i++, idx++) {
        level->grid[empty_tiles[idx][1]][empty_tiles[idx][0]].tile = TILE_COIN;
    }
    for (int i = 0; i < num_chests && idx < empty_count; i++, idx++) {
        level->grid[empty_tiles[idx][1]][empty_tiles[idx][0]].tile = TILE_CHEST;
    }
    for (int i = 0; i < num_enemies && idx < empty_count; i++, idx++) {
        level->grid[empty_tiles[idx][1]][empty_tiles[idx][0]].tile = TILE_ENEMY;
    }
    for (int i = 0; i < num_hearts && idx < empty_count; i++, idx++) {
        level->grid[empty_tiles[idx][1]][empty_tiles[idx][0]].tile = TILE_HEART;
    }
    for (int i = 0; i < num_webs && idx < empty_count; i++, idx++) {
        level->grid[empty_tiles[idx][1]][empty_tiles[idx][0]].tile = TILE_WEB;
    }
}

static void place_portals(Level *level, PRNG *rng) {
    level->portal_group_count = 0;

    if (level->room_count < 2) return;

    /* Find empty tiles for portals */
    int empty_tiles[GRID_WIDTH * GRID_HEIGHT][2];
    int empty_count = 0;

    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            if (level->grid[y][x].tile == TILE_EMPTY) {
                empty_tiles[empty_count][0] = x;
                empty_tiles[empty_count][1] = y;
                empty_count++;
            }
        }
    }

    if (empty_count < 4) return;

    /* Shuffle */
    for (int i = empty_count - 1; i > 0; i--) {
        int j = prng_randint(rng, 0, i);
        int tx = empty_tiles[i][0], ty = empty_tiles[i][1];
        empty_tiles[i][0] = empty_tiles[j][0];
        empty_tiles[i][1] = empty_tiles[j][1];
        empty_tiles[j][0] = tx;
        empty_tiles[j][1] = ty;
    }

    /* Create 1-2 portal groups with 2-3 portals each */
    int num_groups = prng_randint(rng, 1, 2);
    int idx = 0;

    for (int g = 0; g < num_groups && idx + 2 <= empty_count; g++) {
        PortalGroup *group = &level->portal_groups[level->portal_group_count];
        group->count = 0;

        int portals_in_group = prng_randint(rng, 2, 3);
        for (int p = 0; p < portals_in_group && idx < empty_count; p++) {
            int x = empty_tiles[idx][0];
            int y = empty_tiles[idx][1];

            level->grid[y][x].tile = TILE_PORTAL;
            level->grid[y][x].portal_group = level->portal_group_count;

            group->positions[group->count][0] = x;
            group->positions[group->count][1] = y;
            group->count++;
            idx++;
        }

        if (group->count >= 2) {
            level->portal_group_count++;
        }
    }
}

Level *level_generate(uint64_t master_seed, int floor_number) {
    Level *level = (Level *)calloc(1, sizeof(Level));
    if (!level) return NULL;

    level->floor_number = floor_number;
    level->level_seed = hash_seed(master_seed, "level", floor_number);

    uint64_t layout_seed = hash_seed(level->level_seed, "layout", 0);
    uint64_t content_seed = hash_seed(level->level_seed, "content", 0);
    uint64_t portal_seed = hash_seed(level->level_seed, "portals", 0);

    PRNG layout_rng, content_rng, portal_rng;
    prng_seed(&layout_rng, layout_seed);
    prng_seed(&content_rng, content_seed);
    prng_seed(&portal_rng, portal_seed);

    /* Initialize grid with walls */
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            level->grid[y][x].tile = TILE_WALL;
            level->grid[y][x].portal_group = -1;
        }
    }

    generate_rooms(level, &layout_rng);
    connect_rooms(level, &layout_rng);
    place_start_and_stairs(level, &layout_rng);
    int num_doors = place_locked_doors(level, &layout_rng);
    place_portals(level, &portal_rng);
    place_content(level, &content_rng, num_doors);

    return level;
}

void level_free(Level *level) {
    if (level) free(level);
}

/* ==========================================================================
 * SHOP GENERATION
 * ========================================================================== */

void shop_get_floors(uint64_t master_seed, int *floors, int *count) {
    PRNG rng;
    prng_seed(&rng, hash_seed(master_seed, "shop_floors", 0));

    *count = 0;
    int floor = prng_randint(&rng, 7, 10);

    while (floor <= MAX_FLOORS && *count < 20) {
        floors[(*count)++] = floor;
        floor += prng_randint(&rng, 7, 10);
    }
}

Shop *shop_generate(uint64_t master_seed, int floor_number) {
    Shop *shop = (Shop *)calloc(1, sizeof(Shop));
    if (!shop) return NULL;

    shop->floor_number = floor_number;
    shop->shop_seed = hash_seed(master_seed, "shop", floor_number);

    PRNG rng;
    prng_seed(&rng, shop->shop_seed);

    /* Select 4 distinct items */
    int item_pool[ITEM_COUNT];
    for (int i = 0; i < ITEM_COUNT; i++) item_pool[i] = i;
    prng_shuffle_int(&rng, item_pool, ITEM_COUNT);

    for (int i = 0; i < 4; i++) {
        shop->inventory[i] = (ItemType)item_pool[i];
        shop->purchased[i] = false;
    }

    return shop;
}

void shop_free(Shop *shop) {
    if (shop) free(shop);
}

/* ==========================================================================
 * GAME STATE MANAGEMENT
 * ========================================================================== */

Game *game_create(uint64_t seed, Difficulty difficulty) {
    Game *game = (Game *)calloc(1, sizeof(Game));
    if (!game) return NULL;

    game->master_seed = seed;
    game->difficulty = difficulty;
    game->current_floor = 0;
    game->turn_number = 0;
    game->game_over = false;
    game->victory = false;

    /* Initialize player */
    game->player.hp = STARTING_HP;
    game->player.max_hp = MAX_HP;
    game->player.coins = 0;
    game->player.keys = 0;
    game->player.inventory_count = 0;
    game->player.half_next_roll = false;
    game->player.loaded_dice_value = 0;

    /* Get shop floors */
    shop_get_floors(seed, game->shop_floors, &game->shop_floor_count);

    return game;
}

void game_free(Game *game) {
    if (game) {
        if (game->current_level) level_free(game->current_level);
        if (game->current_shop) shop_free(game->current_shop);
        free(game);
    }
}

void game_start(Game *game) {
    game_advance_floor(game, 1);
}

bool game_is_shop_floor(Game *game) {
    for (int i = 0; i < game->shop_floor_count; i++) {
        if (game->shop_floors[i] == game->current_floor) {
            return true;
        }
    }
    return false;
}

void game_advance_floor(Game *game, int floor) {
    game->current_floor = floor;
    game->turn_number = 0;

    /* Clean up old level/shop */
    if (game->current_level) {
        level_free(game->current_level);
        game->current_level = NULL;
    }
    if (game->current_shop) {
        shop_free(game->current_shop);
        game->current_shop = NULL;
    }

    if (game_is_shop_floor(game)) {
        game->current_shop = shop_generate(game->master_seed, floor);
    } else {
        game->current_level = level_generate(game->master_seed, floor);
        game->player.x = game->current_level->start_x;
        game->player.y = game->current_level->start_y;

        prng_seed(&game->turn_rng, hash_seed(game->master_seed, "turns", floor));
    }
}

int game_roll_die(Game *game) {
    if (game->player.loaded_dice_value > 0) {
        int roll = game->player.loaded_dice_value;
        game->player.loaded_dice_value = 0;
        return roll;
    }
    return prng_d6(&game->turn_rng);
}

void game_start_turn(Game *game) {
    game->turn_number++;
    game->player.portal_used_this_turn = false;
    game->player.compass_active = false;
    game->player.parity_flipped = false;
    game->player.path_length = 0;

    /* Add current position to path */
    game->player.path_this_turn[0][0] = game->player.x;
    game->player.path_this_turn[0][1] = game->player.y;
    game->player.path_length = 1;
}

/* ==========================================================================
 * MOVEMENT SYSTEM
 * ========================================================================== */

bool movement_get_allowed_directions(Game *game, int roll, bool *allowed) {
    /* Initialize all to false */
    for (int i = 0; i < DIR_COUNT; i++) allowed[i] = false;

    int effective_roll = roll;
    if (game->player.parity_flipped) {
        effective_roll = (roll % 2 == 0) ? roll + 1 : roll - 1;
    }

    if (game->player.compass_active) {
        /* All directions allowed */
        for (int i = 0; i < DIR_COUNT; i++) allowed[i] = true;
    } else if (effective_roll % 2 == 0) {
        /* Orthogonal only */
        allowed[DIR_N] = allowed[DIR_S] = allowed[DIR_E] = allowed[DIR_W] = true;
    } else {
        /* Diagonal only */
        allowed[DIR_NE] = allowed[DIR_NW] = allowed[DIR_SE] = allowed[DIR_SW] = true;
    }

    return true;
}

bool movement_can_move_to(Game *game, int x, int y, Direction dir) {
    if (x < 0 || x >= GRID_WIDTH || y < 0 || y >= GRID_HEIGHT) return false;

    Level *level = game->current_level;
    if (!level) return false;

    TileType tile = level->grid[y][x].tile;

    if (tile == TILE_WALL) return false;

    if (tile == TILE_LOCKED_DOOR) {
        if (game->player.keys > 0) return true;
        /* Check for lockpick */
        for (int i = 0; i < game->player.inventory_count; i++) {
            if (game->player.inventory[i] == ITEM_LOCKPICK) return true;
        }
        return false;
    }

    /* Check diagonal blocking (corner clipping) */
    if (dir_is_diagonal(dir)) {
        int px = game->player.x, py = game->player.y;
        int dx = DIR_DX[dir], dy = DIR_DY[dir];

        TileType adj1 = level->grid[py][px + dx].tile;
        TileType adj2 = level->grid[py + dy][px].tile;

        bool block1 = (adj1 == TILE_WALL) || (adj1 == TILE_LOCKED_DOOR && game->player.keys == 0);
        bool block2 = (adj2 == TILE_WALL) || (adj2 == TILE_LOCKED_DOOR && game->player.keys == 0);

        if (block1 && block2) return false;
    }

    return true;
}

bool movement_is_legal(Game *game, Direction dir, bool *allowed, bool allow_backtrack) {
    if (!allowed[dir]) return false;

    int nx = game->player.x + DIR_DX[dir];
    int ny = game->player.y + DIR_DY[dir];

    if (!movement_can_move_to(game, nx, ny, dir)) return false;

    /* Check backtracking */
    if (!allow_backtrack) {
        for (int i = 0; i < game->player.path_length; i++) {
            if (game->player.path_this_turn[i][0] == nx &&
                game->player.path_this_turn[i][1] == ny) {
                return false;
            }
        }
    }

    return true;
}

MoveOutcome movement_execute_step(Game *game, Direction dir) {
    MoveOutcome outcome = {0};

    int nx = game->player.x + DIR_DX[dir];
    int ny = game->player.y + DIR_DY[dir];

    outcome.new_x = nx;
    outcome.new_y = ny;

    if (!movement_can_move_to(game, nx, ny, dir)) {
        outcome.result = MOVE_BLOCKED;
        outcome.new_x = game->player.x;
        outcome.new_y = game->player.y;
        return outcome;
    }

    Level *level = game->current_level;
    GridCell *cell = &level->grid[ny][nx];

    /* Handle locked door */
    if (cell->tile == TILE_LOCKED_DOOR) {
        if (game->player.keys > 0) {
            game->player.keys--;
            outcome.keys_used = 1;
        } else {
            /* Use lockpick */
            for (int i = 0; i < game->player.inventory_count; i++) {
                if (game->player.inventory[i] == ITEM_LOCKPICK) {
                    /* Remove lockpick */
                    for (int j = i; j < game->player.inventory_count - 1; j++) {
                        game->player.inventory[j] = game->player.inventory[j + 1];
                    }
                    game->player.inventory_count--;
                    break;
                }
            }
        }
        cell->tile = TILE_EMPTY;
    }

    /* Move player */
    game->player.x = nx;
    game->player.y = ny;

    /* Add to path */
    if (game->player.path_length < 100) {
        game->player.path_this_turn[game->player.path_length][0] = nx;
        game->player.path_this_turn[game->player.path_length][1] = ny;
        game->player.path_length++;
    }

    /* Handle tile effects */
    switch (cell->tile) {
        case TILE_COIN:
            game->player.coins++;
            outcome.coins_gained = 1;
            cell->tile = TILE_EMPTY;
            break;

        case TILE_CHEST: {
            int roll = event_d6(level->level_seed, "CHEST", nx, ny);
            game->player.coins += roll;
            outcome.coins_gained = roll;
            cell->tile = TILE_EMPTY;
            break;
        }

        case TILE_HEART: {
            int roll = event_d6(level->level_seed, "HEART", nx, ny);
            int healed = roll;
            if (game->player.hp + healed > game->player.max_hp) {
                healed = game->player.max_hp - game->player.hp;
            }
            game->player.hp += healed;
            outcome.hp_healed = healed;
            cell->tile = TILE_EMPTY;
            break;
        }

        case TILE_KEY:
            game->player.keys++;
            outcome.keys_gained = 1;
            cell->tile = TILE_EMPTY;
            break;

        case TILE_ENEMY: {
            /* Check for smoke bomb */
            bool has_smoke = false;
            for (int i = 0; i < game->player.inventory_count; i++) {
                if (game->player.inventory[i] == ITEM_SMOKE_BOMB) {
                    /* Use smoke bomb */
                    for (int j = i; j < game->player.inventory_count - 1; j++) {
                        game->player.inventory[j] = game->player.inventory[j + 1];
                    }
                    game->player.inventory_count--;
                    outcome.smoke_bomb_used = true;
                    has_smoke = true;
                    break;
                }
            }

            if (!has_smoke) {
                int damage = (int)game->difficulty;
                game->player.hp -= damage;
                outcome.damage_taken = damage;
                if (game->player.hp <= 0) {
                    outcome.result = MOVE_PLAYER_DIED;
                    return outcome;
                }
            }
            cell->tile = TILE_EMPTY;
            break;
        }

        case TILE_WEB: {
            int roll = event_d6(level->level_seed, "WEB", nx, ny);
            int loss = roll < game->player.coins ? roll : game->player.coins;
            game->player.coins -= loss;
            outcome.coins_lost = loss;
            game->player.half_next_roll = true;
            outcome.result = MOVE_WEB_STOPPED;
            return outcome;
        }

        case TILE_PORTAL:
            if (!game->player.portal_used_this_turn && cell->portal_group >= 0) {
                PortalGroup *group = &level->portal_groups[cell->portal_group];
                /* Find another portal in the group */
                for (int i = 0; i < group->count; i++) {
                    if (group->positions[i][0] != nx || group->positions[i][1] != ny) {
                        game->player.x = group->positions[i][0];
                        game->player.y = group->positions[i][1];
                        outcome.new_x = game->player.x;
                        outcome.new_y = game->player.y;
                        outcome.portal_teleported = true;
                        game->player.portal_used_this_turn = true;

                        /* Add teleport destination to path */
                        if (game->player.path_length < 100) {
                            game->player.path_this_turn[game->player.path_length][0] = game->player.x;
                            game->player.path_this_turn[game->player.path_length][1] = game->player.y;
                            game->player.path_length++;
                        }
                        break;
                    }
                }
            }
            break;

        case TILE_STAIRS:
            outcome.result = MOVE_REACHED_STAIRS;
            return outcome;

        default:
            break;
    }

    outcome.result = MOVE_SUCCESS;
    return outcome;
}

/* ==========================================================================
 * SHOP OPERATIONS
 * ========================================================================== */

bool game_buy_item(Game *game, int index) {
    if (!game->current_shop || index < 0 || index >= 4) return false;

    Shop *shop = game->current_shop;
    if (shop->purchased[index]) return false;

    ItemType item = shop->inventory[index];
    int price = ITEM_COSTS[item];

    if (game->player.coins < price) return false;
    if (game->player.inventory_count >= MAX_INVENTORY) return false;

    game->player.coins -= price;
    game->player.inventory[game->player.inventory_count++] = item;
    shop->purchased[index] = true;

    return true;
}

void game_gamble(Game *game, bool high, bool *won, int *change) {
    uint64_t gamble_seed = hash_seed(game->master_seed, "gamble",
                                     game->current_floor * 1000 + game->turn_number);
    PRNG rng;
    prng_seed(&rng, gamble_seed);
    int roll = prng_d6(&rng);
    game->turn_number++;

    if ((high && roll >= 5) || (!high && roll <= 2)) {
        *won = true;
        *change = 10;
        game->player.coins += 10;
    } else {
        *won = false;
        int loss = 4 < game->player.coins ? 4 : game->player.coins;
        game->player.coins -= loss;
        *change = -loss;
    }
}

void game_leave_shop(Game *game) {
    if (game->current_floor >= MAX_FLOORS) {
        game->victory = true;
        game->game_over = true;
    } else {
        game_advance_floor(game, game->current_floor + 1);
    }
}

/* ==========================================================================
 * ITEM USAGE
 * ========================================================================== */

static bool player_has_item(Player *p, ItemType item) {
    for (int i = 0; i < p->inventory_count; i++) {
        if (p->inventory[i] == item) return true;
    }
    return false;
}

static bool player_use_item(Player *p, ItemType item) {
    for (int i = 0; i < p->inventory_count; i++) {
        if (p->inventory[i] == item) {
            for (int j = i; j < p->inventory_count - 1; j++) {
                p->inventory[j] = p->inventory[j + 1];
            }
            p->inventory_count--;
            return true;
        }
    }
    return false;
}

bool game_use_compass(Game *game) {
    if (player_use_item(&game->player, ITEM_COMPASS)) {
        game->player.compass_active = true;
        return true;
    }
    return false;
}

bool game_use_loaded_dice(Game *game, int value) {
    if (value < 1 || value > 6) return false;
    if (player_use_item(&game->player, ITEM_LOADED_DICE)) {
        game->player.loaded_dice_value = value;
        return true;
    }
    return false;
}

bool game_use_parity_flip(Game *game) {
    if (player_use_item(&game->player, ITEM_PARITY_FLIP)) {
        game->player.parity_flipped = true;
        return true;
    }
    return false;
}

int game_apply_lucky_charm(Game *game, int roll, int adjust) {
    if (adjust != 1 && adjust != -1) return roll;
    if (!player_use_item(&game->player, ITEM_LUCKY_CHARM)) return roll;

    int new_roll = roll + adjust;
    if (new_roll < 1) new_roll = 1;
    if (new_roll > 6) new_roll = 6;
    return new_roll;
}
