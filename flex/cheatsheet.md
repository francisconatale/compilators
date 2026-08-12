flex -> generador de analizadores lexicos
recibe codigo .l con:
1. definicion de patrones
2. reglas lexicas
3. codigo c
y genera codigo c:
flujo : lexer.l -> flex -> lex.yy.c -> gcc -> lexer

`` example.l
%{
#include <stdio.h>
%}

%%

"int" {
    printf("TOKEN: INT\n");
}

%%

int main(void)
{
    yylex();
    return 0;
}
''

para ejecutar:

1. generar codigo c con flex
`` bash
flex lexer.l -> lex.yy.c
``
2. compilar el codigo y generar un ejecutable
`` bash
gcc lex.yy.c -o lexer
``


