package prng

// LCG implements a Linear Congruential Generator
// Uses the same parameters as glibc's rand()
type LCG struct {
	state uint64
}

// NewLCG creates a new LCG generator with the given seed
func NewLCG(seed uint64) *LCG {
	return &LCG{state: seed}
}

// Seed initializes the generator with a seed value
func (l *LCG) Seed(seed uint64) {
	l.state = seed
}

// Uint32 returns the next random uint32
func (l *LCG) Uint32() uint32 {
	// Parameters from Numerical Recipes
	l.state = l.state*6364136223846793005 + 1442695040888963407
	return uint32(l.state >> 32)
}

// Uint64 returns the next random uint64 (combines two uint32s)
func (l *LCG) Uint64() uint64 {
	return (uint64(l.Uint32()) << 32) | uint64(l.Uint32())
}

// Intn returns a random integer in [0, n)
func (l *LCG) Intn(n int) int {
	if n <= 0 {
		return 0
	}
	return int(l.Uint32() % uint32(n))
}

// Float64 returns a random float64 in [0.0, 1.0)
func (l *LCG) Float64() float64 {
	return float64(l.Uint32()) / (1 << 32)
}

// Name returns the algorithm name
func (l *LCG) Name() string {
	return "LCG"
}

// MINSTD implements the Minimal Standard generator (Park-Miller)
type MINSTD struct {
	state uint64
}

// NewMINSTD creates a new MINSTD generator with the given seed
func NewMINSTD(seed uint64) *MINSTD {
	m := &MINSTD{}
	m.Seed(seed)
	return m
}

// Seed initializes the generator with a seed value
func (m *MINSTD) Seed(seed uint64) {
	if seed == 0 {
		seed = 1
	}
	m.state = seed % 2147483647
}

// Uint32 returns the next random uint32
func (m *MINSTD) Uint32() uint32 {
	// Park-Miller parameters: a=48271, m=2^31-1
	m.state = (m.state * 48271) % 2147483647
	return uint32(m.state)
}

// Uint64 returns the next random uint64 (combines two uint32s)
func (m *MINSTD) Uint64() uint64 {
	return (uint64(m.Uint32()) << 32) | uint64(m.Uint32())
}

// Intn returns a random integer in [0, n)
func (m *MINSTD) Intn(n int) int {
	if n <= 0 {
		return 0
	}
	return int(m.Uint32() % uint32(n))
}

// Float64 returns a random float64 in [0.0, 1.0)
func (m *MINSTD) Float64() float64 {
	return float64(m.Uint32()) / 2147483647.0
}

// Name returns the algorithm name
func (m *MINSTD) Name() string {
	return "MINSTD"
}
