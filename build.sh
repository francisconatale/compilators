#!/bin/bash

# Crear directorio de compilación si no existe
mkdir -p build

# Generar archivos de bison y flex en el directorio build
bison -d bison.y -o build/bison.tab.c
flex -o build/lex.yy.c lexer.l

# Compilar parser normal
echo "Compilando parser..."
gcc -I. -Ibuild ast.c build/bison.tab.c build/lex.yy.c -o build/parser

echo "Build finalizado. El ejecutable se encuentra en build/parser"
