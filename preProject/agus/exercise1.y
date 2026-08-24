%{
#include <stdio.h>
#include <stdlib.h>

extern int yylex(void);
extern FILE *yyin;
void yyerror(const char *s) {
    fprintf(stderr, "Error sintactico: %s\n", s);
}
%}

%token MAIN VOID INT BOOL RETURN TRUE_CONST FALSE_CONST ID NUMBER

%left '+' '-'
%left '*'

%%

// Agregamos una super-regla para permitir múltiples main en el mismo archivo
TestSuite: TestSuite Program
         | Program { printf("Analisis sintactico exitoso.\n"); }
         ;

Program: FunctionReturnType MAIN '(' ')' '{' Code '}'
    ;

FunctionReturnType: VOID
    | INT
    | BOOL
    ;

Code: Sentence Code
    | /* empty / lambda */
    ;

Sentence: VariableDeclaration
    | VariableAssignment
    | Return
    ;

VariableDeclaration: VariableType ID ';'
    ;

VariableType: INT
    | BOOL
    ;

VariableAssignment: ID '=' Expression ';'
    ;

Expression: Expression '+' Expression
    | Expression '-' Expression
    | Expression '*' Expression
    | '(' Expression ')'
    | NUMBER
    | TRUE_CONST
    | FALSE_CONST
    | ID
    ;

Return: RETURN Expression ';'
    | RETURN ';'
    ;

%%

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
