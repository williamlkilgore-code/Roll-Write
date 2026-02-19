# Procedural Dungeon Crawler

A dice-based roguelike dungeon crawler where movement is determined by die rolls. Navigate through 100 procedurally generated floors, collect treasure, avoid enemies, and reach the stairs on each level.

## Download

Download the latest Windows executable from the releases page. No installation required - just run the `.exe` file.

- `DungeonCrawler.exe` - 32-bit version
- `DungeonCrawler-x64.exe` - 64-bit version

## How to Play

### Objective
Descend through all 100 floors by reaching the stairs on each level. Manage your HP and collect coins to buy helpful items at shops.

### Movement System
Movement is determined by rolling a d6:
- **Even rolls (2, 4, 6)** = Orthogonal movement (North, South, East, West)
- **Odd rolls (1, 3, 5)** = Diagonal movement (NE, NW, SE, SW)

You must move the exact number of steps shown on the die. You cannot backtrack over tiles you've already visited on the same turn.

### Controls

| Key | Action |
|-----|--------|
| Space | Roll die |
| W / Up Arrow | Move North |
| S / Down Arrow | Move South |
| A / Left Arrow | Move West |
| D / Right Arrow | Move East |
| Q | Move Northwest |
| E | Move Northeast |
| Z | Move Southwest |
| C | Move Southeast |

### Tile Legend

| Icon | Description |
|------|-------------|
| Green Diamond | Player (you) |
| Pink Triangle | Stairs (goal) |
| Yellow Circle | Coin (+1 gold) |
| Brown Box | Chest (roll for gold) |
| Pink Heart | Heart (heals HP) |
| Red X | Enemy (damages you) |
| Gray Web | Spider Web (lose gold, halves next roll) |
| Yellow Key Shape | Key (opens locked doors) |
| Brown Rectangle | Locked Door (requires key) |
| Blue Circles | Portal (teleports to linked portal) |
| Green Square | Start position |
| Dark Squares | Walls |

### Shops
Shops appear every 7-10 floors. You can:
- **Buy items** using collected coins
- **Gamble** - bet on high (5-6) or low (1-2) to win/lose coins

### Items

| Item | Cost | Effect |
|------|------|--------|
| Compass of True North | 8 | Ignore parity rules for one turn |
| Anchor Stone | 5 | Stop movement early |
| Smoke Bomb | 10 | Negate enemy damage once |
| Loaded Dice | 15 | Choose your next roll |
| Lucky Charm | 6 | Adjust roll by +1 or -1 |
| Lockpick | 7 | Open one door without a key |
| Parity Flip | 6 | Swap odd/even movement rules |

### Difficulty Levels

| Difficulty | Enemy Damage |
|------------|--------------|
| Easy | 2 HP |
| Normal | 4 HP |
| Hard | 8 HP |
| Demonic | 10 HP |

### Save/Load
- Click **Save** at the start of any floor to save your progress
- Use **Load** from the start dialog to resume a saved game

## Building from Source

### Requirements
- MinGW-w64 cross-compiler (for building on Linux)
- Or Visual Studio / MinGW on Windows

### Build Commands (Linux)

```bash
# 32-bit build
i686-w64-mingw32-gcc -static -O2 -mwindows \
    -o DungeonCrawler.exe game.c gui.c \
    -lgdi32 -luser32 -lcomctl32 -lm

# 64-bit build
x86_64-w64-mingw32-gcc -static -O2 -mwindows \
    -o DungeonCrawler-x64.exe game.c gui.c \
    -lgdi32 -luser32 -lcomctl32 -lm
```

## Technical Details

- **Language**: C with Win32 API
- **Graphics**: GDI vector graphics (no external assets)
- **PRNG**: Xorshift64 algorithm for deterministic generation
- **Seed System**: Same seed always generates the same dungeon layout

## Credits

Port of the Python dungeon crawler from the PRNG repository to a standalone Windows executable.

## License

MIT License
