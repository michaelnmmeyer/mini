# mini

Finite-state lexicon.

## Purpose

This is a lexicon data structure implemented as a minimal acyclic finite-state
automaton. Such a data structure supports the same operations as an ordered set
(checking for the presence of a word, iterating over the lexicon in
lexicographical order), but is much smaller due to the use of compression. The
following shows the size of a few dictionaries before and after compression:

    dictionary      language    decompressed  compressed
    ---             ---         ---           ---
    Unix            English     920K          284K
    Corriere        Italian     404K          224K
    Duden           German      2.5M          1.5M
    Robert          French      2.0M          516K
    Monier-Williams Sanskrit    2.5M          1.3M

This implementation also supports ordered minimal perfect hashing: there is a
one-to-one correspondence between a word and an ordinal representing its
position in the lexicon. This allows finding a word given its ordinal, and,
conversely, finding the ordinal corresponding to a word, as in a sorted array.

Finally, strings containing embedded zeroes are supported.

## Building

There is no build process. Compile `mini.c` together with your source code, and
use the interface described in `mini.h`. You'll need a C99 compiler, which means
GCC or CLang on Unix.

A command-line tool `mini` is included. Compile and install it with the usual
invocation:

    $ make && sudo make install

A Lua binding is also available. See the file `README.md` in the `lua` directory
for instructions about how to build and use it.

## Usage

The C API is documented in `mini.h`. See the file `example.c` for a concrete
example.

Automata do not allow storage of auxiliary data inside the lexicon, but perfect
hashing can be used to implement this functionality: the ordinal corresponding
to a word can be used as index into an array, mapped to a database row id, etc.,
where the auxiliary data is stored.

## Details

### References

This implementation draws from the following papers:

* [Ciura & Deorowicz (2001), How to Squeeze a Lexicon](http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.35.6055&rep=rep1&type=pdf).
  Describes an efficient encoding format.
* [Kowaltowski & Lucchesi (1993), Applications of finite automata representing large vocabularies](http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.56.5272&rep=rep1&type=pdf).
  Explains how to implement ordered minimal perfect hashing.

### Encoding

Automata are encoded as arrays of 32-bits integers. There is one integer per
transition, which contains the following fields, starting from the least
significant bit:

    bit offset  value
    ---         ---
    0           whether this transition is the last outgoing transition of the
                current state
    1           whether this transition is terminal
    2           transition byte
    10          destination state

If the automaton is numbered, a second array of 32-bits integers follows. This
array contains the number of terminal transitions reachable from the
corresponding transition in the automaton array, for each transition. Although
using a single 64-bit integer to store data related to a given transition might
be faster due to locality of reference, I chose to use two arrays so that the
same code can be used for decoding standard an numbered automata.

Finally, automata are prefixed with a 12-bytes header containing the following
fields:

    byte offset   field
    ---           ---
    0             magic identifier (the string "mini")
    4             data format version (currently, 1)
    8             number of transitions
    9             automaton type (0 = standard, 1 = numbered)

All integers are encoded in network order.
