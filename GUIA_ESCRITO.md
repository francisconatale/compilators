# Guía del Escrito: una construcción a la vez, de punta a punta

Para el escrito de una hora, donde hay que tomar un programa (o un pedazo) y
mostrar **cómo lo ve el compilador en cada etapa**: tokens, árbol, tabla de
símbolos, código de tres direcciones y assembly.

No hace falta recordar nombres exactos de funciones: hace falta saber **qué
decide cada etapa y por qué**. Los nombres están igual, entre paréntesis, por si
cae una pregunta de "¿dónde está eso en su código?".

Todas las salidas de este documento son **reales**: salieron de correr el
compilador. Si algo no coincide, manda el compilador.

---

## Cómo está organizado

Cada sección sigue **una construcción** desde el código fuente hasta el
assembly. Están en orden de dificultad creciente, y cada una se apoya en las
anteriores:

| # | Construcción |
| --- | --- |
| [0](#0-el-andamiaje) | El andamiaje: lo que hay que saber antes de la primera traza |
| [1](#1-declaración-de-variables) | Declaración de variables (`int x;`, `int x, y;`, global y local) |
| [2](#2-declaración-de-métodos) | Declaración de métodos (con tipo, `void`, con y sin parámetros) |
| [3](#3-constantes-e-identificadores) | Constantes e identificadores |
| [4](#4-expresiones-aritméticas) | Expresiones aritméticas y precedencia |
| [5](#5-operadores-unarios---y-) | Unarios `-` y `!` |
| [6](#6--y--el-corto-circuito) | `&&` y `\|\|`: el corto circuito |
| [7](#7-asignación) | Asignación |
| [8](#8-invocación-a-método) | Invocación a método (sentencia y expresión) |
| [9](#9-return) | `return` con y sin expresión |
| [10](#10-if-y-if-else) | `if` y `if`/`else` |
| [11](#11-while) | `while` |
| [12](#12-bloques-y-ocultamiento-de-nombres) | Bloques y ocultamiento de nombres |
| [13](#13-el-marco-de-pila-de-dónde-salen-los-offsets) | El marco de pila: de dónde salen los offsets |
| [14](#14-el-optimizador-en-dos-minutos) | El optimizador, en dos minutos |
| [15](#15-apéndices) | Apéndices: reglas semánticas, opcodes, precedencias, checklist |

---

## 0. El andamiaje

### Las seis etapas y en qué archivo vive cada una

```
archivo.ctds
   │
   ├─ 1. ANÁLISIS LÉXICO ......... src/parser/lexer.l        (Flex)     → tokens
   ├─ 2. ANÁLISIS SINTÁCTICO ..... src/parser/bison.y        (Bison)    → AST
   ├─ 3. ANÁLISIS SEMÁNTICO ...... src/semantic/             → AST anotado + tabla de símbolos
   ├─ 4. CÓDIGO INTERMEDIO ....... src/intermediate_code/    → lista de cuádruplas (TAC)
   ├─ 5. OPTIMIZADOR (opcional) .. src/optimizer/            → la misma lista, más corta
   └─ 6. CÓDIGO OBJETO ........... src/object_code/          → texto assembly x86-64
                                                              → gcc → ejecutable
```

Se puede frenar en cualquier etapa: `-target scan | parse | sem | codinter |
assembly`, que escriben `.lex`, `.sint`, `.sem`, `.ci`, `.ass`. Sin `-target`,
llega hasta el ejecutable. Quien orquesta todo esto es `compile()` en
`src/core/driver.c`, que es un `switch` sobre la etapa pedida donde **cada caso
cae en cascada sobre los anteriores**.

### Las cuatro estructuras que viajan

| Etapa | Qué produce | Estructura |
| --- | --- | --- |
| Léxico | Un **token** por vez, con su valor en `yylval` | `int` + `union` |
| Sintáctico | Un **árbol** | `NodeAST` |
| Semántico | **Anotaciones** sobre ese árbol + la **tabla de símbolos** | `Symbol`, `Scope` |
| Intermedio | Una **lista plana** de cuádruplas | `TACInstruction` |
| Objeto | **Texto** | `string_builder` |

```c
typedef struct NodeAST {
    NodeType nodeType;           // qué clase de nodo (12 posibles)
    DataType type;               // su tipo: lo llena el SEMÁNTICO
    Symbol *symbol;              // su símbolo (si tiene nombre o valor)
    OperationType operationType; // qué operador (solo en OPERATOR_NODE)
    int line;
    NodeAST **children;          // hijos
    int childCount;
} NodeAST;
```

**El AST tiene un solo tipo de nodo para todo**, y se distingue por el campo
`nodeType`. La alternativa era un `struct` por construcción con una unión; se
eligió el nodo único porque **todos los recorridos posteriores son un `switch`
sobre `nodeType`**, y agregar una construcción nueva es agregar un valor al
enum, no un tipo nuevo.

### Las cuatro reglas que se repiten en todas las trazas

Para no repetirlas doce veces:

**1. El parser no sabe nada de tipos ni de ámbitos.** Construye el árbol y nada
más. Cuando ve un identificador crea un `Symbol` **provisorio** que solo guarda
el texto del nombre, con `uid = 0`. El `0` significa *"todavía no está declarado
en ninguna tabla"*.

**2. El `uid` lo asigna el semántico, no el parser.** Cuando una declaración se
registra en un ámbito (`appendSymbol`), recibe el próximo número de un contador
global que arranca en 1. Por eso en el `.sint` todos los símbolos son `#0` y en
el `.sem` ya tienen número: **es la diferencia observable entre las dos etapas**.

**3. Un uso se "reapunta" a su declaración.** Cuando el semántico resuelve un
nombre contra la tabla (`lookupSymbol`), hace que el nodo del uso deje de
apuntar a su símbolo provisorio y apunte al **símbolo declarado**
(`resolveNodeSymbol`), liberando el provisorio. De ahí en adelante, un uso y su
declaración **son el mismo objeto en memoria**, que es lo que hace que el TAC
pueda escribir el mismo nombre calificado en los dos lugares.

**4. Las etapas posteriores no revalidan nada.** Si la generación de código
corre, es porque hubo cero errores semánticos. El generador de TAC **asume** que
los tipos están bien, que todo está declarado y que las aridades coinciden. Si
encuentra algo inválido, es un bug del semántico, no del programa.

### El nombre calificado `x@3`

En el TAC, cada variable se escribe `nombre@uid`:

```
local x@3, INT
```

**Por qué.** Dos variables distintas pueden llamarse igual si están en bloques
distintos. En el árbol se distinguen porque viven en ámbitos distintos, pero el
TAC es una **lista plana sin ámbitos**: si las dos se escribieran `x`, el
generador de código objeto les daría la misma ranura de memoria. El `@uid` las
separa, y como el `uid` es único en todo el programa, **el TAC no necesita tabla
de símbolos**.

Las **globales pierden el sufijo** al convertirse en símbolos del assembly
(`g`, no `g@1`), porque el ámbito global es uno solo y no puede haber
repetidos; así `nm` sobre el ejecutable muestra nombres legibles.

---

## 1. Declaración de variables

### El programa

```c
int g;
int a, b;
void main() {
    int x;
    int y, z;
}
```

### Etapa 1 — Léxico

El lexer devuelve un token por vez. No hay "declaración": hay símbolos sueltos.

```
[Linea 1] INT          int
[Linea 1] ID           g
[Linea 1] ';'          ;
[Linea 2] INT          int
[Linea 2] ID           a
[Linea 2] ','          ,
[Linea 2] ID           b
[Linea 2] ';'          ;
```

Dos cosas que se preguntan:

- **Las palabras reservadas van antes que la regla de identificador** en el
  archivo. Flex usa **la coincidencia más larga** y, ante empate, **la regla que
  aparece primero**. `int` matchea tanto `"int"` como `{ID}` con 3 caracteres:
  gana la primera escrita. Si `{ID}` estuviera arriba, `int` sería un
  identificador y la gramática no reconocería ninguna declaración.
- **El valor del token viaja aparte, en `yylval`.** Para un `ID` el lexer hace
  `yylval.strval = strdup(yytext)` — una **copia**, porque `yytext` apunta al
  buffer interno de Flex y lo pisa el token siguiente. Esa copia es la que
  termina siendo el `id` del símbolo, sin copias intermedias.

### Etapa 2 — Sintáctico

```
VariableDeclaration : Type IdentifierList ';'   { $$ = resolveVariableDefinition($1, $2); }

IdentifierList : ID                             { $$ = appendNode(NULL, newIdentifierNode($1)); }
               | IdentifierList ',' ID          { $$ = appendNode($1, newIdentifierNode($3)); }
```

**Cómo se resuelve `int a, b;` — ésta es la parte interesante.** Bison es
**ascendente (LALR(1))**: reduce de abajo hacia arriba, así que cuando ve `a` y
`b` **todavía no sabe el tipo**, aunque en el texto el `int` esté antes. La
solución tiene dos pasos:

1. `IdentifierList` acumula los identificadores en un arreglo (`NodeArray`) como
   nodos `ID_NODE` **sin tipo**.
2. Al reducir la producción completa ya se tiene el `Type`, y
   `resolveVariableDefinition` recorre el arreglo **convirtiendo cada nodo en
   sitio**: `ID_NODE` → `VARIABLE_DECLARATION_NODE`, y le escribe el tipo al
   nodo y a su símbolo.

O sea: **una declaración múltiple produce N nodos de declaración independientes**,
exactamente como si se hubieran escrito en líneas separadas. No hay un nodo
"declaración múltiple". Eso simplifica todo lo que viene después.

**La recursión es a izquierda** (`IdentifierList ',' ID`), igual que todas las
listas de esta gramática. En un parser ascendente la recursión a derecha obliga
a apilar la lista entera antes de empezar a reducir; con recursión a izquierda
se reduce de a un elemento y **la pila queda acotada**.

El `.sint` (con `-target parse`):

```
Bloque
  | Declaracion Variable: g #0 (Tipo INT)
  | Declaracion Variable: a #0 (Tipo INT)
  | Declaracion Variable: b #0 (Tipo INT)
  | Metodo: main (Retorna VOID)
  |   | Parametros
  |   | Bloque
  |   |   | Declaracion Variable: x #0 (Tipo INT)
  |   |   | Declaracion Variable: y #0 (Tipo INT)
  |   |   | Declaracion Variable: z #0 (Tipo INT)
```

Todos los `uid` son **#0**: el parser no registró nada en ninguna tabla.

### Etapa 3 — Semántico

Recorrido: `analyzeSemantics` crea el ámbito global y baja por el árbol
(`analyzeNodeSemantics` es el `switch`). Para una declaración
(`analyzeVariableDeclarationNode`) hace **dos cosas**:

1. **¿Ya está declarada en este mismo ámbito?** `lookupSymbolInScope`, que mira
   **solo el ámbito actual y no los padres**. Si está → error *"La variable 'x'
   ya fue declarada previamente en este ambito"*.
2. Si no, la registra: `appendSymbol`, que le asigna el `uid` y marca
   `isGlobal = (scope->parent == NULL)`.

**Por qué `lookupSymbolInScope` y no `lookupSymbol`:** redeclarar en el **mismo**
ámbito es un error; declarar el mismo nombre en un bloque **interno** es
ocultamiento y es legal. Las dos funciones de búsqueda existen exactamente por
esa distinción.

El `.sem` ahora tiene los `uid`:

```
  | Declaracion Variable: g #1 (Tipo INT)
  | Declaracion Variable: a #2 (Tipo INT)
  | Declaracion Variable: b #3 (Tipo INT)
  | Metodo: main (Retorna VOID)          <- main se lleva el #4
  |   | Bloque
  |   |   | Declaracion Variable: x #5 (Tipo INT)
  |   |   | Declaracion Variable: y #6 (Tipo INT)
  |   |   | Declaracion Variable: z #7 (Tipo INT)
```

**Los locales arrancan en #5 y no en #4** porque `main` también es un símbolo y
consume el 4. Es un detalle que conviene poder explicar si piden numerar a mano.

**La tabla de símbolos es un árbol de ámbitos, no una tabla plana:**

```
Scope(level 0, parent NULL)         <- global
├── símbolos: g#1, a#2, b#3, main#4
└── hijos:
    └── Scope(level 1, function=main)
        ├── símbolos: x#5, y#6, z#7
        └── hijos: ...
```

Buscar un nombre es `lookupSymbol`: mira el ámbito actual y, si no está, **sube
al padre** recursivamente. Eso implementa el alcance léxico en cinco líneas.

### Etapa 4 — Código intermedio

Una declaración produce **una instrucción**, y cuál depende de un solo campo:

```c
TACOperation declOp = node->symbol->isGlobal ? TAC_GLOBAL_DECL : TAC_LOCAL_DECL;
```

```
global g@1, INT
global a@2, INT
global b@3, INT
begin_func main, 0
local x@5, INT
local y@6, INT
local z@7, INT
ret
end_func main
```

**Por qué el TAC declara explícitamente en vez de deducirlo.** El generador de
código objeto necesita saber **cuánto espacio reservar en el marco** antes de
emitir la primera instrucción de la función, y necesita saber qué va a `.data`.
Tener la declaración como instrucción le permite hacer una pasada previa contando
ranuras, sin reconstruir ámbitos.

### Etapa 5 — Código objeto

**Las globales van a `.data`**, una palabra de 4 bytes inicializada en 0:

```asm
.data
.globl g
.align 4
g:
    .long 0
```

**Las locales no emiten ni una instrucción.** El `case TAC_LOCAL_DECL` del
despachador está vacío a propósito: el espacio ya se reservó **de una sola vez**
en el prólogo con el `subq`, así que declarar no requiere ejecutar nada.

```asm
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $16, %rsp        <- 3 locales x 4 bytes = 12, alineado a 16
```

---

## 2. Declaración de métodos

### El programa

```c
int suma(int a, int b) { return a + b; }
void saludo() { return; }
void main() {
    int r;
    r = suma(3, 4);
    saludo();
}
```

### Etapa 2 — Sintáctico

```
MethodDeclaration : Type ID '(' ParameterList ')' Block   { newMethodDeclaration($1, $2, $4, $6); }
                  | VOID ID '(' ParameterList ')' Block   { newMethodDeclaration(TYPE_VOID, $2, $4, $6); }
```

**Hay dos producciones y no una** porque `VOID` no está en el no terminal `Type`:
`Type` son los tipos que puede tener una **variable**, y `void` no lo es. Separar
las dos producciones evita que la gramática acepte `void x;`.

El nodo queda con **exactamente dos hijos**, y conviene tenerlo memorizado:

```
METHOD_DECLARATION_NODE          symbol = el método, type = tipo de retorno
├── [0] PARAMETERS_NODE          <- siempre existe, aunque esté vacío
└── [1] BLOCK_NODE               <- el cuerpo
```

**El `PARAMETERS_NODE` existe siempre, incluso sin parámetros** (la producción
vacía de `ParameterList` llama igual a `newParametersNode(NULL)`). Así el hijo
`[1]` es *siempre* el cuerpo y ningún recorrido posterior necesita preguntar si
hay parámetros antes de buscar el cuerpo. Es una decisión chica que simplifica
cuatro recorridos.

### Etapa 3 — Semántico

`analyzeMethodNode` hace cinco cosas, **en este orden**:

1. **¿Ya existe el nombre en este ámbito?** → error de redeclaración.
2. **Registra el método en el ámbito actual** (`appendSymbol`) — **antes** de
   analizar el cuerpo. Eso es lo que permite la **recursión**: cuando el cuerpo
   diga `suma(...)`, el nombre ya está en la tabla.
3. **Crea el ámbito del método**, con `function = ` el símbolo del método. Ese
   campo es el que después le permite a un `return` saber **qué tipo se espera**.
4. **Analiza los parámetros en ese ámbito nuevo**, no en el global: los
   parámetros son locales de la función.
5. **Analiza el cuerpo** en el mismo ámbito y, si el método **no es `void`**,
   verifica que todos los caminos terminen en `return` (`guaranteesReturn`).

`guaranteesReturn` es un recorrido chico que vale saber explicar:

| Nodo | ¿Garantiza retorno? |
| --- | --- |
| `return` | Sí |
| Bloque | Sí si **alguna** de sus sentencias lo garantiza |
| `if` **sin** `else` | **No** (la rama falsa puede no ejecutarse) |
| `if`/`else` | Sí **solo si las dos ramas** lo garantizan |
| `while` | **No** |

Un `while(true)` con `return` adentro se rechaza aunque en ejecución siempre
retorne: decidir eso requiere evaluar la condición, que es indecidible en general.
**Es una aproximación conservadora**, y la respuesta correcta si lo preguntan es
exactamente esa.

**El orden de declaración importa:** un método solo ve a los declarados **antes**
que él, porque el registro ocurre al recorrer el árbol de arriba hacia abajo. No
hay declaración adelantada. Es lo que pide la letra ("todo se declara antes de
usarse").

### Etapa 4 — Código intermedio

```
begin_func suma, 2        <- nombre y cantidad de parámetros
param_decl a@2, INT
param_decl b@3, INT
t0 = a@2 + b@3
ret t0
end_func suma
begin_func saludo, 0
ret                       <- este 'ret' estaba escrito en el programa
end_func saludo
begin_func main, 0
local r@6, INT
...
ret                       <- este es IMPLÍCITO: main es void y no terminaba en return
end_func main
```

Tres cosas:

- **`begin_func` lleva la aridad.** El generador de objeto la usa para saber
  cuántos `param_decl` esperar.
- **`param_decl` no es una instrucción ejecutable**: es la declaración de que la
  función recibe ese parámetro. En el objeto se traduce a *copiar el registro de
  argumento a una ranura del marco*.
- **El `ret` implícito**: si el método es `void` y su última instrucción no es un
  `ret`, se emite uno. Así **toda función termina en `ret`**, que es una garantía
  de la que depende el generador de objeto para no caerse del final de una
  función hacia la siguiente.

**Los temporales se reinician en cada método** (`temporaryCount = 0` al empezar
`generateMethodDeclarationCode`): por eso `suma` y `main` tienen las dos un `t0`.
No colisionan porque **viven en marcos distintos**. Las **etiquetas no se
reinician** y son únicas en todo el programa, porque un salto podría —en
principio— cruzar funciones.

### Etapa 5 — Código objeto

```asm
.globl suma
.type suma, @function
suma:
    pushq %rbp              # 1. guardar el marco del llamador
    movq %rsp, %rbp         # 2. el marco nuevo empieza acá
    subq $16, %rsp          # 3. reservar las ranuras de una vez
    movl %edi, -4(%rbp)     # param_decl a: primer registro de argumento
    movl %esi, -8(%rbp)     # param_decl b: segundo
    ...
.L_suma_epilogue:           # todos los return saltan acá
    leave                   # deshace el marco (movq %rbp,%rsp + popq %rbp)
    ret
```

**Por qué se copian los parámetros a la pila apenas entra la función.** Los
registros `%edi`, `%esi`… son *caller-saved*: cualquier llamada interna los
puede pisar. Copiándolos al marco, el resto de la función trata a los parámetros
**igual que a las locales**, y asignarles es automáticamente un efecto local
(que es lo que pide la letra: pasaje por valor).

**Por qué un epílogo común en vez de un `ret` por cada `return`.** Cada `return`
del programa se vuelve un `jmp .L_f_epilogue`, y el desarmado del marco se
escribe **una sola vez**. Con varios `return` el código queda más corto y, sobre
todo, imposible de desincronizar.

`saludo` **no tiene `subq`**: no tiene ni locales ni temporales, así que
`frameSize` es 0 y el prólogo lo omite.

En `main`, `void` + nombre `main` agrega `movl $0, %eax` antes de salir, porque
el sistema operativo toma `%eax` como código de salida del proceso y si no
quedaría basura.

---

## 3. Constantes e identificadores

Son las hojas de toda expresión; conviene tenerlas claras antes de las trazas
que las usan.

### Constantes

```
Expression : NUMBER       { $$ = newIntLiteralNode($1); }
           | BOOL_CONST   { $$ = newBoolLiteralNode($1); }
           | FLOAT_CONST  { $$ = newFloatLiteralNode($1); }
```

El lexer ya convirtió el texto a número (`atoi`, `atof`) y lo dejó en `yylval`.
`true` y `false` **no son identificadores**: el lexer los reconoce como
`BOOL_CONST` con valor 1 y 0.

El nodo es un `CONSTANT_NODE` que cuelga un `Symbol` **sin `id` y sin `uid`**:

> Un `Symbol` con `id == NULL` y `uid == 0` **no es una entrada rota de la tabla**:
> es un literal. La misma estructura sirve para las dos cosas, y se distinguen
> por si tiene nombre.

En el semántico, una constante es el caso más simple: `node->type =
node->symbol->type`. **El tipo lo sabe desde el léxico** — lo determinó la forma
del literal.

En el TAC, una constante **no ocupa un temporal**: se convierte en un operando
inmediato que viaja dentro de la instrucción que la usa.

```
x@3 = 0            <- el 0 no necesitó temporal
t0 = x@3 + 1       <- el 1 tampoco
```

En el assembly se vuelve un inmediato: `movl $1, %edx`.

### Identificadores en una expresión

```
Expression : ID    { $$ = newIdentifierNode($1); }
```

En el semántico (`analyzeIdentifier`) hay **dos errores posibles**:

| Situación | Mensaje |
| --- | --- |
| `lookupSymbol` devuelve `NULL` | `Variable no declarada: x` |
| El símbolo existe pero es un método | `El identificador 'f' no es una variable.` |

El segundo caso es `x = f;` con `f` método. C-TDS no tiene punteros a función:
un método solo puede aparecer en una llamada.

Si resuelve bien, hace las **dos anotaciones** que importan: reapunta el símbolo
a la declaración y copia el tipo al nodo.

---

## 4. Expresiones aritméticas

### El programa

```c
x = 2 + 3 * 4;
```

### Etapa 2 — Sintáctico: la precedencia

La gramática de expresiones es **ambigua a propósito**: todas las operaciones
binarias son `Expression OP Expression`. La ambigüedad se resuelve con las
declaraciones de precedencia, no reescribiendo la gramática:

```
%left OR                /* menor precedencia */
%left AND
%left EQ
%left '<' '>'
%left '+' '-'
%left '*' '/' '%'
%precedence NOT UMINUS  /* mayor precedencia */
```

**Se lee de abajo hacia arriba: la última línea es la que más fuerte ata.**

- `%left` = **asociatividad izquierda**: `a - b - c` es `(a-b)-c`.
- `%precedence` (sin asociatividad) para los unarios, porque `- - x` no tiene
  sentido discutirlo por asociatividad: hay una sola forma de leerlo.

**Por qué con precedencias y no con una gramática estratificada.** La forma
"clásica" (`Expr → Expr + Term`, `Term → Term * Factor`, `Factor → ...`) es
equivalente, pero necesita un no terminal por nivel de precedencia: con 7
niveles son 7 no terminales y 7 reducciones por cada expresión simple. Las
declaraciones de precedencia expresan lo mismo en 7 líneas, y además dejan la
gramática legible. El costo es que la gramática **sola** no determina el árbol:
hay que leer las precedencias para entenderla.

El árbol resultante **ya tiene la precedencia resuelta en su forma**:

```
Asignacion a: x
  | Operacion: +
  |   | Constante INT: 2
  |   | Operacion: *
  |   |   | Constante INT: 3
  |   |   | Constante INT: 4
```

La multiplicación quedó **más abajo**, o sea que se evalúa primero. De acá en
adelante nadie vuelve a mirar precedencias: están en la forma del árbol.

### Etapa 3 — Semántico: el tipo de la operación

`analyzeOperator` es **postorden**: primero los tipos de los operandos, después
el del nodo. `resolveOperation` decide:

| Operador | Operandos válidos | Tipo del resultado |
| --- | --- | --- |
| `+ - * /` | numérico y numérico | el "mayor" de los dos |
| `%` | **`int` e `int` solamente** | `int` |
| `< >` | numérico y numérico | `boolean` |
| `==` | **los dos del mismo tipo básico** | `boolean` |
| `&& \|\|` | boolean y boolean | `boolean` |
| `-` unario | numérico | el mismo |
| `!` | boolean | `boolean` |

Dos asimetrías que son decisiones de diseño documentadas y que el profesor puede
picar:

- **`%` solo entre enteros**, aunque la regla 10 liste `%` entre los operadores
  aritméticos que admiten `int` o `float`. Se eligió la sección "Expresiones",
  más específica, que dice que `%` es el resto de una división **entera**.
- **`==` exige tipos idénticos pero `<` y `>` no.** La regla 11 dice que solo se
  comparan expresiones del mismo tipo; la regla 10 pide operandos numéricos para
  los relacionales sin exigir que coincidan. Consecuencia visible: `int < float`
  se acepta y `int == float` no.

**La propagación de `TYPE_ERROR`:** si un operando ya dio error, el nodo devuelve
`TYPE_ERROR` **sin reportar nada nuevo**. Así un error primario produce
exactamente un mensaje en vez de una cascada.

### Etapa 4 — Código intermedio: temporales

```
t1 = 3 * 4
t0 = 2 + t1
x@3 = t0
```

El recorrido es **postorden**: para emitir una operación hay que tener sus
operandos ya calculados, porque **una cuádrupla no puede contener una expresión**.
Cada operación deja su resultado en un **temporal nuevo**, y ese temporal es lo
que la función devuelve hacia arriba.

**La numeración llama la atención: `t1` se emite antes que `t0`.** Es correcto, y
explicarlo demuestra que se entendió el recorrido:

```c
TACOperand left   = generateExpressionCode(GET_LEFT(node));   // 1. el 2: constante, sin temporal
TACOperand result = newTemporary(node->type);                 // 2. RESERVA t0 para el '+'
TACOperand right  = generateExpressionCode(GET_RIGHT(node));  // 3. el 3*4: acá nace t1
emit(TAC_ADD, result, left, right);                           // 4. recién ahora emite t0 = 2 + t1
```

El temporal del nodo padre **se reserva antes** de generar el subárbol derecho,
pero la **instrucción** del padre se emite al final. Numeración y orden de
emisión no coinciden.

**Por qué cuádruplas y no árbol o texto.** Una cuádrupla es
`{operación, resultado, operando1, operando2}` con **operandos tipados** (`kind`
dice si es temporal, variable, constante, etiqueta o método). Las alternativas
eran generar texto —que habría que volver a parsear para optimizar— o quedarse en
el árbol —donde eliminar una instrucción obliga a reacomodar punteros de padres—.
La lista plana hace que **borrar una instrucción sea desenlazar un nodo**, que es
exactamente lo que el optimizador necesita hacer todo el tiempo.

### Etapa 5 — Código objeto: cargar, operar, guardar

Toda operación binaria sigue **el mismo esquema de tres pasos**:

```asm
movl -4(%rbp), %eax     # cargar el primer operando
movl $1, %edx           # cargar el segundo
addl %edx, %eax         # operar
movl %eax, -12(%rbp)    # guardar el resultado en su ranura
```

**Ningún valor sobrevive en un registro entre dos instrucciones TAC.** Es
deliberado: así el generador **no tiene que llevar la cuenta de qué hay en cada
registro**, y cada cuádrupla se traduce mirando solo a sí misma. Es código lento
—recarga lo que acaba de guardar— y es código que se puede verificar leyéndolo.
La prioridad declarada de la etapa es la correctitud.

**División y módulo son la misma instrucción**, leyendo distinto registro:

```asm
movl  <dividendo>, %eax
movl  <divisor>,   %ecx
cltd                      # extiende el signo de %eax a %edx:%eax
idivl %ecx                # cociente -> %eax ; resto -> %edx
```

Dos detalles obligatorios de x86:

1. **`cltd` antes de `idivl`.** La división con signo toma un dividendo de 64
   bits repartido en `%edx:%eax`. Sin el `cltd`, `%edx` tiene basura y dividir un
   número negativo da cualquier cosa.
2. **El divisor va en `%ecx`, nunca en `%edx`**, porque `cltd` **sobrescribe
   `%edx`** justo antes de usarlo.

---

## 5. Operadores unarios `-` y `!`

### Sintáctico

```
Expression : '-' Expression %prec UMINUS   { newOperatorNode(OP_UMINUS, $2, NULL); }
           | NOT Expression                { newOperatorNode(OP_NOT,    $2, NULL); }
```

**`%prec UMINUS` es la respuesta a una pregunta clásica.** El token `'-'` aparece
en dos producciones: la resta binaria y el menos unario. Como token, tiene **una
sola precedencia** (la de `+ -`, que es baja). Pero el menos unario tiene que
atar más fuerte que la multiplicación: en `-x * y` lo correcto es `(-x) * y`.

`UMINUS` es un **pseudo-token**: no lo produce el lexer, existe solo para tener
un nombre al que darle precedencia alta. `%prec UMINUS` le dice a Bison *"para
esta producción, usá la precedencia de `UMINUS` en vez de la del `'-'`"*.

Los unarios usan **el mismo tipo de nodo que los binarios** (`OPERATOR_NODE`),
con un solo hijo. Por eso el nodo no se llama "binario": se distingue por
`childCount`, o preguntando si el hijo derecho es `NULL`.

### Semántico

`-` exige numérico y devuelve **el mismo tipo**; `!` exige `boolean` y devuelve
`boolean`. Un `!x` con `x` entero es error de tipos: **C-TDS no tiene semántica
*truthy***.

### Intermedio y objeto

```
t2 = -x@3           →    movl -4(%rbp), %eax
                         negl %eax
                         movl %eax, -12(%rbp)

t4 = !t3            →    movl -16(%rbp), %eax
                         xorl $1, %eax
                         movl %eax, -20(%rbp)
```

**`!` se traduce a `xorl $1`, no a una comparación.** Eso solo es correcto si un
booleano vale **exactamente 0 o 1**, y ese invariante lo garantizan las
comparaciones (que terminan en `movzbl`, ver §10) y las constantes `true`/`false`
del TAC. A cambio es **una** instrucción en lugar de tres (`cmpl $0` + `sete` +
`movzbl`). Si alguna vez un booleano pudiera valer 2, esto daría 3 y rompería
todo: es una dependencia que conviene saber nombrar.

---

## 6. `&&` y `||`: el corto circuito

Es **la construcción con más probabilidad de caer**, porque la letra la exige
explícitamente y porque es donde la traducción deja de ser uno a uno.

### El problema

```c
b = (x > 1) && f();
```

La letra exige que si `x > 1` es falso, **`f()` no se llame**. Una cuádrupla
`t = a && b` **no puede cumplir eso**, porque sus dos operandos tienen que estar
calculados antes de ejecutarla.

**Por eso no existen los opcodes `TAC_AND` ni `TAC_OR`.** No es un olvido: es la
decisión de diseño que garantiza la semántica del lenguaje. `&&` y `||` se
traducen a **control de flujo**.

### El esquema generado

```
t5 = x@3 > 1          # 1. evaluar el operando izquierdo
t6 = t5               # 2. el resultado provisorio es el izquierdo
if t6 false goto L0   # 3. si ya decide, saltar sin evaluar la derecha
t7 = call f, 0        # 4. evaluar la derecha (solo si hizo falta)
t6 = t7               # 5. el resultado pasa a ser la derecha
L0:                   # 6. acá convergen los dos caminos
b@4 = t6
```

La idea en una frase: **se asume que el resultado es el operando izquierdo, y
solo si éste no alcanza para decidir se evalúa el derecho y se pisa el
resultado.**

`||` es **idéntico cambiando una sola cosa**: el salto es `if ... true goto`.
En el código es literalmente un ternario:

```c
TACOperation jumpOp = (node->operationType == OP_AND) ? TAC_IF_FALSE : TAC_IF_TRUE;
```

| Operador | Salta si… | Porque… |
| --- | --- | --- |
| `&&` | el izquierdo es **falso** | falso `&&` lo que sea = falso |
| `\|\|` | el izquierdo es **verdadero** | verdadero `\|\|` lo que sea = verdadero |

**Las etiquetas se resuelven en una sola pasada, sin backpatching.** En un
compilador de una pasada habría que emitir el salto antes de conocer su destino y
volver después a completarlo. Acá no hace falta: **el AST ya está completo**, así
que la etiqueta se pide primero (`newLabel()`) y se emite después, sin lista de
pendientes.

**Teoría:** esto se llama ***jumping code*** o código de salto, y es la
implementación canónica del corto circuito. La alternativa —calcular los dos
operandos y combinarlos— se llama *código de valor numérico* y es justamente la
que el lenguaje prohíbe.

---

## 7. Asignación

### Sintáctico

```
Assignment : ID '=' Expression   { $$ = newAssignmentNode($1, $3); }
Statement  : Assignment ';'      { $$ = $1; }
```

**La asignación es su propio no terminal**, separado del punto y coma. Eso es
deliberado: una asignación **sin** `;` es exactamente lo que necesitaría la
cabecera de un `for` (`for (i = 0; ...; i = i + 1)`). Con la producción embebida
en `Statement` no se podría escribir.

El nodo tiene **un solo hijo** —la expresión— y el destino va en `node->symbol`,
no como hijo. Por eso la macro de acceso es `GET_EXPRESSION(node)`:

```
ASSIGNMENT_NODE   symbol = la variable destino
└── [0] la expresión
```

### Semántico

`analyzeAssignmentNode`, en orden:

1. `lookupSymbol` del destino → si no está, *"Asignacion a variable no
   declarada"*.
2. ¿Es una **variable** y no un método? → si no, error.
3. **Reapunta** el nodo al símbolo declarado.
4. Analiza la expresión y pregunta `isAssignable(tipoDestino, tipoExpresión)`.

```c
bool isAssignable(DataType expected, DataType actual) {
    if (expected == actual) return true;
    if (expected == TYPE_FLOAT && actual == TYPE_INT) return true;  // regla 14
    return false;
}
```

**Solo ensancha, nunca trunca.** La regla 14 habla de "coerciones **o
truncamientos**", así que esto es más restrictivo que la letra: rechaza
`int x; x = 3.5;`. Se eligió así porque truncar en silencio pierde información
sin que el programador lo pida. Es una **aclaración de alcance** que conviene
poder nombrar.

### Intermedio y objeto

```
x@3 = t0                 →    movl -12(%rbp), %eax
                              movl %eax, -4(%rbp)
```

Si el destino es **global**, el guardado cambia de forma pero no de fondo:

```asm
movl %eax, g(%rip)
```

`g(%rip)` es direccionamiento **relativo al contador de programa**, que es lo que
permite cargar el ejecutable en cualquier dirección (*código independiente de la
posición*). Es el modo por defecto de los enlazadores actuales.

---

## 8. Invocación a método

### Sintáctico

```
MethodCall : ID '(' ')'                { newMethodCallNode($1, NULL); }
           | ID '(' ArgumentList ')'   { newMethodCallNode($1, $3); }
```

**Los argumentos son los hijos del nodo**, en orden:

```
METHOD_CALL_NODE   symbol = el método
├── [0] primer argumento
├── [1] segundo argumento
└── ...
```

El mismo no terminal aparece en **dos lugares** de la gramática:

```
Statement  : MethodCall ';'     <- como sentencia (se descarta el resultado)
Expression : MethodCall         <- como expresión (se usa el resultado)
```

Es la forma de permitir que un método con resultado se invoque como sentencia,
como pide la letra. **El nodo es el mismo**; lo que cambia es quién lo consume.

### Semántico

`analyzeMethodCall` recibe un `bool isExpression` justamente para distinguir los
dos casos, y valida **cinco cosas en orden**:

1. Que el método exista → *"Método no declarado"*.
2. Que el nombre sea de un método y no de una variable.
3. **Si se usa como expresión, que no sea `void`** → *"El metodo 'g' es void y no
   puede usarse como expresion"*. Al revés es legal: un método con resultado
   **sí** puede usarse como sentencia.
4. Que la **cantidad** de argumentos coincida con la de parámetros formales.
5. Que **cada argumento** sea asignable a su parámetro, por posición.

Los tipos esperados salen de `method->parameters->children[i]->type`, o sea
**del nodo de la declaración del método**. Eso funciona porque el símbolo del
método guarda un puntero a su nodo `PARAMETERS_NODE`.

### Intermedio

```
param 3
param 4
t0 = call suma, 2        # con resultado
...
call saludo, 0           # sin resultado (sentencia void)
```

- Un `TAC_PARAM` **por argumento**, emitidos en orden de índice creciente, o sea
  **de izquierda a derecha** como exige la letra.
- El `call` lleva **la aridad**, que es lo que le dice al generador de objeto
  cuántos `param` pendientes consumir.
- Si la llamada tiene resultado, el `call` lleva un temporal destino; si es una
  sentencia, no.

**Por qué `param` separado y no argumentos dentro del `call`.** Una cuádrupla
tiene lugar para dos operandos: una llamada de cinco argumentos no entra. La
secuencia `param…param + call n` es la representación estándar y además refleja
el orden real de evaluación.

### Objeto: la convención de llamada

Esta parte es **ABI System V AMD64**, y es la que más se pregunta.

```asm
movl $3, %eax
movl %eax, -12(%rbp)     # cada 'param' guarda su valor en una ranura del marco
movl $4, %eax
movl %eax, -16(%rbp)
movl -12(%rbp), %edi     # recién en el 'call' se cargan los registros
movl -16(%rbp), %esi
call suma
movl %eax, -8(%rbp)      # el resultado vuelve en %eax
```

| Posición del argumento | Registro |
| :---: | :---: |
| 1 | `%edi` |
| 2 | `%esi` |
| 3 | `%edx` |
| 4 | `%ecx` |
| 5 | `%r8d` |
| 6 | `%r9d` |
| 7 en adelante | por la pila, **en orden inverso** |

Y el retorno vuelve siempre en **`%eax`**.

**Por qué el `param` no carga el registro directamente.** Porque los argumentos
se evalúan de a uno y evaluar el segundo puede requerir una llamada interna
—`f(g(1), h(2))`— que pisaría el registro ya cargado. Guardándolos primero en
ranuras del marco y cargando los registros **justo antes del `call`**, las
llamadas anidadas funcionan solas.

**El relleno de alineación.** La ABI exige `%rsp` alineado a 16 bytes en el
momento del `call`. Cada `pushq` mueve `%rsp` 8 bytes, así que si se apila una
cantidad **impar** de argumentos hay que restar 8 extra antes y limpiarlo
después.

**Los argumentos de pila se apilan en orden inverso** porque la pila crece hacia
abajo y la ABI exige que el séptimo argumento quede en la dirección más baja.

---

## 9. `return`

### Sintáctico

```
Statement : RETURN Expression ';'   { newReturnNode($2); }
          | RETURN ';'              { newReturnNode(NULL); }
```

Dos producciones, y el nodo tiene **cero o un hijo**. Ésa es toda la diferencia.

### Semántico

`analyzeReturnNode` necesita saber **en qué método está**, y para eso usa
`currentScope->function` — el campo que el ámbito del método guardó al crearse.
Es exactamente la razón por la que ese campo existe.

| Caso | Resultado |
| --- | --- |
| `return;` en método `void` | OK |
| `return;` en método con tipo | Error: *"espera un valor de retorno de tipo INT"* |
| `return expr;` en método `void` | Error: *"es void y no deberia retornar un valor"* |
| `return expr;` con tipo incompatible | Error, usando `isAssignable` (o sea, `int` en un método `float` se acepta) |
| `return` fuera de todo método | Error: *"fuera del cuerpo de una funcion"* |

Y aparte, en `analyzeMethodNode`, la verificación de que **todos los caminos**
retornen (§2).

### Intermedio y objeto

```
ret t0          →   movl -12(%rbp), %eax
                    jmp .L_suma_epilogue
ret             →   jmp .L_main_epilogue
```

El valor se deja en `%eax` y **se salta al epílogo común**. Ver §2 para el porqué.

---

## 10. `if` y `if`/`else`

### Sintáctico

```
Statement : IF '(' Expression ')' Block
          | IF '(' Expression ')' Block ELSE Block
```

**Pregunta casi segura: ¿y el *dangling else*?** El conflicto clásico (en
`if (a) if (b) S else T`, ¿a cuál `if` pertenece el `else`?) **no existe en esta
gramática**, porque el cuerpo de un `if` es un `Block`, y `Block` **exige
llaves**. Las llaves desambiguan en el texto, así que la gramática no tiene qué
desambiguar: `bison -Wall` reporta **cero conflictos**.

Si el cuerpo fuera `Statement` en vez de `Block`, habría un conflicto
desplazar/reducir y habría que resolverlo (lo habitual: que gane el desplazamiento,
o sea que el `else` se asocie al `if` más cercano).

El nodo tiene **dos o tres hijos**, y la macro de acceso lo refleja:

```
IF_ELSE_NODE
├── [0] condición        GET_CONDITION
├── [1] bloque then      GET_IF_BLOCK
└── [2] bloque else      GET_ELSE_BLOCK   <- puede no existir
```

### Semántico

Una sola verificación propia: **la condición tiene que ser `boolean`**
(`analyzeBooleanCondition`). `if (x)` con `x` entero es error. Después analiza
los dos bloques.

### Intermedio: las dos formas

**Sin `else`** — una etiqueta:

```
t2 = x@2 > 2
if t2 false goto L2      # si NO se cumple, saltear el cuerpo
x@2 = 10
L2:
```

**Con `else`** — dos etiquetas:

```
t3 = x@2 == 10
if t3 false goto L3      # si NO se cumple, ir al else
x@2 = 1
goto L4                  # al terminar el then, SALTEAR el else
L3:
x@2 = 2
L4:
```

**El `goto L4` del final del `then` es la pieza que más se olvida.** Sin él, el
flujo caería del `then` al `else` y se ejecutarían los dos.

**La condición se niega.** El `if` del lenguaje dice "si es verdadero, hacé el
cuerpo"; el TAC emite "si es **falso**, saltá el cuerpo". Esa inversión es el
esquema estándar, y existe porque el camino verdadero es el que sigue en línea
recta y no cuesta un salto.

### Objeto

```asm
    movl -4(%rbp), %eax      # cargar los dos operandos
    movl $2, %edx
    cmpl %edx, %eax          # comparar
    setg %al                 # %al = 1 o 0  (SOLO 8 bits)
    movzbl %al, %eax         # extender a 32 bits con ceros
    movl %eax, -16(%rbp)     # guardar el booleano
    movl -16(%rbp), %eax     # releerlo
    cmpl $0, %eax            # ¿es cero?
    je .L2                   # si sí, saltar
```

Tres cosas que se preguntan:

1. **`setCC` solo escribe en un registro de 8 bits**, así que hace falta el
   `movzbl` para extender con ceros a 32. Sin él, el booleano arrastraría los
   bits altos de lo que hubiera antes en `%eax` y el `if` compararía contra cero
   un número que no es ni 0 ni 1.
2. **El orden de `cmpl` en sintaxis AT&T es al revés de como se lee**:
   `cmpl %edx, %eax` calcula `%eax - %edx`, así que `setg` significa
   *primer operando mayor que el segundo*. Es fácil invertirlo por accidente.
3. Se calcula el booleano, se guarda, se relee y recién se compara contra cero.
   Es la consecuencia directa de que **ningún valor viva en un registro entre
   dos cuádruplas**.

**Con `-opt peephole`, las dos últimas instrucciones desaparecen:** si una
comparación es consumida inmediatamente por un salto, se emiten fusionadas.

```asm
    cmpl %edx, %eax
    jge .L1              # en vez de setl + movzbl + cmpl $0 + je
```

Cuatro instrucciones menos por cada condición, y la negación se resuelve
**eligiendo el mnemónico opuesto** (`jge` es la negación de `jl`).

---

## 11. `while`

### Sintáctico

```
Statement : WHILE '(' Expression ')' Block
```

```
WHILE_NODE
├── [0] condición        GET_CONDITION
└── [1] cuerpo           GET_WHILE_BLOCK
```

Mismo chequeo semántico que el `if`: la condición tiene que ser `boolean`.

### Intermedio

```
L0:                      # etiqueta de INICIO
t0 = x@2 < 3
if t0 false goto L1      # salida del ciclo
t1 = x@2 + 1
x@2 = t1
goto L0                  # volver a evaluar la condición
L1:                      # etiqueta de FIN
```

**La diferencia con el `if` es el `goto` de vuelta al inicio**, y que la etiqueta
de inicio se emite **antes de la condición**, porque la condición se reevalúa en
cada vuelta.

Detalle fino: **las dos etiquetas se piden juntas al empezar** (`L0` y `L1`),
antes de generar nada del cuerpo. Por eso están numeradas consecutivamente aunque
`L1:` aparezca mucho más abajo. Si en el cuerpo hay otro `while`, sus etiquetas
tendrán números **mayores**, y eso es lo que permite anidar sin colisiones.

### Objeto

Traducción directa: la etiqueta se vuelve `.L0:` (con punto adelante, lo que la
marca como **local al archivo** y evita que entre en la tabla de símbolos del
objeto) y el `goto` se vuelve `jmp .L0`.

---

## 12. Bloques y ocultamiento de nombres

### El programa

```c
int x;              // global
void main() {
    int x;          // oculta a la global
    x = 1;
    {
        int x;      // oculta a la anterior
        x = 2;
    }
}
```

### Qué pasa en el semántico

Cada `BLOCK_NODE` crea un **ámbito nuevo** colgado del actual
(`analyzeSemantics` se llama recursivamente a sí misma con el ámbito padre). Las
tres `x` se registran en ámbitos distintos, así que **no hay error de
redeclaración**: la verificación usa `lookupSymbolInScope`, que mira solo el
ámbito actual.

```
Bloque
  | Declaracion Variable: x #1       <- global
  | Metodo: main
  |   | Bloque
  |   |   | Declaracion Variable: x #3
  |   |   | Asignacion a: x #3        <- resolvió a la del medio
  |   |   | Bloque
  |   |   |   | Declaracion Variable: x #4
  |   |   |   | Asignacion a: x #4    <- resolvió a la interna
```

Cada uso resolvió a la declaración **más cercana hacia arriba**, porque
`lookupSymbol` sube por los padres y devuelve la primera que encuentra.

### Qué pasa en el TAC

```
global x@1, INT
begin_func main, 0
local x@3, INT
x@3 = 1
local x@4, INT
x@4 = 2
```

**Acá se ve para qué sirve el `@uid`.** Las tres variables se llaman `x`, pero en
la lista plana son `x@1`, `x@3` y `x@4`: tres nombres distintos, tres ranuras
distintas. Sin el sufijo, el generador de objeto les daría la misma dirección y
el programa calcularía mal.

**El bloque interno no genera ninguna instrucción propia.** Los bloques existen
en el árbol y en la tabla de símbolos; en el TAC se "aplanan". El ámbito ya hizo
su trabajo —desambiguar los nombres— y a partir de ahí no hace falta.

---

## 13. El marco de pila: de dónde salen los offsets

Si piden mostrar el assembly, va a haber que justificar los números. El marco se
arma en **dos pasadas sobre el TAC de la función**:

**Pasada 1 — dimensionar** (`buildFrameLayout`): recorre de `begin_func` a
`end_func` y pide una ranura de **4 bytes** por cada:

- `param_decl` (parámetro formal)
- `local` (variable local)
- resultado que sea un **temporal**
- argumento pendiente del **pico** de llamadas anidadas

Al final, `frameSize = ALIGN_UP(total, 16)`.

**Pasada 2 — emitir**: ahora que se conoce el tamaño, se emite el prólogo con el
`subq` y se traduce instrucción por instrucción.

**Por qué dos pasadas.** El `subq $N, %rsp` del prólogo es **la primera
instrucción** que se emite, y `N` no se sabe hasta haber visto la función entera.
O se hacen dos pasadas, o habría que emitir a un buffer y parchear el número
después.

### Ejemplo verificado

```c
int main() {
    int x;
    x = 0;
    while (x < 3) { x = x + 1; }
    if (x > 2) { x = 10; }
    if (x == 10) { x = 1; } else { x = 2; }
    return x;
}
```

| Ranura | Qué es | Offset |
| --- | --- | --- |
| 1 | `x@2` | `-4(%rbp)` |
| 2 | `t0` (la comparación del `while`) | `-8(%rbp)` |
| 3 | `t1` (`x + 1`) | `-12(%rbp)` |
| 4 | `t2` (la comparación del primer `if`) | `-16(%rbp)` |
| 5 | `t3` (la comparación del segundo `if`) | `-20(%rbp)` |

5 ranuras × 4 bytes = **20** → alineado a 16 → **32**. Y el assembly real dice
`subq $32, %rsp`. ✔

**Todas las ranuras tienen offset negativo** respecto de `%rbp`, porque la pila
crece hacia abajo. Los **argumentos que llegaron por la pila** son la excepción:
están en `+16(%rbp)` y para arriba (8 de la dirección de retorno + 8 del `%rbp`
guardado).

**Cada ranura ocupa 4 bytes aunque el tipo sea `boolean`**: alinear todo a 4
evita cualquier cuenta de relleno, y la memoria desperdiciada es irrelevante.

---

## 14. El optimizador, en dos minutos

No se aplica salvo que se pase `-opt`. Son **siete optimizaciones**, seis sobre
el TAC y una sobre el assembly:

| Flag | Qué hace |
| --- | --- |
| `const-fold` | `2 + 3` → `5` en tiempo de compilación |
| `const-prop` | Si `x = 5`, reemplaza los usos de `x` por `5` |
| `const-branch` | `if false goto L` con condición constante → se borra o se vuelve `goto` |
| `dead-code` | Borra lo inalcanzable (tras un `return`/`goto`) y las asignaciones que nadie lee |
| `self-assign` | Borra `x = x` |
| `peephole` | Borra un `goto L` seguido de `L:`, y fusiona comparación + salto en el assembly |
| `strength-reduce` | `x * 8` → `sall $3` (desplazamiento) |

Se aplican **en un bucle hasta punto fijo** (máximo 16 vueltas), porque una
habilita a otra: plegar crea una constante, que la propagación lleva más lejos,
lo que puede volver constante una condición, lo que vuelve inalcanzable un
bloque. **Termina** porque ninguna pasada agrega instrucciones.

La única que necesita un **análisis** previo es `dead-code`, que se apoya en
**análisis de variables vivas**: se parte el TAC en bloques básicos, se arma el
grafo de flujo de control (CFG) y se resuelven las ecuaciones

```
LIVE_OUT(B) = unión de LIVE_IN(S) para cada sucesor S
LIVE_IN(B)  = USE(B) ∪ (LIVE_OUT(B) − DEF(B))
```

hasta que nada cambia. Es un **análisis hacia atrás** (la información fluye del
final hacia el principio), y los conjuntos se representan como **máscaras de
bits** (`uint64_t`), de modo que la unión es un `or`, la diferencia un `and` con
negación y la pertenencia un desplazamiento.

---

## 15. Apéndices

### A. Las 14 reglas semánticas y dónde se verifica cada una

| # | Regla | Dónde |
| --- | --- | --- |
| 1 | Todo identificador se declara antes de usarse | `lookupSymbol` en identificador / llamada / asignación |
| 2 | No redeclarar en el mismo ámbito | `lookupSymbolInScope` en declaración de variable y de método |
| 3 | Existe `main` sin parámetros | `checkMainMethodExists`, al cerrar el ámbito global |
| 4-8 | Tipos de operandos por operador | `resolveOperation` (la tabla de §4) |
| 9 | Condición de `if`/`while` booleana | `analyzeBooleanCondition` |
| 10 | Operandos de aritméticos y relacionales | `resolveNumericOperation` |
| 11 | `==` entre el mismo tipo básico | `resolveOperation`, caso `OP_EQ` |
| 12 | Aridad y tipos en una invocación | `analyzeMethodCall` |
| 13 | Tipo de retorno compatible | `analyzeReturnNode` + `guaranteesReturn` |
| 14 | Coerción `int`/`float` | `isAssignable` |

### B. Los 23 opcodes del TAC

| Familia | Opcodes |
| --- | --- |
| Métodos | `begin_func`, `end_func`, `param_decl`, `param`, `call`, `ret` |
| Declaraciones | `global`, `local` |
| Control | `label`, `goto`, `if_false`, `if_true` |
| Asignación | `assign` |
| Aritmética | `add`, `sub`, `mul`, `div`, `mod` |
| Relacionales | `lt`, `gt`, `eq` |
| Unarios | `uminus`, `not` |

**No hay `and` ni `or`** (§6). **No hay opcodes de `float`**: esta implementación
genera código solo para tipos enteros y lógicos, que es el alcance que fija el
enunciado, y un programa con `float` se **rechaza** al empezar a generar, con su
número de línea. El front-end sí lo entiende entero, incluida la regla 14.

### C. Precedencia, de menor a mayor

```
||   <   &&   <   ==   <   < >   <   + -   <   * / %   <   ! - (unarios)
```

Todos **asociativos a izquierda**, salvo los unarios.

### D. Checklist: "me dieron un programa, ¿qué hago?"

1. **Tokens.** Palabra por palabra. Las reservadas antes que los identificadores.
   Los literales llevan su valor aparte.
2. **Árbol.** De abajo hacia arriba. Precedencia alta = más abajo en el árbol.
   Los bloques y los métodos son nodos con hijos; los argumentos de una llamada
   son hijos del nodo de llamada.
3. **Tabla de símbolos.** Un ámbito por bloque, colgando del padre. Numerar los
   `uid` **en el orden en que aparecen las declaraciones al recorrer el árbol de
   arriba hacia abajo**, sin olvidar que los métodos también consumen un número.
4. **Tipos.** De las hojas hacia la raíz. Las constantes ya vienen tipadas.
5. **TAC.** Postorden: operandos antes que operación. Un temporal por operación.
   Las etiquetas se piden por par en `if`/`while` y de a una en el corto
   circuito. Reiniciar los temporales en cada método; **no** reiniciar las
   etiquetas.
6. **Assembly.** Contar las ranuras del marco (locales + parámetros + temporales),
   multiplicar por 4, alinear a 16. Después, cada cuádrupla: **cargar, operar,
   guardar**.

### E. Las cinco frases que conviene tener listas

- *"El parser no sabe de tipos ni de ámbitos: solo construye el árbol."*
- *"El `uid` lo asigna el semántico al registrar la declaración; por eso el TAC
  puede ser una lista plana sin tabla de símbolos."*
- *"No hay opcodes `and` ni `or` porque el corto circuito es control de flujo, no
  una operación."*
- *"Ningún valor sobrevive en un registro entre dos cuádruplas: por eso el código
  es largo y por eso es verificable."*
- *"El marco se arma en dos pasadas porque el `subq` del prólogo necesita un
  número que recién se conoce al final."*
