package main

import (
	"bufio"
	"flag"
	"fmt"
	"os"
	"strconv"
	"strings"
	"time"

	"rollwrite/prng"
)

const version = "1.0.0"

var (
	// Command line flags
	algorithm   string
	seed        uint64
	count       int
	min         int
	max         int
	dice        string
	cards       int
	shuffle     string
	coins       int
	uuid        int
	password    int
	passLen     int
	interactive bool
	showHelp    bool
	showVersion bool
)

func init() {
	flag.StringVar(&algorithm, "algo", "xorshift", "PRNG algorithm: xorshift, pcg, lcg, minstd, mt")
	flag.Uint64Var(&seed, "seed", 0, "Seed value (0 = use current time)")
	flag.IntVar(&count, "n", 1, "Number of random values to generate")
	flag.IntVar(&min, "min", 1, "Minimum value for range")
	flag.IntVar(&max, "max", 100, "Maximum value for range")
	flag.StringVar(&dice, "dice", "", "Roll dice (e.g., 2d6, 1d20+5, 4d6dl1)")
	flag.IntVar(&cards, "cards", 0, "Draw N cards from a deck")
	flag.StringVar(&shuffle, "shuffle", "", "Shuffle a comma-separated list")
	flag.IntVar(&coins, "coins", 0, "Flip N coins")
	flag.IntVar(&uuid, "uuid", 0, "Generate N UUID-like strings")
	flag.IntVar(&password, "password", 0, "Generate N random passwords")
	flag.IntVar(&passLen, "passlen", 16, "Password length")
	flag.BoolVar(&interactive, "i", false, "Interactive mode")
	flag.BoolVar(&showHelp, "help", false, "Show help")
	flag.BoolVar(&showVersion, "version", false, "Show version")
}

func main() {
	flag.Parse()

	if showVersion {
		fmt.Printf("Roll-Write PRNG v%s\n", version)
		fmt.Println("A standalone pseudo-random number generator for Windows")
		return
	}

	if showHelp {
		printHelp()
		return
	}

	// Initialize seed
	if seed == 0 {
		seed = uint64(time.Now().UnixNano())
	}

	// Create the PRNG
	rng := prng.NewPRNG(prng.Algorithm(algorithm), seed)

	// Handle different modes
	if interactive {
		runInteractive(rng)
		return
	}

	if dice != "" {
		rollDice(rng, dice, count)
		return
	}

	if cards > 0 {
		drawCards(rng, cards)
		return
	}

	if shuffle != "" {
		shuffleList(rng, shuffle)
		return
	}

	if coins > 0 {
		flipCoins(rng, coins)
		return
	}

	if uuid > 0 {
		generateUUIDs(rng, uuid)
		return
	}

	if password > 0 {
		generatePasswords(rng, password, passLen)
		return
	}

	// Default: generate random numbers in range
	generateNumbers(rng, count, min, max)
}

func printHelp() {
	fmt.Println(`Roll-Write PRNG - Standalone Random Number Generator
====================================================

Usage: rollwrite [options]

Options:
  -algo string    PRNG algorithm (default "xorshift")
                  Available: xorshift, pcg, lcg, minstd, mt
  -seed uint      Seed value (0 = use current time)
  -n int          Number of values to generate (default 1)
  -min int        Minimum value for range (default 1)
  -max int        Maximum value for range (default 100)

Special Modes:
  -dice string    Roll dice (e.g., "2d6", "1d20+5", "4d6dl1")
  -cards int      Draw N cards from a shuffled deck
  -shuffle string Shuffle a comma-separated list
  -coins int      Flip N coins
  -uuid int       Generate N UUID-like strings
  -password int   Generate N random passwords
  -passlen int    Password length (default 16)
  -i              Interactive mode

Information:
  -help           Show this help
  -version        Show version

Examples:
  rollwrite -n 5                    Generate 5 random numbers (1-100)
  rollwrite -min 1 -max 6 -n 3      Roll 3 six-sided dice
  rollwrite -dice 2d6+3             Roll 2d6 and add 3
  rollwrite -dice 4d6dl1            Roll 4d6 and drop lowest
  rollwrite -cards 5                Draw 5 cards
  rollwrite -shuffle "a,b,c,d,e"    Shuffle a list
  rollwrite -coins 10               Flip 10 coins
  rollwrite -password 3 -passlen 20 Generate 3 passwords of length 20
  rollwrite -algo pcg -seed 12345   Use PCG algorithm with specific seed
  rollwrite -i                      Enter interactive mode

Algorithms:
  xorshift  - Xorshift128+ (fast, high quality, default)
  pcg       - PCG32 (excellent statistical properties)
  mt        - Mersenne Twister (classic, period 2^19937-1)
  lcg       - Linear Congruential Generator (simple)
  minstd    - Minimal Standard (Park-Miller)`)
}

func generateNumbers(rng prng.PRNG, count, min, max int) {
	if min > max {
		min, max = max, min
	}
	rangeSize := max - min + 1

	for i := 0; i < count; i++ {
		value := min + rng.Intn(rangeSize)
		if count == 1 {
			fmt.Println(value)
		} else {
			fmt.Printf("%d\n", value)
		}
	}
}

func rollDice(rng prng.PRNG, notation string, times int) {
	for t := 0; t < times; t++ {
		numDice, sides, modifier, dropLowest, err := parseDiceNotation(notation)
		if err != nil {
			fmt.Println("Error:", err)
			return
		}

		rolls := make([]int, numDice)
		for i := 0; i < numDice; i++ {
			rolls[i] = rng.Intn(sides) + 1
		}

		// Handle drop lowest
		droppedRolls := rolls
		if dropLowest > 0 && dropLowest < numDice {
			// Sort and drop lowest
			sorted := make([]int, len(rolls))
			copy(sorted, rolls)
			// Simple bubble sort for small arrays
			for i := 0; i < len(sorted)-1; i++ {
				for j := 0; j < len(sorted)-i-1; j++ {
					if sorted[j] > sorted[j+1] {
						sorted[j], sorted[j+1] = sorted[j+1], sorted[j]
					}
				}
			}
			droppedRolls = sorted[dropLowest:]
		}

		total := 0
		for _, r := range droppedRolls {
			total += r
		}
		total += modifier

		if times == 1 {
			if dropLowest > 0 {
				fmt.Printf("Rolls: %v (dropped %d lowest) + %d = %d\n", rolls, dropLowest, modifier, total)
			} else if modifier != 0 {
				fmt.Printf("Rolls: %v + %d = %d\n", rolls, modifier, total)
			} else {
				fmt.Printf("Rolls: %v = %d\n", rolls, total)
			}
		} else {
			fmt.Printf("Roll %d: %v = %d\n", t+1, rolls, total)
		}
	}
}

func parseDiceNotation(notation string) (numDice, sides, modifier, dropLowest int, err error) {
	notation = strings.ToLower(strings.TrimSpace(notation))

	// Check for drop lowest modifier (e.g., "dl1")
	if idx := strings.Index(notation, "dl"); idx != -1 {
		dlPart := notation[idx+2:]
		notation = notation[:idx]
		dropLowest, _ = strconv.Atoi(dlPart)
		if dropLowest == 0 {
			dropLowest = 1
		}
	}

	// Check for +/- modifier
	var modSign int = 1
	var modPart string
	if idx := strings.LastIndex(notation, "+"); idx != -1 {
		modPart = notation[idx+1:]
		notation = notation[:idx]
		modSign = 1
	} else if idx := strings.LastIndex(notation, "-"); idx != -1 {
		modPart = notation[idx+1:]
		notation = notation[:idx]
		modSign = -1
	}
	if modPart != "" {
		modifier, _ = strconv.Atoi(modPart)
		modifier *= modSign
	}

	// Parse NdS format
	parts := strings.Split(notation, "d")
	if len(parts) != 2 {
		return 0, 0, 0, 0, fmt.Errorf("invalid dice notation: %s (use format like 2d6)", notation)
	}

	if parts[0] == "" {
		numDice = 1
	} else {
		numDice, err = strconv.Atoi(parts[0])
		if err != nil || numDice < 1 {
			return 0, 0, 0, 0, fmt.Errorf("invalid number of dice: %s", parts[0])
		}
	}

	sides, err = strconv.Atoi(parts[1])
	if err != nil || sides < 1 {
		return 0, 0, 0, 0, fmt.Errorf("invalid number of sides: %s", parts[1])
	}

	return numDice, sides, modifier, dropLowest, nil
}

var cardSuits = []string{"Hearts", "Diamonds", "Clubs", "Spades"}
var cardRanks = []string{"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"}

func drawCards(rng prng.PRNG, n int) {
	// Create a deck
	deck := make([]string, 52)
	idx := 0
	for _, suit := range cardSuits {
		for _, rank := range cardRanks {
			deck[idx] = fmt.Sprintf("%s of %s", rank, suit)
			idx++
		}
	}

	// Shuffle the deck
	for i := len(deck) - 1; i > 0; i-- {
		j := rng.Intn(i + 1)
		deck[i], deck[j] = deck[j], deck[i]
	}

	// Draw cards
	if n > 52 {
		n = 52
	}
	fmt.Println("Cards drawn:")
	for i := 0; i < n; i++ {
		fmt.Printf("  %s\n", deck[i])
	}
}

func shuffleList(rng prng.PRNG, list string) {
	items := strings.Split(list, ",")
	for i := range items {
		items[i] = strings.TrimSpace(items[i])
	}

	// Fisher-Yates shuffle
	for i := len(items) - 1; i > 0; i-- {
		j := rng.Intn(i + 1)
		items[i], items[j] = items[j], items[i]
	}

	fmt.Println("Shuffled order:")
	for i, item := range items {
		fmt.Printf("  %d. %s\n", i+1, item)
	}
}

func flipCoins(rng prng.PRNG, n int) {
	heads := 0
	tails := 0
	results := make([]string, n)

	for i := 0; i < n; i++ {
		if rng.Intn(2) == 0 {
			results[i] = "H"
			heads++
		} else {
			results[i] = "T"
			tails++
		}
	}

	fmt.Printf("Flips: %s\n", strings.Join(results, " "))
	fmt.Printf("Heads: %d, Tails: %d\n", heads, tails)
}

func generateUUIDs(rng prng.PRNG, n int) {
	for i := 0; i < n; i++ {
		// Generate UUID v4 format
		uuid := fmt.Sprintf("%08x-%04x-4%03x-%04x-%012x",
			rng.Uint32(),
			rng.Uint32()&0xFFFF,
			rng.Uint32()&0x0FFF,
			(rng.Uint32()&0x3FFF)|0x8000,
			rng.Uint64()&0xFFFFFFFFFFFF,
		)
		fmt.Println(uuid)
	}
}

const passwordChars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%^&*()_+-=[]{}|;:,.<>?"

func generatePasswords(rng prng.PRNG, n, length int) {
	for i := 0; i < n; i++ {
		password := make([]byte, length)
		for j := 0; j < length; j++ {
			password[j] = passwordChars[rng.Intn(len(passwordChars))]
		}
		fmt.Println(string(password))
	}
}

func runInteractive(rng prng.PRNG) {
	scanner := bufio.NewScanner(os.Stdin)

	fmt.Println("Roll-Write Interactive Mode")
	fmt.Printf("Using %s algorithm (seed: %d)\n", rng.Name(), seed)
	fmt.Println("Type 'help' for commands, 'quit' to exit")
	fmt.Println()

	for {
		fmt.Print("> ")
		if !scanner.Scan() {
			break
		}

		input := strings.TrimSpace(scanner.Text())
		if input == "" {
			continue
		}

		parts := strings.Fields(input)
		cmd := strings.ToLower(parts[0])

		switch cmd {
		case "quit", "exit", "q":
			fmt.Println("Goodbye!")
			return

		case "help", "h", "?":
			fmt.Println(`Commands:
  roll NdS[+/-M]  - Roll dice (e.g., roll 2d6, roll 1d20+5)
  number [min] [max] - Generate random number in range
  flip [n]        - Flip n coins (default 1)
  card [n]        - Draw n cards (default 1)
  password [len]  - Generate a password
  uuid            - Generate a UUID
  algo [name]     - Switch algorithm
  seed [value]    - Set new seed
  help            - Show this help
  quit            - Exit`)

		case "roll", "r", "dice", "d":
			if len(parts) < 2 {
				fmt.Println("Usage: roll NdS (e.g., roll 2d6)")
				continue
			}
			rollDice(rng, parts[1], 1)

		case "number", "num", "n":
			min, max := 1, 100
			if len(parts) >= 2 {
				min, _ = strconv.Atoi(parts[1])
			}
			if len(parts) >= 3 {
				max, _ = strconv.Atoi(parts[2])
			}
			generateNumbers(rng, 1, min, max)

		case "flip", "coin", "f":
			n := 1
			if len(parts) >= 2 {
				n, _ = strconv.Atoi(parts[1])
			}
			flipCoins(rng, n)

		case "card", "cards", "c":
			n := 1
			if len(parts) >= 2 {
				n, _ = strconv.Atoi(parts[1])
			}
			drawCards(rng, n)

		case "password", "pass", "pw":
			length := 16
			if len(parts) >= 2 {
				length, _ = strconv.Atoi(parts[1])
			}
			generatePasswords(rng, 1, length)

		case "uuid":
			generateUUIDs(rng, 1)

		case "algo", "algorithm":
			if len(parts) < 2 {
				fmt.Printf("Current algorithm: %s\n", rng.Name())
				fmt.Println("Available: xorshift, pcg, lcg, minstd, mt")
				continue
			}
			rng = prng.NewPRNG(prng.Algorithm(parts[1]), seed)
			fmt.Printf("Switched to %s\n", rng.Name())

		case "seed":
			if len(parts) < 2 {
				fmt.Printf("Current seed: %d\n", seed)
				continue
			}
			newSeed, err := strconv.ParseUint(parts[1], 10, 64)
			if err != nil {
				fmt.Println("Invalid seed value")
				continue
			}
			seed = newSeed
			rng.Seed(seed)
			fmt.Printf("Seed set to %d\n", seed)

		default:
			// Try to parse as dice notation
			if strings.Contains(cmd, "d") {
				rollDice(rng, cmd, 1)
			} else {
				fmt.Println("Unknown command. Type 'help' for available commands.")
			}
		}
	}
}
