# Preproyecto

Este preproyecto es el desarrollo de las primeras fases de un compilador para la materia de Compiladores. Implementa el análisis léxico, el análisis sintáctico, la tabla de símbolos y un intérprete que evalúa el código y genera sentencias de tres direcciones (pseudo-assembly).

## Introducción

El compilador está compuesto por las siguientes etapas:

- **Análisis Léxico**: Realizado con Flex, convierte el código fuente en tokens.
- **Análisis Sintáctico**: Realizado con Bison, verifica la gramática y construye el AST.
- **Análisis Semántico e Interpretación**: Recorre el AST validando variables contra la Tabla de Símbolos y evalúa las expresiones matemáticas.

## Estructura

El código está organizado en los siguientes módulos:

- `lexer.l`: Definición de expresiones regulares y tokens.
- `bison.y`: Reglas gramaticales y lógica de construcción del AST.
- `ast.c` / `ast.h`: Definición de la estructura del árbol y manejo de memoria dinámica.
- `symtab.c` / `symtab.h`: Implementación de la tabla de símbolos para el control de variables.
- `interpreter.c` / `interpreter.h`: Funciones de evaluación del AST y generación de código intermedio.
- `build.sh`: Script para automatizar la compilación con gcc.
- `test_suite.sh`: Script para correr las pruebas unitarias.

## Requisitos

Para compilar y ejecutar el proyecto en un entorno Linux se necesita:
- `gcc` 
- `flex`
- `bison`

## Compilación y Uso

Para construir el ejecutable principal, simplemente ejecuta el script de compilación provisto:

```bash
./build.sh
```

Esto generará los archivos de C a partir de Flex/Bison y compilará todo el proyecto. El binario resultante se llamará `mi_compilador` y se guardará en la raíz del proyecto.

Para ejecutar el compilador pasándole el código fuente de prueba (`prueba.c--`):

```bash
./mi_compilador prueba.c--
```

## Decisiones y progreso actual

Se redacto un informe en /documentation/preproject.pdf profundizando el codigo fuente y lo realizado hasta el momento  
