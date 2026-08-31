#!/bin/bash

# compilo
bison -d exercise1.y
flex exercise1.l
gcc ast.c exercise1.tab.c lex.yy.c -o parser

# pruebas
echo ">> test 1: todo ok"
echo "void main() { int x; x = 1; }" | ./parser

echo ">> test 2: booleanos"
echo "bool main() { bool a; a = true; return a; }" | ./parser

echo ">> test 3: math"
echo "void main() { int p; p = (2 + 3) * 4; }" | ./parser

echo ">> test 4: error de sintaxis (asignar en la declaracion)"
echo "void main() { int x = 1; }" | ./parser

echo ">> test 5: falta ;"
echo "void main() { int x }" | ./parser

echo ">> test 6: no codigo "
echo "void main(){}" | ./parser

echo ">> test 7: sin funcion "
echo "int x; x=1;" | ./parser
