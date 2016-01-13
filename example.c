#include <string.h>
#include "mini.h"

static const char *const words[] = {
   "greenish",
   "greenness",
   "greens",
   "greet",
   "greeting",
   "greets",
   "gregarious",
   "gregariously",
   NULL,
};

static const char *lexicon_path = "lexicon.dat";

/* Error handling omitted for brevity! */
int main(void)
{
   /* Create an automaton encoding words in the above array. */
   struct mini_enc *enc = mn_enc_new(MN_STANDARD);
   for (size_t i = 0; words[i]; i++)
      mn_enc_add(enc, words[i], strlen(words[i]));
   FILE *fp = fopen(lexicon_path, "wb");
   mn_enc_dump_file(enc, fp);
   mn_enc_free(enc);

   /* Load the automaton we just created. */
   fp = freopen(lexicon_path, "rb", fp);
   struct mini *lexicon;
   mn_load_file(&lexicon, fp);
   fclose(fp);
   
   /* Print all words that have "greet" as prefix. */
   struct mini_iter itor;
   mn_iter_initp(&itor, lexicon, "greet", sizeof "greet" - 1);
   const char *word;
   while ((word = mn_iter_next(&itor, NULL)))
      puts(word);

   /* Cleanup. */
   mn_free(lexicon);
   remove(lexicon_path);
}
