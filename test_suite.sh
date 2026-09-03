#!/bin/bash

# Create build directory
mkdir -p build

# Generar archivos de bison y flex en el directorio build
bison -d bison.y -o build/bison.tab.c
flex -o build/lex.yy.c lexer.l

# Compilar parser normal
gcc -I. -Ibuild ast.c build/bison.tab.c build/lex.yy.c -o build/parser

# Compilar tests con Unity
echo "Compilando tests..."
gcc -DUNITY_TESTING -I. -Ibuild -I./tests/unity tests/unity/unity.c tests/test_parser.c ast.c build/bison.tab.c build/lex.yy.c -o build/test_runner

# Ejecutar tests
echo "Ejecutando tests con Unity..."
./build/test_runner
