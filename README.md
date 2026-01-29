# Roll-Write PRNG

A standalone pseudo-random number generator for Windows 10/11 that requires **no additional software installation**.

## Downloads

- `rollwrite-windows-amd64.exe` - For 64-bit Windows (most modern systems)
- `rollwrite-windows-386.exe` - For 32-bit Windows (older systems)

Simply download and run from Command Prompt or PowerShell. No installation needed.

## Features

- **Multiple PRNG Algorithms**: Xorshift128+, PCG32, Mersenne Twister, LCG, MINSTD
- **Dice Rolling**: Full dice notation support (2d6, 1d20+5, 4d6dl1)
- **Card Drawing**: Draw cards from a shuffled deck
- **Coin Flipping**: Flip multiple coins
- **Random Numbers**: Generate numbers in any range
- **Passwords**: Generate secure random passwords
- **UUIDs**: Generate UUID-like strings
- **Shuffling**: Shuffle any comma-separated list
- **Interactive Mode**: User-friendly command prompt

## Usage

### Basic Random Numbers
```
rollwrite.exe -n 5              # Generate 5 numbers (1-100)
rollwrite.exe -min 1 -max 6     # Generate number from 1-6
rollwrite.exe -n 10 -min 0 -max 1  # Generate 10 binary values
```

### Dice Rolling
```
rollwrite.exe -dice 2d6         # Roll 2 six-sided dice
rollwrite.exe -dice 1d20+5      # Roll d20 and add 5
rollwrite.exe -dice 4d6dl1      # Roll 4d6, drop lowest (D&D stats)
rollwrite.exe -dice 3d8-2       # Roll 3d8 and subtract 2
```

### Card Drawing
```
rollwrite.exe -cards 5          # Draw 5 cards from shuffled deck
rollwrite.exe -cards 7          # Draw a poker hand
```

### Coin Flipping
```
rollwrite.exe -coins 10         # Flip 10 coins
```

### Password Generation
```
rollwrite.exe -password 1               # Generate 1 password (16 chars)
rollwrite.exe -password 5 -passlen 24   # Generate 5 passwords (24 chars each)
```

### UUID Generation
```
rollwrite.exe -uuid 3           # Generate 3 UUID-like strings
```

### List Shuffling
```
rollwrite.exe -shuffle "Alice,Bob,Charlie,David"
```

### Interactive Mode
```
rollwrite.exe -i
```

Commands in interactive mode:
- `roll 2d6` - Roll dice
- `number 1 100` - Random number in range
- `flip 5` - Flip 5 coins
- `card 3` - Draw 3 cards
- `password 20` - Generate 20-character password
- `uuid` - Generate UUID
- `algo pcg` - Switch to PCG algorithm
- `seed 12345` - Set seed for reproducibility
- `help` - Show all commands
- `quit` - Exit

## Algorithm Selection

Use `-algo` to select a PRNG algorithm:

| Algorithm | Flag | Description |
|-----------|------|-------------|
| Xorshift128+ | `-algo xorshift` | Fast, high-quality (default) |
| PCG32 | `-algo pcg` | Excellent statistical properties |
| Mersenne Twister | `-algo mt` | Classic, period 2^19937-1 |
| LCG | `-algo lcg` | Simple linear congruential |
| MINSTD | `-algo minstd` | Park-Miller minimal standard |

## Reproducibility

Use `-seed` for reproducible results:
```
rollwrite.exe -seed 12345 -n 5   # Same seed = same results
```

## Examples

```
# D&D character stats (4d6 drop lowest, 6 times)
rollwrite.exe -dice 4d6dl1 -n 6

# Poker hand
rollwrite.exe -cards 5

# Random team order
rollwrite.exe -shuffle "Team A,Team B,Team C,Team D"

# Lottery numbers (6 numbers from 1-49)
rollwrite.exe -min 1 -max 49 -n 6

# Generate secure passwords
rollwrite.exe -password 10 -passlen 20
```

## Building from Source

Requires Go 1.21+:
```bash
# Windows 64-bit
GOOS=windows GOARCH=amd64 go build -ldflags="-s -w" -o rollwrite-windows-amd64.exe .

# Windows 32-bit
GOOS=windows GOARCH=386 go build -ldflags="-s -w" -o rollwrite-windows-386.exe .
```

## License

MIT License
