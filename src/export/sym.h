/*
 * $Locker$
 *
 * sym	-	symbol table definitions, VRMIX kernel pseudo-TOC
 */

struct toc_syment {
	union {
		char		_n_name[8];	/* old COFF version */
		struct {
			int	_n_zeroes;	/* new == 0 */
			int	_n_offset;	/* offset into string table */
		} _n_n;
		char		*_n_nptr[2];	/* allows for overlaying */
	} _n;
	int			n_value;	/* value of symbol */
};
#define n_name		_n._n_name
#define n_nptr		_n._n_nptr[1]
#define n_zeroes	_n._n_n._n_zeroes
#define n_offset	_n._n_n._n_offset

typedef struct toc_syment sym_t;

extern struct toc_syment *toc_syms;	/* symbol table		*/
extern caddr_t toc_strs;		/* string table		*/
extern toc_nsyms;			/* # symbols		*/
extern sym_t *sym_flex();
extern sym_t *sym_lookup();
