%{
#include <stdio.h>
#include "example.tab.h"
%}

SIMBOLO  [0-9]
OPERANDO [-+]

%%

{SIMBOLO}+ {
    printf("SIMBOLO: %s\n", yytext);
    return SIMBOLO;
}

{OPERANDO} {
    printf("OPERANDO: %s\n", yytext);
    return OPERANDO;
}

[ \t\n] ;

. {
    printf("CARACTER INVALIDO: %s\n", yytext);
    return 0;
}

%%