local mini = require("mini")

local lexicon_path = "lexicon.dat"

-- Create a lexicon.
local enc = mini.encoder()
for word in io.lines("/usr/share/dict/words") do
   enc:add(word)
end
enc:dump(lexicon_path)

-- Load the lexicon we just created.
local lexicon = mini.load(lexicon_path)

-- Print all words that start with the string "show".
for word in lexicon:iter("show", "prefix") do
   print(word)
end

os.remove(lexicon_path)
