#include "defs.h"

static int nvectors;
static int nentries;
static short **froms;
static short **tos;
static short *tally;
static short *width;
static short *state_count;
static short *order;
static short *base;
static short *pos;
static int maxtable;
static short *table;
static short *check;
static int lowzero;
static int high;

void output_stored_text();
void output_debug();
void output_trailing_text();

output()
{
  extern char *header[], *define_tables[];

  free_itemsets();
  free_shifts();
  free_reductions();
  write_section(header);
  output_stored_text();
  output_transl();
  output_rule_data();
  output_yydefred();
  output_actions();
  free_parser();
  output_debug();
  if (sflag)
    fprintf(output_file,
      "let yyact = make_vect %d (fun () -> (failwith \"parser\" : obj));;\n",
      ntotalrules);
  else
    fprintf(output_file,
      "let yyact = [|\n  (fun () -> failwith \"parser\")\n");
  output_semantic_actions();
  if (!sflag)
    fprintf(output_file, "|];;\n");
  write_section(define_tables);
  output_entries();
  output_trailing_text();
}


static void output_char(n)
     unsigned n;
{
  n = n & 0xFF;
  putc('\\', output_file);
  putc('0' + n / 100, output_file);
  putc('0' + (n / 10) % 10, output_file);
  putc('0' + n % 10, output_file);
}

static void output_short(n)
     int n;
{
  output_char(n);
  output_char(n >> 8);
}

output_rule_data()
{
    register int i;
    register int j;

  
    fprintf(output_file, "let yylhs = \"");
    output_short(symbol_value[start_symbol]);

    j = 8;
    for (i = 3; i < nrules; i++)
    {
	if (j >= 8)
	{
	    if (!rflag) ++outline;
	    fprintf(output_file, "\\\n");
	    j = 1;
	}
        else
	    ++j;

        output_short(symbol_value[rlhs[i]]);
    }
    if (!rflag) outline += 2;
    fprintf(output_file, "\";;\n\n");

    fprintf(output_file, "let yylen = \"");
    output_short(2);

    j = 8;
    for (i = 3; i < nrules; i++)
    {
	if (j >= 8)
	{
	    if (!rflag) ++outline;
	    fprintf(output_file, "\\\n");
	    j = 1;
	}
	else
	  j++;

        output_short(rrhs[i + 1] - rrhs[i] - 1);
    }
    if (!rflag) outline += 2;
    fprintf(output_file, "\";;\n\n");
}


output_yydefred()
{
    register int i, j;

    fprintf(output_file, "let yydefred = \"");
    output_short(defred[0] ? defred[0] - 2 : 0);

    j = 8;
    for (i = 1; i < nstates; i++)
    {
	if (j < 8)
	    ++j;
	else
	{
	    if (!rflag) ++outline;
	    fprintf(output_file, "\\\n");
	    j = 1;
	}

	output_short(defred[i] ? defred[i] - 2 : 0);
    }

    if (!rflag) outline += 2;
    fprintf(output_file, "\";;\n\n");
}


output_actions()
{
    nvectors = 2*nstates + nvars;

    froms = NEW2(nvectors, short *);
    tos = NEW2(nvectors, short *);
    tally = NEW2(nvectors, short);
    width = NEW2(nvectors, short);

    token_actions();
    FREE(lookaheads);
    FREE(LA);
    FREE(LAruleno);
    FREE(accessing_symbol);

    goto_actions();
    FREE(goto_map + ntokens);
    FREE(from_state);
    FREE(to_state);

    sort_actions();
    pack_table();
    output_base();
    output_table();
    output_check();
}


token_actions()
{
    register int i, j;
    register int shiftcount, reducecount;
    register int max, min;
    register short *actionrow, *r, *s;
    register action *p;

    actionrow = NEW2(2*ntokens, short);
    for (i = 0; i < nstates; ++i)
    {
	if (parser[i])
	{
	    for (j = 0; j < 2*ntokens; ++j)
	    actionrow[j] = 0;

	    shiftcount = 0;
	    reducecount = 0;
	    for (p = parser[i]; p; p = p->next)
	    {
		if (p->suppressed == 0)
		{
		    if (p->action_code == SHIFT)
		    {
			++shiftcount;
			actionrow[p->symbol] = p->number;
		    }
		    else if (p->action_code == REDUCE && p->number != defred[i])
		    {
			++reducecount;
			actionrow[p->symbol + ntokens] = p->number;
		    }
		}
	    }

	    tally[i] = shiftcount;
	    tally[nstates+i] = reducecount;
	    width[i] = 0;
	    width[nstates+i] = 0;
	    if (shiftcount > 0)
	    {
		froms[i] = r = NEW2(shiftcount, short);
		tos[i] = s = NEW2(shiftcount, short);
		min = MAXSHORT;
		max = 0;
		for (j = 0; j < ntokens; ++j)
		{
		    if (actionrow[j])
		    {
			if (min > symbol_value[j])
			    min = symbol_value[j];
			if (max < symbol_value[j])
			    max = symbol_value[j];
			*r++ = symbol_value[j];
			*s++ = actionrow[j];
		    }
		}
		width[i] = max - min + 1;
	    }
	    if (reducecount > 0)
	    {
		froms[nstates+i] = r = NEW2(reducecount, short);
		tos[nstates+i] = s = NEW2(reducecount, short);
		min = MAXSHORT;
		max = 0;
		for (j = 0; j < ntokens; ++j)
		{
		    if (actionrow[ntokens+j])
		    {
			if (min > symbol_value[j])
			    min = symbol_value[j];
			if (max < symbol_value[j])
			    max = symbol_value[j];
			*r++ = symbol_value[j];
			*s++ = actionrow[ntokens+j] - 2;
		    }
		}
		width[nstates+i] = max - min + 1;
	    }
	}
    }
    FREE(actionrow);
}

void save_column(int symbol, int default_state);

goto_actions()
{
    register int i, j, k;

    state_count = NEW2(nstates, short);

    k = default_goto(start_symbol + 1);
    fprintf(output_file, "let yydgoto = \"");
    output_short(k);

    save_column(start_symbol + 1, k);

    j = 8;
    for (i = start_symbol + 2; i < nsyms; i++)
    {
	if (j >= 8)
	{
	    if (!rflag) ++outline;
	    fprintf(output_file, "\\\n");
	    j = 1;
	}
	else
	    ++j;

	k = default_goto(i);
	output_short(k);
	save_column(i, k);
    }

    if (!rflag) outline += 2;
    fprintf(output_file, "\";;\n\n");
    FREE(state_count);
}

int
default_goto(symbol)
int symbol;
{
    register int i;
    register int m;
    register int n;
    register int default_state;
    register int max;

    m = goto_map[symbol];
    n = goto_map[symbol + 1];

    if (m == n) return (0);

    for (i = 0; i < nstates; i++)
	state_count[i] = 0;

    for (i = m; i < n; i++)
	state_count[to_state[i]]++;

    max = 0;
    default_state = 0;
    for (i = 0; i < nstates; i++)
    {
	if (state_count[i] > max)
	{
	    max = state_count[i];
	    default_state = i;
	}
    }

    return (default_state);
}


void
save_column(symbol, default_state)
int symbol;
int default_state;
{
    register int i;
    register int m;
    register int n;
    register short *sp;
    register short *sp1;
    register short *sp2;
    register int count;
    register int symno;

    m = goto_map[symbol];
    n = goto_map[symbol + 1];

    count = 0;
    for (i = m; i < n; i++)
    {
	if (to_state[i] != default_state)
	    ++count;
    }
    if (count == 0) return;

    symno = symbol_value[symbol] + 2*nstates;

    froms[symno] = sp1 = sp = NEW2(count, short);
    tos[symno] = sp2 = NEW2(count, short);

    for (i = m; i < n; i++)
    {
	if (to_state[i] != default_state)
	{
	    *sp1++ = from_state[i];
	    *sp2++ = to_state[i];
	}
    }

    tally[symno] = count;
    width[symno] = sp1[-1] - sp[0] + 1;
}

sort_actions()
{
  register int i;
  register int j;
  register int k;
  register int t;
  register int w;

  order = NEW2(nvectors, short);
  nentries = 0;

  for (i = 0; i < nvectors; i++)
    {
      if (tally[i] > 0)
	{
	  t = tally[i];
	  w = width[i];
	  j = nentries - 1;

	  while (j >= 0 && (width[order[j]] < w))
	    j--;

	  while (j >= 0 && (width[order[j]] == w) && (tally[order[j]] < t))
	    j--;

	  for (k = nentries - 1; k > j; k--)
	    order[k + 1] = order[k];

	  order[j + 1] = i;
	  nentries++;
	}
    }
}


pack_table()
{
    register int i;
    register int place;
    register int state;

    base = NEW2(nvectors, short);
    pos = NEW2(nentries, short);

    maxtable = 1000;
    table = NEW2(maxtable, short);
    check = NEW2(maxtable, short);

    lowzero = 0;
    high = 0;

    for (i = 0; i < maxtable; i++)
	check[i] = -1;

    for (i = 0; i < nentries; i++)
    {
	state = matching_vector(i);

	if (state < 0)
	    place = pack_vector(i);
	else
	    place = base[state];

	pos[i] = place;
	base[order[i]] = place;
    }

    for (i = 0; i < nvectors; i++)
    {
	if (froms[i])
	    FREE(froms[i]);
	if (tos[i])
	    FREE(tos[i]);
    }

    FREE(froms);
    FREE(tos);
    FREE(pos);
}


/*  The function matching_vector determines if the vector specified by	*/
/*  the input parameter matches a previously considered	vector.  The	*/
/*  test at the start of the function checks if the vector represents	*/
/*  a row of shifts over terminal symbols or a row of reductions, or a	*/
/*  column of shifts over a nonterminal symbol.  Berkeley Yacc does not	*/
/*  check if a column of shifts over a nonterminal symbols matches a	*/
/*  previously considered vector.  Because of the nature of LR parsing	*/
/*  tables, no two columns can match.  Therefore, the only possible	*/
/*  match would be between a row and a column.  Such matches are	*/
/*  unlikely.  Therefore, to save time, no attempt is made to see if a	*/
/*  column matches a previously considered vector.			*/
/*									*/
/*  Matching_vector is poorly designed.  The test could easily be made	*/
/*  faster.  Also, it depends on the vectors being in a specific	*/
/*  order.								*/

int
matching_vector(vector)
int vector;
{
    register int i;
    register int j;
    register int k;
    register int t;
    register int w;
    register int match;
    register int prev;

    i = order[vector];
    if (i >= 2*nstates)
	return (-1);

    t = tally[i];
    w = width[i];

    for (prev = vector - 1; prev >= 0; prev--)
    {
	j = order[prev];
	if (width[j] != w || tally[j] != t)
	    return (-1);

	match = 1;
	for (k = 0; match && k < t; k++)
	{
	    if (tos[j][k] != tos[i][k] || froms[j][k] != froms[i][k])
		match = 0;
	}

	if (match)
	    return (j);
    }

    return (-1);
}



int
pack_vector(vector)
int vector;
{
    register int i, j, k, l;
    register int t;
    register int loc;
    register int ok;
    register short *from;
    register short *to;
    int newmax;

    i = order[vector];
    t = tally[i];
    assert(t);

    from = froms[i];
    to = tos[i];

    j = lowzero - from[0];
    for (k = 1; k < t; ++k)
	if (lowzero - from[k] > j)
	    j = lowzero - from[k];
    for (;; ++j)
    {
	if (j == 0)
	    continue;
	ok = 1;
	for (k = 0; ok && k < t; k++)
	{
	    loc = j + from[k];
	    if (loc >= maxtable)
	    {
		if (loc >= MAXTABLE)
		    fatal("maximum table size exceeded");

		newmax = maxtable;
		do { newmax += 200; } while (newmax <= loc);
		table = (short *) REALLOC(table, newmax*sizeof(short));
		if (table == 0) no_space();
		check = (short *) REALLOC(check, newmax*sizeof(short));
		if (check == 0) no_space();
		for (l  = maxtable; l < newmax; ++l)
		{
		    table[l] = 0;
		    check[l] = -1;
		}
		maxtable = newmax;
	    }

	    if (check[loc] != -1)
		ok = 0;
	}
	for (k = 0; ok && k < vector; k++)
	{
	    if (pos[k] == j)
		ok = 0;
	}
	if (ok)
	{
	    for (k = 0; k < t; k++)
	    {
		loc = j + from[k];
		table[loc] = to[k];
		check[loc] = from[k];
		if (loc > high) high = loc;
	    }

	    while (check[lowzero] != -1)
		++lowzero;

	    return (j);
	}
    }
}



output_base()
{
    register int i, j;

    fprintf(output_file, "let yysindex = \"");
    output_short(base[0]);

    j = 8;
    for (i = 1; i < nstates; i++)
    {
	if (j >= 8)
	{
	    if (!rflag) ++outline;
	    fprintf(output_file, "\\\n");
	    j = 1;
	}
	else
	    ++j;

	output_short(base[i]);
    }

    if (!rflag) outline += 2;
    fprintf(output_file, "\";;\n\n");

    fprintf(output_file, "let yyrindex = \"");
    output_short(base[nstates]);

    j = 8;
    for (i = nstates + 1; i < 2*nstates; i++)
    {
	if (j >= 8)
	{
	    if (!rflag) ++outline;
	    fprintf(output_file, "\\\n");
	    j = 1;
	}
	else
	    ++j;

	output_short(base[i]);
    }

    if (!rflag) outline += 2;
    fprintf(output_file, "\";;\n\n");

    fprintf(output_file, "let yygindex = \"");
    output_short(base[2*nstates]);

    j = 8;
    for (i = 2*nstates + 1; i < nvectors - 1; i++)
    {
	if (j >= 8)
	{
	    if (!rflag) ++outline;
	    fprintf(output_file, "\\\n");
	    j = 1;
	}
	else
	    ++j;

	output_short(base[i]);
    }

    if (!rflag) outline += 2;
    fprintf(output_file, "\";;\n\n");
    FREE(base);
}



output_table()
{
    register int i;
    register int j;

    ++outline;
    fprintf(code_file, "let YYTABLESIZE = %d;;\n", high);
    fprintf(output_file, "let yytable = \"");
    output_short(table[0]);

    j = 8;
    for (i = 1; i <= high; i++)
    {
	if (j >= 8)
	{
	    if (!rflag) ++outline;
	    fprintf(output_file, "\\\n");
	    j = 1;
	}
	else
	    ++j;

	output_short(table[i]);
    }

    if (!rflag) outline += 2;
    fprintf(output_file, "\";;\n\n");
    FREE(table);
}



output_check()
{
    register int i;
    register int j;

    fprintf(output_file, "let yycheck = \"");
    output_short(check[0]);

    j = 8;
    for (i = 1; i <= high; i++)
    {
	if (j >= 8)
	{
	    if (!rflag) ++outline;
	    fprintf(output_file, "\\\n");
	    j = 1;
	}
	else
	    ++j;

	output_short(check[i]);
    }

    if (!rflag) outline += 2;
    fprintf(output_file, "\";;\n\n");
    FREE(check);
}


output_transl()
{
  int i;

  fprintf(code_file, "let yytransl = [|\n");
  for (i = 0; i < ntokens; i++) {
    if (symbol_true_token[i]) {
      fprintf(code_file, "  %3d (* %s *);\n", symbol_value[i], symbol_name[i]);
    }
  }
  fprintf(code_file, "    0|];;\n\n");
}

void
output_stored_text()
{
    register int c;
    register FILE *in, *out;

    fclose(text_file);
    text_file = fopen(text_file_name, "r");
    if (text_file == NULL)
	open_error(text_file_name);
    in = text_file;
    if ((c = getc(in)) == EOF)
	return;
    out = code_file;
    if (c ==  '\n')
	++outline;
    putc(c, out);
    while ((c = getc(in)) != EOF)
    {
	if (c == '\n')
	    ++outline;
	putc(c, out);
    }
    if (!lflag)
	fprintf(out, line_format, ++outline + 1, code_file_name);
}

void
output_debug()
{
}

void
output_trailing_text()
{
    register int c, last;
    register FILE *in, *out;

    if (line == 0)
	return;

    in = input_file;
    out = code_file;
    c = *cptr;
    if (c == '\n')
    {
	++lineno;
	if ((c = getc(in)) == EOF)
	    return;
	if (!lflag)
	{
	    ++outline;
	    fprintf(out, line_format, lineno, input_file_name);
	}
	if (c == '\n')
	    ++outline;
	putc(c, out);
	last = c;
    }
    else
    {
	if (!lflag)
	{
	    ++outline;
	    fprintf(out, line_format, lineno, input_file_name);
	}
	do { putc(c, out); } while ((c = *++cptr) != '\n');
	++outline;
	putc('\n', out);
	last = '\n';
    }

    while ((c = getc(in)) != EOF)
    {
	if (c == '\n')
	    ++outline;
	putc(c, out);
	last = c;
    }

    if (last != '\n')
    {
	++outline;
	putc('\n', out);
    }
    if (!lflag)
	fprintf(out, line_format, ++outline + 1, code_file_name);
}

void
copy_file(file, file_name)
     FILE ** file;
     char * file_name;
{
  register int c, last;
  register FILE *out;

  fclose(*file);
    *file = fopen(file_name, "r");
    if (*file == NULL)
	open_error(file_name);

    if ((c = getc(*file)) == EOF)
	return;

    out = code_file;
    last = c;
    if (c == '\n')
	++outline;
    putc(c, out);
    while ((c = getc(*file)) != EOF)
    {
	if (c == '\n')
	    ++outline;
	putc(c, out);
	last = c;
    }

    if (last != '\n')
    {
	++outline;
	putc('\n', out);
    }

}

output_semantic_actions()
{
  copy_file (&action_file, action_file_name);
}

output_entries()
{
  copy_file (&entry_file, entry_file_name);
}

free_itemsets()
{
    register core *cp, *next;

    FREE(state_table);
    for (cp = first_state; cp; cp = next)
    {
	next = cp->next;
	FREE(cp);
    }
}


free_shifts()
{
    register shifts *sp, *next;

    FREE(shift_table);
    for (sp = first_shift; sp; sp = next)
    {
	next = sp->next;
	FREE(sp);
    }
}



free_reductions()
{
    register reductions *rp, *next;

    FREE(reduction_table);
    for (rp = first_reduction; rp; rp = next)
    {
	next = rp->next;
	FREE(rp);
    }
}
