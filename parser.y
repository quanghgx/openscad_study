/*
 *  OpenSCAD (www.openscad.at)
 *  Copyright (C) 2009  Clifford Wolf <clifford@clifford.at>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

%{

#include "openscad.h"

int parserlex(void);
void yyerror(char const *s);

int lexerget_lineno(void);
int lexerlex_destroy(void);
int lexerlex(void);

QVector<Module*> module_stack;
Module *module;

class ArgContainer {
public:
	QString argname;
	Expression *argexpr;
};
class ArgsContainer {
public:
	QVector<QString> argnames;
	QVector<Expression*> argexpr;
};

%}

%union {
	char *text;
	double number;
	class Value *value;
	class Expression *expr;
	class ModuleInstanciation *inst;
	class ArgContainer *arg;
	class ArgsContainer *args;
}

%token TOK_MODULE
%token TOK_FUNCTION

%token <text> TOK_ID
%token <text> TOK_STRING
%token <number> TOK_NUMBER

%token TOK_TRUE
%token TOK_FALSE
%token TOK_UNDEF

%token LE GE EQ NE AND OR

%left '[' ']'
%left '!' '+' '-'
%left '*' '/' '%'
%left '.'
%left '<' LE GE '>'
%left EQ NE
%left AND
%left OR
%right '?' ':'

%type <expr> expr
%type <value> vector_const
%type <expr> vector_expr

%type <inst> module_instantciation
%type <inst> module_instantciation_list
%type <inst> single_module_instantciation

%type <args> arguments_call
%type <args> arguments_decl

%type <arg> argument_call
%type <arg> argument_decl

%debug

%%

input: 
	/* empty */ |
	statement input ;

statement:
	';' |
	'{' input '}' |
	module_instantciation {
		if ($1) {
			module->children.append($1);
		} else {
			delete $1;
		}
	} |
	TOK_ID '=' expr ';' {
		module->assignments_var.append($1);
		module->assignments_expr.append($3);
		free($1);
	} |
	TOK_MODULE TOK_ID '(' arguments_decl ')' {
		Module *p = module;
		module_stack.append(module);
		module = new Module();
		p->modules[$2] = module;
		module->argnames = $4->argnames;
		module->argexpr = $4->argexpr;
		free($2);
		delete $4;
	} statement {
		module = module_stack.last();
		module_stack.pop_back();
	} |
	TOK_FUNCTION TOK_ID '(' arguments_decl ')' '=' expr {
		Function *func = new Function();
		func->argnames = $4->argnames;
		func->argexpr = $4->argexpr;
		func->expr = $7;
		module->functions[$2] = func;
		free($2);
		delete $4;
	} ';' ;

module_instantciation:
	single_module_instantciation ';' {
		$$ = $1;
	} |
	single_module_instantciation '{' module_instantciation_list '}' {
		$$ = $1;
		if ($$) {
			$$->children = $3->children;
		} else {
			for (int i = 0; i < $3->children.count(); i++)
				delete $3->children[i];
		}
		$3->children.clear();
		delete $3;
	} |
	single_module_instantciation module_instantciation {
		$$ = $1;
		if ($$) {
			if ($2)
				$$->children.append($2);
		} else {
			delete $2;
		}
	} ;

module_instantciation_list:
	/* empty */ {
		$$ = new ModuleInstanciation();
	} |
	module_instantciation_list module_instantciation {
		$$ = $1;
		if ($$) {
			if ($2)
				$$->children.append($2);
		} else {
			delete $2;
		}
	} ;

single_module_instantciation:
	TOK_ID '(' arguments_call ')' {
		$$ = new ModuleInstanciation();
		$$->modname = QString($1);
		$$->argnames = $3->argnames;
		$$->argexpr = $3->argexpr;
		free($1);
		delete $3;
	} |
	TOK_ID ':' single_module_instantciation {
		$$ = $3;
		if ($$)
			$$->label = QString($1);
		free($1);
	} |
	'!' single_module_instantciation {
		$$ = $2;
		if ($$)
			$$->tag_root = true;
	} |
	'#' single_module_instantciation {
		$$ = $2;
		if ($$)
			$$->tag_highlight = true;
	} |
	'%' single_module_instantciation {
		$$ = $2;
		if ($$)
			$$->tag_background = true;
	} |
	'*' single_module_instantciation {
		delete $2;
		$$ = NULL;
	};

expr:
	TOK_TRUE {
		$$ = new Expression();
		$$->type = "C";
		$$->const_value = new Value(true);
	} |
	TOK_FALSE {
		$$ = new Expression();
		$$->type = "C";
		$$->const_value = new Value(false);
	} |
	TOK_UNDEF {
		$$ = new Expression();
		$$->type = "C";
		$$->const_value = new Value();
	} |
	TOK_ID {
		$$ = new Expression();
		$$->type = "L";
		$$->var_name = QString($1);
		free($1);
	} |
	expr '.' TOK_ID {
		$$ = new Expression();
		$$->type = "N";
		$$->children.append($1);
		$$->var_name = QString($3);
		free($3);
	} |
	TOK_STRING {
		$$ = new Expression();
		$$->type = "C";
		$$->const_value = new Value(QString($1));
		free($1);
	} |
	TOK_NUMBER {
		$$ = new Expression();
		$$->type = "C";
		$$->const_value = new Value($1);
	} |
	'[' expr ':' expr ']' {
		Expression *e_one = new Expression();
		e_one->type = "C";
		e_one->const_value = new Value(1.0);
		$$ = new Expression();
		$$->type = "R";
		$$->children.append($2);
		$$->children.append(e_one);
		$$->children.append($4);
	} |
	'[' expr ':' expr ':' expr ']' {
		$$ = new Expression();
		$$->type = "R";
		$$->children.append($2);
		$$->children.append($4);
		$$->children.append($6);
	} |
	'[' ']' {
		$$ = new Expression();
		$$->type = "C";
		$$->const_value = new Value();
		$$->const_value->type = Value::VECTOR;
	} |
	'[' vector_const ']' {
		$$ = new Expression();
		$$->type = "C";
		$$->const_value = $2;
	} |
	'[' vector_expr ']' {
		$$ = $2;
	} |
	expr '*' expr {
		$$ = new Expression();
		$$->type = "*";
		$$->children.append($1);
		$$->children.append($3);
	} |
	expr '/' expr {
		$$ = new Expression();
		$$->type = "/";
		$$->children.append($1);
		$$->children.append($3);
	} |
	expr '%' expr {
		$$ = new Expression();
		$$->type = "%";
		$$->children.append($1);
		$$->children.append($3);
	} |
	expr '+' expr {
		$$ = new Expression();
		$$->type = "+";
		$$->children.append($1);
		$$->children.append($3);
	} |
	expr '-' expr {
		$$ = new Expression();
		$$->type = "-";
		$$->children.append($1);
		$$->children.append($3);
	} |
	expr '<' expr {
		$$ = new Expression();
		$$->type = "<";
		$$->children.append($1);
		$$->children.append($3);
	} |
	expr LE expr {
		$$ = new Expression();
		$$->type = "<=";
		$$->children.append($1);
		$$->children.append($3);
	} |
	expr EQ expr {
		$$ = new Expression();
		$$->type = "==";
		$$->children.append($1);
		$$->children.append($3);
	} |
	expr NE expr {
		$$ = new Expression();
		$$->type = "!=";
		$$->children.append($1);
		$$->children.append($3);
	} |
	expr GE expr {
		$$ = new Expression();
		$$->type = ">=";
		$$->children.append($1);
		$$->children.append($3);
	} |
	expr '>' expr {
		$$ = new Expression();
		$$->type = "<";
		$$->children.append($1);
		$$->children.append($3);
	} |
	expr AND expr {
		$$ = new Expression();
		$$->type = "&&";
		$$->children.append($1);
		$$->children.append($3);
	} |
	expr OR expr {
		$$ = new Expression();
		$$->type = "||";
		$$->children.append($1);
		$$->children.append($3);
	} |
	'+' expr {
		$$ = $2;
	} |
	'-' expr {
		$$ = new Expression();
		$$->type = "I";
		$$->children.append($2);
	} |
	'!' expr {
		$$ = new Expression();
		$$->type = "!";
		$$->children.append($2);
	} |
	'(' expr ')' {
		$$ = $2;
	} |
	expr '?' expr ':' expr {
		$$ = new Expression();
		$$->type = "?:";
		$$->children.append($1);
		$$->children.append($3);
		$$->children.append($5);
	} |
	expr '[' expr ']' {
		$$ = new Expression();
		$$->type = "[]";
		$$->children.append($1);
		$$->children.append($3);
	} |
	TOK_ID '(' arguments_call ')' {
		$$ = new Expression();
		$$->type = "F";
		$$->call_funcname = QString($1);
		$$->call_argnames = $3->argnames;
		$$->children = $3->argexpr;
		free($1);
		delete $3;
	} ;

vector_const:
	TOK_NUMBER TOK_NUMBER {
		$$ = new Value();
		$$->type = Value::VECTOR;
		$$->vec.append(new Value($1));
		$$->vec.append(new Value($2));
	} |
	vector_const TOK_NUMBER {
		$$ = $1;
		$$->vec.append(new Value($2));
	} ;

vector_expr:
	expr {
		$$ = new Expression();
		$$->type = 'V';
		$$->children.append($1);
	} |
	vector_expr ',' expr {
		$$ = $1;
		$$->children.append($3);
	} ;

arguments_decl:
	/* empty */ {
		$$ = new ArgsContainer();
	} |
	argument_decl {
		$$ = new ArgsContainer();
		$$->argnames.append($1->argname);
		$$->argexpr.append($1->argexpr);
		delete $1;
	} |
	arguments_decl ',' argument_decl {
		$$ = $1;
		$$->argnames.append($3->argname);
		$$->argexpr.append($3->argexpr);
		delete $3;
	} ;

argument_decl:
	TOK_ID {
		$$ = new ArgContainer();
		$$->argname = QString($1);
		$$->argexpr = NULL;
		free($1);
	} |
	TOK_ID '=' expr {
		$$ = new ArgContainer();
		$$->argname = QString($1);
		$$->argexpr = $3;
		free($1);
	} ;

arguments_call:
	/* empty */ {
		$$ = new ArgsContainer();
	} |
	argument_call {
		$$ = new ArgsContainer();
		$$->argnames.append($1->argname);
		$$->argexpr.append($1->argexpr);
		delete $1;
	} |
	arguments_call ',' argument_call {
		$$ = $1;
		$$->argnames.append($3->argname);
		$$->argexpr.append($3->argexpr);
		delete $3;
	} ;

argument_call:
	expr {
		$$ = new ArgContainer();
		$$->argexpr = $1;
	} |
	TOK_ID '=' expr {
		$$ = new ArgContainer();
		$$->argname = QString($1);
		$$->argexpr = $3;
		free($1);
	} ;

%%

int parserlex(void)
{
	return lexerlex();
}

void yyerror (char const *s)
{
	// FIXME: We leak memory on parser errors...
	PRINTF("Parser error in line %d: %s\n", lexerget_lineno(), s);
	module = NULL;
}

extern FILE *lexerin;
extern const char *parser_input_buffer;
const char *parser_input_buffer;

AbstractModule *parse(const char *text, int debug)
{
	lexerin = NULL;
	parser_input_buffer = text;

	module_stack.clear();
	module = new Module();

	parserdebug = debug;
	parserparse();

	lexerlex_destroy();

	return module;
}

