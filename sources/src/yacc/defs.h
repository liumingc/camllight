#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#ifdef __STDC__
#include <stdlib.h>
#endif
#include <string.h>

#ifdef macintosh
#include <CursorCtl.h>
#endif

#ifdef macintosh
#include ":::config:m.h"
#include ":::config:s.h"
#else
#include "../../config/m.h"
#include "../../config/s.h"
#endif

#ifdef MSDOS
#define NO_UNIX
#endif
#ifdef macintosh
#define NO_UNIX
#endif

/*  machine-dependent definitions                       */
/*  the following definitions are for the Tahoe         */
/*  they might have to be changed for other machines    */

/*  MAXCHAR is the largest unsigned character value     */
/*  MAXSHORT is the largest value of a C short          */
/*  MINSHORT is the most negative value of a C short    */
/*  MAXTABLE is the maximum table size                  */
/*  BITS_PER_WORD is the number of bits in a C unsigned */
/*  WORDSIZE computes the number of words needed to     */
/*      store n bits                                    */
/*  BIT returns the value of the n-th bit starting      */
/*      from r (0-indexed)                              */
/*  SETBIT sets the n-th bit starting from r            */

#define MAXCHAR         255
#define MAXSHORT        32767
#define MINSHORT        -32768
#define MAXTABLE        32500

#ifdef SIXTEEN
#define BITS_PER_WORD   16
#define WORDSIZE(n)     (((n)+(BITS_PER_WORD-1))/BITS_PER_WORD)
#define BIT(r, n)       ((((r)[(n)>>4])>>((n)&15))&1)
#define SETBIT(r, n)    ((r)[(n)>>4]|=((unsigned)1<<((n)&15)))
#else
#define BITS_PER_WORD   32
#define WORDSIZE(n)     (((n)+(BITS_PER_WORD-1))/BITS_PER_WORD)
#define BIT(r, n)       ((((r)[(n)>>5])>>((n)&31))&1)
#define SETBIT(r, n)    ((r)[(n)>>5]|=((unsigned)1<<((n)&31)))
#endif

/*  character names  */

#define NUL             '\0'    /*  the null character  */
#define NEWLINE         '\n'    /*  line feed  */
#define SP              ' '     /*  space  */
#define BS              '\b'    /*  backspace  */
#define HT              '\t'    /*  horizontal tab  */
#define VT              '\013'  /*  vertical tab  */
#define CR              '\r'    /*  carriage return  */
#define FF              '\f'    /*  form feed  */
#define QUOTE           '\''    /*  single quote  */
#define DOUBLE_QUOTE    '\"'    /*  double quote  */
#define BACKSLASH       '\\'    /*  backslash  */


/* defines for constructing filenames */

#ifndef MSDOS
#define CODE_SUFFIX     ".code.c"
#define DEFINES_SUFFIX  ".tab.h"
#define OUTPUT_SUFFIX   ".ml"
#define VERBOSE_SUFFIX  ".output"
#define INTERFACE_SUFFIX ".mli"
#else
#define CODE_SUFFIX     ".cod"
#define DEFINES_SUFFIX  ".h"
#define OUTPUT_SUFFIX   ".ml"
#define VERBOSE_SUFFIX  ".out"
#define INTERFACE_SUFFIX ".mli"
#endif

/* keyword codes */

#define TOKEN 0
#define LEFT 1
#define RIGHT 2
#define NONASSOC 3
#define MARK 4
#define TEXT 5
#define TYPE 6
#define START 7
#define UNION 8
#define IDENT 9

/*  symbol classes  */

#define UNKNOWN 0
#define TERM 1
#define NONTERM 2


/*  the undefined value  */

#define UNDEFINED (-1)


/*  action codes  */

#define SHIFT 1
#define REDUCE 2


/*  character macros  */

#define IS_IDENT(c)     (isalnum(c) || (c) == '_' || (c) == '.' || (c) == '$')
#define IS_OCTAL(c)     ((c) >= '0' && (c) <= '7')
#define NUMERIC_VALUE(c)        ((c) - '0')


/*  symbol macros  */

#define ISTOKEN(s)      ((s) < start_symbol)
#define ISVAR(s)        ((s) >= start_symbol)


/*  storage allocation macros  */

#define CALLOC(k,n)     (calloc((unsigned)(k),(unsigned)(n)))
#ifdef macintosh
#define FREE(x)         (SpinCursor ((short) 1), free((char*)(x)))
#else
#define FREE(x)         (free((char*)(x)))
#endif
#define MALLOC(n)       (malloc((unsigned)(n)))
#define NEW(t)          ((t*)allocate(sizeof(t)))
#define NEW2(n,t)       ((t*)allocate((unsigned)((n)*sizeof(t))))
#define REALLOC(p,n)    (realloc((char*)(p),(unsigned)(n)))


/*  the structure of a symbol table entry  */

typedef struct bucket bucket;
struct bucket
{
    struct bucket *link;
    struct bucket *next;
    char *name;
    char *tag;
    short value;
    short index;
    short prec;
    char class;
    char assoc;
    char entry;
    char true_token;
};

/* TABLE_SIZE is the number of entries in the symbol table. */
/* TABLE_SIZE must be a power of two.                       */

#define TABLE_SIZE 1024

/*  the structure of the LR(0) state machine  */

typedef struct core core;
struct core
{
    struct core *next;
    struct core *link;
    short number;
    short accessing_symbol;
    short nitems;
    short items[1];
};


/*  the structure used to record shifts  */

typedef struct shifts shifts;
struct shifts
{
    struct shifts *next;
    short number;
    short nshifts;
    short shift[1];
};


/*  the structure used to store reductions  */

typedef struct reductions reductions;
struct reductions
{
    struct reductions *next;
    short number;
    short nreds;
    short rules[1];
};


/*  the structure used to represent parser actions  */

typedef struct action action;
struct action
{
    struct action *next;
    short symbol;
    short number;
    short prec;
    char action_code;
    char assoc;
    char suppressed;
};


/* global variables */

extern char dflag;
extern char lflag;
extern char rflag;
extern char tflag;
extern char vflag;
extern char sflag;
extern char big_endian;

extern char *myname;
extern char *cptr;
extern char *line;
extern int lineno;
extern int outline;

extern char *action_file_name;
extern char *entry_file_name;
extern char *code_file_name;
extern char *defines_file_name;
extern char *input_file_name;
extern char *output_file_name;
extern char *text_file_name;
extern char *union_file_name;
extern char *verbose_file_name;
extern char *interface_file_name;

extern FILE *action_file;
extern FILE *entry_file;
extern FILE *code_file;
extern FILE *defines_file;
extern FILE *input_file;
extern FILE *output_file;
extern FILE *text_file;
extern FILE *union_file;
extern FILE *verbose_file;
extern FILE *interface_file;

extern int nitems;
extern int nrules;
extern int ntotalrules;
extern int nsyms;
extern int ntokens;
extern int nvars;
extern int ntags;

extern char unionized;
extern char line_format[];

extern int   start_symbol;
extern char  **symbol_name;
extern short *symbol_value;
extern short *symbol_prec;
extern char  *symbol_assoc;
extern char  **symbol_tag;
extern char  *symbol_true_token;

extern short *ritem;
extern short *rlhs;
extern short *rrhs;
extern short *rprec;
extern char  *rassoc;

extern short **derives;
extern char *nullable;

extern bucket *first_symbol;
extern bucket *last_symbol;

extern int nstates;
extern core *first_state;
extern shifts *first_shift;
extern reductions *first_reduction;
extern short *accessing_symbol;
extern core **state_table;
extern shifts **shift_table;
extern reductions **reduction_table;
extern unsigned *LA;
extern short *LAruleno;
extern short *lookaheads;
extern short *goto_map;
extern short *from_state;
extern short *to_state;

extern action **parser;
extern int SRtotal;
extern int RRtotal;
extern short *SRconflicts;
extern short *RRconflicts;
extern short *defred;
extern short *rules_used;
extern short nunused;
extern short final_state;

/* global functions */

extern char *allocate();
extern bucket *lookup();
extern bucket *make_bucket();

extern void reflexive_transitive_closure(unsigned *, int);
// main.c
extern void done(int);

// error.c
extern void fatal(char *msg);
extern void no_space();
extern void open_error(char *filename);

// closure.c
extern void set_first_derives();
extern void finalize_closure();
extern void closure(short *nucleus, int n);

// reader.c
extern void reader();

// lr0.c
extern void lr0();

// lalr.c
extern void lalr();

/* system variables */

extern int errno;


/* system functions */

#ifndef __STDC__

extern void free();
extern char *calloc();
extern char *malloc();
extern char *realloc();
extern char *strcpy();

#endif
