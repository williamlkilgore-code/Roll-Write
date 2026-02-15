/*
 * Procedural Dungeon Crawler - Win32 GUI
 * Port from Python/tkinter to C/Win32 for Windows/Wine compatibility
 */

#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0500

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "game.h"

/* ==========================================================================
 * GUI CONSTANTS
 * ========================================================================== */

#define WIN_WIDTH   900
#define WIN_HEIGHT  700
#define CELL_SIZE   32
#define GRID_OFFSET_X  20
#define GRID_OFFSET_Y  60

/* Control IDs */
#define ID_ROLL_BTN     101
#define ID_DIR_N        110
#define ID_DIR_S        111
#define ID_DIR_E        112
#define ID_DIR_W        113
#define ID_DIR_NE       114
#define ID_DIR_NW       115
#define ID_DIR_SE       116
#define ID_DIR_SW       117
#define ID_CLEAR_BTN    120
#define ID_SAVE_BTN     121
#define ID_HELP_BTN     122
#define ID_NEW_BTN      123
#define ID_SHOP_1       130
#define ID_SHOP_2       131
#define ID_SHOP_3       132
#define ID_SHOP_4       133
#define ID_GAMBLE_HIGH  134
#define ID_GAMBLE_LOW   135
#define ID_LEAVE_SHOP   136
#define ID_LOG          140
#define ID_INVENTORY    141

/* Colors */
#define COLOR_BG        RGB(26, 26, 26)
#define COLOR_PANEL     RGB(37, 37, 37)
#define COLOR_WALL      RGB(26, 26, 46)
#define COLOR_FLOOR     RGB(45, 45, 45)
#define COLOR_PLAYER    RGB(0, 255, 0)
#define COLOR_COIN      RGB(255, 215, 0)
#define COLOR_CHEST     RGB(205, 133, 63)
#define COLOR_HEART     RGB(255, 105, 180)
#define COLOR_ENEMY     RGB(255, 68, 68)
#define COLOR_WEB       RGB(170, 170, 170)
#define COLOR_KEY       RGB(255, 215, 0)
#define COLOR_DOOR      RGB(139, 69, 19)
#define COLOR_PORTAL    RGB(0, 191, 255)
#define COLOR_STAIRS    RGB(221, 160, 221)
#define COLOR_START     RGB(144, 238, 144)
#define COLOR_TEXT      RGB(255, 255, 255)
#define COLOR_DIM       RGB(136, 136, 136)
#define COLOR_ACCENT    RGB(74, 158, 255)
#define COLOR_HP_BAR    RGB(255, 68, 68)

/* Tile symbols */
static const wchar_t *TILE_SYMBOLS[] = {
    L"·",  /* EMPTY */
    L"█",  /* WALL */
    L"⌂",  /* START */
    L"▼",  /* STAIRS */
    L"●",  /* COIN */
    L"◆",  /* CHEST */
    L"♥",  /* HEART */
    L"◈",  /* ENEMY */
    L"※",  /* WEB */
    L"⚷",  /* KEY */
    L"▣",  /* LOCKED_DOOR */
    L"◎"   /* PORTAL */
};

/* ==========================================================================
 * GLOBAL STATE
 * ========================================================================== */

static Game *g_game = NULL;
static HWND g_hwnd = NULL;
static HWND g_hLog = NULL;
static HWND g_hInventory = NULL;
static HWND g_hRollBtn = NULL;
static HWND g_dirBtns[8] = {0};
static HWND g_shopBtns[4] = {0};
static HWND g_hGambleHigh = NULL;
static HWND g_hGambleLow = NULL;
static HWND g_hLeaveShop = NULL;

static int g_currentRoll = 0;
static int g_remainingSteps = 0;
static bool g_awaitingDirection = false;
static bool g_awaitingDirectionChange = false;
static Direction g_currentDirection = DIR_N;
static bool g_allowedDirs[DIR_COUNT] = {false};
static bool g_backtrackAllowed = false;

static HFONT g_fontNormal = NULL;
static HFONT g_fontSymbol = NULL;
static HFONT g_fontTitle = NULL;

/* ==========================================================================
 * HELPER FUNCTIONS
 * ========================================================================== */

static void LogMessage(const wchar_t *msg) {
    if (g_hLog) {
        int len = GetWindowTextLengthW(g_hLog);
        SendMessageW(g_hLog, EM_SETSEL, len, len);
        SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)msg);
        SendMessageW(g_hLog, EM_REPLACESEL, FALSE, (LPARAM)L"\r\n");
    }
}

static void LogMessageA(const char *msg) {
    wchar_t wbuf[512];
    MultiByteToWideChar(CP_UTF8, 0, msg, -1, wbuf, 512);
    LogMessage(wbuf);
}

static void ClearLog(void) {
    if (g_hLog) SetWindowTextW(g_hLog, L"");
}

static COLORREF GetTileColor(TileType tile) {
    switch (tile) {
        case TILE_WALL: return COLOR_WALL;
        case TILE_START: return COLOR_START;
        case TILE_STAIRS: return COLOR_STAIRS;
        case TILE_COIN: return COLOR_COIN;
        case TILE_CHEST: return COLOR_CHEST;
        case TILE_HEART: return COLOR_HEART;
        case TILE_ENEMY: return COLOR_ENEMY;
        case TILE_WEB: return COLOR_WEB;
        case TILE_KEY: return COLOR_KEY;
        case TILE_LOCKED_DOOR: return COLOR_DOOR;
        case TILE_PORTAL: return COLOR_PORTAL;
        default: return COLOR_FLOOR;
    }
}

/* ==========================================================================
 * UI UPDATE FUNCTIONS
 * ========================================================================== */

static void UpdateStats(HWND hwnd) {
    InvalidateRect(hwnd, NULL, FALSE);
}

static void UpdateInventory(void) {
    if (!g_hInventory || !g_game) return;

    SendMessageW(g_hInventory, LB_RESETCONTENT, 0, 0);
    for (int i = 0; i < g_game->player.inventory_count; i++) {
        wchar_t buf[64];
        MultiByteToWideChar(CP_UTF8, 0, ITEM_NAMES[g_game->player.inventory[i]], -1, buf, 64);
        SendMessageW(g_hInventory, LB_ADDSTRING, 0, (LPARAM)buf);
    }
}

static void UpdateDirectionButtons(void) {
    if (!g_game) return;

    /* Get legal directions */
    bool legal[DIR_COUNT];
    for (int i = 0; i < DIR_COUNT; i++) {
        legal[i] = movement_is_legal(g_game, (Direction)i, g_allowedDirs, g_backtrackAllowed);
    }

    /* Update button states */
    EnableWindow(g_dirBtns[0], legal[DIR_N]);   /* N */
    EnableWindow(g_dirBtns[1], legal[DIR_S]);   /* S */
    EnableWindow(g_dirBtns[2], legal[DIR_E]);   /* E */
    EnableWindow(g_dirBtns[3], legal[DIR_W]);   /* W */
    EnableWindow(g_dirBtns[4], legal[DIR_NE]);  /* NE */
    EnableWindow(g_dirBtns[5], legal[DIR_NW]);  /* NW */
    EnableWindow(g_dirBtns[6], legal[DIR_SE]);  /* SE */
    EnableWindow(g_dirBtns[7], legal[DIR_SW]);  /* SW */
}

static void DisableAllDirectionButtons(void) {
    for (int i = 0; i < 8; i++) {
        EnableWindow(g_dirBtns[i], FALSE);
    }
}

static void ShowShopUI(bool show) {
    for (int i = 0; i < 4; i++) {
        ShowWindow(g_shopBtns[i], show ? SW_SHOW : SW_HIDE);
    }
    ShowWindow(g_hGambleHigh, show ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hGambleLow, show ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hLeaveShop, show ? SW_SHOW : SW_HIDE);

    EnableWindow(g_hRollBtn, !show);
}

/* ==========================================================================
 * GAME LOGIC HANDLERS
 * ========================================================================== */

static void OnRoll(void) {
    if (!g_game || game_is_shop_floor(g_game)) return;
    if (g_awaitingDirection || g_awaitingDirectionChange) return;

    game_start_turn(g_game);
    g_currentRoll = game_roll_die(g_game);

    int effective = g_currentRoll;
    if (g_game->player.half_next_roll) {
        effective = g_currentRoll / 2;
        g_game->player.half_next_roll = false;
        char buf[64];
        sprintf(buf, "Web effect! Roll %d -> %d", g_currentRoll, effective);
        LogMessageA(buf);
    }

    g_remainingSteps = effective;
    movement_get_allowed_directions(g_game, g_currentRoll, g_allowedDirs);

    char buf[64];
    sprintf(buf, "Rolled %d (%s)", g_currentRoll,
            (g_currentRoll % 2 == 0) ? "orthogonal" : "diagonal");
    LogMessageA(buf);

    /* Check if any legal moves exist */
    bool hasLegal = false;
    for (int i = 0; i < DIR_COUNT; i++) {
        if (movement_is_legal(g_game, (Direction)i, g_allowedDirs, false)) {
            hasLegal = true;
            break;
        }
    }

    if (!hasLegal) {
        LogMessage(L"No legal moves! Turn skipped.");
        DisableAllDirectionButtons();
        return;
    }

    g_awaitingDirection = true;
    g_backtrackAllowed = false;
    UpdateDirectionButtons();
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void ContinueMovement(void);

static void OnDirection(Direction dir) {
    if (!g_awaitingDirection && !g_awaitingDirectionChange) return;

    bool legal = movement_is_legal(g_game, dir, g_allowedDirs, g_backtrackAllowed);
    if (!legal) return;

    g_currentDirection = dir;

    if (g_awaitingDirection) {
        g_awaitingDirection = false;
    } else if (g_awaitingDirectionChange) {
        g_awaitingDirectionChange = false;
        g_backtrackAllowed = false;
    }

    ContinueMovement();
}

static void FinishTurn(void) {
    g_awaitingDirection = false;
    g_awaitingDirectionChange = false;
    g_currentRoll = 0;
    g_remainingSteps = 0;
    DisableAllDirectionButtons();
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void CompleteFloor(void) {
    char buf[64];
    sprintf(buf, "Completed floor %d!", g_game->current_floor);
    LogMessageA(buf);

    if (g_game->current_floor >= MAX_FLOORS) {
        MessageBoxW(g_hwnd, L"You conquered all 100 floors!\nCongratulations!",
                    L"Victory!", MB_ICONINFORMATION);
        g_game->victory = true;
        g_game->game_over = true;
        return;
    }

    game_advance_floor(g_game, g_game->current_floor + 1);
    FinishTurn();

    if (game_is_shop_floor(g_game)) {
        ShowShopUI(true);
        LogMessage(L"Welcome to the shop!");
    } else {
        ShowShopUI(false);
    }

    UpdateInventory();
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void GameOver(void) {
    char buf[128];
    sprintf(buf, "You died on floor %d!\nFinal coins: %d",
            g_game->current_floor, g_game->player.coins);
    wchar_t wbuf[128];
    MultiByteToWideChar(CP_UTF8, 0, buf, -1, wbuf, 128);
    MessageBoxW(g_hwnd, wbuf, L"Game Over", MB_ICONINFORMATION);
    g_game->game_over = true;
}

static void ContinueMovement(void) {
    if (g_remainingSteps <= 0) {
        FinishTurn();
        return;
    }

    MoveOutcome outcome = movement_execute_step(g_game, g_currentDirection);

    /* Log effects */
    char buf[256] = "";
    if (outcome.smoke_bomb_used) strcat(buf, "Smoke Bomb blocked enemy! ");
    if (outcome.coins_gained) { char t[32]; sprintf(t, "+%d coins ", outcome.coins_gained); strcat(buf, t); }
    if (outcome.coins_lost) { char t[32]; sprintf(t, "-%d coins ", outcome.coins_lost); strcat(buf, t); }
    if (outcome.hp_healed) { char t[32]; sprintf(t, "+%d HP ", outcome.hp_healed); strcat(buf, t); }
    if (outcome.damage_taken) { char t[32]; sprintf(t, "-%d HP ", outcome.damage_taken); strcat(buf, t); }
    if (outcome.keys_gained) strcat(buf, "+1 key ");
    if (outcome.keys_used) strcat(buf, "used 1 key ");
    if (outcome.portal_teleported) strcat(buf, "Teleported! ");

    if (strlen(buf) > 0) LogMessageA(buf);

    UpdateInventory();
    InvalidateRect(g_hwnd, NULL, FALSE);

    /* Handle results */
    if (outcome.result == MOVE_PLAYER_DIED) {
        GameOver();
        return;
    }

    if (outcome.result == MOVE_REACHED_STAIRS) {
        CompleteFloor();
        return;
    }

    if (outcome.result == MOVE_WEB_STOPPED) {
        LogMessage(L"Caught in web! Turn ends.");
        FinishTurn();
        return;
    }

    if (outcome.result == MOVE_BLOCKED) {
        LogMessage(L"Hit a wall!");
        movement_get_allowed_directions(g_game, g_currentRoll, g_allowedDirs);

        /* Check for legal directions (no backtrack) */
        bool hasLegal = false;
        for (int i = 0; i < DIR_COUNT; i++) {
            if (i == dir_reverse(g_currentDirection)) continue;
            if (movement_is_legal(g_game, (Direction)i, g_allowedDirs, false)) {
                hasLegal = true;
                break;
            }
        }

        if (!hasLegal) {
            /* Allow backtracking as last resort */
            g_backtrackAllowed = true;
            for (int i = 0; i < DIR_COUNT; i++) {
                if (movement_is_legal(g_game, (Direction)i, g_allowedDirs, true)) {
                    hasLegal = true;
                    break;
                }
            }
            if (!hasLegal) {
                LogMessage(L"No valid moves. Turn ends.");
                FinishTurn();
                return;
            }
            LogMessage(L"Trapped! Backtracking allowed.");
        }

        UpdateDirectionButtons();
        g_awaitingDirectionChange = true;
        return;
    }

    /* Successful step */
    g_remainingSteps--;

    if (g_remainingSteps > 0) {
        /* Continue movement after short delay */
        Sleep(100);
        ContinueMovement();
    } else {
        FinishTurn();
    }
}

/* ==========================================================================
 * SHOP HANDLERS
 * ========================================================================== */

static void OnShopBuy(int index) {
    if (!g_game || !g_game->current_shop) return;

    if (game_buy_item(g_game, index)) {
        char buf[64];
        sprintf(buf, "Bought %s!", ITEM_NAMES[g_game->current_shop->inventory[index]]);
        LogMessageA(buf);
        UpdateInventory();
        InvalidateRect(g_hwnd, NULL, FALSE);
    } else {
        LogMessage(L"Can't afford or already purchased!");
    }
}

static void OnGamble(bool high) {
    if (!g_game || !game_is_shop_floor(g_game)) return;

    bool won;
    int change;
    game_gamble(g_game, high, &won, &change);

    char buf[64];
    if (won) {
        sprintf(buf, "Won %d coins!", change);
    } else {
        sprintf(buf, "Lost %d coins!", -change);
    }
    LogMessageA(buf);
    InvalidateRect(g_hwnd, NULL, FALSE);
}

static void OnLeaveShop(void) {
    if (!g_game || !game_is_shop_floor(g_game)) return;

    game_leave_shop(g_game);
    ShowShopUI(false);

    if (game_is_shop_floor(g_game)) {
        ShowShopUI(true);
        LogMessage(L"Welcome to the shop!");
    } else {
        char buf[64];
        sprintf(buf, "Now on floor %d", g_game->current_floor);
        LogMessageA(buf);
    }

    UpdateInventory();
    InvalidateRect(g_hwnd, NULL, FALSE);
}

/* ==========================================================================
 * START GAME DIALOG
 * ========================================================================== */

static INT_PTR CALLBACK StartDlgProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    static uint64_t *pSeed;
    static Difficulty *pDiff;

    switch (msg) {
        case WM_INITDIALOG: {
            LONG_PTR *params = (LONG_PTR *)lParam;
            pSeed = (uint64_t *)params[0];
            pDiff = (Difficulty *)params[1];

            SetDlgItemTextW(hdlg, 101, L"12345");
            CheckRadioButton(hdlg, 103, 106, 104);  /* Normal selected */
            return TRUE;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDOK: {
                    wchar_t buf[32];
                    GetDlgItemTextW(hdlg, 101, buf, 32);

                    /* Parse seed */
                    *pSeed = 0;
                    for (int i = 0; buf[i]; i++) {
                        if (buf[i] >= '0' && buf[i] <= '9') {
                            *pSeed = *pSeed * 10 + (buf[i] - '0');
                        } else {
                            /* Hash non-numeric seeds */
                            char mbuf[32];
                            WideCharToMultiByte(CP_UTF8, 0, buf, -1, mbuf, 32, NULL, NULL);
                            *pSeed = hash_seed(0, mbuf, 0);
                            break;
                        }
                    }
                    if (*pSeed == 0) *pSeed = 12345;

                    /* Get difficulty */
                    if (IsDlgButtonChecked(hdlg, 103)) *pDiff = DIFF_EASY;
                    else if (IsDlgButtonChecked(hdlg, 104)) *pDiff = DIFF_NORMAL;
                    else if (IsDlgButtonChecked(hdlg, 105)) *pDiff = DIFF_HARD;
                    else if (IsDlgButtonChecked(hdlg, 106)) *pDiff = DIFF_DEMONIC;
                    else *pDiff = DIFF_NORMAL;

                    EndDialog(hdlg, IDOK);
                    return TRUE;
                }
                case IDCANCEL:
                    EndDialog(hdlg, IDCANCEL);
                    return TRUE;
            }
            break;

        case WM_CLOSE:
            EndDialog(hdlg, IDCANCEL);
            return TRUE;
    }
    return FALSE;
}

static bool ShowStartDialog(HWND parent, uint64_t *seed, Difficulty *diff) {
    /* Create dialog template in memory */
    WORD *p;
    DLGTEMPLATE *dlg;
    BYTE buf[1024];

    memset(buf, 0, sizeof(buf));
    dlg = (DLGTEMPLATE *)buf;
    dlg->style = DS_MODALFRAME | DS_CENTER | WS_POPUP | WS_CAPTION | WS_SYSMENU;
    dlg->cdit = 9;  /* Number of controls */
    dlg->x = 0; dlg->y = 0;
    dlg->cx = 150; dlg->cy = 120;

    p = (WORD *)(dlg + 1);
    *p++ = 0; *p++ = 0;  /* Menu, class */

    /* Title */
    wcscpy((wchar_t *)p, L"Dungeon Crawler");
    p += wcslen(L"Dungeon Crawler") + 1;

    /* Align to DWORD */
    if ((ULONG_PTR)p & 2) p++;

    /* Seed label */
    DLGITEMTEMPLATE *item = (DLGITEMTEMPLATE *)p;
    item->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
    item->x = 10; item->y = 10; item->cx = 30; item->cy = 10;
    item->id = 100;
    p = (WORD *)(item + 1);
    *p++ = 0xFFFF; *p++ = 0x0082;  /* Static */
    wcscpy((wchar_t *)p, L"Seed:");
    p += wcslen(L"Seed:") + 1;
    *p++ = 0;
    if ((ULONG_PTR)p & 2) p++;

    /* Seed edit */
    item = (DLGITEMTEMPLATE *)p;
    item->style = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL;
    item->x = 45; item->y = 8; item->cx = 95; item->cy = 12;
    item->id = 101;
    p = (WORD *)(item + 1);
    *p++ = 0xFFFF; *p++ = 0x0081;  /* Edit */
    *p++ = 0; *p++ = 0;
    if ((ULONG_PTR)p & 2) p++;

    /* Difficulty label */
    item = (DLGITEMTEMPLATE *)p;
    item->style = WS_CHILD | WS_VISIBLE | SS_LEFT;
    item->x = 10; item->y = 28; item->cx = 50; item->cy = 10;
    item->id = 102;
    p = (WORD *)(item + 1);
    *p++ = 0xFFFF; *p++ = 0x0082;
    wcscpy((wchar_t *)p, L"Difficulty:");
    p += wcslen(L"Difficulty:") + 1;
    *p++ = 0;
    if ((ULONG_PTR)p & 2) p++;

    /* Radio buttons */
    const wchar_t *diffs[] = {L"Easy", L"Normal", L"Hard", L"Demonic"};
    for (int i = 0; i < 4; i++) {
        item = (DLGITEMTEMPLATE *)p;
        item->style = WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | (i == 0 ? WS_GROUP : 0);
        item->x = 10 + i * 35; item->y = 40; item->cx = 33; item->cy = 10;
        item->id = 103 + i;
        p = (WORD *)(item + 1);
        *p++ = 0xFFFF; *p++ = 0x0080;  /* Button */
        wcscpy((wchar_t *)p, diffs[i]);
        p += wcslen(diffs[i]) + 1;
        *p++ = 0;
        if ((ULONG_PTR)p & 2) p++;
    }

    /* OK button */
    item = (DLGITEMTEMPLATE *)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON;
    item->x = 25; item->y = 95; item->cx = 45; item->cy = 14;
    item->id = IDOK;
    p = (WORD *)(item + 1);
    *p++ = 0xFFFF; *p++ = 0x0080;
    wcscpy((wchar_t *)p, L"Start");
    p += wcslen(L"Start") + 1;
    *p++ = 0;
    if ((ULONG_PTR)p & 2) p++;

    /* Cancel button */
    item = (DLGITEMTEMPLATE *)p;
    item->style = WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON;
    item->x = 80; item->y = 95; item->cx = 45; item->cy = 14;
    item->id = IDCANCEL;
    p = (WORD *)(item + 1);
    *p++ = 0xFFFF; *p++ = 0x0080;
    wcscpy((wchar_t *)p, L"Cancel");
    p += wcslen(L"Cancel") + 1;
    *p++ = 0;

    LONG_PTR params[2] = {(LONG_PTR)seed, (LONG_PTR)diff};
    INT_PTR result = DialogBoxIndirectParamW(NULL, dlg, parent, StartDlgProc, (LPARAM)params);

    return result == IDOK;
}

/* ==========================================================================
 * PAINT FUNCTIONS
 * ========================================================================== */

static void PaintGrid(HDC hdc, int offsetX, int offsetY) {
    if (!g_game || !g_game->current_level) return;

    Level *level = g_game->current_level;

    SelectObject(hdc, g_fontSymbol);
    SetBkMode(hdc, TRANSPARENT);

    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            int px = offsetX + x * CELL_SIZE;
            int py = offsetY + y * CELL_SIZE;

            GridCell *cell = &level->grid[y][x];
            COLORREF bgColor = (cell->tile == TILE_WALL) ? COLOR_WALL : COLOR_FLOOR;
            COLORREF fgColor = GetTileColor(cell->tile);

            /* Draw cell background */
            HBRUSH bgBrush = CreateSolidBrush(bgColor);
            RECT rect = {px, py, px + CELL_SIZE, py + CELL_SIZE};
            FillRect(hdc, &rect, bgBrush);
            DeleteObject(bgBrush);

            /* Draw border */
            HPEN pen = CreatePen(PS_SOLID, 1, RGB(51, 51, 51));
            HPEN oldPen = SelectObject(hdc, pen);
            MoveToEx(hdc, px, py, NULL);
            LineTo(hdc, px + CELL_SIZE, py);
            LineTo(hdc, px + CELL_SIZE, py + CELL_SIZE);
            SelectObject(hdc, oldPen);
            DeleteObject(pen);

            /* Draw symbol or player */
            const wchar_t *symbol;
            if (x == g_game->player.x && y == g_game->player.y) {
                symbol = L"@";
                fgColor = COLOR_PLAYER;

                /* Player highlight */
                HBRUSH hlBrush = CreateSolidBrush(RGB(0, 68, 0));
                RECT hlRect = {px + 2, py + 2, px + CELL_SIZE - 2, py + CELL_SIZE - 2};
                FillRect(hdc, &hlRect, hlBrush);
                DeleteObject(hlBrush);
            } else {
                symbol = TILE_SYMBOLS[cell->tile];
            }

            SetTextColor(hdc, fgColor);
            DrawTextW(hdc, symbol, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
    }
}

static void PaintShop(HDC hdc, int offsetX, int offsetY) {
    if (!g_game || !g_game->current_shop) return;

    Shop *shop = g_game->current_shop;

    SelectObject(hdc, g_fontTitle);
    SetTextColor(hdc, COLOR_ACCENT);
    SetBkMode(hdc, TRANSPARENT);

    wchar_t buf[128];
    swprintf(buf, 128, L"SHOP - Floor %d", g_game->current_floor);

    RECT rect = {offsetX, offsetY, offsetX + 400, offsetY + 40};
    DrawTextW(hdc, buf, -1, &rect, DT_CENTER);

    swprintf(buf, 128, L"Your coins: %d", g_game->player.coins);
    SetTextColor(hdc, COLOR_COIN);
    rect.top += 35;
    rect.bottom += 35;
    DrawTextW(hdc, buf, -1, &rect, DT_CENTER);

    SelectObject(hdc, g_fontNormal);
    int y = offsetY + 90;

    for (int i = 0; i < 4; i++) {
        ItemType item = shop->inventory[i];
        int price = ITEM_COSTS[item];
        bool purchased = shop->purchased[i];
        bool canAfford = g_game->player.coins >= price;

        if (purchased) {
            swprintf(buf, 128, L"%d. %S - SOLD", i + 1, ITEM_NAMES[item]);
            SetTextColor(hdc, COLOR_DIM);
        } else if (canAfford) {
            swprintf(buf, 128, L"%d. %S - %d coins", i + 1, ITEM_NAMES[item], price);
            SetTextColor(hdc, COLOR_TEXT);
        } else {
            swprintf(buf, 128, L"%d. %S - %d coins", i + 1, ITEM_NAMES[item], price);
            SetTextColor(hdc, COLOR_DIM);
        }

        RECT itemRect = {offsetX, y, offsetX + 400, y + 25};
        DrawTextW(hdc, buf, -1, &itemRect, DT_CENTER);
        y += 30;
    }
}

static void PaintStats(HDC hdc, int x, int y) {
    if (!g_game) return;

    SelectObject(hdc, g_fontTitle);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, COLOR_ACCENT);

    wchar_t buf[64];
    swprintf(buf, 64, L"Floor: %d/%d", g_game->current_floor, MAX_FLOORS);
    TextOutW(hdc, x, y, buf, (int)wcslen(buf));

    SelectObject(hdc, g_fontNormal);
    y += 30;

    /* HP */
    SetTextColor(hdc, COLOR_TEXT);
    swprintf(buf, 64, L"HP: %d/%d", g_game->player.hp, g_game->player.max_hp);
    TextOutW(hdc, x, y, buf, (int)wcslen(buf));
    y += 20;

    /* HP Bar */
    RECT hpBg = {x, y, x + 150, y + 15};
    HBRUSH bgBrush = CreateSolidBrush(RGB(74, 26, 26));
    FillRect(hdc, &hpBg, bgBrush);
    DeleteObject(bgBrush);

    int hpWidth = (150 * g_game->player.hp) / g_game->player.max_hp;
    RECT hpFg = {x, y, x + hpWidth, y + 15};
    HBRUSH fgBrush = CreateSolidBrush(COLOR_HP_BAR);
    FillRect(hdc, &hpFg, fgBrush);
    DeleteObject(fgBrush);
    y += 25;

    /* Coins */
    SetTextColor(hdc, COLOR_COIN);
    swprintf(buf, 64, L"Coins: %d", g_game->player.coins);
    TextOutW(hdc, x, y, buf, (int)wcslen(buf));
    y += 20;

    /* Keys */
    SetTextColor(hdc, COLOR_KEY);
    swprintf(buf, 64, L"Keys: %d", g_game->player.keys);
    TextOutW(hdc, x, y, buf, (int)wcslen(buf));
    y += 20;

    /* Roll */
    SetTextColor(hdc, COLOR_TEXT);
    if (g_currentRoll > 0) {
        swprintf(buf, 64, L"Roll: %d (%d left)", g_currentRoll, g_remainingSteps);
    } else {
        swprintf(buf, 64, L"Roll: -");
    }
    TextOutW(hdc, x, y, buf, (int)wcslen(buf));
}

/* ==========================================================================
 * WINDOW PROCEDURE
 * ========================================================================== */

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hwnd = hwnd;

            /* Create fonts */
            g_fontNormal = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");

            g_fontSymbol = CreateFontW(18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI Symbol");

            g_fontTitle = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH, L"Segoe UI");

            int rightX = GRID_OFFSET_X + GRID_WIDTH * CELL_SIZE + 30;

            /* Roll button */
            g_hRollBtn = CreateWindowW(L"BUTTON", L"Roll Die",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                rightX, 200, 120, 35, hwnd, (HMENU)ID_ROLL_BTN, NULL, NULL);

            /* Direction buttons */
            int dirY = 250;
            g_dirBtns[5] = CreateWindowW(L"BUTTON", L"NW", WS_CHILD | WS_VISIBLE | WS_DISABLED,
                rightX, dirY, 35, 30, hwnd, (HMENU)ID_DIR_NW, NULL, NULL);
            g_dirBtns[0] = CreateWindowW(L"BUTTON", L"N", WS_CHILD | WS_VISIBLE | WS_DISABLED,
                rightX + 40, dirY, 35, 30, hwnd, (HMENU)ID_DIR_N, NULL, NULL);
            g_dirBtns[4] = CreateWindowW(L"BUTTON", L"NE", WS_CHILD | WS_VISIBLE | WS_DISABLED,
                rightX + 80, dirY, 35, 30, hwnd, (HMENU)ID_DIR_NE, NULL, NULL);

            g_dirBtns[3] = CreateWindowW(L"BUTTON", L"W", WS_CHILD | WS_VISIBLE | WS_DISABLED,
                rightX, dirY + 35, 35, 30, hwnd, (HMENU)ID_DIR_W, NULL, NULL);
            g_dirBtns[2] = CreateWindowW(L"BUTTON", L"E", WS_CHILD | WS_VISIBLE | WS_DISABLED,
                rightX + 80, dirY + 35, 35, 30, hwnd, (HMENU)ID_DIR_E, NULL, NULL);

            g_dirBtns[7] = CreateWindowW(L"BUTTON", L"SW", WS_CHILD | WS_VISIBLE | WS_DISABLED,
                rightX, dirY + 70, 35, 30, hwnd, (HMENU)ID_DIR_SW, NULL, NULL);
            g_dirBtns[1] = CreateWindowW(L"BUTTON", L"S", WS_CHILD | WS_VISIBLE | WS_DISABLED,
                rightX + 40, dirY + 70, 35, 30, hwnd, (HMENU)ID_DIR_S, NULL, NULL);
            g_dirBtns[6] = CreateWindowW(L"BUTTON", L"SE", WS_CHILD | WS_VISIBLE | WS_DISABLED,
                rightX + 80, dirY + 70, 35, 30, hwnd, (HMENU)ID_DIR_SE, NULL, NULL);

            /* Inventory label and listbox */
            CreateWindowW(L"STATIC", L"Inventory:",
                WS_CHILD | WS_VISIBLE,
                rightX, 370, 100, 20, hwnd, NULL, NULL, NULL);

            g_hInventory = CreateWindowW(L"LISTBOX", L"",
                WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL | LBS_NOTIFY,
                rightX, 390, 150, 80, hwnd, (HMENU)ID_INVENTORY, NULL, NULL);

            /* Log */
            CreateWindowW(L"STATIC", L"Log:",
                WS_CHILD | WS_VISIBLE,
                rightX, 480, 100, 20, hwnd, NULL, NULL, NULL);

            g_hLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                rightX, 500, 200, 120, hwnd, (HMENU)ID_LOG, NULL, NULL);

            /* Shop buttons (hidden by default) */
            int shopY = 250;
            for (int i = 0; i < 4; i++) {
                wchar_t lbl[16];
                swprintf(lbl, 16, L"Buy %d", i + 1);
                g_shopBtns[i] = CreateWindowW(L"BUTTON", lbl,
                    WS_CHILD | BS_PUSHBUTTON,
                    200 + i * 60, shopY, 55, 25, hwnd, (HMENU)(ID_SHOP_1 + i), NULL, NULL);
            }

            g_hGambleHigh = CreateWindowW(L"BUTTON", L"Gamble High",
                WS_CHILD | BS_PUSHBUTTON,
                200, shopY + 40, 100, 25, hwnd, (HMENU)ID_GAMBLE_HIGH, NULL, NULL);

            g_hGambleLow = CreateWindowW(L"BUTTON", L"Gamble Low",
                WS_CHILD | BS_PUSHBUTTON,
                310, shopY + 40, 100, 25, hwnd, (HMENU)ID_GAMBLE_LOW, NULL, NULL);

            g_hLeaveShop = CreateWindowW(L"BUTTON", L"Leave Shop",
                WS_CHILD | BS_PUSHBUTTON,
                250, shopY + 80, 100, 30, hwnd, (HMENU)ID_LEAVE_SHOP, NULL, NULL);

            /* Start game */
            uint64_t seed = 12345;
            Difficulty diff = DIFF_NORMAL;

            if (ShowStartDialog(hwnd, &seed, &diff)) {
                g_game = game_create(seed, diff);
                game_start(g_game);

                char buf[64];
                sprintf(buf, "Game started! Seed: %llu", (unsigned long long)seed);
                LogMessageA(buf);
                sprintf(buf, "Shop floors: %d, %d, %d...",
                        g_game->shop_floors[0], g_game->shop_floors[1], g_game->shop_floors[2]);
                LogMessageA(buf);

                ShowShopUI(false);
                UpdateInventory();
            } else {
                PostQuitMessage(0);
            }

            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);

            /* Double buffering */
            RECT clientRect;
            GetClientRect(hwnd, &clientRect);
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBM = CreateCompatibleBitmap(hdc, clientRect.right, clientRect.bottom);
            SelectObject(memDC, memBM);

            /* Clear background */
            HBRUSH bgBrush = CreateSolidBrush(COLOR_BG);
            FillRect(memDC, &clientRect, bgBrush);
            DeleteObject(bgBrush);

            /* Draw title */
            SelectObject(memDC, g_fontTitle);
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, COLOR_ACCENT);
            TextOutW(memDC, GRID_OFFSET_X, 15, L"Procedural Dungeon Crawler", 26);

            /* Draw stats */
            PaintStats(memDC, GRID_OFFSET_X + GRID_WIDTH * CELL_SIZE + 30, 60);

            /* Draw grid or shop */
            if (g_game) {
                if (game_is_shop_floor(g_game)) {
                    PaintShop(memDC, GRID_OFFSET_X, GRID_OFFSET_Y);
                } else {
                    PaintGrid(memDC, GRID_OFFSET_X, GRID_OFFSET_Y);
                }
            }

            /* Blit to screen */
            BitBlt(hdc, 0, 0, clientRect.right, clientRect.bottom, memDC, 0, 0, SRCCOPY);

            DeleteObject(memBM);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case ID_ROLL_BTN: OnRoll(); break;
                case ID_DIR_N:  OnDirection(DIR_N); break;
                case ID_DIR_S:  OnDirection(DIR_S); break;
                case ID_DIR_E:  OnDirection(DIR_E); break;
                case ID_DIR_W:  OnDirection(DIR_W); break;
                case ID_DIR_NE: OnDirection(DIR_NE); break;
                case ID_DIR_NW: OnDirection(DIR_NW); break;
                case ID_DIR_SE: OnDirection(DIR_SE); break;
                case ID_DIR_SW: OnDirection(DIR_SW); break;
                case ID_SHOP_1: OnShopBuy(0); break;
                case ID_SHOP_2: OnShopBuy(1); break;
                case ID_SHOP_3: OnShopBuy(2); break;
                case ID_SHOP_4: OnShopBuy(3); break;
                case ID_GAMBLE_HIGH: OnGamble(true); break;
                case ID_GAMBLE_LOW: OnGamble(false); break;
                case ID_LEAVE_SHOP: OnLeaveShop(); break;
            }
            return 0;

        case WM_KEYDOWN:
            /* Keyboard shortcuts */
            if (g_awaitingDirection || g_awaitingDirectionChange) {
                switch (wParam) {
                    case 'W': case VK_UP:    OnDirection(DIR_N); break;
                    case 'S': case VK_DOWN:  OnDirection(DIR_S); break;
                    case 'D': case VK_RIGHT: OnDirection(DIR_E); break;
                    case 'A': case VK_LEFT:  OnDirection(DIR_W); break;
                    case 'E': OnDirection(DIR_NE); break;
                    case 'Q': OnDirection(DIR_NW); break;
                    case 'C': OnDirection(DIR_SE); break;
                    case 'Z': OnDirection(DIR_SW); break;
                }
            } else if (wParam == VK_SPACE || wParam == VK_RETURN) {
                OnRoll();
            }
            return 0;

        case WM_DESTROY:
            if (g_fontNormal) DeleteObject(g_fontNormal);
            if (g_fontSymbol) DeleteObject(g_fontSymbol);
            if (g_fontTitle) DeleteObject(g_fontTitle);
            if (g_game) game_free(g_game);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* ==========================================================================
 * WINMAIN ENTRY POINT
 * ========================================================================== */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"DungeonCrawlerClass";
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, L"DungeonCrawlerClass",
        L"Procedural Dungeon Crawler",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, WIN_WIDTH, WIN_HEIGHT,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) return 1;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
