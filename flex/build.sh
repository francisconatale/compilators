#!/bin/bash
set -e

SRC="${1:-lexer.l}"
OUT="${2:-lexer}"

flex "$SRC"
gcc lex.yy.c -lfl -o "$OUT"
echo "OK: $OUT generado"