/*
 * $Locker$
 *
 * symtab -	symbol table routines
 *
 * Copyright (c) 1988, IBM
 */
#include "sys/types.h"
#include "sym.h"

/*
 * using the toc_syment structure, that we fabricate:
 *	sym->n_offset is the string pointer
 */
#define	sym_off(sym)	((sym)->n_offset)
#define	sym_str(sym)	\
	((sym)->n_zeroes == 0 ? (char *) sym_off(sym) : (sym)->n_name)

sym_t *
sym_lookup(name, value)
char *name; {
	static sym_t *symsrch(), *search();
	char buf[64];
	register sym_t *sym;

	if (name) {
		/*
		 * Heuristic:
		 * first, try just the name. if that fails, try with a
		 * prefix '.', and failing that, a prefix '_'.
		 */
		if (sym = symsrch(name))
			return sym;
		bcopy(name, buf+1, sizeof (buf) - 2);
		buf[0] = '.';

		if (sym = symsrch(buf))
			return sym;

		buf[0] = '_';

		return symsrch(buf);
	} else
		return search(value);
}

static sym_t *
search(addr)
unsigned addr; {
	register sym_t *sp;
	register sym_t *save;
	unsigned value;

	value = 0;
	save  = 0;

	for (sp = toc_syms; sp < &toc_syms[toc_nsyms]; ++sp) {
		if (sp->n_value <= addr && sp->n_value >= value) {
			value = sp->n_value;
			save  = sp;
			if (sp->n_value == addr)
				break;
		}
	}
	return save ? sym_flex(save) : 0;
}

static sym_t *
symsrch(s)
register char *s; {
	register sym_t *sp;
	register sym_t *found;
	register len;
	register char *p;

	/*
	 * determine length of symbol
	 */
	for (len = 0, p = s; *p; ++p)
		++len;

	found = 0;
	for (sp = toc_syms; sp < &toc_syms[toc_nsyms]; ++sp) {
		/*
		 * exact matches preferred
		 */
		if (strcmp(sym_str(sp), s) == 0) {
			found = sp;
			break;
		}
		/*
		 * otherwise, prefices might interest us.
		 */
		if (!found && (strncmp(sym_str(sp), s, len) == 0)) {
			found = sp;
			continue;
		}
	}

	return found ? sym_flex(found) : 0;
}

/*
 * sym_flex -	convert a symbol so that there is no distinction between
 *		flex-string and non flex-string format.
 *
 * Input:
 *	sym	-	^ to symbol table entry
 *
 * Returns:
 *	^ to static location containing modified symbol.
 */
sym_t *
sym_flex(sym)
register sym_t *sym; {
	static sym_t symbol;
	static char name[48];

	strncpy(name, sym_str(sym), sizeof (name) - 1);

	if (sym->n_zeroes != 0)
		name[8] = 0;	/* make sure that we truncate correctly	*/

	symbol          = *sym;
	symbol.n_zeroes = 0;
	symbol.n_nptr   = name;

	return &symbol;
}
