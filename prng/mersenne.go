package prng

// MersenneTwister implements the MT19937 algorithm
// This is a widely-used PRNG with a period of 2^19937-1
type MersenneTwister struct {
	mt    [624]uint32
	index int
}

// NewMersenneTwister creates a new Mersenne Twister generator with the given seed
func NewMersenneTwister(seed uint64) *MersenneTwister {
	m := &MersenneTwister{}
	m.Seed(seed)
	return m
}

// Seed initializes the generator with a seed value
func (m *MersenneTwister) Seed(seed uint64) {
	m.mt[0] = uint32(seed)
	for i := 1; i < 624; i++ {
		m.mt[i] = 1812433253*(m.mt[i-1]^(m.mt[i-1]>>30)) + uint32(i)
	}
	m.index = 624
}

// generateNumbers generates the next 624 values
func (m *MersenneTwister) generateNumbers() {
	for i := 0; i < 624; i++ {
		y := (m.mt[i] & 0x80000000) | (m.mt[(i+1)%624] & 0x7fffffff)
		m.mt[i] = m.mt[(i+397)%624] ^ (y >> 1)
		if y%2 != 0 {
			m.mt[i] ^= 0x9908b0df
		}
	}
}

// Uint32 returns the next random uint32
func (m *MersenneTwister) Uint32() uint32 {
	if m.index >= 624 {
		m.generateNumbers()
		m.index = 0
	}

	y := m.mt[m.index]
	m.index++

	// Tempering
	y ^= y >> 11
	y ^= (y << 7) & 0x9d2c5680
	y ^= (y << 15) & 0xefc60000
	y ^= y >> 18

	return y
}

// Uint64 returns the next random uint64 (combines two uint32s)
func (m *MersenneTwister) Uint64() uint64 {
	return (uint64(m.Uint32()) << 32) | uint64(m.Uint32())
}

// Intn returns a random integer in [0, n)
func (m *MersenneTwister) Intn(n int) int {
	if n <= 0 {
		return 0
	}
	return int(m.Uint32() % uint32(n))
}

// Float64 returns a random float64 in [0.0, 1.0)
func (m *MersenneTwister) Float64() float64 {
	return float64(m.Uint32()) / (1 << 32)
}

// Name returns the algorithm name
func (m *MersenneTwister) Name() string {
	return "Mersenne Twister (MT19937)"
}
