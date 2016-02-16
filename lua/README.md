# mini-lua



Lua binding of `mini`.



## Building



Check the value of the variable `LUA_VERSION` in the makefile in this directory.

Then invoke the usual:



    $ make && sudo make install



You can also pass in the correct version number on the command-line:



    $ make LUA_VERSION=5.3 && sudo make install LUA_VERSION=5.3



## Usage



See the file `example.lua` in this directory for an introductory example. The

following is a more formal description.



### Constants



`mini.VERSION`  

The library version.



`mini.MAX_WORD_LEN`  

Maximum allowed length of a word.



### Automaton encoder



`mini.encoder([automaton_type])`  

Allocates a new automaton encoder.  

`automaton_type`, if given, must be one of the strings `"standard"` and

`"numbered"`. Choose `"standard"` if you want to create a standard automaton,

`"numbered"` if you want to create an automaton with ordered minimal perfect

hashing capabilities. Default is `"standard"`.



`encoder:add(word)`  

Adds a new word to an automaton. Words must be added in lexicographical order.

The length of a word must be > 0 and <= `mini.MAX_WORD_LEN`.



`encoder:dump(path)`  

Dumps an automaton to a file. Returns `true` on success, `nil` plus an error

message otherwise. The automaton is freezed after this function is called, so no

new words should be added afterwards, unless `encoder:clear()` is called first.



`encoder:clear()`  

Clears an encoder. After this is called, the encoder object can be used again to

encode a new set of words.



### Automaton



`mini.load(lexicon_path)`  

Loads a lexicon from a file. On error, returns `nil` plus an error message,

otherwise a lexicon handle.



`lexicon:contains(word)`  

Checks if a lexicon contains a word. Returns `true` if so, `false` otherwise.



`lexicon:locate(word)`  

Returns the ordinal corresponding to a word, if this word is present in the

lexicon and if the automaton is numbered. Otherwise, returns `nil`.



`lexicon:extract(position)`  

Returns the word at a given position in a lexicon, if the position is valid and

if the lexicon is numbered. Otherwise, returns `nil`. A negative value can be

given for `position`. -1 corresponds to the last word in the lexicon, -2 to the

penultimate, and so on.



`lexicon:type()`  

Returns the type of a lexicon (one of the strings `"standard"` and

`"numbered"`).



`lexicon:size()`  

`#lexicon`  

Returns the number of words in a lexicon.



`lexicon:iter([from[, mode]])`  

Returns an iterator over a lexicon.  

If `from` is not given or `nil`, the iterator will be initialized to iterate

over the whole lexicon. If `from` is a string and `mode` is `"string"` (the

default), iteration will start at this string if it is present in the lexicon,

otherwise just after it. If `from` is a string and `mode` is `"prefix"`, the

iterator will be initialized to iterate over all words that have this string as

prefix. If `from` is a number and the automaton is numbered, iteration will

start at the corresponding position in the lexicon. Negative positions are

valid: -1 corresponds to the last word in the lexicon, -2 to the penultimate,

and so on.  

Examples:



    -- Iterate over all words that start with "sub".

    for word in lexicon:iter("sub", "prefix") do print(word) end

    -- Iterate over all words >= "diction".

    for word in lexicon:iter("diction") do print(word) end

    -- Iterate over all words, starting at the 333th.

    for word in lexicon:iter(333) do print(word) end

