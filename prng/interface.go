package prng

// PRNG defines the interface that all random number generators must implement
type PRNG interface {
	// Seed initializes the generator with a seed value
	Seed(seed uint64)

	// Uint32 returns the next random uint32
	Uint32() uint32

	// Uint64 returns the next random uint64
	Uint64() uint64

	// Intn returns a random integer in [0, n)
	Intn(n int) int

	// Float64 returns a random float64 in [0.0, 1.0)
	Float64() float64

	// Name returns the algorithm name
	Name() string
}

// Algorithm represents a PRNG algorithm type
type Algorithm string

const (
	AlgoXorshift128Plus Algorithm = "xorshift"
	AlgoPCG32           Algorithm = "pcg"
	AlgoLCG             Algorithm = "lcg"
	AlgoMINSTD          Algorithm = "minstd"
	AlgoMersenneTwister Algorithm = "mt"
)

// NewPRNG creates a new PRNG instance based on the algorithm name
func NewPRNG(algo Algorithm, seed uint64) PRNG {
	switch algo {
	case AlgoXorshift128Plus:
		return NewXorshift128Plus(seed)
	case AlgoPCG32:
		return NewPCG32(seed)
	case AlgoLCG:
		return NewLCG(seed)
	case AlgoMINSTD:
		return NewMINSTD(seed)
	case AlgoMersenneTwister:
		return NewMersenneTwister(seed)
	default:
		return NewXorshift128Plus(seed)
	}
}

// AvailableAlgorithms returns a list of all available algorithm names
func AvailableAlgorithms() []Algorithm {
	return []Algorithm{
		AlgoXorshift128Plus,
		AlgoPCG32,
		AlgoLCG,
		AlgoMINSTD,
		AlgoMersenneTwister,
	}
}
