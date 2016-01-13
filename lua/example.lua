local mini = require("mini")

local lexicon_path = "lexicon.dat"

-- Create a lexicon.
mini.encode(lexicon_path, io.lines("/usr/share/dict/words"))

-- Load the lexicon we just created.
local lexicon = mini.load(lexicon_path)

-- Print all words that start with the string "show".
for word in lexicon:iter("show", "prefix") do
   print(word)
end

os.remove(lexicon_path)
