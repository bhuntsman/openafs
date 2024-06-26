/* @(#)rpc_cout.c	1.1 87/11/04 3.9 RPCSRC */
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

/*
 * rpc_cout.c, XDR routine outputter for the RPC protocol compiler 
 * Copyright (C) 1987, Sun Microsystems, Inc. 
 */
#include <afs/param.h>
#include <stdio.h>
#if defined(AFS_SUN5_ENV) || defined(AFS_NT40_ENV)
#include <string.h>
#else
#include <strings.h>
#endif
#include "rpc_util.h"
#include "rpc_parse.h"

static print_header();
static print_trailer();
static space();
static emit_enum();
static emit_union();
static emit_struct();
static emit_typedef();
static print_stat();

/*
 * Emit the C-routine for the given definition 
 */
void
emit(def)
	definition *def;
{
	if (def->def_kind == DEF_PROGRAM || def->def_kind == DEF_CONST) {
		return;
	}
	print_header(def);
	switch (def->def_kind) {
	case DEF_UNION:
		emit_union(def);
		break;
	case DEF_ENUM:
		emit_enum(def);
		break;
	case DEF_STRUCT:
		emit_struct(def);
		break;
	case DEF_TYPEDEF:
		emit_typedef(def);
		break;
	}
	print_trailer();
}

static
findtype(def, type)
	definition *def;
	char *type;
{
	if (def->def_kind == DEF_PROGRAM || def->def_kind == DEF_CONST) {
		return (0);
	} else {
		return (streq(def->def_name, type));
	}
}

static
undefined(type)
	char *type;
{
	definition *def;

	def = (definition *) FINDVAL(defined, type, findtype);
	return (def == NULL);
}


static
print_header(def)
	definition *def;
{
	space();
	f_print(fout, "bool_t\n");
	f_print(fout, "xdr_%s(xdrs, objp)\n", def->def_name);
	f_print(fout, "\tXDR *xdrs;\n");
	f_print(fout, "\t%s ", def->def_name);
	if (def->def_kind != DEF_TYPEDEF ||
	    !isvectordef(def->def.ty.old_type, def->def.ty.rel)) {
		f_print(fout, "*");
	}
	f_print(fout, "objp;\n");
	f_print(fout, "{\n");
}

static
print_trailer()
{
	f_print(fout, "\treturn (TRUE);\n");
	f_print(fout, "}\n");
	space();
}


static
print_ifopen(indent, name)
	int indent;
	char *name;
{
	tabify(fout, indent);
	f_print(fout, "if (!xdr_%s(xdrs", name);
}


static
print_ifarg(arg)
	char *arg;
{
	f_print(fout, ", %s", arg);
}


static
print_ifsizeof(prefix, type)
	char *prefix;
	char *type;
{
	if (streq(type, "bool")) {
		f_print(fout, ", sizeof(bool_t), xdr_bool");
	} else {
		f_print(fout, ", sizeof(");
		if (undefined(type) && prefix) {
			f_print(fout, "%s ", prefix);
		}
		f_print(fout, "%s), xdr_%s", type, type);
	}
}

static
print_ifclose(indent)
	int indent;
{
	f_print(fout, ")) {\n");
	tabify(fout, indent);
	f_print(fout, "\treturn (FALSE);\n");
	tabify(fout, indent);
	f_print(fout, "}\n");
}

static
space()
{
	f_print(fout, "\n\n");
}

static
print_ifstat(indent, prefix, type, rel, amax, objname, name)
	int indent;
	char *prefix;
	char *type;
	relation rel;
	char *amax;
	char *objname;
	char *name;
{
	char *alt = NULL;

	switch (rel) {
	case REL_POINTER:
		print_ifopen(indent, "pointer");
		print_ifarg("(char **)");
		f_print(fout, "%s", objname);
		print_ifsizeof(prefix, type);
		break;
	case REL_VECTOR:
		if (streq(type, "string")) {
			alt = "string";
		} else if (streq(type, "opaque")) {
			alt = "opaque";
		}
		if (alt) {
			print_ifopen(indent, alt);
			print_ifarg(objname);
		} else {
			print_ifopen(indent, "vector");
			print_ifarg("(char *)");
			f_print(fout, "%s", objname);
		}
		print_ifarg(amax);
		if (!alt) {
			print_ifsizeof(prefix, type);
		}
		break;
	case REL_ARRAY:
		if (streq(type, "string")) {
			alt = "string";
		} else if (streq(type, "opaque")) {
			alt = "bytes";
		}
		if (streq(type, "string")) {
			print_ifopen(indent, alt);
			print_ifarg(objname);
		} else {
			if (alt) {
				print_ifopen(indent, alt);
			} else {
				print_ifopen(indent, "array");
			}
			print_ifarg("(char **)");
			if (*objname == '&') {
				f_print(fout, "%s.%s_val, (u_int *)%s.%s_len",
					objname, name, objname, name);
			} else {
				f_print(fout, "&%s->%s_val, (u_int *)&%s->%s_len",
					objname, name, objname, name);
			}
		}
		print_ifarg(amax);
		if (!alt) {
			print_ifsizeof(prefix, type);
		}
		break;
	case REL_ALIAS:
		print_ifopen(indent, type);
		print_ifarg(objname);
		break;
	}
	print_ifclose(indent);
}


/* ARGSUSED */
static
emit_enum(def)
	definition *def;
{
	print_ifopen(1, "enum");
	print_ifarg("(enum_t *)objp");
	print_ifclose(1);
}


static
emit_union(def)
	definition *def;
{
	declaration *dflt;
	case_list *cl;
	declaration *cs;
	char *object;
	char *format = "&objp->%s_u.%s";

	print_stat(&def->def.un.enum_decl);
	f_print(fout, "\tswitch (objp->%s) {\n", def->def.un.enum_decl.name);
	for (cl = def->def.un.cases; cl != NULL; cl = cl->next) {
		cs = &cl->case_decl;
		f_print(fout, "\tcase %s:\n", cl->case_name);
		if (!streq(cs->type, "void")) {
			object = alloc(strlen(def->def_name) + strlen(format) +
				       strlen(cs->name) + 1);
			s_print(object, format, def->def_name, cs->name);
			print_ifstat(2, cs->prefix, cs->type, cs->rel, cs->array_max,
				     object, cs->name);
			free(object);
		}
		f_print(fout, "\t\tbreak;\n");
	}
	dflt = def->def.un.default_decl;
	if (dflt != NULL) {
		if (!streq(dflt->type, "void")) {
			f_print(fout, "\tdefault:\n");
			object = alloc(strlen(def->def_name) + strlen(format) +
				       strlen(dflt->name) + 1);
			s_print(object, format, def->def_name, dflt->name);
			print_ifstat(2, dflt->prefix, dflt->type, dflt->rel,
				     dflt->array_max, object, dflt->name);
			free(object);
			f_print(fout, "\t\tbreak;\n");
		}
	} else {
		f_print(fout, "\tdefault:\n");
		f_print(fout, "\t\treturn (FALSE);\n");
	}
	f_print(fout, "\t}\n");
}



static
emit_struct(def)
	definition *def;
{
	decl_list *dl;

	for (dl = def->def.st.decls; dl != NULL; dl = dl->next) {
		print_stat(&dl->decl);
	}
}




static
emit_typedef(def)
	definition *def;
{
	char *prefix = def->def.ty.old_prefix;
	char *type = def->def.ty.old_type;
	char *amax = def->def.ty.array_max;
	relation rel = def->def.ty.rel;

	print_ifstat(1, prefix, type, rel, amax, "objp", def->def_name);
}





static
print_stat(dec)
	declaration *dec;
{
	char *prefix = dec->prefix;
	char *type = dec->type;
	char *amax = dec->array_max;
	relation rel = dec->rel;
	char name[256];

	if (isvectordef(type, rel)) {
		s_print(name, "objp->%s", dec->name);
	} else {
		s_print(name, "&objp->%s", dec->name);
	}
	print_ifstat(1, prefix, type, rel, amax, name, dec->name);
}

extern proc1_list *Proc_list;
extern int PerProcCounter;

static
print_hout(dec)
declaration *dec;
{
    char prefix[8];

    if (hflag) {
	if (dec->prefix)
	    s_print(prefix, "%s ", dec->prefix);
	else
	    prefix[0] = 0;
	f_print(fout, "\ntypedef ");
	switch (dec->rel) {
		case REL_ARRAY:
		    f_print(fout, "struct %s {\n", dec->name);
		    f_print(fout, "\tu_int %s_len;\n", dec->name);
		    f_print(fout, "\t%s%s *%s_val;\n", prefix, dec->type, dec->name);
		    f_print(fout, "} %s", dec->name);
		    break;
	}
	f_print(fout, ";\n");
	f_print(fout, "bool_t xdr_%s();\n", dec->name);
    }
}


static
print_cout(dec)
declaration *dec;
{

    if (cflag) {
	space();
	f_print(fout, "bool_t\n");
	f_print(fout, "xdr_%s(xdrs, objp)\n", dec->name);
	f_print(fout, "\tXDR *xdrs;\n");
	f_print(fout, "\t%s *objp;\n", dec->name);
	f_print(fout, "{\n");
	print_ifstat(1, dec->prefix, dec->type, dec->rel, dec->array_max, "objp", dec->name);
	print_trailer();
    }
}


static
print_rxifopen(typename)
char *typename;
{
    sprintf(Proc_list->code, "xdr_%s(&z_xdrs", typename);
    sprintf(Proc_list->scode, "xdr_%s(z_xdrs", typename);
}


static
print_rxifarg(amp, arg, costant)
char *amp;
char *arg;
int costant;
{
    char code[100], scode[100];

    sprintf(code, ", %s%s", amp, arg);
    if (costant)
	sprintf(scode, ", %s", arg);
    else
	sprintf(scode, ", &%s", arg);
    strcat(Proc_list->code, code);
    strcat(Proc_list->scode, scode);
}


static
print_rxifsizeof(prefix, type)
char *prefix, *type;
{
    char name[256];

    if (streq(type, "bool")) {
	strcat(Proc_list->code, ", sizeof(bool_t), xdr_bool");
	strcat(Proc_list->scode, ", sizeof(bool_t), xdr_bool");
    } else {
	strcat(Proc_list->code, ", sizeof(");
	strcat(Proc_list->scode, ", sizeof(");
	if (undefined(type) && prefix) {
	    sprintf(name, "%s ", prefix);
	    strcat(Proc_list->code, name);
	    strcat(Proc_list->scode, name);
	}
	sprintf(name, "%s), xdr_%s", type, type);
	strcat(Proc_list->code, name);
	strcat(Proc_list->scode, name);
    }
}


print_param(dec)
declaration *dec;
{
	char *prefix = dec->prefix;
	char *type = dec->type;
	char *amax = dec->array_max;
	relation rel = dec->rel;
	char *name = dec->name;
	char *alt = NULL;
	char temp[256];
	char *objname, *amp="";

	if (rel == REL_POINTER)
	    Proc_list->pl.param_flag |= INDIRECT_PARAM;
	else {
	    amp = "&";
	    Proc_list->pl.param_flag &= ~INDIRECT_PARAM;
	}   
	objname = Proc_list->pl.param_name;
	switch (rel) {
	case REL_POINTER:
	        print_rxifopen(type);
		print_rxifarg(amp, objname, 0);
/*
	        print_rxifopen("pointer");
		print_rxifarg(amp, "(char **)", 1);
		sprintf(temp, "%s", objname);
		strcat(Proc_list->code, temp);
		strcat(Proc_list->scode, temp);
		print_rxifsizeof(prefix, type);
*/
		break;
	case REL_VECTOR:
		if (streq(type, "string")) {
			alt = "string";
		} else if (streq(type, "opaque")) {
			alt = "opaque";
		}
		if (alt) {
			print_rxifopen(alt);
			print_rxifarg(amp, objname, 0);
		} else {
			print_rxifopen("vector");
			print_rxifarg(amp, "(char *)", 0);
			sprintf(temp, "%s", objname);
			strcat(Proc_list->code, temp);
			strcat(Proc_list->scode, temp);			
		}
		print_rxifarg("", amax, 1);
		if (!alt) {
			print_rxifsizeof(prefix, type);
		}
		break;
	case REL_ARRAY:
		if (streq(type, "string")) {
			alt = "string";
		} else if (streq(type, "opaque")) {
			alt = "bytes";
		}
		if (streq(type, "string")) {
			print_rxifopen(alt);
			if ((Proc_list->pl.param_kind == DEF_OUTPARAM) ||
			    (Proc_list->pl.param_kind == DEF_INOUTPARAM)) {
			    Proc_list->pl.param_flag |= OUT_STRING;
			    print_rxifarg("", objname, 0);
			} else
			    print_rxifarg("&", objname, 0);
/*			print_rxifarg(amp, objname, 0);	*/
			print_rxifarg("", amax, 1);
			if (!alt) {
			    print_rxifsizeof(prefix, type);
			}
		} else {
char typecontents[100];

		    print_hout(dec);
		    print_cout(dec);
		    strcpy(temp, dec->name);
		    strcpy(typecontents, dec->name);
		    strcat(typecontents, " *");
		    strcpy(Proc_list->pl.param_type, typecontents);
		    sprintf(typecontents, "%s_%d", Proc_list->pl.param_name, ++PerProcCounter);
		    strcpy(Proc_list->pl.param_name, typecontents);
		    Proc_list->pl.param_flag |= FREETHIS_PARAM;
		    print_rxifopen(temp);
		    print_rxifarg(amp, name, 0);
		}
/*
			if (alt) {
				print_rxifopen(alt);
			} else {
				print_rxifopen("array");
			}
			print_rxifarg(amp, "(char **)", 1);
			if (*objname == '&') {
				sprintf(temp, "%s.%s_val, (u_int *)%s.%s_len",
					objname, name, objname, name);
			} else {
				sprintf(temp, "&%s->%s_val, (u_int *)&%s->%s_len",
					objname, name, objname, name);
			}
			strcat(Proc_list->code, temp);
			strcat(Proc_list->scode, temp);
		}
		print_rxifarg("", amax, 1);
		if (!alt) {
			print_rxifsizeof(prefix, type);
		}
*/
		break;
	case REL_ALIAS:
	        print_rxifopen(type);
		print_rxifarg(amp, objname, 0);
		break;
	}
	strcat(Proc_list->code, ")");
	strcat(Proc_list->scode, ")");
}
