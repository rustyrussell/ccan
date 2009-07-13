/* ANSI-C code produced by gperf version 3.0.3 */
/* Command-line: gperf keywords.gperf  */
/* Computed positions: -k'5,$' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 1 "keywords.gperf"

#line 4 "keywords.gperf"
struct op_table {
	const char *name;
	enum op_type type;
	void (*enhance_op)(const char *filename,
			   struct op op[], unsigned int op_num, char *words[]);
};
/* maximum key range = 43, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static unsigned int
hash_keyword (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 15, 51, 25,
       5,  0, 10,  0,  0, 51, 51,  0,  0,  0,
      15, 51, 15, 51, 51,  0,  5,  0, 51,  0,
      51, 15, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51, 51, 51, 51, 51,
      51, 51, 51, 51, 51, 51
    };
  return len + asso_values[(unsigned char)str[4]] + asso_values[(unsigned char)str[len - 1]];
}

#ifdef __GNUC__
__inline
#ifdef __GNUC_STDC_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
const struct op_table *
find_keyword (register const char *str, register unsigned int len)
{
  enum
    {
      TOTAL_KEYWORDS = 32,
      MIN_WORD_LENGTH = 8,
      MAX_WORD_LENGTH = 25,
      MIN_HASH_VALUE = 8,
      MAX_HASH_VALUE = 50
    };

  static const struct op_table wordlist[] =
    {
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
#line 43 "keywords.gperf"
      {"traverse", OP_TDB_TRAVERSE, op_add_key_data,},
#line 33 "keywords.gperf"
      {"tdb_store", OP_TDB_STORE, op_add_store,},
#line 32 "keywords.gperf"
      {"tdb_exists", OP_TDB_EXISTS, op_add_key_ret,},
#line 16 "keywords.gperf"
      {"tdb_lockall", OP_TDB_LOCKALL, op_add_nothing,},
#line 36 "keywords.gperf"
      {"tdb_wipe_all", OP_TDB_WIPE_ALL, op_add_nothing,},
#line 20 "keywords.gperf"
      {"tdb_unlockall", OP_TDB_UNLOCKALL, op_add_nothing,},
#line 35 "keywords.gperf"
      {"tdb_get_seqnum", OP_TDB_GET_SEQNUM, op_add_seqnum,},
#line 47 "keywords.gperf"
      {"tdb_delete", OP_TDB_DELETE, op_add_key_ret,},
#line 17 "keywords.gperf"
      {"tdb_lockall_mark", OP_TDB_LOCKALL_MARK, op_add_nothing,},
      {""},
#line 18 "keywords.gperf"
      {"tdb_lockall_unmark", OP_TDB_LOCKALL_UNMARK, op_add_nothing,},
#line 46 "keywords.gperf"
      {"tdb_fetch", OP_TDB_FETCH, op_add_key_data,},
#line 19 "keywords.gperf"
      {"tdb_lockall_nonblock", OP_TDB_LOCKALL_NONBLOCK, op_add_nothing,},
#line 21 "keywords.gperf"
      {"tdb_lockall_read", OP_TDB_LOCKALL_READ, op_add_nothing,},
      {""},
#line 23 "keywords.gperf"
      {"tdb_unlockall_read", OP_TDB_UNLOCKALL_READ, op_add_nothing,},
      {""},
#line 22 "keywords.gperf"
      {"tdb_lockall_read_nonblock", OP_TDB_LOCKALL_READ_NONBLOCK, op_add_nothing,},
#line 42 "keywords.gperf"
      {"tdb_traverse_end", OP_TDB_TRAVERSE_END, op_analyze_traverse,},
#line 38 "keywords.gperf"
      {"tdb_transaction_cancel", OP_TDB_TRANSACTION_CANCEL, op_analyze_transaction,},
#line 41 "keywords.gperf"
      {"tdb_traverse_start", OP_TDB_TRAVERSE_START, op_add_traverse,},
      {""},
#line 34 "keywords.gperf"
      {"tdb_append", OP_TDB_APPEND, op_add_append,},
#line 37 "keywords.gperf"
      {"tdb_transaction_start", OP_TDB_TRANSACTION_START, op_add_transaction,},
#line 39 "keywords.gperf"
      {"tdb_transaction_commit", OP_TDB_TRANSACTION_COMMIT, op_analyze_transaction,},
#line 40 "keywords.gperf"
      {"tdb_traverse_read_start", OP_TDB_TRAVERSE_READ_START, op_add_traverse,},
      {""}, {""},
#line 31 "keywords.gperf"
      {"tdb_parse_record", OP_TDB_PARSE_RECORD, op_add_key_ret,},
#line 44 "keywords.gperf"
      {"tdb_firstkey", OP_TDB_FIRSTKEY, op_add_key,},
#line 24 "keywords.gperf"
      {"tdb_chainlock", OP_TDB_CHAINLOCK, op_add_key,},
      {""},
#line 28 "keywords.gperf"
      {"tdb_chainunlock", OP_TDB_CHAINUNLOCK, op_add_key,},
#line 45 "keywords.gperf"
      {"tdb_nextkey", OP_TDB_NEXTKEY, op_add_key_data,},
      {""},
#line 26 "keywords.gperf"
      {"tdb_chainlock_mark", OP_TDB_CHAINLOCK_MARK, op_add_key,},
      {""},
#line 27 "keywords.gperf"
      {"tdb_chainlock_unmark", OP_TDB_CHAINLOCK_UNMARK, op_add_key,},
      {""},
#line 25 "keywords.gperf"
      {"tdb_chainlock_nonblock", OP_TDB_CHAINLOCK_NONBLOCK, op_add_key_ret,},
#line 29 "keywords.gperf"
      {"tdb_chainlock_read", OP_TDB_CHAINLOCK_READ, op_add_key,},
      {""},
#line 30 "keywords.gperf"
      {"tdb_chainunlock_read", OP_TDB_CHAINUNLOCK_READ, op_add_key,}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = hash_keyword (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = wordlist[key].name;

          if (*str == *s && !strcmp (str + 1, s + 1))
            return &wordlist[key];
        }
    }
  return 0;
}
