%{
#include <stdio.h>
%}


DIGITO [0-9]
%%

{DIGITO}+ {
    printf("NUMBER(%s)\n", yytext);
}

"+" {
    printf("PLUS\n");
}

[ \t\n]+ ;

. {
    printf("UNKNOWN(%s)\n", yytext);
}

%%

int main(void)
{
    yylex();
    return 0;
}

int yywrap(void)
{
    return 1;
}
