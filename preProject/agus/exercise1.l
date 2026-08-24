%{
#include <stdio.h>

int yywrap(void) {
    return 1;
}
%}

DIGIT       [0-9]
CHAR        [a-zA-Z]
ID          {CHAR}({CHAR}|{DIGIT})*
NUMBER      {DIGIT}+

%%

main|void|int|bool|return   { printf("Palabra reservada: %s\n", yytext); }
true|false                  { printf("Constante logica: %s\n", yytext); }
{ID}                        { printf("Variable: %s\n", yytext); }
{NUMBER}                    { printf("Constante entera: %s\n", yytext); }
"+"|"-"|"*"|"="             { printf("Operador: %s\n", yytext); }
"("|")"|"{"|"}"|";"         { printf("Delimitador: %s\n", yytext); }

[ \t\n]+                    { /* eliminar blancos */ }
.                           { printf("Token invalido: %s\n", yytext); }

%%

int main(int argc, char** argv) {
    /* Segun la diapo 19 del PDF, se puede leer de archivo o stdin */
    if (argc > 1) {
        yyin = fopen(argv[1], "r");
    } else {
        yyin = stdin;
    }
    
    yylex();
    return 0;
}
