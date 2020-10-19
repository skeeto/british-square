# British Square Engine (Analysis and Perfect AI Player)

This program exhaustively explores the complete game tree for [British
Square][bs], an [abstract strategy board game][pm], allowing for perfect
play. Having the entire game tree in memory also enables analysis.

The full analysis takes 3.5 seconds on modern hardware. In [Minimax
mode][mm] (the default), the entire game tree fits in ~66MiB of memory.
Source comments document the engine's bitboard.

Full article: [I Solved British Square][about]

![](https://nullprogram.com/img/british-square/british-square.jpg)

## Rules

The game is played with two players on a 5x5 grid. The players take
turns placing pieces of their color on the grid. Pieces may not be
placed on tiles 4-adjacent to an opposing piece. As a special rule, the
first player may not play the center tile on the first turn.

A player with no legal moves passes. The game ends when both players
pass (i.e. neither has legal moves). The final score is the difference
between the number of pieces placed by each player.

The original board game is played repeatedly until a player accumulates
7 points. Further, each player has only 11 pieces, and so the maximum
possible score for a round is 6 points. This engine is focused only on a
single round, and the 11 piece maximum is regarded as a manufacturing
limitation rather than a rule. Ultimately neither really matter.

## Discoveries

Not accounting for symmetries, there are 4,233,789,642,926,592 possible
playouts. In these playouts, the first player wins 2,179,847,574,830,592
(~51%), the second player wins 1,174,071,341,606,400 (~28%), and the
remaining 879,870,726,489,600 (~21%) are ties. It's already obvious the
first player has a huge advantage.

Accounting for symmetries, there are 8,659,987 total game states. Of
these, 6,955 are terminal states, of which the first player wins 3,599
(~52%) and the second player wins 2,506 (~36%).

**The first player can always win by 2**. If center play is permitted
for the first player on the first turn, that center play would only
introduce another avenue for win-by-2. Here is the map of the first
player's first turn options:

    11111
    12021
    10-01
    12021
    11111

The numbers indicate the first player's final score (i.e. 0 is a tie)
given perfect play starting from that point. In other words, the first
player should open on one of the "2" tiles.

If the first player blunders into a choosing one of the four moves that
result in a tie, perfect play by the second player is trivial: Mirror
the first player's moves. (This could be discovered without computer
assistance.)

Default program operation enables further exploration of the game tree.

## Build options

Using "tally" mode does the full, exhaustive count that ignores
symmetries. It also uses 4x the memory.

    make OPTS=-DTALLY

Using "benchmark" mode disables interactive "play", used to tweak
bitboard and hash table settings.

    make OPTS=-DBENCHMARK

## Supported systems

This program fully works on any unix-like system and Windows 10. It's
been tested with GCC, Clang, and Visual Studio.


[about]: https://nullprogram.com/blog/2020/10/19/
[bs]: https://boardgamegeek.com/boardgame/3719/british-square
[mm]: https://en.wikipedia.org/wiki/Minimax
[pm]: https://www.youtube.com/watch?v=PChKZbut3lM&t=10m
