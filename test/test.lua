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

local function encode_fsa(path, itor, type)
   local enc = mini.encoder(type)
   for word in itor do enc:add(word) end
   assert(enc:dump(path))
end

local test = {}

function test.basic()
   -- Constants
   assert(type(mini.MAX_WORD_LEN) == "number")
   -- Should not leak.
   assert(not pcall(mini.load, nil))
   -- Error while calling the callback.
   assert(not pcall(encode_fsa, path, function() error("foo") end))
   -- Bad return value.
   assert(not pcall(encode_fsa, path, function() return true end))
   -- Attempt to add words out of order.
   assert(not pcall(encode_fsa, path, get_iter{"z","a"}))
   -- Attempt to add the empty string.
   assert(not pcall(encode_fsa, path, get_iter{""}))
   -- Attempt to add a too long word.
   local s = string.rep("z", mini.MAX_WORD_LEN + 1)
   assert(not pcall(encode_fsa, path, get_iter{s}))
end

local function test_functions(ref_words, num_words)
   local path = os.tmpname()
   local fsa_type = math.random(2) == 1 and "standard" or "numbered"
   encode_fsa(path, get_iter(ref_words), fsa_type)
   local words = assert(mini.load(path))

   -- Main functions.
   for i = 1, num_words do
      local word = assert(ref_words[i])
      assert(words:contains(word))
      if fsa_type == "numbered" then
         assert(words:locate(word) == i)
         assert(words:extract(i) == word)
      else
         assert(words:locate(word) == nil)
         assert(words:extract(i) == nil)
      end
   end

   assert(not words:locate("zefonaodnaozndozfneozoz"))
   assert(not words:extract(num_words + 1))

   -- Size (__len metamethod)
   assert(words:size() == num_words and #words == num_words)

   -- Iteration from an ordinal.
   local pos = math.random(num_words)
   if fsa_type == "numbered" then
      local itor, start_pos = assert(words:iter(pos))
      assert(start_pos == pos)
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
   else
      local itor, start_pos = words:iter(pos)
      assert(not itor() and not start_pos)
   end

   -- Iteration from a string.
   local pref = ref_words[pos]
   pref = pref:sub(1, math.random(#pref))

   local mode = math.random(2) == 1 and "string" or "prefix"
   local it, start_pos = words:iter(pref, mode)
   if fsa_type == "numbered" then
      assert(words:extract(start_pos) == it())
   end
   local it, start_pos = words:iter(pref, mode)

   local nword
   for i = 1, num_words do
      local word = ref_words[i]
      if word:starts_with(pref) then
         nword = it()
         assert(nword == word)
      elseif nword then
         if mode == "string" then
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
   for word in io.lines("words.txt") do
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
   encode_fsa(path, function() return nil end)
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
   encode_fsa(path, get_iter{"\0", "\0\1"}, "numbered")
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
   encode_fsa(path, io.lines("words.txt"), "standard")
   local aut = assert(mini.load(path))
   assert(aut:type() == "standard")
   assert(not aut:iter(23)())
   assert(not aut:extract(23))

   -- Numbered
   encode_fsa(path, io.lines("words.txt"), "numbered")
   local aut = assert(mini.load(path))
   assert(aut:type() == "numbered")
   assert(aut:iter(23)())
   assert(aut:extract(23))

   -- Typo in the lexicon type
   assert(not pcall(encode_fsa, path, io.lines("words.txt"), "foobar"))

   os.remove(path)
end

-- Ensure a lexicon object is not collected while there are remaining iterators.
-- This must be run under valgrind to be useful at all.
function test.lexicon_collection(module)
   local path = os.tmpname()
   encode_fsa(path, io.lines("words.txt"))
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

-- Should be able to dump several times the same automaton.
function test.dump_several_times()
   local enc = mini.encoder()
   for word in io.lines("words.txt") do
      enc:add(word)
   end
   local path1, path2 = os.tmpname(), os.tmpname()
   enc:dump(path1); enc:dump(path2)
   assert(io.open(path1, "rb"):read("*a") == io.open(path2, "rb"):read("*a"))
   os.remove(path1); os.remove(path2)
end

-- Should be able to reuse the same encoder several times.
function test.reuse_several_times()
   local enc = mini.encoder()
   local path = os.tmpname()
   enc:add("a")
   assert(enc:dump(path))
   enc:clear()
   enc:add("z")
   assert(enc:dump(path))
   local itor = assert(mini.load(path)):iter()
   assert(itor() == "z" and not itor())
   os.remove(path)
end

-- Segfaulted on that before.
function test.iter_from_last_word()
   local last = "Appaloosa"
   local lex = mini.load("core_dump.mn")
   for x in lex:iter(last, "prefix") do print("N", x) end
end

local names = {}
for name in pairs(test) do table.insert(names, name) end
table.sort(names)

for _, name in ipairs(names) do
   print(name)
   test[name]()
end
