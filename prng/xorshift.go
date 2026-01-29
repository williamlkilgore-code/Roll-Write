// Package prng provides various pseudo-random number generator implementations
package prng

// Xorshift128Plus implements the xorshift128+ algorithm
// This is a fast, high-quality PRNG with a period of 2^128-1
type Xorshift128Plus struct {
	s0, s1 uint64
}

// NewXorshift128Plus creates a new Xorshift128Plus generator with the given seed
func NewXorshift128Plus(seed uint64) *Xorshift128Plus {
	x := &Xorshift128Plus{}
	x.Seed(seed)
	return x
}

// Seed initializes the generator with a seed value
func (x *Xorshift128Plus) Seed(seed uint64) {
	// Use splitmix64 to initialize the state from a single seed
	x.s0 = splitmix64(&seed)
	x.s1 = splitmix64(&seed)
	// Ensure state is not all zeros
	if x.s0 == 0 && x.s1 == 0 {
		x.s0 = 1
	}
}

// Uint64 returns the next random uint64
func (x *Xorshift128Plus) Uint64() uint64 {
	s1 := x.s0
	s0 := x.s1
	result := s0 + s1
	x.s0 = s0
	s1 ^= s1 << 23
	x.s1 = s1 ^ s0 ^ (s1 >> 18) ^ (s0 >> 5)
	return result
}

// Uint32 returns the next random uint32
func (x *Xorshift128Plus) Uint32() uint32 {
	return uint32(x.Uint64() >> 32)
}

// Intn returns a random integer in [0, n)
func (x *Xorshift128Plus) Intn(n int) int {
	if n <= 0 {
		return 0
	}
	return int(x.Uint64() % uint64(n))
}

// Float64 returns a random float64 in [0.0, 1.0)
func (x *Xorshift128Plus) Float64() float64 {
	return float64(x.Uint64()>>11) / (1 << 53)
}

// Name returns the algorithm name
func (x *Xorshift128Plus) Name() string {
	return "Xorshift128+"
}

// splitmix64 is used to initialize the state from a seed
func splitmix64(state *uint64) uint64 {
	*state += 0x9e3779b97f4a7c15
	z := *state
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9
	z = (z ^ (z >> 27)) * 0x94d049bb133111eb
	return z ^ (z >> 31)
}
