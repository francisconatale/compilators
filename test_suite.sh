#!/bin/bash

# Generar archivos de bison y flex
bison -d bison.y
flex lexer.l

# Compilar parser normal
gcc ast.c bison.tab.c lex.yy.c -o parser

# Compilar tests con Unity
echo "Compilando tests..."
gcc -DUNITY_TESTING -I./tests/unity tests/unity/unity.c tests/test_parser.c ast.c bison.tab.c lex.yy.c -o test_runner

# Ejecutar tests
echo "Ejecutando tests con Unity..."
./test_runner
