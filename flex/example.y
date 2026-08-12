%{
#include <stdio.h>

int yylex(void);

void yyerror(const char *s)
{
    printf("NO PERTENECE\n");
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
    if (yyparse() == 0)
        printf("PERTENECE\n");
    else
        printf("NO PERTENECE\n");

    return 0;
}