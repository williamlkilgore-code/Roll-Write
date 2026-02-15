/*
 * Roll-Write PRNG - Windows GUI Version
 * Standalone executable with graphical interface
 * Compiled with MinGW for Windows 10/11 + Wine/Proton compatibility
 */

#define UNICODE
#define _UNICODE

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma comment(lib, "comctl32.lib")

#define VERSION L"1.2.0"

/* Window dimensions */
#define WIN_WIDTH  500
#define WIN_HEIGHT 450

/* Control IDs */
#define ID_OUTPUT       101
#define ID_DICE_INPUT   102
#define ID_ROLL_BTN     103
#define ID_MIN_INPUT    104
#define ID_MAX_INPUT    105
#define ID_COUNT_INPUT  106
#define ID_NUMBER_BTN   107
#define ID_CARDS_INPUT  108
#define ID_CARDS_BTN    109
#define ID_COINS_INPUT  110
#define ID_COINS_BTN    111
#define ID_CLEAR_BTN    112
#define ID_ALGO_COMBO   113
#define ID_PASSWORD_BTN 114
#define ID_UUID_BTN     115

/* ============================================================
 * PRNG Algorithms (same as console version)
 * ============================================================ */

typedef struct {
    unsigned long long s0;
    unsigned long long s1;
} Xorshift128;

void xorshift_seed(Xorshift128 *rng, unsigned long long seed) {
    rng->s0 = seed;
    rng->s1 = seed ^ 0x6C3F2A1B9E7D5C4AULL;
    int i;
    for (i = 0; i < 20; i++) {
        unsigned long long s1 = rng->s0;
        unsigned long long s0 = rng->s1;
        rng->s0 = s0;
        s1 ^= s1 << 23;
        rng->s1 = s1 ^ s0 ^ (s1 >> 17) ^ (s0 >> 26);
    }
}

unsigned long long xorshift_next(Xorshift128 *rng) {
    unsigned long long s1 = rng->s0;
    unsigned long long s0 = rng->s1;
    rng->s0 = s0;
    s1 ^= s1 << 23;
    rng->s1 = s1 ^ s0 ^ (s1 >> 17) ^ (s0 >> 26);
    return rng->s1 + s0;
}

typedef struct {
    unsigned long long state;
    unsigned long long inc;
} PCG32;

void pcg_seed(PCG32 *rng, unsigned long long seed) {
    rng->state = 0;
    rng->inc = (seed << 1) | 1;
    rng->state = rng->state * 6364136223846793005ULL + rng->inc;
    rng->state += seed;
    rng->state = rng->state * 6364136223846793005ULL + rng->inc;
}

unsigned int pcg_next(PCG32 *rng) {
    unsigned long long old = rng->state;
    rng->state = old * 6364136223846793005ULL + rng->inc;
    unsigned int xorshifted = (unsigned int)(((old >> 18) ^ old) >> 27);
    unsigned int rot = (unsigned int)(old >> 59);
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

typedef struct {
    unsigned long long state;
} LCG;

void lcg_seed(LCG *rng, unsigned long long seed) {
    rng->state = seed;
}

unsigned int lcg_next(LCG *rng) {
    rng->state = rng->state * 6364136223846793005ULL + 1442695040888963407ULL;
    return (unsigned int)(rng->state >> 32);
}

typedef enum { ALG_XORSHIFT, ALG_PCG, ALG_LCG } Algorithm;

typedef struct {
    Algorithm alg;
    Xorshift128 xor_state;
    PCG32       pcg_state;
    LCG         lcg_state;
} PRNG;

void prng_init(PRNG *rng, Algorithm alg, unsigned long long seed) {
    rng->alg = alg;
    switch (alg) {
        case ALG_XORSHIFT: xorshift_seed(&rng->xor_state, seed); break;
        case ALG_PCG:      pcg_seed(&rng->pcg_state, seed);      break;
        case ALG_LCG:      lcg_seed(&rng->lcg_state, seed);      break;
    }
}

unsigned int prng_next(PRNG *rng) {
    switch (rng->alg) {
        case ALG_XORSHIFT: return (unsigned int)xorshift_next(&rng->xor_state);
        case ALG_PCG:      return pcg_next(&rng->pcg_state);
        case ALG_LCG:      return lcg_next(&rng->lcg_state);
    }
    return 0;
}

int prng_intn(PRNG *rng, int n) {
    if (n <= 0) return 0;
    return (int)(prng_next(rng) % (unsigned int)n);
}

/* ============================================================
 * Global state
 * ============================================================ */

PRNG g_rng;
HWND g_hOutput;
HWND g_hDiceInput, g_hMinInput, g_hMaxInput, g_hCountInput;
HWND g_hCardsInput, g_hCoinsInput;
HWND g_hAlgoCombo;

/* ============================================================
 * Output helpers
 * ============================================================ */

void AppendOutput(const wchar_t *text) {
    int len = GetWindowTextLengthW(g_hOutput);
    SendMessageW(g_hOutput, EM_SETSEL, len, len);
    SendMessageW(g_hOutput, EM_REPLACESEL, FALSE, (LPARAM)text);
}

void AppendOutputA(const char *text) {
    wchar_t wbuf[1024];
    MultiByteToWideChar(CP_UTF8, 0, text, -1, wbuf, 1024);
    AppendOutput(wbuf);
}

void ClearOutput(void) {
    SetWindowTextW(g_hOutput, L"");
}

/* ============================================================
 * PRNG Features
 * ============================================================ */

int parse_dice(const char *notation, int *num_dice, int *sides, int *modifier, int *drop_lowest) {
    const char *p = notation;
    char buf[32];
    int idx = 0;

    *num_dice = 0; *sides = 0; *modifier = 0; *drop_lowest = 0;

    while (*p >= '0' && *p <= '9') buf[idx++] = *p++;
    buf[idx] = '\0';
    *num_dice = idx > 0 ? atoi(buf) : 1;

    if (*p != 'd' && *p != 'D') return -1;
    p++;

    idx = 0;
    while (*p >= '0' && *p <= '9') buf[idx++] = *p++;
    buf[idx] = '\0';
    if (idx == 0) return -1;
    *sides = atoi(buf);

    if ((*p == 'd' || *p == 'D') && (*(p+1) == 'l' || *(p+1) == 'L')) {
        p += 2;
        idx = 0;
        while (*p >= '0' && *p <= '9') buf[idx++] = *p++;
        buf[idx] = '\0';
        *drop_lowest = idx > 0 ? atoi(buf) : 1;
    }

    if (*p == '+' || *p == '-') {
        int sign = (*p == '+') ? 1 : -1;
        p++;
        idx = 0;
        while (*p >= '0' && *p <= '9') buf[idx++] = *p++;
        buf[idx] = '\0';
        *modifier = atoi(buf) * sign;
    }

    return 0;
}

void DoRollDice(void) {
    char notation[64];
    wchar_t wbuf[64];
    char outbuf[256];
    int nd, sd, mod, dl;
    int *rolls, i, j, total = 0, tmp;

    GetWindowTextW(g_hDiceInput, wbuf, 64);
    WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, notation, 64, NULL, NULL);

    if (parse_dice(notation, &nd, &sd, &mod, &dl) != 0) {
        AppendOutput(L"Invalid dice notation. Use format like 2d6, 1d20+5, 4d6dl1\r\n");
        return;
    }

    rolls = (int *)malloc(nd * sizeof(int));
    for (i = 0; i < nd; i++) {
        rolls[i] = prng_intn(&g_rng, sd) + 1;
    }

    sprintf(outbuf, "Rolling %dd%d: [", nd, sd);
    for (i = 0; i < nd; i++) {
        if (i > 0) strcat(outbuf, " ");
        char num[16];
        sprintf(num, "%d", rolls[i]);
        strcat(outbuf, num);
    }
    strcat(outbuf, "]");

    if (dl > 0 && dl < nd) {
        for (i = 0; i < nd - 1; i++) {
            for (j = 0; j < nd - i - 1; j++) {
                if (rolls[j] > rolls[j+1]) {
                    tmp = rolls[j]; rolls[j] = rolls[j+1]; rolls[j+1] = tmp;
                }
            }
        }
        char dlbuf[32];
        sprintf(dlbuf, " (dropped %d lowest)", dl);
        strcat(outbuf, dlbuf);
        for (i = dl; i < nd; i++) total += rolls[i];
    } else {
        for (i = 0; i < nd; i++) total += rolls[i];
    }

    total += mod;

    if (mod > 0) {
        char modbuf[32];
        sprintf(modbuf, " + %d", mod);
        strcat(outbuf, modbuf);
    } else if (mod < 0) {
        char modbuf[32];
        sprintf(modbuf, " - %d", -mod);
        strcat(outbuf, modbuf);
    }

    char totbuf[32];
    sprintf(totbuf, " = %d\r\n", total);
    strcat(outbuf, totbuf);

    AppendOutputA(outbuf);
    free(rolls);
}

void DoRandomNumbers(void) {
    wchar_t wbuf[32];
    int lo, hi, count, i;
    char outbuf[64];

    GetWindowTextW(g_hMinInput, wbuf, 32);
    lo = _wtoi(wbuf);
    GetWindowTextW(g_hMaxInput, wbuf, 32);
    hi = _wtoi(wbuf);
    GetWindowTextW(g_hCountInput, wbuf, 32);
    count = _wtoi(wbuf);

    if (count < 1) count = 1;
    if (count > 100) count = 100;
    if (lo > hi) { int t = lo; lo = hi; hi = t; }

    int range = hi - lo + 1;
    AppendOutput(L"Random numbers:\r\n");
    for (i = 0; i < count; i++) {
        sprintf(outbuf, "  %d\r\n", lo + prng_intn(&g_rng, range));
        AppendOutputA(outbuf);
    }
}

static const char *suits[] = {"Hearts", "Diamonds", "Clubs", "Spades"};
static const char *ranks[] = {"A","2","3","4","5","6","7","8","9","10","J","Q","K"};

void DoDrawCards(void) {
    wchar_t wbuf[32];
    int n, deck[52], i, j, tmp;
    char outbuf[64];

    GetWindowTextW(g_hCardsInput, wbuf, 32);
    n = _wtoi(wbuf);
    if (n < 1) n = 1;
    if (n > 52) n = 52;

    for (i = 0; i < 52; i++) deck[i] = i;
    for (i = 51; i > 0; i--) {
        j = prng_intn(&g_rng, i + 1);
        tmp = deck[i]; deck[i] = deck[j]; deck[j] = tmp;
    }

    AppendOutput(L"Cards drawn:\r\n");
    for (i = 0; i < n; i++) {
        sprintf(outbuf, "  %s of %s\r\n", ranks[deck[i] % 13], suits[deck[i] / 13]);
        AppendOutputA(outbuf);
    }
}

void DoFlipCoins(void) {
    wchar_t wbuf[32];
    int n, i, heads = 0, tails = 0;
    char outbuf[256];

    GetWindowTextW(g_hCoinsInput, wbuf, 32);
    n = _wtoi(wbuf);
    if (n < 1) n = 1;
    if (n > 100) n = 100;

    strcpy(outbuf, "Flips: ");
    for (i = 0; i < n; i++) {
        if (prng_intn(&g_rng, 2) == 0) {
            strcat(outbuf, "H ");
            heads++;
        } else {
            strcat(outbuf, "T ");
            tails++;
        }
    }
    strcat(outbuf, "\r\n");
    AppendOutputA(outbuf);

    sprintf(outbuf, "Heads: %d, Tails: %d\r\n", heads, tails);
    AppendOutputA(outbuf);
}

static const char pw_chars[] =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789"
    "!@#$%^&*";

void DoPassword(void) {
    char pw[32];
    int i, nchars = (int)strlen(pw_chars);
    for (i = 0; i < 16; i++) {
        pw[i] = pw_chars[prng_intn(&g_rng, nchars)];
    }
    pw[16] = '\0';

    char outbuf[64];
    sprintf(outbuf, "Password: %s\r\n", pw);
    AppendOutputA(outbuf);
}

void DoUUID(void) {
    char outbuf[64];
    sprintf(outbuf, "UUID: %08x-%04x-4%03x-%04x-%08x%04x\r\n",
        prng_next(&g_rng),
        prng_next(&g_rng) & 0xFFFF,
        prng_next(&g_rng) & 0x0FFF,
        (prng_next(&g_rng) & 0x3FFF) | 0x8000,
        prng_next(&g_rng),
        prng_next(&g_rng) & 0xFFFF);
    AppendOutputA(outbuf);
}

void OnAlgoChange(void) {
    int sel = (int)SendMessageW(g_hAlgoCombo, CB_GETCURSEL, 0, 0);
    Algorithm alg = ALG_XORSHIFT;
    switch (sel) {
        case 0: alg = ALG_XORSHIFT; break;
        case 1: alg = ALG_PCG; break;
        case 2: alg = ALG_LCG; break;
    }
    prng_init(&g_rng, alg, (unsigned long long)time(NULL) ^ GetTickCount());
}

/* ============================================================
 * Window Procedure
 * ============================================================ */

HWND CreateLabel(HWND parent, const wchar_t *text, int x, int y, int w, int h) {
    return CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE,
        x, y, w, h, parent, NULL, NULL, NULL);
}

HWND CreateEdit(HWND parent, const wchar_t *text, int x, int y, int w, int h, int id) {
    return CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text,
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
}

HWND CreateBtn(HWND parent, const wchar_t *text, int x, int y, int w, int h, int id) {
    return CreateWindowW(L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        x, y, w, h, parent, (HMENU)(INT_PTR)id, NULL, NULL);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        int y = 10;

        /* Output area */
        g_hOutput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            10, y, WIN_WIDTH - 30, 120, hwnd, (HMENU)ID_OUTPUT, NULL, NULL);
        y += 130;

        /* Algorithm selector */
        CreateLabel(hwnd, L"Algorithm:", 10, y + 3, 70, 20);
        g_hAlgoCombo = CreateWindowW(L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            85, y, 120, 100, hwnd, (HMENU)ID_ALGO_COMBO, NULL, NULL);
        SendMessageW(g_hAlgoCombo, CB_ADDSTRING, 0, (LPARAM)L"Xorshift128+");
        SendMessageW(g_hAlgoCombo, CB_ADDSTRING, 0, (LPARAM)L"PCG32");
        SendMessageW(g_hAlgoCombo, CB_ADDSTRING, 0, (LPARAM)L"LCG");
        SendMessageW(g_hAlgoCombo, CB_SETCURSEL, 0, 0);

        CreateBtn(hwnd, L"Clear", WIN_WIDTH - 90, y, 70, 25, ID_CLEAR_BTN);
        y += 35;

        /* Dice rolling */
        CreateLabel(hwnd, L"Dice:", 10, y + 3, 40, 20);
        g_hDiceInput = CreateEdit(hwnd, L"2d6", 55, y, 80, 24, ID_DICE_INPUT);
        CreateBtn(hwnd, L"Roll", 145, y, 60, 25, ID_ROLL_BTN);
        y += 35;

        /* Random numbers */
        CreateLabel(hwnd, L"Min:", 10, y + 3, 30, 20);
        g_hMinInput = CreateEdit(hwnd, L"1", 45, y, 50, 24, ID_MIN_INPUT);
        CreateLabel(hwnd, L"Max:", 105, y + 3, 30, 20);
        g_hMaxInput = CreateEdit(hwnd, L"100", 140, y, 50, 24, ID_MAX_INPUT);
        CreateLabel(hwnd, L"Count:", 200, y + 3, 45, 20);
        g_hCountInput = CreateEdit(hwnd, L"1", 250, y, 40, 24, ID_COUNT_INPUT);
        CreateBtn(hwnd, L"Generate", 300, y, 80, 25, ID_NUMBER_BTN);
        y += 35;

        /* Cards */
        CreateLabel(hwnd, L"Cards:", 10, y + 3, 45, 20);
        g_hCardsInput = CreateEdit(hwnd, L"5", 60, y, 40, 24, ID_CARDS_INPUT);
        CreateBtn(hwnd, L"Draw", 110, y, 60, 25, ID_CARDS_BTN);

        /* Coins */
        CreateLabel(hwnd, L"Coins:", 180, y + 3, 45, 20);
        g_hCoinsInput = CreateEdit(hwnd, L"10", 230, y, 40, 24, ID_COINS_INPUT);
        CreateBtn(hwnd, L"Flip", 280, y, 60, 25, ID_COINS_BTN);
        y += 35;

        /* Password & UUID */
        CreateBtn(hwnd, L"Password", 10, y, 90, 28, ID_PASSWORD_BTN);
        CreateBtn(hwnd, L"UUID", 110, y, 70, 28, ID_UUID_BTN);

        /* Welcome message */
        AppendOutput(L"Roll-Write PRNG v");
        AppendOutput(VERSION);
        AppendOutput(L"\r\nReady. Select an action above.\r\n");

        return 0;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_ROLL_BTN:     DoRollDice(); break;
        case ID_NUMBER_BTN:   DoRandomNumbers(); break;
        case ID_CARDS_BTN:    DoDrawCards(); break;
        case ID_COINS_BTN:    DoFlipCoins(); break;
        case ID_PASSWORD_BTN: DoPassword(); break;
        case ID_UUID_BTN:     DoUUID(); break;
        case ID_CLEAR_BTN:    ClearOutput(); break;
        case ID_ALGO_COMBO:
            if (HIWORD(wParam) == CBN_SELCHANGE) OnAlgoChange();
            break;
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

/* ============================================================
 * WinMain
 * ============================================================ */

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSW wc = {0};
    HWND hwnd;
    MSG msg;

    (void)hPrevInstance;
    (void)lpCmdLine;

    /* Initialize PRNG */
    prng_init(&g_rng, ALG_XORSHIFT, (unsigned long long)time(NULL) ^ GetTickCount());

    /* Register window class */
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wc.lpszClassName = L"RollWriteClass";
    RegisterClassW(&wc);

    /* Create window */
    hwnd = CreateWindowExW(0, L"RollWriteClass", L"Roll-Write PRNG",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, WIN_WIDTH, WIN_HEIGHT,
        NULL, NULL, hInstance, NULL);

    if (!hwnd) {
        MessageBoxW(NULL, L"Failed to create window", L"Error", MB_ICONERROR);
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    /* Message loop */
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return (int)msg.wParam;
}
