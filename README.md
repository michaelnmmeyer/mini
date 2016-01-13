# mini

Finite-state lexicon

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

## Note

Automata do not allow storage of auxiliary data inside the lexicon. But perfect
hashing can be used to implement this functionality: the ordinal corresponding
to a word can be used as index into an array, mapped to a database row id, etc.,
where the auxiliary data is stored.
