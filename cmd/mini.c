#include <string.h>
#include <stdio.h>
#include "cmd.h"
#include "../mini.h"

static enum mn_type type_from_str(const char *name)
{
   if (!strcmp(name, "standard"))
      return MN_STANDARD;
   if (!strcmp(name, "numbered"))
      return MN_NUMBERED;
   die("invalid automaton type: '%s'", name);
}

static const char *read_line(size_t *len_p, size_t *line_no_p)
{
   static char line[MN_MAX_WORD_LEN + 2];    /* word, newline, zero */
   static size_t line_no;

   while (fgets(line, sizeof line, stdin)) {
      size_t len = strlen(line);
      line_no++;
      if (len && line[len - 1] == '\n')
         line[--len] = '\0';
      if (!len)
         continue;
      if (len > MN_MAX_WORD_LEN)
         die("word '%s' too long at line %zu (length limit is %d)", line, line_no, MN_MAX_WORD_LEN);
      *len_p = len;
      *line_no_p = line_no;
      return line;
   }
   if (ferror(stdin))
      die("IO error:");
   return NULL;
}

static void create(int argc, char **argv)
{
   const char *type = "standard";
   struct option opts[] = {
      {'t', "type", OPT_STR(type)},
      {0}
   };
   parse_options(opts, NULL, &argc, &argv);
   if (argc != 1)
      die("wrong number of arguments");

   struct mini_enc *enc = mn_enc_new(type_from_str(type));
   const char *word;
   size_t len, line_no;
   while ((word = read_line(&len, &line_no))) {
      int ret = mn_enc_add(enc, word, len);
      if (ret)
         die("cannot add word '%s' at line %zu: %s", word, line_no, mn_strerror(ret));
   }

   const char *path = *argv;
   FILE *fp = fopen(path, "wb");
   if (!fp)
      die("cannot open '%s' for writing:", path);
   int ret = mn_enc_dump_file(enc, fp);
   if (ret)
      die("cannot dump automaton: %s", mn_strerror(ret));
   if (fclose(fp))
      die("IO error:");
   
   mn_enc_free(enc);
}

static enum mn_type format_from_str(const char *name)
{
   if (!strcmp(name, "txt"))
      return MN_DUMP_TXT;
   if (!strcmp(name, "tsv"))
      return MN_DUMP_TSV;
   if (!strcmp(name, "dot"))
      return MN_DUMP_DOT;
   die("invalid dump format: '%s'", name);
}

static void dump(int argc, char **argv)
{
   const char *format = "txt";
   struct option opts[] = {
      {'f', "format", OPT_STR(format)},
      {0}
   };
   parse_options(opts, NULL, &argc, &argv);
   if (argc != 1)
      die("wrong number of arguments");
   
   enum mn_dump_format fmt = format_from_str(format);
   const char *path = *argv;
   FILE *fp = fopen(path, "rb");
   if (!fp)
      die("cannot open '%s':", path);
   
   struct mini *mn;
   int ret = mn_load_file(&mn, fp);
   fclose(fp);
   if (ret)
      die("cannot load automaton: %s", mn_strerror(ret));

   ret = mn_dump(mn, stdout, fmt);
   if (ret)
      die("cannot dump automaton: %s", mn_strerror(ret));
   
   mn_free(mn);
}

int main(int argc, char **argv)
{
   struct command cmds[] = {
      {"create", create},
      {"dump", dump},
      {0}
   };
   const char *help =
      #include "mini.ih"
   ;
   
   parse_command(cmds, help, argc, argv);
}
