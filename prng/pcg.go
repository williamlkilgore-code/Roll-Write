package prng

// PCG32 implements the PCG (Permuted Congruential Generator) algorithm
// PCG is a family of simple, fast, space-efficient, statistically good
// algorithms for random number generation
type PCG32 struct {
	state uint64
	inc   uint64
}

// NewPCG32 creates a new PCG32 generator with the given seed
func NewPCG32(seed uint64) *PCG32 {
	p := &PCG32{}
	p.Seed(seed)
	return p
}

// Seed initializes the generator with a seed value
func (p *PCG32) Seed(seed uint64) {
	p.state = 0
	p.inc = (seed << 1) | 1 // Must be odd
	p.Uint32()              // Advance once
	p.state += seed
	p.Uint32() // Advance again
}

// Uint32 returns the next random uint32
func (p *PCG32) Uint32() uint32 {
	oldstate := p.state
	p.state = oldstate*6364136223846793005 + p.inc

	xorshifted := uint32(((oldstate >> 18) ^ oldstate) >> 27)
	rot := uint32(oldstate >> 59)
	return (xorshifted >> rot) | (xorshifted << ((-rot) & 31))
}

// Uint64 returns the next random uint64 (combines two uint32s)
func (p *PCG32) Uint64() uint64 {
	return (uint64(p.Uint32()) << 32) | uint64(p.Uint32())
}

// Intn returns a random integer in [0, n)
func (p *PCG32) Intn(n int) int {
	if n <= 0 {
		return 0
	}
	return int(p.Uint32() % uint32(n))
}

// Float64 returns a random float64 in [0.0, 1.0)
func (p *PCG32) Float64() float64 {
	return float64(p.Uint32()) / (1 << 32)
}

// Name returns the algorithm name
func (p *PCG32) Name() string {
	return "PCG32"
}
