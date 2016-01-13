package.cpath = "../lua/?.so"

local mini = require("mini")

local seed = arg[1] and tonumber(arg[1]) or os.time()
print("seed", seed)
math.randomseed(seed)

local function get_iter(words)
   local i = 0
   return function()
      i = i + 1
      return words[i]
   end
end

function string:starts_with(prefix)
   return self:sub(1, #prefix) == prefix
end

local test = {}

function test.basic()
   -- Constants
   assert(type(mini.MAX_WORD_LEN) == "number")
   -- Should not leak.
   assert(not pcall(mini.load, nil))
   -- Error while calling the callback.
   assert(not pcall(mini.encode, path, function() error("foo") end))
   -- Bad return value.
   assert(not pcall(mini.encode, path, function() return true end))
   -- Error while adding words.
   assert(not pcall(mini.encode, path, get_iter{"z","a"}))
end

local function test_functions(ref_words, num_words)
   local path = os.tmpname()
   mini.encode(path, get_iter(ref_words), "numbered")
   local words = assert(mini.load(path))
   
   -- Main functions.
   for i = 1, num_words do
      local word = assert(ref_words[i])
      assert(words:contains(word))
      assert(words:locate(word) == i)
      assert(words:extract(i) == word)
   end
   
   assert(not words:locate("zefonaodnaozndozfneozoz"))
   assert(not words:extract(num_words + 1))
   
   -- Size (__len metamethod)
   assert(words:size() == num_words and #words == num_words)

   -- Iteration from an ordinal.
   local pos = math.random(num_words)
   local itor = assert(words:iter(pos))
   local cnt = 0
   for word in itor do
      assert(ref_words[pos + cnt] == word)
      cnt = cnt + 1
   end
   -- Should not break when the iterator is exhausted.
   itor()
   itor()
   itor()
   assert(not ref_words[pos + cnt])
   -- Iteration out of bound.
   assert(not words:iter(num_words + 1)())

   -- Iteration from a string.
   local pref = ref_words[pos]
   pref = pref:sub(1, math.random(#pref))
   
   local whole = math.random(2) == 1
   local it = words:iter(pref, whole and "string" or "prefix")

   local nword
   for i = 1, num_words do
      local word = ref_words[i]
      if word:starts_with(pref) then
         nword = it()
         assert(nword == word, nword .. " *** " .. word .. " $$$ " .. pref)
      elseif nword then
         if whole then
            assert(it() == word)
         else
            assert(not it())
         end
      end
   end
   -- Iteration out of bound.
   assert(not words:iter("ÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿ")())
   assert(not words:iter("ÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿÿ", "prefix")())
end

function test.functions()
   local words = {}
   for word in io.lines("/usr/share/dict/words") do
      table.insert(words, word)
   end
   -- Test with variable vocabulary size.
   local min = math.random(#words)
   local max = min + math.random(33)
   for i = max, min, -1 do
      words[i] = nil
      test_functions(words, i - 1)
   end
end

function test.empty_lexicon()
   local path = os.tmpname()
   mini.encode(path, function() return nil end)
   local words = assert(mini.load(path))
   
   assert(not words:contains(""))
   assert(not words:locate("foo"))
   assert(not words:locate(""))
   
   assert(not words:iter()())
   assert(not words:iter("")())
   assert(not words:iter(1)())
   
   os.remove(path)
end

function test.binary_strings()
   local path = os.tmpname()
   mini.encode(path, get_iter{"\0", "\0\1"}, "numbered")
   local words = assert(mini.load(path))
   
   assert(words:contains("\0"))
   assert(words:contains("\0\1"))
   assert(words:locate("\0") == 1)
   assert(words:locate("\0\1") == 2)
   assert(words:extract(1) == "\0")
   assert(words:extract(2) == "\0\1")
   
   local itor = words:iter()
   assert(itor() == "\0")
   assert(itor() == "\0\1")
   
   itor = words:iter("\0\1")
   assert(itor() == "\0\1")
   assert(not itor())
   
   os.remove(path)
end

function test.fsa_types()
   local path = os.tmpname()
   
   -- Not numbered
   mini.encode(path, io.lines("/usr/share/dict/words"), "standard")
   local aut = assert(mini.load(path))
   assert(aut:type() == "standard")
   assert(not aut:iter(23)())
   assert(not aut:extract(23))
   
   -- Numbered
   mini.encode(path, io.lines("/usr/share/dict/words"), "numbered")
   local aut = assert(mini.load(path))
   assert(aut:type() == "numbered")
   assert(aut:iter(23)())
   assert(aut:extract(23))
   
   -- Typo in the lexicon type
   assert(not pcall(mini.encode, path, io.lines("/usr/share/dict/words"), "foobar"))
   
   os.remove(path)
end

-- Ensure a lexicon object is not collected while there are remaining iterators.
-- This must be run under valgrind to be useful at all.
function test.lexicon_collection(module)
   local path = os.tmpname() 
   mini.encode(path, io.lines("/usr/share/dict/words"))
   local lex = mini.load(path)
   os.remove(path)
   
   local itors = {}
   for i = 1, math.random(20) do
      itors[i] = lex:iter()
   end
   lex = nil; collectgarbage()
   for i = 1, #itors do
      itors[i]()
   end
end

local names = {}
for name in pairs(test) do table.insert(names, name) end
table.sort(names)

for _, name in ipairs(names) do
   print(name)
   test[name]()
end
