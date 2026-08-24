%{
#include <stdio.h>
#include "exercise1.tab.h"

int yywrap(void) {
    return 1;
}
%}

DIGIT       [0-9]
CHAR        [a-zA-Z]
ID          {CHAR}({CHAR}|{DIGIT})*
NUMBER      {DIGIT}+

%%

"main"                      { return MAIN; }
"void"                      { return VOID; }
"int"                       { return INT; }
"bool"                      { return BOOL; }
"return"                    { return RETURN; }
"true"                      { return TRUE_CONST; }
"false"                     { return FALSE_CONST; }
{ID}                        { return ID; }
{NUMBER}                    { return NUMBER; }
"+"|"-"|"*"|"="             { return yytext[0]; }
"("|")"|"{"|"}"|";"         { return yytext[0]; }

"//".*                    { /* ignorar comentarios */ }
[ \t\n]+                    { /* eliminar blancos */ }
.                           { printf("Token invalido: %s\n", yytext); }

%%
