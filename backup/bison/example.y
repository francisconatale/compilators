%{
#include <stdio.h>

int yylex(void);

void yyerror(const char *s)
{
    fprintf(stderr, "Error: %s\n", s);
}
%}

%token SIMBOLO
%token OPERANDO

%%

expresion:
      SIMBOLO
    | expresion OPERANDO SIMBOLO
    ;

%%

int main(void)
{
    return yyparse();
}