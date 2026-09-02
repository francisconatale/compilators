# Flex + Bison

## Visión general

Flex y Bison forman una cadena de análisis de un lenguaje.

```
Código fuente
      |
      v
    FLEX
      |
      | tokens
      v
   BISON
      |
      | análisis sintáctico
      v
Acepta / Rechaza
```

Flex recibe una secuencia de caracteres y determina qué tokens contiene.


## Conexión entre Flex y Bison

Bison genera un archivo `.h` con las definiciones de los tokens.

Al ejecutar:

```
bison -d example.y
```

se generan:

```
example.tab.c
example.tab.h
```

El lexer incluye ese archivo:

```c
#include "example.tab.h"
```

De esta manera ambos utilizan los mismos tokens.

```
              example.y
                  |
                Bison
                  |
          +-------+-------+
          |               |
          v               v
   example.tab.c    example.tab.h
                          |
                          |
                       lexer.l
                          |
                         Flex
                          |
                          v
                      lex.yy.c
```

## Bison

Bison recibe los tokens producidos por Flex y comprueba si forman una estructura válida según la gramática.


##  Gramática en Bison

Una producción matemática:

```
A → B C
```

se escribe:

```
A:
    B C
    ;
```

Una producción con alternativas:

```
A → B | C
```

se escribe:

```
A:
      B
    | C
    ;
```

El símbolo `|` representa una alternativa de la gramática.

## Recursividad

Las gramáticas pueden definirse recursivamente.

Por ejemplo:

```
expresion → SIMBOLO
          | expresion OPERANDO SIMBOLO
```

En Bison:

```c
expresion:
      SIMBOLO
    | expresion OPERANDO SIMBOLO
    ;
```

Esto permite reconocer:

```
10
10+20
10+20-30
10+20-30+40
```

La recursividad permite expresar una cantidad arbitraria de operaciones sin tener que escribir una regla para cada cantidad.

## yyparse()

`yyparse()` es la función principal del parser generado por Bison.

No la escribimos nosotros.

Bison la genera automáticamente dentro de `example.tab.c`.

Normalmente el `main` llama:

```c
int main(void)
{
    return yyparse();
}
```

`yyparse()` controla el análisis sintáctico y solicita tokens a Flex mediante `yylex()`.

## yylex()

`yylex()` es la función que realiza el análisis léxico.

Cuando Bison necesita otro token:

```
yyparse()
    |
    | necesita token
    v
yylex()
    |
    | analiza caracteres
    v
return SIMBOLO
```

Flex genera automáticamente `yylex()` a partir del archivo `.l`.

Por lo tanto:

- `yyparse()` → Parser / Bison
- `yylex()` → Lexer / Flex

## Flujo durante la ejecución

Supongamos la entrada:

```
10+20-30
```

El flujo es:

```
                  "10+20-30"
                       |
                       v
                     Flex
                       |
                       v
      SIMBOLO OPERANDO SIMBOLO OPERANDO SIMBOLO
                       |
                       v
                     Bison
                       |
                       v
                    yyparse()
                       |
                       v
                  expresion
                       |
                 +-----+-----+
                 |           |
                 v           v
             válida       inválida
                 |           |
                 v           v
            PERTENECE   NO PERTENECE
```

## Compilación

Partiendo de:

```
example.y
lexer.l
```

### Paso 1: generar Bison

```
bison -d example.y
```

Genera:

```
example.tab.c
example.tab.h
```

### Paso 2: generar Flex

```
flex lexer.l
```

Genera:

```
lex.yy.c
```

### Paso 3: compilar

```
gcc example.tab.c lex.yy.c -o example -lfl
```

Genera:

```
example
```

## Orden completo

```
1. Definir tokens y gramática en Bison
              |
              v
2. bison -d example.y
              |
              v
3. Se genera example.tab.h
              |
              v
4. Flex incluye example.tab.h
              |
              v
5. flex lexer.l
              |
              v
6. gcc example.tab.c lex.yy.c -o example -lfl
              |
              v
7. Ejecutar ./example
```


