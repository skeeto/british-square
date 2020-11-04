package main

import (
	"bufio"
	"fmt"
	"io"
	"math/bits"
	"os"
)

// State is a game state bitboard encoding the entire game state. No
// methods actually modify the State, rather return an updated State.
type State uint64

// Mask efficiently validate moves, mostly behaving like a State. No
// methods actually modify the Mask, rather return an updated Mask.
type Mask uint64

// Turn returns the 0-indexed turn count.
func (s State) Turn() int {
	return int(s >> 50)
}

// Turn returns the 0-indexed turn count.
func (m Mask) Turn() int {
	return int(m >> 50)
}

// Pass the current turn without placing a piece.
func (s State) Pass() State {
	turn := s.Turn()
	bits := s & 0x3ffffffffffff
	return State(turn+1)<<50 | bits
}

// Pass the current turn without placing a piece.
func (m Mask) Pass() Mask {
	turn := m.Turn()
	bits := m & 0x3ffffffffffff
	return Mask(turn+1)<<50 | bits
}

// Place a piece at a specific position and advance the turn.
func (s State) Place(i int) State {
	turn := s.Turn()
	who := turn % 2
	bits := s & 0x3ffffffffffff
	bit := State(1) << (who*25 + i)
	return State(turn+1)<<50 | bits | bit
}

var masks = [...]Mask{
	0x0000023, 0x0000047, 0x000008e, 0x000011c, 0x0000218,
	0x0000461, 0x00008e2, 0x00011c4, 0x0002388, 0x0004310,
	0x0008c20, 0x0011c40, 0x0023880, 0x0047100, 0x0086200,
	0x0118400, 0x0238800, 0x0471000, 0x08e2000, 0x10c4000,
	0x0308000, 0x0710000, 0x0e20000, 0x1c40000, 0x1880000,
}

// Place a piece at a specific position and advance the turn.
func (m Mask) Place(i int) Mask {
	turn := m.Turn()
	who := turn % 2
	bits := m & 0x3ffffffffffff
	other := masks[i] << ((1 ^ who) * 25)
	self := Mask(1) << (who*25 + i)
	return Mask(turn+1)<<50 | bits | other | self
}

// Derive a validation mask from a game state.
func (s State) Derive() Mask {
	var ns [2]int
	var moves [2][25]int
	for i := 0; i < 25; i++ {
		if s>>i&1 == 1 {
			moves[0][ns[0]] = i
			ns[0]++
		}
		if s>>(i+25)&1 == 1 {
			moves[1][ns[1]] = i
			ns[1]++
		}
	}

	var m Mask
	for i := 0; m.Turn() < s.Turn(); i++ {
		if i/2 < ns[i%2] {
			m = m.Place(moves[i%2][i/2])
		} else {
			m = m.Pass()
		}
	}
	return m
}

// Transpose around the 0-6-12-18-24 diagonal.
func (s State) Transpose() State {
	return ((s >> 16) & 0x00000020000010) |
		((s >> 12) & 0x00000410000208) |
		((s >> 8) & 0x00008208004104) |
		((s >> 4) & 0x00104104082082) |
		((s >> 0) & 0xfe082083041041) |
		((s << 4) & 0x01041040820820) |
		((s << 8) & 0x00820800410400) |
		((s << 12) & 0x00410000208000) |
		((s << 16) & 0x00200000100000)
}

// Flip vertically.
func (s State) Flip() State {
	return ((s >> 20) & 0x0000003e00001f) |
		((s >> 10) & 0x000007c00003e0) |
		((s >> 0) & 0xfc00f800007c00) |
		((s << 10) & 0x001f00000f8000) |
		((s << 20) & 0x03e00001f00000)
}

// Canonicalize to a specific orientation.
func (s State) Canonicalize() State {
	min := func(a, b State) State {
		if a < b {
			return a
		}
		return b
	}
	c := s
	s = s.Transpose()
	c = min(s, c)
	s = s.Flip()
	c = min(s, c)
	s = s.Transpose()
	c = min(s, c)
	s = s.Flip()
	c = min(s, c)
	s = s.Transpose()
	c = min(s, c)
	s = s.Flip()
	c = min(s, c)
	s = s.Transpose()
	c = min(s, c)
	return c
}

// Valid indicates if a move is permitted.
func (m Mask) Valid(i int) bool {
	turn := m.Turn()
	who := turn % 2
	if turn == 0 {
		return i != 12
	}
	return (m >> (who*25 + i) & 1) == 0
}

// NoMoves indicates if the current player has no moves.
func (s State) NoMoves(m Mask) bool {
	turn := s.Turn()
	who := turn % 2
	const M = 0x1ffffff
	return ((uint64(s)>>(who*25) | uint64(m)>>(who*25)) & M) == M
}

// IsComplete indicates if the game has completed (no more moves).
func (s State) IsComplete(m Mask) bool {
	return ((uint64(s)>>25|uint64(m)>>25)&0x1ffffff) == 0x1ffffff &&
		((uint64(s)>>0|uint64(m)>>0)&0x1ffffff) == 0x1ffffff
}

// Print an ANSI-escape represenation of the game state.
func (s State) Print(w io.Writer, m Mask) error {
	buf := bufio.NewWriter(w)
	for y := 0; y < 5; y++ {
		for x := 0; x < 5; x++ {
			i := y*5 + x
			p0 := s >> i & 1
			p1 := s >> (i + 25) & 1
			x0 := m >> (i + 25) & 1
			x1 := m >> i & 1
			c := "∙"
			if p0 == 1 {
				c = "\x1b[94m█\x1b[0m"
			} else if p1 == 1 {
				c = "\x1b[91m█\x1b[0m"
			} else if x0 == 1 && x1 == 1 {
				c = " "
			} else if x0 == 1 {
				c = "\x1b[94m░\x1b[0m"
			} else if x1 == 1 {
				c = "\x1b[91m░\x1b[0m"
			}
			buf.WriteString(c)
		}
		buf.WriteRune('\n')
	}
	buf.WriteRune('\n')
	return buf.Flush()
}

// InitScore returns the initial minimax score for this turn.
func (s State) InitScore() int {
	if s.Turn()%2 == 1 {
		return +25
	}
	return -25
}

// Score computes the final game score.
func (s State) Score() int {
	p0 := bits.OnesCount(uint(s & 0x1ffffff))
	p1 := bits.OnesCount(uint(s >> 25 & 0x1ffffff))
	return p0 - p1
}

// Minimax is a game evaluator storing the explored game tree. It always
// explores to game completion and plays perfectly.
type Minimax map[State]int8

// New returns an empty minimax tree.
func New() Minimax {
	return make(map[State]int8)
}

// Evaluate the minimax score at a game state.
func (t Minimax) Evaluate(s State, m Mask) int {
	s0 := s.Canonicalize()
	score8, ok := t[s0]
	if ok {
		return int(score8)
	}

	if s.IsComplete(m) {
		score := s0.Score()
		t[s0] = int8(score)
		return score
	}

	if s.NoMoves(m) {
		score := t.Evaluate(s.Pass(), m.Pass())
		t[s0] = int8(score)
		return score
	}

	score := s.InitScore()
	for i := 0; i < 5*5; i++ {
		if m.Valid(i) {
			tmp := t.Evaluate(s.Place(i), m.Place(i))
			if s.Turn()%2 == 1 {
				if tmp < score {
					score = tmp // min
				}
			} else {
				if tmp > score {
					score = tmp // max
				}
			}
		}
	}

	t[s0] = int8(score)
	return score
}

// Print an ANSI-escape representation of the scores for each position.
func (t Minimax) Print(w io.Writer, s State, m Mask) error {
	buf := bufio.NewWriter(w)
	for i := 0; i < 5*5; i++ {
		if m.Valid(i) {
			score := t.Evaluate(s.Place(i), m.Place(i))
			if score > 0 {
				fmt.Fprintf(buf, "\x1b[94m%x\x1b[0m", +score)
			} else if score < 0 {
				fmt.Fprintf(buf, "\x1b[91m%x\x1b[0m", -score)
			} else {
				buf.WriteRune('0')
			}
		} else {
			buf.WriteRune('-')
		}
		if i%5 == 4 {
			buf.WriteRune('\n')
		}
	}
	buf.WriteRune('\n')
	return buf.Flush()
}

func main() {
	t := New()
	t.Evaluate(0, 0)
	fmt.Println(len(t))

	var p1Wins, p2Wins, ties int
	for s, score := range t {
		m := s.Derive()
		if s.IsComplete(m) {
			if score > 0 {
				p1Wins++
			} else if score < 0 {
				p2Wins++
			} else {
				ties++
			}
		}
	}
	fmt.Printf("Total endings: %d\n", p1Wins+p2Wins+ties)
	fmt.Printf("Player 1 wins: %d\n", p1Wins)
	fmt.Printf("Player 2 wins: %d\n", p2Wins)

	t.Print(os.Stdout, State(0).Place(6), Mask(0).Place(6))
	State(0).Place(6).Print(os.Stdout, Mask(0).Place(6))
}
