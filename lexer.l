%{
#include <stdio.h>
#include <string.h>
#include "ast.h"
#include "bison.tab.h"

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
"true"                      { yylval.intval = 1; return BOOL_CONST; }
"false"                     { yylval.intval = 0; return BOOL_CONST; }
{ID}                        { yylval.strval = strdup(yytext); return ID; }
{NUMBER}                    { yylval.intval = atoi(yytext); return NUMBER; }
"+"|"-"|"*"|"="             { return yytext[0]; }
"("|")"|"{"|"}"|";"         { return yytext[0]; }

[ \t\n]+                    { /* eliminar blancos */ }
.                           { printf("Token invalido: %s\n", yytext); }

%%
