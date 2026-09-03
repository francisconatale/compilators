%{
#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

extern int yylex(void);
extern FILE *yyin;
void yyerror(const char *s) {
    fprintf(stderr, "Error sintactico: %s\n", s);
}
%}

%token MAIN VOID INT BOOL RETURN

%union {
  int intval;
  char* strval;
  struct ASTNode* node;
}

%token <strval> ID
%token <intval> NUMBER
%token <intval> BOOL_CONST

%type <node> Program Code Sentence VariableDeclaration VariableAssignment Expression Return
%type <intval> FunctionReturnType VariableType

%left '+' '-'
%left '*'

%%

    Program
    : FunctionReturnType MAIN '(' ')' '{' Code '}' {
    $$ = create_program_node($6);
    printf("Analisis sintáctico exitoso y AST construido. \n");
    }
    ;

    FunctionReturnType
    : VOID {$$ = 2;}
    | INT {$$ = 0;}
    | BOOL {$$ = 1;}
    ;

    Code
    : Sentence Code {$$ = create_statement_list_node($1, $2);}
    | /* empty, lambda */ {$$ = NULL;}
    ;

    Sentence
    : VariableDeclaration {$$ = $1;}
    | VariableAssignment {$$ = $1;}
    | Return {$$ = $1;}
    ;

    VariableDeclaration
    : VariableType ID ';' {$$ = create_declaration_node($1, $2);}
    ;

    VariableType
    : INT {$$ = 0;}
    | BOOL {$$ = 1;}
    ;

    VariableAssignment
    : ID '=' Expression ';' {$$ = create_assignment_node($1, $3);}
    ;

    Expression
    : Expression '+' Expression {$$ = create_binop_node($1, '+', $3 );} // Los nros del 1 en adelante representan el nro de elemento que hallamos en la expresion: Exp + Exp es (Exp=1, + = 2, Exp=3)
    | Expression '-' Expression {$$ = create_binop_node($1, '-', $3);}
    | Expression '*' Expression {$$ = create_binop_node($1, '*', $3);}
    | '(' Expression ')' {$$ = $2;} // Ignoramos los parentesis, pues el orden de precedencia queda definido en la estructura del arbol
    | NUMBER {$$ = create_constant_node($1);}
    | BOOL_CONST {$$ = create_constant_node($1);}
    | ID {$$ = create_id_node($1);}
    ;

    Return
    : RETURN Expression ';' {$$ = create_return_node($2);}
    | RETURN ';' {$$ = create_return_node(NULL);}
    ;

%%

#ifndef UNITY_TESTING // notacion que ignora la compilacion del main, ya que tiene que correr el main de la suite de tests
int main(int argc, char** argv) {
    if (argc > 1) {
        yyin = fopen(argv[1], "r");
        if (!yyin) {
            perror("Error abriendo archivo");
            return 1;
        }
    } else {
        yyin = stdin;
    }

    if (yyparse() == 0) {
        // Success handled in the rule
    }

    return 0;
}
#endif
