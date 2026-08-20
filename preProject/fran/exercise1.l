%{
#include<stdio.h>


int yywrap(void){ // avisa a yylex() que no tiene que leer mas
	return 1;
}
%}
VAR ("int"|"bool")
NAME_OF_VAR [a-zA-Z][a-zA-Z0-9]*
RETURN "return"
BOOLEAN ("true"|"false")
NUMBER [0-9]
%%

{VAR}[ \t]+{NAME_OF_VAR}";" { printf("declaracion encontrada (%s)", yytext);}
{RETURN}[ \t]*({NAME_OF_VAR}|{BOOLEAN}|{NUMBER})*";" { printf("return encontrado(%s)",yytext); }
[ \t\n]+    ;
. { printf("lexema desconocido (%s)", yytext); }
%%

void main(){
yylex();
}
