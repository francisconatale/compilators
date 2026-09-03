# Documentación del Preproyecto: Construcción de un Compilador y Generación de AST

Esta documentación describe la arquitectura, conceptos teóricos, detalles de implementación y herramientas utilizadas en el desarrollo del preproyecto de Compiladores (Puntos 1 al 5), además de establecer las bases para los pasos siguientes (Puntos 6 y 7).

---

## 1. Fases en la Construcción de un Compilador

La construcción de un compilador estándar se divide en varias fases secuenciales:

```
+------------------+
|   Código Fuente  |
+------------------+
         |
         v
+------------------+
|  Análisis Léxico |  <-- Flex (exercise1.l)
+------------------+
         |  Tokens + Valores Semánticos (yylval)
         v
+------------------+
| Análisis Sintáct.|  <-- Bison (exercise1.y)
+------------------+
         |  Construcción
         v
+------------------+
|       AST        |  <-- ast.h / ast.c (Estructura en memoria)
+------------------+
         |
         +-----------------------------+
         |                             |
         v                             v
+------------------+          +--------------------+
| Análisis Semánt. |          | Generación Código  |
| y Evaluación     |          | (Pseudo-Assembly)  |
| (Punto 6)        |          | (Punto 7)          |
+------------------+          +--------------------+
```

1. **Análisis Léxico (Scanner / Lexer)**: Convierte el flujo de caracteres del código fuente en una secuencia de unidades atómicas con significado llamadas **tokens** (e.g., palabras reservadas, identificadores, números, operadores).
2. **Análisis Sintáctico (Parser)**: Comprueba que la secuencia de tokens cumpla con la gramática libre de contexto del lenguaje formal.
3. **Generación de AST (Árbol Sintáctico Abstracto)**: Construye en memoria una estructura arbórea que retiene la jerarquía y el significado de las operaciones, eliminando detalles puramente sintácticos (como paréntesis o puntos y comas).
4. **Análisis Semántico**: Recorre el AST junto a una **Tabla de Símbolos** para verificar que las variables estén declaradas antes de su uso, que no haya redeclaraciones inválidas y que los tipos sean consistentes.
5. **Evaluación / Interpretación / Generación de Código**: A partir del AST validado, se evalúan las expresiones (intérprete) o se emiten instrucciones intermedias / pseudo-assembly.

---

## 2. Herramientas Utilizadas y su Rol

### Flex (Fast Lexical Analyzer Generator)
* **Archivo**: `exercise1.l`
* **Función**: Herramienta que genera un autómata finito determinista en código C (`lex.yy.c`) a partir de expresiones regulares. Reconoce los lexemas y los clasifica en tokens.
* **Componentes clave**:
  - `yytext`: Variable global (puntero a `char`) que contiene la cadena exacta de texto que acaba de reconocer el analizador léxico.
  - `yylval`: Variable global compartida entre Flex y Bison. Se utiliza para transferir el **valor semántico** del token (por ejemplo, el valor numérico de un `NUMBER` o la cadena duplicada de un `ID`).
  - `yywrap()`: Función que indica si hay más archivos por procesar al llegar al fin de archivo (`EOF`).

### Bison (Parser Generator / LALR)
* **Archivo**: `exercise1.y`
* **Función**: Genera un analizador sintáctico ascendente (*Bottom-Up LALR(1)*) en C (`exercise1.tab.c` y `exercise1.tab.h`).
* **Componentes clave**:
  - `%union`: Define la estructura heterogénea de datos para `yylval` y para los atributos sintácticos de las reglas.
  - `%token <campo>`: Declara tokens terminales y especifica qué campo del `%union` contiene su valor.
  - `%type <campo>`: Declara no terminales (reglas) y especifica qué tipo de dato devuelven al resolverse.
  - Símbolos posicionales (`$1`, `$2`, ..., `$$`):
    - `$$`: El valor semántico resultante de la regla que se está reduciendo.
    - `$n`: El valor semántico del *n-ésimo* símbolo a la derecha de la producción.
  - Manejo de Precedencia y Asociatividad:
    - `%left '+' '-'`
    - `%left '*'`
    Define el orden de reducción para resolver conflictos *shift/reduce* en expresiones ambiguas.

---

## 3. ¿Qué es un AST y por qué es Necesario?

Un **Árbol Sintáctico Abstracto (AST)** es una representación jerárquica y abstracta de la estructura sintáctica del programa.

### Diferencia entre Parse Tree y AST
* **Parse Tree (Árbol Concreto)**: Modela cada paso de derivación gramatical incluyendo puntuación, paréntesis, llaves y no-terminales intermedios.
* **AST (Árbol Abstracto)**: Descarta la "sintaxis superficial" y se enfoca en la semántica de la operación.
  - Ejemplo para `(2 + 3) * 4`:
    ```
          BINOP [*]
          /       \
     BINOP [+]   CONSTANT [4]
      /     \
    [2]     [3]
    ```
    Los paréntesis desaparecen porque la propia jerarquía del árbol establece la precedencia.

### ¿Por qué no basta con el Parser?
El parser de Bison procesa el código de izquierda a derecha de forma secuencial. Una vez que valida una sentencia, no almacena su estructura para consultas futuras. El AST mantiene todo el programa estructurado en memoria, permitiendo múltiples pasadas (recorridos recursivos) para análisis de tipos, optimizaciones y generación de código.

---

## 4. Estructura e Implementación del AST (`ast.h` y `ast.c`)

### 4.1. Tipos de Nodos (`Node_Type`)
En `ast.h`, se define un enumerador con cada clase de nodo que compone el lenguaje:

```c
typedef enum Node_Type {
    PROGRAM_NODE,        // Nodo raíz: contiene la función main y su bloque de sentencias
    STATEMENT_LIST_NODE, // Lista enlazada de sentencias consecutivas
    DECLARATION_NODE,    // Declaración de variable (tipo y nombre)
    ASSIGN_NODE,         // Asignación (nombre de variable y expresión a evaluar)
    RETURN_NODE,         // Sentencia de retorno (expresión o NULL)
    BINOP_NODE,          // Operación binaria (+, -, *) entre dos expresiones
    CONSTANT_NODE,       // Literal numérico o booleano (0 o 1)
    ID_NODE              // Uso de identificador dentro de una expresión
} Node_Type;
```

### 4.2. Estructura Polimórfica (`ASTNode`) con `union`
Dado que diferentes nodos requieren almacenar información distinta, se implementa una estructura en C con un campo discriminador (`type`) y una `union` interna:

```c
typedef struct ASTNode {
    Node_Type type;

    union {
        struct {
            struct ASTNode* statements;
        } program;

        struct {
            struct ASTNode* statement;
            struct ASTNode* next;
        } statement_list;

        struct {
            int data_type; // 0 = int, 1 = bool
            char* var_name;
        } declaration;
        
        struct {
            char* var_name;
            struct ASTNode* expression;
        } assignment;
        
        struct {
            struct ASTNode* expression; // NULL si es "return;"
        } return_stmt;

        struct {
            struct ASTNode* left;
            char op;
            struct ASTNode* right;
        } binop;

        struct {
            int value; // Valor numérico o 1/0 para booleano
        } constant;

        struct {
            char* var_name;
        } id;

    } data;

} ASTNode;
```

### 4.3. Constructores y Gestión de Memoria Dinámica (`ast.c`)
En `ast.c` se implementan funciones que solicitan memoria mediante `malloc` e inicializan los campos de cada variante de nodo.

* **Ejemplo: Creación de Operación Binaria**:
  ```c
  ASTNode* create_binop_node(ASTNode* left, char op, ASTNode* right) {
      ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
      node->type = BINOP_NODE;
      node->data.binop.left = left;
      node->data.binop.op = op;
      node->data.binop.right = right;
      return node;
  }
  ```

* **Liberación Recursiva de Memoria (`free_ast`)**:
  Para evitar fugas de memoria (*memory leaks*), se recorre el árbol de forma *post-order*:
  ```c
  void free_ast(ASTNode* node) {
      if (node == NULL) return;
      switch (node->type) {
          case PROGRAM_NODE:
              free_ast(node->data.program.statements);
              break;
          case STATEMENT_LIST_NODE:
              free_ast(node->data.statement_list.statement);
              free_ast(node->data.statement_list.next);
              break;
          case DECLARATION_NODE:
              free(node->data.declaration.var_name);
              break;
          case ASSIGN_NODE:
              free(node->data.assignment.var_name);
              free_ast(node->data.assignment.expression);
              break;
          case RETURN_NODE:
              free_ast(node->data.return_stmt.expression);
              break;
          case BINOP_NODE:
              free_ast(node->data.binop.left);
              free_ast(node->data.binop.right);
              break;
          case CONSTANT_NODE:
              break;
          case ID_NODE:
              free(node->data.id.var_name);
              break;
      }
      free(node); 
  }
  ```

---

## 5. Integración entre Flex, Bison y el AST

### 5.1. Flujo Léxico (`lexer.l`)
1. Reconoce identificadores y duplica su cadena para persistir en memoria:
   ```lex
   {ID}        { yylval.strval = strdup(yytext); return ID; }
   ```
2. Convierte cadenas de dígitos a enteros:
   ```lex
   {NUMBER}    { yylval.intval = atoi(yytext); return NUMBER; }
   ```
3. Mapea constantes lógicas a valores enteros (1 y 0):
   ```lex
   "true"      { yylval.intval = 1; return BOOL_CONST; }
   "false"     { yylval.intval = 0; return BOOL_CONST; }
   ```

### 5.2. Flujo Sintáctico y Construcción (`bison.y`)
1. Declaración de tipos semánticos:
   ```bison
   %union {
     int intval;
     char* strval;
     struct ASTNode* node;
   }

   %token <strval> ID
   %token <intval> NUMBER BOOL_CONST
   %type <node> Program Code Sentence VariableDeclaration VariableAssignment Expression Return
   %type <intval> FunctionReturnType VariableType
   ```
2. Reglas gramaticales con acciones semánticas asociadas:
   - **Programa raíz**:
     ```bison
     Program
     : FunctionReturnType MAIN '(' ')' '{' Code '}' {
         $$ = create_program_node($6);
         printf("Analisis sintáctico exitoso y AST construido. \n");
     }
     ;
     ```
   - **Lista de Sentencias**:
     ```bison
     Code
     : Sentence Code        { $$ = create_statement_list_node($1, $2); }
     | /* empty, lambda */  { $$ = NULL; }
     ;
     ```
   - **Asignaciones y Declaraciones**:
     ```bison
     VariableDeclaration
     : VariableType ID ';'  { $$ = create_declaration_node($1, $2); }
     ;

     VariableAssignment
     : ID '=' Expression ';' { $$ = create_assignment_node($1, $3); }
     ;
     ```
   - **Expresiones Aritméticas**:
     ```bison
     Expression
     : Expression '+' Expression { $$ = create_binop_node($1, '+', $3); }
     | Expression '-' Expression { $$ = create_binop_node($1, '-', $3); }
     | Expression '*' Expression { $$ = create_binop_node($1, '*', $3); }
     | '(' Expression ')'        { $$ = $2; }
     | NUMBER                    { $$ = create_constant_node($1); }
     | BOOL_CONST                { $$ = create_constant_node($1); }
     | ID                        { $$ = create_id_node($1); }
     ;
     ```

---

## 6. Compilación y Ejecución de Pruebas (`test_suite.sh`)

El proceso de construcción y enlace se define mediante el script `test_suite.sh`:

```bash
#!/bin/bash

# 1. Creación del directorio
mkdir -p build

# 2. Generación del Parser (bison.tab.c y bison.tab.h) en build/
bison -d bison.y -o build/bison.tab.c

# 3. Generación del Scanner (lex.yy.c) en build/
flex -o build/lex.yy.c lexer.l

# 4. Compilación y enlazado con el módulo AST (Parser normal) en build/
gcc -I. -Ibuild ast.c build/bison.tab.c build/lex.yy.c -o build/parser

# 5. Compilación y Ejecución de Batería de Pruebas (Unity)
echo "Compilando tests..."
gcc -DUNITY_TESTING -I. -Ibuild -I./tests/unity tests/unity/unity.c tests/test_parser.c ast.c build/bison.tab.c build/lex.yy.c -o build/test_runner

echo "Ejecutando tests con Unity..."
./build/test_runner
```

---

## 7. Próximos Pasos del Preproyecto

Basado en la especificación de `preproyecto.pdf`:

### Punto 6: Intérprete / Evaluador de Expresiones y Tabla de Símbolos
1. **Tabla de Símbolos**:
   - Estructura de datos (e.g., tabla hash o lista de pares `clave-valor`) para almacenar nombre de variable, tipo de dato (`int`, `bool`) y valor actual.
   - Funciones: `insert_symbol(name, type)`, `lookup_symbol(name)`, `update_symbol_value(name, value)`.
2. **Chequeo Semántico**:
   - Verificar que cualquier `ID_NODE` utilizado haya sido declarado previamente en la tabla.
   - Comprobación de compatibilidad de tipos (e.g., evitar sumar booleanos con enteros o asignar tipos incompatibles).
3. **Evaluador / Intérprete**:
   - Función recursiva `int evaluate_ast(ASTNode* node)` que recorra el árbol, ejecute las asignaciones actualizando la tabla y evalúe las expresiones aritméticas y los retornos.

### Punto 7: Generador de Pseudo-Assembly
1. Diseñar un conjunto de instrucciones simplificado (e.g., `LOAD`, `STORE`, `ADD`, `SUB`, `MUL`, `RET`).
2. Implementar una función `void generate_code(ASTNode* node)` que recorra el AST y emita instrucciones pseudo-assembly utilizando registros temporales (`t0`, `t1`, etc.).
