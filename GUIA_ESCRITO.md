# Guía del Escrito: una construcción a la vez, de punta a punta

Para el escrito de una hora, donde hay que tomar un programa (o un pedazo) y
mostrar **cómo lo ve el compilador en cada etapa**: tokens, árbol, tabla de
símbolos, código de tres direcciones y assembly.

No hace falta recordar nombres exactos de funciones: hace falta saber **qué
decide cada etapa y por qué**. Los nombres están igual, entre paréntesis, por si
cae una pregunta de "¿dónde está eso en su código?".

**Las doce secciones tienen las cinco etapas completas, siempre**, aunque se
repitan cosas ya explicadas. Es a propósito: cada ejemplo sirve como repaso de
los anteriores, y en el escrito vas a tener que dibujar el árbol entero, no la
parte nueva.

Todas las salidas son **reales**: salieron de compilar esos programas. Si algo no
coincide, manda el compilador.

---

## Índice

| # | Construcción | Lo nuevo que trae |
| --- | --- | --- |
| [0](#0-el-andamiaje) | El andamiaje | Cómo leer los diagramas; las reglas que se repiten |
| [1](#1-declaración-de-variables) | Declaración de variables | Globales vs locales, `int a, b;` |
| [2](#2-declaración-de-métodos) | Declaración de métodos | Parámetros, `void`, ámbito del método |
| [3](#3-constantes-e-identificadores) | Constantes e identificadores | Las hojas del árbol |
| [4](#4-expresiones-aritméticas) | Expresiones aritméticas | Precedencia y temporales |
| [5](#5-operadores-unarios---y-) | Unarios `-` y `!` | `%prec UMINUS`, nodos de un solo hijo |
| [6](#6--y--el-corto-circuito) | `&&` y `\|\|` | Corto circuito con etiquetas |
| [7](#7-asignación) | Asignación | Destino local vs global |
| [8](#8-invocación-a-método) | Invocación a método | Convención de llamada |
| [9](#9-return) | `return` | Todos los caminos, epílogo común |
| [10](#10-if-y-if-else) | `if` y `if`/`else` | Una etiqueta vs dos |
| [11](#11-while) | `while` | El salto de vuelta |
| [12](#12-bloques-y-ocultamiento-de-nombres) | Bloques y ocultamiento | Ámbitos anidados, para qué sirve el `@uid` |
| [13](#13-el-marco-de-pila) | El marco de pila | De dónde salen los offsets |
| [14](#14-el-optimizador) | El optimizador | Las siete pasadas |
| [15](#15-apéndices) | Apéndices | Reglas, opcodes, precedencias, checklist |

---

## 0. El andamiaje

### Las seis etapas y en qué archivo vive cada una

```
archivo.ctds
   │
   ├─ 1. ANÁLISIS LÉXICO ......... src/parser/lexer.l        (Flex)   → tokens
   ├─ 2. ANÁLISIS SINTÁCTICO ..... src/parser/bison.y        (Bison)  → AST
   ├─ 3. ANÁLISIS SEMÁNTICO ...... src/semantic/             → AST anotado + tabla de símbolos
   ├─ 4. CÓDIGO INTERMEDIO ....... src/intermediate_code/    → lista de cuádruplas (TAC)
   ├─ 5. OPTIMIZADOR (opcional) .. src/optimizer/            → la misma lista, más corta
   └─ 6. CÓDIGO OBJETO ........... src/object_code/          → texto assembly x86-64
                                                               → gcc → ejecutable
```

Se puede frenar en cualquier etapa: `-target scan | parse | sem | codinter |
assembly`, que escriben `.lex`, `.sint`, `.sem`, `.ci`, `.ass`. Sin `-target`,
llega hasta el ejecutable. Quien orquesta es `compile()` en
`src/core/driver.c`: un `switch` sobre la etapa pedida donde **cada caso cae en
cascada sobre los anteriores**.

### Cómo leer los diagramas de esta guía

**Árbol sintáctico (AST).** Cada caja es un nodo. Debajo del nombre va lo que el
nodo guarda: su símbolo, su tipo, su operador. Las ramas son los hijos, **en
orden de índice**:

```
METODO  suma : INT              ← nodeType = METHOD_DECLARATION_NODE
├── PARAMETROS                  ← hijo [0]
│   └── DECL_VAR  a : INT
└── BLOQUE  (cuerpo)            ← hijo [1]
```

**Árbol de ámbitos (tabla de símbolos).** Cada caja es un `Scope`. Adentro, los
símbolos que registra con su `uid`. Las ramas son los sub-ámbitos:

```
ÁMBITO 0 — global          parent = NULL
│   suma#1 : método INT
└── ÁMBITO 1 — cuerpo de suma    parent = ámbito 0 · function = suma#1
        a#2 : INT
```

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

**Hay un solo tipo de nodo para todo**, y se distingue por `nodeType`. La
alternativa era un `struct` por construcción; se eligió el nodo único porque
**todos los recorridos posteriores son un `switch` sobre `nodeType`**, y agregar
una construcción nueva es agregar un valor al enum, no un tipo nuevo.

### Las cuatro reglas que se repiten en todas las trazas

**1. El parser no sabe nada de tipos ni de ámbitos.** Construye el árbol y nada
más. Cuando ve un identificador crea un `Symbol` **provisorio** que solo guarda
el texto del nombre, con `uid = 0`. El `0` significa *"todavía no está declarado
en ninguna tabla"*.

**2. El `uid` lo asigna el semántico.** Cuando una declaración se registra en un
ámbito (`appendSymbol`) recibe el próximo número de un contador global que
arranca en 1. Por eso en el `.sint` todos los símbolos son `#0` y en el `.sem` ya
tienen número: **es la diferencia observable entre las dos etapas**.

**3. Un uso se "reapunta" a su declaración.** Al resolver un nombre contra la
tabla (`lookupSymbol`), el nodo del uso deja de apuntar a su símbolo provisorio y
pasa a apuntar al **símbolo declarado** (`resolveNodeSymbol`), liberando el
provisorio. De ahí en adelante, un uso y su declaración **son el mismo objeto en
memoria**.

**4. Las etapas posteriores no revalidan nada.** Si la generación de código
corre, es porque hubo cero errores semánticos. El generador de TAC **asume** que
los tipos están bien. Si encuentra algo inválido, es un bug del semántico.

### El nombre calificado `x@3`

En el TAC, cada variable se escribe `nombre@uid`. **Por qué:** dos variables
distintas pueden llamarse igual si están en bloques distintos; en el árbol se
distinguen por el ámbito, pero el TAC es una **lista plana sin ámbitos**. Si las
dos se escribieran `x`, el generador de objeto les daría la misma ranura. El
`@uid` las separa, y como es único en todo el programa, **el TAC no necesita
tabla de símbolos**.

Las **globales pierden el sufijo** al volverse símbolos del assembly (`g`, no
`g@1`): el ámbito global es uno solo, no puede haber repetidos, y así `nm` sobre
el ejecutable muestra nombres legibles.

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

### Etapa 1 — Análisis léxico

El lexer devuelve un token por vez. **No existe la noción de "declaración"**: hay
símbolos sueltos.

```
[Linea 1] INT          int
[Linea 1] ID           g
[Linea 1] ';'          ;
[Linea 2] INT          int
[Linea 2] ID           a
[Linea 2] ','          ,
[Linea 2] ID           b
[Linea 2] ';'          ;
[Linea 3] VOID         void
[Linea 3] ID           main
[Linea 3] '('          (
[Linea 3] ')'          )
[Linea 3] '{'          {
[Linea 4] INT          int
[Linea 4] ID           x
[Linea 4] ';'          ;
[Linea 5] INT          int
[Linea 5] ID           y
[Linea 5] ','          ,
[Linea 5] ID           z
[Linea 5] ';'          ;
[Linea 6] '}'          }
```

Qué reglas del lexer actuaron:

| Texto | Regla | Token |
| --- | --- | --- |
| `int` | `"int"` | `INT` |
| `g`, `a`, `b`… | `{ID}` = `{ALPHA}({ALPHA_NUM})*` | `ID` + `yylval.strval = strdup(yytext)` |
| `;` `,` `(` `)` `{` `}` | `"("\|")"\|"{"\|"}"\|";"\|","` | **el carácter mismo** (`return yytext[0]`) |
| espacios y saltos | `[ \t\r\n]+` | ninguno: se descartan |

Dos cosas que se preguntan:

- **Las palabras reservadas van antes que `{ID}` en el archivo.** Flex usa la
  **coincidencia más larga** y, ante empate, **la regla escrita primero**. `int`
  matchea `"int"` y `{ID}` con 3 caracteres: gana la primera. Si `{ID}` estuviera
  arriba, `int` sería un identificador y no se reconocería ninguna declaración.
- **Los delimitadores no tienen un token con nombre**: se devuelve el código
  ASCII del carácter. Por eso en la gramática aparecen como `';'` y `','` entre
  comillas simples. Ahorra declarar seis tokens.

### Etapa 2 — Análisis sintáctico

Producciones que intervienen:

```
Program              : Declarations                        { newBlockNode($1); }
Declarations         : %empty | Declarations Declaration
Declaration          : VariableDeclaration | MethodDeclaration
VariableDeclaration  : Type IdentifierList ';'             { resolveVariableDefinition($1, $2); }
IdentifierList       : ID                                  { appendNode(NULL, newIdentifierNode($1)); }
                     | IdentifierList ',' ID               { appendNode($1, newIdentifierNode($3)); }
Type                 : INT { TYPE_INT } | BOOLEAN { TYPE_BOOL } | FLOAT { TYPE_FLOAT }
```

**Cómo se resuelve `int a, b;` — ésta es la parte interesante.** Bison es
**ascendente (LALR(1))**: reduce de abajo hacia arriba, así que cuando ve `a` y
`b` **todavía no redujo el tipo**, aunque en el texto el `int` esté antes. Se
resuelve en dos pasos:

1. `IdentifierList` acumula los identificadores en un arreglo temporal
   (`NodeArray`) como nodos `ID_NODE` **sin tipo**.
2. Al reducir la producción completa ya se tiene el `Type`, y
   `resolveVariableDefinition` recorre el arreglo **convirtiendo cada nodo en
   sitio**: `ID_NODE` → `VARIABLE_DECLARATION_NODE`, y le escribe el tipo al nodo
   y a su símbolo.

O sea: **una declaración múltiple produce N nodos independientes**, exactamente
como si se hubieran escrito en líneas separadas. No hay un nodo "declaración
múltiple", y eso simplifica todo lo que viene después.

**La recursión es a izquierda** (`IdentifierList ',' ID`), igual que todas las
listas de esta gramática: en un parser ascendente la recursión a derecha obliga a
apilar la lista entera antes de reducir; con recursión a izquierda se reduce de a
un elemento y la pila queda acotada.

**El árbol que queda:**

```
BLOQUE                          ← raíz del programa (Program → Declarations)
├── DECL_VAR  g : INT           ← hijo [0]
├── DECL_VAR  a : INT           ← hijo [1]   (de "int a, b;")
├── DECL_VAR  b : INT           ← hijo [2]   (del mismo "int a, b;")
└── METODO  main : VOID         ← hijo [3]
    ├── PARAMETROS              ← hijo [0] del método: existe aunque esté vacío
    └── BLOQUE  (cuerpo)        ← hijo [1] del método
        ├── DECL_VAR  x : INT
        ├── DECL_VAR  y : INT
        └── DECL_VAR  z : INT
```

Sí: **la raíz es un `BLOQUE` con cuatro hijos** (tres declaraciones y un método),
y el método tiene exactamente **dos** hijos, `PARAMETROS` y `BLOQUE`, y ese
bloque tiene tres hijos. `a` y `b` son **dos nodos hermanos**, no uno con dos
nombres.

El `.sint` que imprime el compilador (`-target parse`) es ese mismo árbol en
texto, con la indentación marcando la profundidad:

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

### Etapa 3 — Análisis semántico

Recorrido: `analyzeSemantics` crea el ámbito global y baja por el árbol
(`analyzeNodeSemantics` es el `switch` sobre `nodeType`). Para una declaración
(`analyzeVariableDeclarationNode`) hace **dos cosas**:

1. **¿Ya está declarada en este mismo ámbito?** `lookupSymbolInScope`, que mira
   **solo el ámbito actual y no los padres**. Si está → *"La variable 'x' ya fue
   declarada previamente en este ambito"*.
2. Si no, la registra: `appendSymbol`, que le asigna el `uid` y marca
   `isGlobal = (scope->parent == NULL)`.

**Por qué `lookupSymbolInScope` y no `lookupSymbol`:** redeclarar en el **mismo**
ámbito es un error; declarar el mismo nombre en un bloque **interno** es
ocultamiento y es legal. Las dos funciones de búsqueda existen por esa
distinción.

**El árbol de ámbitos que queda:**

```
ÁMBITO 0 — global                     parent = NULL · function = NULL · level 0
│   g#1    : INT      (isGlobal = true)
│   a#2    : INT      (isGlobal = true)
│   b#3    : INT      (isGlobal = true)
│   main#4 : método VOID, 0 parámetros
│
└── ÁMBITO 1 — cuerpo de main         parent = ámbito 0 · function = main#4 · level 1
        x#5 : INT     (isGlobal = false)
        y#6 : INT     (isGlobal = false)
        z#7 : INT     (isGlobal = false)
```

Tres cosas para tener listas:

- **`main` también es un símbolo y consume el `uid` 4.** Por eso los locales
  arrancan en 5. Si te piden numerar a mano, no te olvides de los métodos.
- **El cuerpo del método no crea un ámbito aparte.** El ámbito del método **es**
  el del cuerpo: los parámetros y las locales de primer nivel conviven en el
  mismo. Un bloque **anidado** adentro sí crearía uno nuevo (ver §12).
- **`function = main#4`** es lo que después le permite a un `return` saber qué
  tipo se espera.

El `.sem` es el mismo árbol del `.sint` pero con los `uid` resueltos:

```
Bloque
  | Declaracion Variable: g #1 (Tipo INT)
  | Declaracion Variable: a #2 (Tipo INT)
  | Declaracion Variable: b #3 (Tipo INT)
  | Metodo: main (Retorna VOID)
  |   | Parametros
  |   | Bloque
  |   |   | Declaracion Variable: x #5 (Tipo INT)
  |   |   | Declaracion Variable: y #6 (Tipo INT)
  |   |   | Declaracion Variable: z #7 (Tipo INT)
```

### Etapa 4 — Código intermedio

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

Línea por línea, **por qué está cada una**:

| Instrucción | Por qué |
| --- | --- |
| `global g@1, INT` | Una variable global necesita espacio en el **segmento de datos** del ejecutable. El TAC lo declara explícitamente para que el generador de objeto sepa qué poner en `.data` sin reconstruir ámbitos. El tipo viaja porque determina el tamaño |
| `begin_func main, 0` | Marca dónde **empieza** una función y con **cuántos parámetros**. El generador de objeto la usa para saber dónde abrir un marco de pila nuevo y cuántos `param_decl` esperar |
| `local x@5, INT` | Una local necesita una **ranura en el marco**. No ejecuta nada: es la información que la pasada de dimensionamiento cuenta para calcular el tamaño del marco |
| `ret` | **Implícito**: `main` es `void` y su cuerpo no terminaba en un `return` escrito. Se agrega para que **toda función termine en `ret`**, garantía de la que depende el generador de objeto |
| `end_func main` | Marca el **fin** de la función. Delimita hasta dónde llega el cuerpo: el recorrido del generador de objeto corre "de `begin_func` a `end_func`" |

**Por qué `global` y `local` son opcodes distintos** en vez de uno solo con una
bandera: van a lugares completamente distintos del ejecutable —`.data` contra el
marco de pila— y se emiten en momentos distintos (las globales, todas juntas al
principio; las locales, dentro de su función). Separarlos evita un `if` en cada
uso. Cuál se emite lo decide una sola línea:

```c
TACOperation declOp = node->symbol->isGlobal ? TAC_GLOBAL_DECL : TAC_LOCAL_DECL;
```

### Etapa 5 — Código objeto

```asm
.data                      # ① sección de datos inicializados
.globl g                   # ② el símbolo g es visible para el enlazador
.align 4                   # ③ alinearlo a 4 bytes
g:                         # ④ la etiqueta: acá empieza g
    .long 0                # ⑤ 4 bytes inicializados en 0
.globl a
.align 4
a:
    .long 0
.globl b
.align 4
b:
    .long 0

.text                      # ⑥ a partir de acá viene código, no datos

.globl main
.type main, @function      # ⑦ decirle al enlazador que main es una función
main:
    pushq %rbp             # ⑧ guardar el marco del llamador
    movq %rsp, %rbp        # ⑨ el marco nuevo empieza acá
    subq $16, %rsp         # ⑩ reservar las ranuras de x, y, z de una vez
    jmp .L_main_epilogue   # ⑪ el 'ret' del TAC
.L_main_epilogue:
    movl $0, %eax          # ⑫ código de salida 0
    leave                  # ⑬ deshacer el marco
    ret                    # ⑭ volver al llamador

.section .note.GNU-stack,"",@progbits   # ⑮ la pila no necesita ser ejecutable
```

Por qué cada una, en este ejemplo:

- **①–⑤ Las tres globales.** `.long 0` reserva 4 bytes puestos en cero: C-TDS no
  tiene inicializadores en la declaración, así que todas arrancan en 0. `.align 4`
  evita accesos desalineados. `.globl` las hace visibles al enlazador.
- **⑥ El orden importa.** `.data` va **antes** que `.text` porque el ensamblador
  resuelve los símbolos en una pasada: cuando el código mencione `g`, ya tiene que
  estar declarada.
- **⑧⑨⑩ El prólogo.** `pushq %rbp` + `movq %rsp, %rbp` es el preámbulo estándar
  que establece el marco; `subq $16` reserva el espacio. **Las tres locales no
  emiten ni una instrucción**: el `case TAC_LOCAL_DECL` del despachador está
  **vacío a propósito**, porque el espacio ya se reservó acá, de una sola vez.
  3 locales × 4 bytes = 12, alineado a 16 → `$16`.
- **⑪ El `jmp` al epílogo.** Todo `ret` del TAC se vuelve un salto al epílogo
  común, en vez de duplicar el desarmado del marco en cada `return`.
- **⑫ `movl $0, %eax`** aparece **solo porque esta función es `main` y es
  `void`**: el sistema operativo toma `%eax` como código de salida del proceso, y
  sin esta línea devolvería lo que haya quedado en el registro.
- **⑬ `leave`** es la abreviatura de `movq %rbp, %rsp` + `popq %rbp`: deshace lo
  que hizo el prólogo.
- **⑮** Sin esa nota, el enlazador marca la pila como ejecutable por
  compatibilidad, lo que es un problema de seguridad y ensucia la compilación.

---

## 2. Declaración de métodos

### El programa

```c
int suma(int a, int b) {
    return a + b;
}
void saludo() {
}
void main() {
}
```

### Etapa 1 — Análisis léxico

```
[Linea 1] INT          int
[Linea 1] ID           suma
[Linea 1] '('          (
[Linea 1] INT          int
[Linea 1] ID           a
[Linea 1] ','          ,
[Linea 1] INT          int
[Linea 1] ID           b
[Linea 1] ')'          )
[Linea 1] '{'          {
[Linea 2] RETURN       return
[Linea 2] ID           a
[Linea 2] '+'          +
[Linea 2] ID           b
[Linea 2] ';'          ;
[Linea 3] '}'          }
[Linea 4] VOID         void
[Linea 4] ID           saludo
...
```

El lexer **no distingue** el `int` del tipo de retorno del `int` de un parámetro:
devuelve el mismo token `INT` en los dos casos. Quien les da significados
distintos es la gramática, según la posición. Lo mismo con `suma`: es un `ID`
igual que `a`; que sea un método lo decide la producción que lo consume.

### Etapa 2 — Análisis sintáctico

```
MethodDeclaration : Type ID '(' ParameterList ')' Block   { newMethodDeclaration($1, $2, $4, $6); }
                  | VOID ID '(' ParameterList ')' Block   { newMethodDeclaration(TYPE_VOID, $2, $4, $6); }

ParameterList     : %empty      { newParametersNode(NULL); }
                  | Parameters  { newParametersNode($1); }

Parameters        : Type ID                   { appendNode(NULL, newParameterDeclaration($1, $2)); }
                  | Parameters ',' Type ID    { appendNode($1, newParameterDeclaration($3, $4)); }

Block             : '{' VariableDeclarations Statements '}'  { newBlockNode(mergeNodeArrays($2, $3)); }
```

**Hay dos producciones de método y no una** porque `VOID` no está dentro del no
terminal `Type`: `Type` son los tipos que puede tener una **variable**, y `void`
no lo es. Separarlas evita que la gramática acepte `void x;`.

**`Parameters` no es lo mismo que `ParameterList`.** `ParameterList` es el que
puede estar vacío y el que **siempre** construye el nodo contenedor; `Parameters`
es la lista no vacía. Por eso el `PARAMETROS` existe aunque no haya parámetros.

**`Block` junta declaraciones y sentencias en un solo arreglo** (`mergeNodeArrays`),
lo que impone que **las declaraciones van primero**, como pide la letra: la
gramática ni siquiera permite escribir una declaración después de una sentencia.

**El árbol que queda:**

```
BLOQUE                                    ← raíz del programa
├── METODO  suma : INT
│   ├── PARAMETROS                        ← hijo [0]
│   │   ├── DECL_VAR  a : INT
│   │   └── DECL_VAR  b : INT
│   └── BLOQUE  (cuerpo de suma)          ← hijo [1]
│       └── RETURN
│           └── OPERADOR  +
│               ├── ID  a
│               └── ID  b
├── METODO  saludo : VOID
│   ├── PARAMETROS                        ← sin hijos, pero el nodo existe
│   └── BLOQUE  (cuerpo vacío)            ← sin hijos, pero el nodo existe
└── METODO  main : VOID
    ├── PARAMETROS
    └── BLOQUE  (cuerpo vacío)
```

**Los parámetros formales son `DECL_VAR`, el mismo tipo de nodo que una variable
local.** Tiene sentido: un parámetro *es* una variable local, solo que su valor
inicial lo pone el llamador. Esa decisión hace que el semántico los registre con
el mismo código y que el generador de objeto les dé una ranura igual que a las
locales.

El `.sint`:

```
Bloque
  | Metodo: suma (Retorna INT)
  |   | Parametros
  |   |   | Declaracion Variable: a #0 (Tipo INT)
  |   |   | Declaracion Variable: b #0 (Tipo INT)
  |   | Bloque
  |   |   | Return
  |   |   |   | Operacion: +
  |   |   |   |   | ID: a #0 (Tipo VOID)
  |   |   |   |   | ID: b #0 (Tipo VOID)
  | Metodo: saludo (Retorna VOID)
  |   | Parametros
  |   | Bloque
  | Metodo: main (Retorna VOID)
  |   | Parametros
  |   | Bloque
```

Fijate que los `ID` dicen **`(Tipo VOID)`**: no es que sean `void`, es que
`newNode` inicializa el tipo en `TYPE_VOID` y **el parser nunca lo toca**. El
tipo de una expresión lo pone el semántico.

### Etapa 3 — Análisis semántico

`analyzeMethodNode` hace cinco cosas **en este orden**, y el orden importa:

1. **¿Ya existe el nombre en este ámbito?** → error de redeclaración.
2. **Registra el método en el ámbito actual** (`appendSymbol`) — **antes** de
   analizar el cuerpo. Eso es lo que permite la **recursión**: cuando el cuerpo
   diga `suma(...)`, el nombre ya está en la tabla.
3. **Crea el ámbito del método**, con `function = ` el símbolo del método.
4. **Analiza los parámetros en ese ámbito nuevo**, no en el global: los
   parámetros son locales de la función.
5. **Analiza el cuerpo** en el mismo ámbito y, si el método **no es `void`**,
   verifica que todos los caminos terminen en `return` (`guaranteesReturn`, §9).

**El árbol de ámbitos:**

```
ÁMBITO 0 — global                        parent = NULL · level 0
│   suma#1   : método INT, 2 parámetros
│   saludo#4 : método VOID, 0 parámetros
│   main#5   : método VOID, 0 parámetros
│
├── ÁMBITO 1 — cuerpo de suma            parent = ámbito 0 · function = suma#1 · level 1
│       a#2 : INT
│       b#3 : INT
│
├── ÁMBITO 2 — cuerpo de saludo          parent = ámbito 0 · function = saludo#4 · level 1
│       (vacío)
│
└── ÁMBITO 3 — cuerpo de main            parent = ámbito 0 · function = main#5 · level 1
        (vacío)
```

**El orden de los `uid` cuenta la historia del recorrido:** `suma` se registra
primero (#1), después sus parámetros en su propio ámbito (#2, #3), después
`saludo` (#4) y después `main` (#5). O sea que el recorrido es **en profundidad**:
entra al método y lo termina antes de pasar al siguiente.

**Los tres ámbitos de método son hermanos**, todos hijos del global. Ninguno ve
las variables del otro: por eso `a` y `b` no existen fuera de `suma`.

El `.sem`:

```
Bloque
  | Metodo: suma (Retorna INT)
  |   | Parametros
  |   |   | Declaracion Variable: a #2 (Tipo INT)
  |   |   | Declaracion Variable: b #3 (Tipo INT)
  |   | Bloque
  |   |   | Return
  |   |   |   | Operacion: +
  |   |   |   |   | ID: a #2 (Tipo INT)     ← ahora sí tiene tipo
  |   |   |   |   | ID: b #3 (Tipo INT)
```

Los `ID` pasaron de `#0 (Tipo VOID)` a `#2 (Tipo INT)`: **se resolvieron contra
la declaración** y copiaron su tipo.

### Etapa 4 — Código intermedio

```
begin_func suma, 2
param_decl a@2, INT
param_decl b@3, INT
t0 = a@2 + b@3
ret t0
end_func suma
begin_func saludo, 0
ret
end_func saludo
begin_func main, 0
ret
end_func main
```

| Instrucción | Por qué |
| --- | --- |
| `begin_func suma, 2` | Abre la función. El **2** es la aridad: le dice al generador de objeto cuántos `param_decl` va a encontrar y cuántos registros de argumento tiene que leer |
| `param_decl a@2, INT` | **No es ejecutable**: declara que la función *recibe* ese parámetro. Se traduce a "copiar el registro de argumento número N a una ranura del marco". Tiene que ser una instrucción aparte porque el orden de los `param_decl` **es** el orden de los registros |
| `t0 = a@2 + b@3` | La suma necesita un lugar donde dejar su resultado, y una cuádrupla no puede anidar expresiones: de ahí el temporal |
| `ret t0` | Devuelve el valor. El operando dice **qué** se devuelve |
| `end_func suma` | Cierra la función |
| `ret` (en `saludo` y `main`) | **Implícito**: los dos son `void` y sus cuerpos están vacíos |

**Los temporales se reinician en cada método** (`temporaryCount = 0` al empezar
cada declaración): por eso si hubiera temporales en `main` también empezarían en
`t0`. No colisionan porque **viven en marcos distintos**.

### Etapa 5 — Código objeto

```asm
.text                      # no hay sección .data: no hay globales

.globl suma
.type suma, @function
suma:
    pushq %rbp             # prólogo
    movq %rsp, %rbp
    subq $16, %rsp         # 3 ranuras (a, b, t0) = 12 → alineado a 16
    movl %edi, -4(%rbp)    # param_decl a: PRIMER registro de argumento → su ranura
    movl %esi, -8(%rbp)    # param_decl b: SEGUNDO registro → su ranura
    movl -4(%rbp), %eax    # t0 = a + b : cargar a
    movl -8(%rbp), %edx    #              cargar b
    addl %edx, %eax        #              operar
    movl %eax, -12(%rbp)   #              guardar en la ranura de t0
    movl -12(%rbp), %eax   # ret t0 : el valor de retorno va en %eax
    jmp .L_suma_epilogue
.L_suma_epilogue:
    leave
    ret

.globl saludo
.type saludo, @function
saludo:
    pushq %rbp
    movq %rsp, %rbp        # ¡no hay subq!
    jmp .L_saludo_epilogue
.L_saludo_epilogue:
    leave
    ret
```

Lo que hay que poder justificar de este bloque:

- **`movl %edi, -4(%rbp)` y `movl %esi, -8(%rbp)`.** La ABI System V AMD64 manda
  los argumentos enteros en `%edi, %esi, %edx, %ecx, %r8d, %r9d`, **en ese
  orden**. El primer `param_decl` lee el primero, el segundo el segundo.
  **Por qué se copian a la pila apenas entra la función:** esos registros son
  *caller-saved*, o sea que cualquier llamada interna los puede pisar. Copiándolos
  al marco, el resto de la función trata a los parámetros **igual que a las
  locales**, y asignarles es automáticamente un efecto local (pasaje por valor).
- **`movl -12(%rbp), %eax` antes del `jmp`.** El valor de retorno va en `%eax`
  porque así lo define la ABI. Es llamativo que acabe de guardar en `-12` y lo
  relea de inmediato: es la consecuencia directa de que **ningún valor sobreviva
  en un registro entre dos cuádruplas**. Es deliberado — así cada cuádrupla se
  traduce mirando solo a sí misma — y es lo que hace al código largo pero
  verificable.
- **`saludo` no tiene `subq`.** No tiene ni parámetros, ni locales, ni
  temporales, así que `frameSize` es 0 y el prólogo omite la instrucción.
- **El epílogo común.** Los dos `return` posibles (el escrito y el implícito)
  saltan al mismo lugar: el desarmado del marco se escribe **una sola vez** y es
  imposible desincronizarlo.
- **`main` lleva `movl $0, %eax`** y `suma` no: solo `main` y solo si es `void`.

---

## 3. Constantes e identificadores

Son las **hojas** de toda expresión. Conviene tenerlas aisladas antes de las
trazas que las usan.

### El programa

```c
void main() {
    int x;
    boolean b;
    x = 7;
    b = true;
}
```

### Etapa 1 — Análisis léxico

```
[Linea 1] VOID         void
[Linea 1] ID           main
[Linea 1] '('          (
[Linea 1] ')'          )
[Linea 1] '{'          {
[Linea 2] INT          int
[Linea 2] ID           x
[Linea 2] ';'          ;
[Linea 3] BOOLEAN      boolean
[Linea 3] ID           b
[Linea 3] ';'          ;
[Linea 4] ID           x
[Linea 4] '='          =
[Linea 4] NUMBER       7
[Linea 4] ';'          ;
[Linea 5] ID           b
[Linea 5] '='          =
[Linea 5] BOOL_CONST   true
[Linea 5] ';'          ;
[Linea 6] '}'          }
```

| Texto | Regla | Qué pasa con el valor |
| --- | --- | --- |
| `7` | `{NUMBER}` = `{DIGIT}+` | `yylval.intval = atoi(yytext)` |
| `true` | `"true"` | `yylval.intval = 1`, token `BOOL_CONST` |
| `false` | `"false"` | `yylval.intval = 0`, token `BOOL_CONST` |
| `x`, `b` | `{ID}` | `yylval.strval = strdup(yytext)` |

Tres cosas:

- **`true` y `false` NO son identificadores.** Están escritos como reglas propias
  **antes** de `{ID}`, así que el lexer los reconoce como constantes booleanas. Si
  no lo estuvieran, serían variables no declaradas.
- **El lexer ya convierte el texto a número.** Las etapas de arriba nunca ven la
  cadena `"7"`: ven el entero 7. Convertir una sola vez, en el único lugar que
  tiene el texto crudo, evita repetir `atoi` en cada etapa.
- **`strdup` es obligatorio para los `ID`.** `yytext` apunta al buffer interno de
  Flex y lo pisa el token siguiente; sin la copia, el nombre se corrompería. Esa
  copia es la que termina siendo el `id` del símbolo, sin copias intermedias.

### Etapa 2 — Análisis sintáctico

```
Expression : ID           { newIdentifierNode($1); }
           | NUMBER       { newIntLiteralNode($1); }
           | BOOL_CONST   { newBoolLiteralNode($1); }
           | FLOAT_CONST  { newFloatLiteralNode($1); }
```

**El árbol:**

```
BLOQUE
└── METODO  main : VOID
    ├── PARAMETROS
    └── BLOQUE  (cuerpo)
        ├── DECL_VAR  x : INT
        ├── DECL_VAR  b : BOOL
        ├── ASIGNACION  destino = x          ← el destino va en el SÍMBOLO, no es hijo
        │   └── CONSTANTE  INT  7            ← único hijo: la expresión
        └── ASIGNACION  destino = b
            └── CONSTANTE  BOOL  true
```

**El destino de una asignación no es un hijo del nodo**: va en `node->symbol`. Por
eso el nodo tiene **un solo hijo**, la expresión, y la macro de acceso se llama
`GET_EXPRESSION(node)`. Es una decisión que conviene poder explicar: el destino no
es una expresión cualquiera —tiene que ser un nombre de variable— así que
guardarlo como símbolo en vez de como subárbol hace imposible escribir
`2 + 3 = x` por accidente, y le ahorra al semántico tener que verificar que el
hijo izquierdo sea un identificador.

Un `CONSTANT_NODE` cuelga un `Symbol` **sin `id` y sin `uid`**:

> Un `Symbol` con `id == NULL` y `uid == 0` **no es una entrada rota de la
> tabla**: es un literal. La misma estructura sirve para las dos cosas y se
> distinguen por si tiene nombre.

```
Bloque
  | Metodo: main (Retorna VOID)
  |   | Parametros
  |   | Bloque
  |   |   | Declaracion Variable: x #0 (Tipo INT)
  |   |   | Declaracion Variable: b #0 (Tipo BOOL)
  |   |   | Asignacion a: x #0 (Tipo VOID)
  |   |   |   | Constante INT: 7
  |   |   | Asignacion a: b #0 (Tipo VOID)
  |   |   |   | Constante BOOL: true
```

### Etapa 3 — Análisis semántico

**Una constante es el caso más simple de todos:**

```c
case CONSTANT_NODE:
    node->type = node->symbol->type;    // y nada más
    return node->type;
```

**El tipo de una constante se conoce desde el léxico**, porque lo determina la
forma del literal: `7` es `int`, `7.5` es `float`, `true` es `boolean`. No hay
nada que resolver ni contra qué chequear.

**Un identificador** (`analyzeIdentifier`) sí tiene trabajo, y dos errores
posibles:

| Situación | Mensaje |
| --- | --- |
| `lookupSymbol` devuelve `NULL` | `Variable no declarada: x` |
| El símbolo existe pero **es un método** | `El identificador 'f' no es una variable.` |

El segundo caso es `x = f;` con `f` método: C-TDS no tiene punteros a función.

Si resuelve bien, hace **las dos anotaciones**: reapunta el símbolo del nodo a la
declaración y copia el tipo.

**El árbol de ámbitos:**

```
ÁMBITO 0 — global                  parent = NULL
│   main#1 : método VOID
│
└── ÁMBITO 1 — cuerpo de main      parent = ámbito 0 · function = main#1
        x#2 : INT
        b#3 : BOOL
```

El `.sem` muestra las dos asignaciones ya tipadas:

```
  |   |   | Asignacion a: x #2 (Tipo INT)
  |   |   |   | Constante INT: 7
  |   |   | Asignacion a: b #3 (Tipo BOOL)
  |   |   |   | Constante BOOL: true
```

El nodo de asignación pasó de `(Tipo VOID)` a `(Tipo INT)`: el semántico le anota
el tipo del destino cuando la asignación es válida.

### Etapa 4 — Código intermedio

```
begin_func main, 0
local x@2, INT
local b@3, BOOL
x@2 = 7
b@3 = true
ret
end_func main
```

| Instrucción | Por qué |
| --- | --- |
| `x@2 = 7` | **Una constante no consume un temporal.** Se vuelve un operando *inmediato* que viaja dentro de la instrucción que la usa |
| `b@3 = true` | Un booleano se imprime como `true`/`false` para que el `.ci` se lea, pero internamente es 1 o 0 |

**Por qué las constantes no necesitan temporal:** un temporal existe para guardar
un valor **que hay que calcular**. Una constante ya está calculada, y el formato
de cuádrupla permite que un operando sea un literal. Gastar un temporal en
`t0 = 7` sería una instrucción y una ranura de pila de más.

### Etapa 5 — Código objeto

```asm
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $16, %rsp         # 2 ranuras (x, b) = 8 → alineado a 16
    movl $7, %eax          # x@2 = 7 : el 7 como INMEDIATO ($)
    movl %eax, -4(%rbp)    #           guardar en la ranura de x
    movl $1, %eax          # b@3 = true : true es el inmediato 1
    movl %eax, -8(%rbp)    #              guardar en la ranura de b
    jmp .L_main_epilogue
.L_main_epilogue:
    movl $0, %eax
    leave
    ret
```

- **El `$` marca un inmediato**: `movl $7, %eax` pone el número 7 en el registro;
  `movl 7, %eax` leería la dirección de memoria 7. Confundirlos es un clásico.
- **`true` se volvió `$1`.** Un booleano ocupa los mismos 4 bytes que un entero y
  vale exactamente 0 o 1. Ese invariante es el que después permite traducir `!`
  como un simple `xorl $1` (§5).
- **Por qué pasa por `%eax` en vez de `movl $7, -4(%rbp)` directo.** x86 permite
  mover un inmediato a memoria en una instrucción, pero el generador usa **siempre
  el mismo esquema**: cargar a registro, operar, guardar. Uniformidad sobre
  longitud, que es la prioridad declarada de esta etapa.
- **Las dos `local` no emitieron nada**: su espacio ya está en el `subq $16`.

---

## 4. Expresiones aritméticas

### El programa

```c
void main() {
    int x;
    x = 2 + 3 * 4;
}
```

### Etapa 1 — Análisis léxico

```
[Linea 3] ID           x
[Linea 3] '='          =
[Linea 3] NUMBER       2
[Linea 3] '+'          +
[Linea 3] NUMBER       3
[Linea 3] '*'          *
[Linea 3] NUMBER       4
[Linea 3] ';'          ;
```

Los operadores aritméticos salen de **una sola regla**:

```
"+"|"-"|"*"|"/"|"%"    { return yytext[0]; }
```

Devuelve el carácter, igual que los delimitadores. **El lexer no sabe nada de
precedencia**: para él, `+` y `*` son dos símbolos cualesquiera. La precedencia es
asunto exclusivo del parser.

### Etapa 2 — Análisis sintáctico: la precedencia

La gramática de expresiones es **ambigua a propósito**: todas las operaciones
binarias tienen la misma forma.

```
Expression : Expression '+' Expression   { newOperatorNode(OP_ADD, $1, $3); }
           | Expression '*' Expression   { newOperatorNode(OP_MUL, $1, $3); }
           | ...
```

La ambigüedad se resuelve con las declaraciones de precedencia:

```
%left OR                /* MENOR precedencia */
%left AND
%left EQ
%left '<' '>'
%left '+' '-'
%left '*' '/' '%'
%precedence NOT UMINUS  /* MAYOR precedencia */
```

**Se lee de abajo hacia arriba: la última línea es la que más fuerte ata.**

- `%left` = **asociatividad izquierda**: `a - b - c` es `(a-b)-c`.
- `%precedence` (sin asociatividad) para los unarios, porque no tiene sentido
  discutir la asociatividad de `- - x`.

**Por qué con precedencias y no con una gramática estratificada.** La forma
clásica (`Expr → Expr + Term`, `Term → Term * Factor`, `Factor → ...`) es
equivalente, pero necesita **un no terminal por nivel de precedencia**: con 7
niveles son 7 no terminales y 7 reducciones por cada expresión simple. Las
declaraciones expresan lo mismo en 7 líneas y dejan la gramática legible. El
costo es que la gramática **sola** ya no determina el árbol: hay que leer las
precedencias.

**El árbol:**

```
BLOQUE
└── METODO  main : VOID
    ├── PARAMETROS
    └── BLOQUE
        ├── DECL_VAR  x : INT
        └── ASIGNACION  destino = x
            └── OPERADOR  +                ← la suma quedó ARRIBA
                ├── CONSTANTE  INT  2
                └── OPERADOR  *            ← la multiplicación quedó ABAJO
                    ├── CONSTANTE  INT  3
                    └── CONSTANTE  INT  4
```

**La precedencia ya está resuelta en la forma del árbol**: lo que ata más fuerte
queda **más abajo**, y lo de más abajo se evalúa primero. De acá en adelante nadie
vuelve a mirar precedencias. Si el programa fuera `(2 + 3) * 4`, el árbol estaría
al revés — y los paréntesis **no dejan ningún nodo**: la producción
`'(' Expression ')'` devuelve el hijo tal cual (`$$ = $2`).

```
Bloque
  | Metodo: main (Retorna VOID)
  |   | Parametros
  |   | Bloque
  |   |   | Declaracion Variable: x #0 (Tipo INT)
  |   |   | Asignacion a: x #0 (Tipo VOID)
  |   |   |   | Operacion: +
  |   |   |   |   | Constante INT: 2
  |   |   |   |   | Operacion: *
  |   |   |   |   |   | Constante INT: 3
  |   |   |   |   |   | Constante INT: 4
```

### Etapa 3 — Análisis semántico

`analyzeOperator` es **postorden**: primero resuelve los tipos de los operandos,
después el del nodo. `resolveOperation` decide con esta tabla:

| Operador | Operandos válidos | Tipo del resultado |
| --- | --- | --- |
| `+ - * /` | numérico y numérico | el "mayor" de los dos |
| `%` | **`int` e `int` solamente** | `int` |
| `< >` | numérico y numérico | `boolean` |
| `==` | **los dos del mismo tipo básico** | `boolean` |
| `&& \|\|` | boolean y boolean | `boolean` |
| `-` unario | numérico | el mismo |
| `!` | boolean | `boolean` |

Acá: `3 * 4` → los dos `int` → `int`. Después `2 + (int)` → `int`. Y la
asignación verifica `isAssignable(INT, INT)` → válida.

Dos asimetrías que son decisiones documentadas y que pueden picar:

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

**El árbol de ámbitos** (idéntico al de §3, porque no hay nada nuevo que
registrar):

```
ÁMBITO 0 — global                  parent = NULL
│   main#1 : método VOID
│
└── ÁMBITO 1 — cuerpo de main      parent = ámbito 0 · function = main#1
        x#2 : INT
```

Las constantes **no entran en la tabla de símbolos**: no tienen nombre, no se
pueden referenciar, no hay nada que buscar. Su `Symbol` cuelga del nodo y se acabó.

### Etapa 4 — Código intermedio

```
begin_func main, 0
local x@2, INT
t1 = 3 * 4
t0 = 2 + t1
x@2 = t0
ret
end_func main
```

| Instrucción | Por qué |
| --- | --- |
| `t1 = 3 * 4` | Una cuádrupla **no puede contener una expresión**: la multiplicación necesita su propio lugar donde dejar el resultado |
| `t0 = 2 + t1` | La suma consume el resultado de la multiplicación. Se emite **después** porque sus operandos tienen que estar listos |
| `x@2 = t0` | La asignación copia el resultado final al destino |

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
dice si es temporal, variable, constante, etiqueta o método). Generar texto
obligaría a reparsear para optimizar; quedarse en el árbol haría que eliminar una
instrucción implique reacomodar punteros de padres. La lista plana hace que
**borrar sea desenlazar un nodo**, que es lo que el optimizador necesita todo el
tiempo.

### Etapa 5 — Código objeto

```asm
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $16, %rsp         # 3 ranuras (x, t1, t0) = 12 → alineado a 16
    movl $3, %eax          # t1 = 3 * 4 : cargar
    movl $4, %edx
    imull %edx, %eax       #              operar
    movl %eax, -8(%rbp)    #              guardar t1
    movl $2, %eax          # t0 = 2 + t1 : cargar el 2
    movl -8(%rbp), %edx    #               releer t1 de su ranura
    addl %edx, %eax        #               operar
    movl %eax, -12(%rbp)   #               guardar t0
    movl -12(%rbp), %eax   # x@2 = t0 : releer t0
    movl %eax, -4(%rbp)    #            guardar en x
    jmp .L_main_epilogue
```

- **Toda operación binaria sigue el mismo esquema de cuatro pasos**: cargar el
  primer operando a `%eax`, cargar el segundo a `%edx`, operar, guardar el
  resultado. Cambia el mnemónico (`addl`, `subl`, `imull`) y nada más.
- **`t1` está en `-8` y `t0` en `-12`, aunque `t0` tenga el número menor.** Las
  ranuras se asignan **en el orden en que aparecen las instrucciones en el TAC**,
  no por número de temporal. Como `t1 = 3 * 4` se emitió primero, se llevó la
  ranura de más arriba. Es un detalle que demuestra que entendiste cómo se arma el
  marco.
- **Se guarda `t1` y se relee en la instrucción siguiente.** Un compilador con
  asignación de registros lo dejaría en `%eax`. Acá no, **a propósito**: así el
  generador no necesita llevar la cuenta de qué hay en cada registro, y cada
  cuádrupla se traduce mirando solo a sí misma.
- **Si el programa tuviera una división**, el esquema cambia porque `idivl` lo
  exige:

```asm
movl  <dividendo>, %eax
movl  <divisor>,   %ecx
cltd                      # extiende el signo de %eax a %edx:%eax
idivl %ecx                # cociente → %eax ; resto → %edx
```

  **`cltd` es obligatorio**: la división con signo toma un dividendo de 64 bits
  repartido en `%edx:%eax`, así que hay que propagar el bit de signo. Sin él,
  `%edx` tiene basura y dividir un negativo da cualquier cosa. Y **el divisor va
  en `%ecx`, nunca en `%edx`**, porque `cltd` sobrescribe `%edx` justo antes de
  usarlo. `/` y `%` son **la misma secuencia**, leyendo distinto registro al final.

---

## 5. Operadores unarios `-` y `!`

### El programa

```c
void main() {
    int x;
    boolean b;
    x = 5;
    x = -x;
    b = !(x > 1);
}
```

### Etapa 1 — Análisis léxico

```
[Linea 5] ID           x
[Linea 5] '='          =
[Linea 5] '-'          -
[Linea 5] ID           x
[Linea 5] ';'          ;
[Linea 6] ID           b
[Linea 6] '='          =
[Linea 6] NOT          !
[Linea 6] '('          (
[Linea 6] ID           x
[Linea 6] '>'          >
[Linea 6] NUMBER       1
[Linea 6] ')'          )
[Linea 6] ';'          ;
```

**El `-` unario y el `-` binario son EL MISMO TOKEN.** El lexer no tiene forma de
distinguirlos: ve un `-` y devuelve `'-'`. Quién decide si es resta o negación es
el parser, **por la posición**. En cambio `!` tiene su propio token con nombre
(`NOT`) porque solo existe en una forma.

### Etapa 2 — Análisis sintáctico

```
Expression : '-' Expression %prec UMINUS   { newOperatorNode(OP_UMINUS, $2, NULL); }
           | NOT Expression                { newOperatorNode(OP_NOT,    $2, NULL); }
```

**`%prec UMINUS` es la respuesta a una pregunta casi segura.** El token `'-'`
aparece en dos producciones: la resta binaria y el menos unario. Como token tiene
**una sola precedencia**, la de `+ -`, que es baja. Pero el menos unario tiene que
atar más fuerte que la multiplicación: en `-x * y` lo correcto es `(-x) * y`, no
`-(x * y)`.

`UMINUS` es un **pseudo-token**: no lo produce el lexer, existe solo para tener un
nombre al que darle precedencia alta en la tabla. `%prec UMINUS` le dice a Bison
*"para esta producción, usá la precedencia de `UMINUS` en vez de la del `'-'`"*.

**Los unarios usan el mismo tipo de nodo que los binarios** (`OPERATOR_NODE`),
con un solo hijo. Por eso el nodo **no se llama "binario"**: se distingue por
`childCount`, o preguntando si el hijo derecho es `NULL`.

**El árbol:**

```
BLOQUE
└── METODO  main : VOID
    ├── PARAMETROS
    └── BLOQUE
        ├── DECL_VAR  x : INT
        ├── DECL_VAR  b : BOOL
        ├── ASIGNACION  destino = x
        │   └── CONSTANTE  INT  5
        ├── ASIGNACION  destino = x
        │   └── OPERADOR  -(unario)          ← UN SOLO HIJO
        │       └── ID  x
        └── ASIGNACION  destino = b
            └── OPERADOR  !                  ← UN SOLO HIJO
                └── OPERADOR  >              ← éste sí tiene dos
                    ├── ID  x
                    └── CONSTANTE  INT  1
```

Fijate que **el paréntesis de `!(x > 1)` no dejó ningún nodo**: solo agrupó. El
`!` quedó como padre de la comparación, que es lo que los paréntesis pedían.

```
Bloque
  | Metodo: main (Retorna VOID)
  |   | Parametros
  |   | Bloque
  |   |   | Declaracion Variable: x #0 (Tipo INT)
  |   |   | Declaracion Variable: b #0 (Tipo BOOL)
  |   |   | Asignacion a: x #0 (Tipo VOID)
  |   |   |   | Constante INT: 5
  |   |   | Asignacion a: x #0 (Tipo VOID)
  |   |   |   | Operacion: - (unario)
  |   |   |   |   | ID: x #0 (Tipo VOID)
  |   |   | Asignacion a: b #0 (Tipo VOID)
  |   |   |   | Operacion: !
  |   |   |   |   | Operacion: >
  |   |   |   |   |   | ID: x #0 (Tipo VOID)
  |   |   |   |   |   | Constante INT: 1
```

El propio compilador escribe **`- (unario)`** para distinguirlo de la resta.

### Etapa 3 — Análisis semántico

`analyzeOperator` trata los unarios con una línea especial:

```c
DataType rightType = (rightNode != NULL) ? analyzeExpression(rightNode, scope) : TYPE_VOID;
```

O sea: si no hay hijo derecho, el "tipo derecho" es `TYPE_VOID`, y
`resolveOperation` lo ignora para los casos unarios.

| Operador | Exige | Devuelve |
| --- | --- | --- |
| `-` unario | numérico (`int` o `float`) | **el mismo tipo del operando** |
| `!` | `boolean` | `boolean` |

Acá: `-x` con `x` entero → `int`, y la asignación a `x` es válida. `x > 1` →
`boolean`, y `!boolean` → `boolean`, asignable a `b`.

**`!x` con `x` entero es un error de tipos.** C-TDS **no tiene semántica
*truthy***: un entero no es una condición. Es una diferencia con C que conviene
poder nombrar.

**El árbol de ámbitos:**

```
ÁMBITO 0 — global                  parent = NULL
│   main#1 : método VOID
│
└── ÁMBITO 1 — cuerpo de main      parent = ámbito 0 · function = main#1
        x#2 : INT
        b#3 : BOOL
```

El `.sem`, con los tipos ya propagados de abajo hacia arriba:

```
  |   |   | Asignacion a: x #2 (Tipo INT)
  |   |   |   | Operacion: - (unario)
  |   |   |   |   | ID: x #2 (Tipo INT)
  |   |   | Asignacion a: b #3 (Tipo BOOL)
  |   |   |   | Operacion: !
  |   |   |   |   | Operacion: >
  |   |   |   |   |   | ID: x #2 (Tipo INT)
  |   |   |   |   |   | Constante INT: 1
```

### Etapa 4 — Código intermedio

```
begin_func main, 0
local x@2, INT
local b@3, BOOL
x@2 = 5
t0 = -x@2
x@2 = t0
t1 = x@2 > 1
t2 = !t1
b@3 = t2
ret
end_func main
```

| Instrucción | Por qué |
| --- | --- |
| `t0 = -x@2` | El unario **también necesita un temporal**: su resultado es un valor nuevo. La cuádrupla usa solo el primer operando y deja el segundo vacío |
| `x@2 = t0` | La asignación es una instrucción **aparte**. El TAC no tiene "negar en el lugar" |
| `t1 = x@2 > 1` | La comparación produce un booleano en un temporal |
| `t2 = !t1` | La negación lógica consume ese temporal y produce otro |
| `b@3 = t2` | Y recién ahí se copia al destino |

**Tres instrucciones para `b = !(x > 1)`** es la consecuencia directa de que cada
cuádrupla haga **una sola** operación. Es lo que permite que el generador de
objeto sea un `switch` de un caso por opcode.

### Etapa 5 — Código objeto

```asm
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $32, %rsp         # 5 ranuras (x, b, t0, t1, t2) = 20 → alineado a 32
    movl $5, %eax          # x@2 = 5
    movl %eax, -4(%rbp)
    movl -4(%rbp), %eax    # t0 = -x : cargar x
    negl %eax              #           negar EN EL REGISTRO (complemento a dos)
    movl %eax, -12(%rbp)   #           guardar t0
    movl -12(%rbp), %eax   # x@2 = t0
    movl %eax, -4(%rbp)
    movl -4(%rbp), %eax    # t1 = x > 1 : cargar x
    movl $1, %edx          #              cargar el 1
    cmpl %edx, %eax        #              comparar (setea flags)
    setg %al               #              %al = 1 o 0  (SOLO 8 BITS)
    movzbl %al, %eax       #              extender a 32 bits con ceros
    movl %eax, -16(%rbp)   #              guardar t1
    movl -16(%rbp), %eax   # t2 = !t1 : cargar t1
    xorl $1, %eax          #            invertir el bit 0
    movl %eax, -20(%rbp)   #            guardar t2
    movl -20(%rbp), %eax   # b@3 = t2
    movl %eax, -8(%rbp)
    jmp .L_main_epilogue
```

Lo que hay que poder justificar:

- **`negl` es una sola instrucción** porque x86 tiene negación aritmética nativa:
  hace complemento a dos sobre el registro.
- **`setg %al` solo escribe 8 bits.** Las instrucciones `setCC` escriben un byte,
  no una palabra: ponen 1 o 0 en `%al`, que es el byte bajo de `%eax`. Los otros
  24 bits quedan con lo que hubiera antes.
- **Por eso `movzbl %al, %eax` es obligatorio**: extiende con ceros a 32 bits. Sin
  él, el booleano arrastraría basura en los bits altos, y después `xorl $1` daría
  un número que no es ni 0 ni 1, rompiendo cualquier `if` que lo use.
- **`cmpl %edx, %eax` compara al revés de como se lee.** En sintaxis AT&T,
  `cmpl %edx, %eax` calcula `%eax - %edx`, así que `setg` significa *el primer
  operando es mayor que el segundo*. Es fácil invertirlo por accidente.
- **`!` se tradujo a `xorl $1`, no a una comparación.** Eso **solo es correcto si
  un booleano vale exactamente 0 o 1**, y ese invariante lo garantizan el
  `movzbl` de arriba y las constantes `true`/`false` del TAC. A cambio es **una**
  instrucción en vez de tres (`cmpl $0` + `sete` + `movzbl`). Si alguna vez un
  booleano pudiera valer 2, esto daría 3 y rompería todo: es una dependencia que
  conviene poder nombrar.
- **El marco es de 32 y no de 16** porque hay 5 ranuras: la variable, el booleano
  y **tres temporales**. Cada operación intermedia se lleva su ranura.

---

## 6. `&&` y `||`: el corto circuito

Es **la construcción con más probabilidad de caer**: la letra la exige
explícitamente y es donde la traducción deja de ser uno a uno.

### El programa

```c
boolean f() {
    return true;
}
void main() {
    boolean b;
    b = (1 > 0) && f();
}
```

### Etapa 1 — Análisis léxico

```
[Linea 6] ID           b
[Linea 6] '='          =
[Linea 6] '('          (
[Linea 6] NUMBER       1
[Linea 6] '>'          >
[Linea 6] NUMBER       0
[Linea 6] ')'          )
[Linea 6] AND          &&
[Linea 6] ID           f
[Linea 6] '('          (
[Linea 6] ')'          )
[Linea 6] ';'          ;
```

`&&` y `||` tienen **tokens con nombre** (`AND`, `OR`) porque son de dos
caracteres: no se pueden devolver como `yytext[0]`. La regla está escrita antes
que cualquier otra que empiece con `&`, aunque acá no hay ambigüedad porque el
lenguaje no tiene `&` suelto.

### Etapa 2 — Análisis sintáctico

```
Expression : Expression AND Expression   { newOperatorNode(OP_AND, $1, $3); }
           | Expression OR  Expression   { newOperatorNode(OP_OR,  $1, $3); }
```

A nivel de árbol, `&&` es **un operador binario como cualquier otro**. La
diferencia aparece recién al generar código.

Por precedencia, `||` ata **más flojo** que `&&`, así que `a || b && c` es
`a || (b && c)`, igual que en C.

**El árbol:**

```
BLOQUE
├── METODO  f : BOOL
│   ├── PARAMETROS
│   └── BLOQUE
│       └── RETURN
│           └── CONSTANTE  BOOL  true
└── METODO  main : VOID
    ├── PARAMETROS
    └── BLOQUE
        ├── DECL_VAR  b : BOOL
        └── ASIGNACION  destino = b
            └── OPERADOR  &&
                ├── OPERADOR  >              ← operando izquierdo
                │   ├── CONSTANTE  INT  1
                │   └── CONSTANTE  INT  0
                └── LLAMADA  f               ← operando derecho (sin argumentos)
```

```
Bloque
  | Metodo: f (Retorna BOOL)
  |   | Parametros
  |   | Bloque
  |   |   | Return
  |   |   |   | Constante BOOL: true
  | Metodo: main (Retorna VOID)
  |   | Parametros
  |   | Bloque
  |   |   | Declaracion Variable: b #0 (Tipo BOOL)
  |   |   | Asignacion a: b #0 (Tipo VOID)
  |   |   |   | Operacion: &&
  |   |   |   |   | Operacion: >
  |   |   |   |   |   | Constante INT: 1
  |   |   |   |   |   | Constante INT: 0
  |   |   |   |   | Llamada a Metodo: f
```

### Etapa 3 — Análisis semántico

`&&` y `||` exigen **los dos operandos `boolean`** y devuelven `boolean`. Acá:
`1 > 0` → `boolean`, `f()` → `boolean` (su tipo de retorno). Válido.

**El semántico no sabe nada del corto circuito**: para él, `&&` es un operador
más. La evaluación perezosa es una propiedad de **cómo se ejecuta**, no de los
tipos.

Sí hay un chequeo relevante acá: `f()` se usa **como expresión**, así que
`analyzeMethodCall` verifica que `f` **no sea `void`**. Si lo fuera:
*"El metodo 'f' es void y no puede usarse como expresion"*.

**El árbol de ámbitos:**

```
ÁMBITO 0 — global                  parent = NULL
│   f#1    : método BOOL, 0 parámetros
│   main#2 : método VOID, 0 parámetros
│
├── ÁMBITO 1 — cuerpo de f         parent = ámbito 0 · function = f#1
│       (vacío)
│
└── ÁMBITO 2 — cuerpo de main      parent = ámbito 0 · function = main#2
        b#3 : BOOL
```

### Etapa 4 — Código intermedio

Acá está el nudo del asunto.

**El problema:** la letra exige que si `1 > 0` fuera falso, **`f()` no se llame**.
Una cuádrupla `t = a && b` **no puede cumplir eso**, porque sus dos operandos
tienen que estar calculados antes de ejecutarla.

**Por eso no existen los opcodes `TAC_AND` ni `TAC_OR`.** No es un olvido: es la
decisión de diseño que garantiza la semántica del lenguaje. Se traducen a
**control de flujo**.

```
begin_func f, 0
ret true
end_func f
begin_func main, 0
local b@3, BOOL
t0 = 1 > 0           ← 1. evaluar el operando IZQUIERDO
t1 = t0              ← 2. el resultado PROVISORIO es el izquierdo
if t1 false goto L0  ← 3. si ya alcanza para decidir, SALTAR sin evaluar la derecha
t2 = call f, 0       ← 4. evaluar el derecho (solo si hizo falta)
t1 = t2              ← 5. el resultado pasa a ser el derecho
L0:                  ← 6. acá convergen los dos caminos
b@3 = t1             ← 7. y recién ahí se copia al destino
ret
end_func main
```

La idea en una frase: **se asume que el resultado es el operando izquierdo, y
solo si éste no alcanza para decidir se evalúa el derecho y se pisa el
resultado.**

| Instrucción | Por qué |
| --- | --- |
| `t1 = t0` | Parece redundante, pero **`t1` es el resultado de la expresión entera** y tiene que tener un valor válido por los dos caminos. El camino "corto" sale por el `goto` sin pasar por el paso 5 |
| `if t1 false goto L0` | Es **todo** el corto circuito: si el izquierdo ya decide, se saltea la evaluación del derecho |
| `L0:` | El punto donde se juntan los dos caminos |

**`||` es idéntico cambiando una sola cosa:** el salto es `if ... true goto`. En
el código es literalmente un ternario:

```c
TACOperation jumpOp = (node->operationType == OP_AND) ? TAC_IF_FALSE : TAC_IF_TRUE;
```

| Operador | Salta si… | Porque… |
| --- | --- | --- |
| `&&` | el izquierdo es **falso** | falso `&&` lo que sea = falso |
| `\|\|` | el izquierdo es **verdadero** | verdadero `\|\|` lo que sea = verdadero |

**Las etiquetas se resuelven en una sola pasada, sin backpatching.** En un
compilador de una pasada habría que emitir el salto antes de conocer su destino y
volver después a completarlo con una lista de pendientes. Acá no hace falta:
**el AST ya está completo**, así que la etiqueta se pide primero (`newLabel()`) y
se emite donde corresponda.

**Teoría:** esto se llama ***jumping code*** o código de salto, y es la
implementación canónica del corto circuito. La alternativa —calcular los dos
operandos y combinarlos— se llama *código de valor numérico*, y es justamente la
que el lenguaje prohíbe.

### Etapa 5 — Código objeto

```asm
.globl f
.type f, @function
f:
    pushq %rbp
    movq %rsp, %rbp        # sin subq: f no tiene locales ni temporales
    movl $1, %eax          # ret true : el booleano true es el inmediato 1
    jmp .L_f_epilogue
.L_f_epilogue:
    leave
    ret

.globl main
.type main, @function
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $16, %rsp         # 4 ranuras (b, t0, t1, t2) = 16
    movl $1, %eax          # t0 = 1 > 0 : cargar el 1
    movl $0, %edx          #              cargar el 0
    cmpl %edx, %eax        #              comparar
    setg %al               #              1 o 0 en 8 bits
    movzbl %al, %eax       #              extender a 32
    movl %eax, -8(%rbp)    #              guardar t0
    movl -8(%rbp), %eax    # t1 = t0
    movl %eax, -12(%rbp)
    movl -12(%rbp), %eax   # if t1 false goto L0 : cargar t1
    cmpl $0, %eax          #                       ¿es cero (falso)?
    je .L0                 #                       si sí, SALTAR LA LLAMADA
    call f                 # t2 = call f, 0 : llamar
    movl %eax, -16(%rbp)   #                  el resultado vuelve en %eax
    movl -16(%rbp), %eax   # t1 = t2 : pisar el resultado provisorio
    movl %eax, -12(%rbp)
.L0:                       # los dos caminos se juntan acá
    movl -12(%rbp), %eax   # b@3 = t1
    movl %eax, -4(%rbp)
    jmp .L_main_epilogue
```

- **`je .L0` es el corto circuito ejecutándose.** Si la comparación dio falso, el
  `call f` **no se ejecuta nunca**. Eso es exactamente lo que la letra exige, y se
  ve en el assembly.
- **`call f` no tiene instrucciones de argumentos** porque `f` no recibe ninguno.
  Si recibiera, antes del `call` habría `movl …, %edi` y compañía (§8).
- **El resultado de la llamada se lee de `%eax`** inmediatamente después del
  `call`: es el registro de retorno que define la ABI.
- **`f` no tiene `subq`**: su cuerpo es un solo `return` de una constante, así que
  no necesita ni una ranura.
- **La etiqueta se emite como `.L0`, con punto adelante.** Eso la marca como
  **local al archivo** y evita que entre en la tabla de símbolos del objeto: si
  dos funciones tuvieran una `L0`, no colisionan.

---

## 7. Asignación

### El programa

```c
int g;
void main() {
    int x;
    x = 1;
    g = x;
}
```

Las dos asignaciones son distintas a propósito: una a **local**, otra a **global**.

### Etapa 1 — Análisis léxico

```
[Linea 1] INT          int
[Linea 1] ID           g
[Linea 1] ';'          ;
[Linea 2] VOID         void
[Linea 2] ID           main
[Linea 2] '('          (
[Linea 2] ')'          )
[Linea 2] '{'          {
[Linea 3] INT          int
[Linea 3] ID           x
[Linea 3] ';'          ;
[Linea 4] ID           x
[Linea 4] '='          =
[Linea 4] NUMBER       1
[Linea 4] ';'          ;
[Linea 5] ID           g
[Linea 5] '='          =
[Linea 5] ID           x
[Linea 5] ';'          ;
[Linea 6] '}'          }
```

**El `=` tiene su propia regla**, separada de los operadores aritméticos, y
devuelve `'='`. **El lexer no distingue** el `x` de la declaración del `x` de la
asignación: los dos son `ID`. Que uno declare y el otro use lo decide la
gramática por la posición.

### Etapa 2 — Análisis sintáctico

```
Assignment : ID '=' Expression   { newAssignmentNode($1, $3); }
Statement  : Assignment ';'      { $$ = $1; }
```

**La asignación es su propio no terminal**, separado del punto y coma. Es
deliberado: una asignación **sin** `;` es exactamente la pieza que necesitaría la
cabecera de un `for` (`for (i = 0; ...; i = i + 1)`). Con la producción embebida
en `Statement` no se podría escribir.

**El árbol:**

```
BLOQUE
├── DECL_VAR  g : INT                   ← global
└── METODO  main : VOID
    ├── PARAMETROS
    └── BLOQUE
        ├── DECL_VAR  x : INT           ← local
        ├── ASIGNACION  destino = x     ← el destino NO es un hijo
        │   └── CONSTANTE  INT  1
        └── ASIGNACION  destino = g
            └── ID  x
```

**El destino va en `node->symbol`, no como hijo**, así que el nodo tiene **un
solo hijo**: la expresión. Dos razones:

1. El destino no es una expresión cualquiera: **tiene que ser un nombre**.
   Guardarlo como símbolo hace imposible escribir `2 + 3 = x` por construcción.
2. El semántico no necesita verificar que el hijo izquierdo sea un identificador:
   ya lo garantizó la gramática.

```
Bloque
  | Declaracion Variable: g #0 (Tipo INT)
  | Metodo: main (Retorna VOID)
  |   | Parametros
  |   | Bloque
  |   |   | Declaracion Variable: x #0 (Tipo INT)
  |   |   | Asignacion a: x #0 (Tipo VOID)
  |   |   |   | Constante INT: 1
  |   |   | Asignacion a: g #0 (Tipo VOID)
  |   |   |   | ID: x #0 (Tipo VOID)
```

### Etapa 3 — Análisis semántico

`analyzeAssignmentNode`, en orden:

1. `lookupSymbol` del destino, que **sube por los ámbitos**. Si no está:
   *"Asignacion a variable no declarada"*.
2. ¿Es una **variable** y no un método? Si no: *"Ese nombre de simbolo
   corresponde al metodo 'f'"*.
3. **Reapunta** el nodo al símbolo declarado (`resolveNodeSymbol`) y libera el
   provisorio.
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

**El árbol de ámbitos:**

```
ÁMBITO 0 — global                  parent = NULL · level 0
│   g#1    : INT   (isGlobal = TRUE)
│   main#2 : método VOID
│
└── ÁMBITO 1 — cuerpo de main      parent = ámbito 0 · function = main#2 · level 1
        x#3 : INT   (isGlobal = false)
```

**La asignación `g = x` es la que muestra para qué sirve que `lookupSymbol`
suba.** `g` no está en el ámbito 1; la búsqueda falla ahí, sube al padre y la
encuentra en el 0. Eso es alcance léxico implementado en cinco líneas de
recursión.

```
  | Declaracion Variable: g #1 (Tipo INT)
  |   |   | Declaracion Variable: x #3 (Tipo INT)
  |   |   | Asignacion a: x #3 (Tipo INT)
  |   |   |   | Constante INT: 1
  |   |   | Asignacion a: g #1 (Tipo INT)     ← resolvió a la GLOBAL
  |   |   |   | ID: x #3 (Tipo INT)           ← resolvió a la LOCAL
```

### Etapa 4 — Código intermedio

```
global g@1, INT
begin_func main, 0
local x@3, INT
x@3 = 1
g@1 = x@3
ret
end_func main
```

| Instrucción | Por qué |
| --- | --- |
| `x@3 = 1` | Asignación directa de un inmediato. Sin temporal: el `1` ya es un valor |
| `g@1 = x@3` | **Copia de variable a variable.** Tampoco necesita temporal: la cuádrupla `assign` admite una variable como operando |

**La asignación es `TAC_ASSIGN`, el opcode más simple**: `result = firstOperand`,
con el segundo operando vacío. En el TAC **no se distingue** si el destino es
global o local: los dos son `OPERAND_VARIABLE` y la diferencia está en el símbolo
(`isGlobal`). Quien la usa es el generador de objeto.

### Etapa 5 — Código objeto

```asm
.data
.globl g
.align 4
g:
    .long 0                # la global vive en el segmento de datos

.text

.globl main
.type main, @function
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $16, %rsp         # 1 ranura (x) = 4 → alineado a 16
    movl $1, %eax          # x@3 = 1 : el inmediato al registro
    movl %eax, -4(%rbp)    #           guardar en la RANURA DEL MARCO
    movl -4(%rbp), %eax    # g@1 = x@3 : leer x de su ranura
    movl %eax, g(%rip)     #             guardar en la GLOBAL, por su nombre
    jmp .L_main_epilogue
.L_main_epilogue:
    movl $0, %eax
    leave
    ret
```

**La diferencia entre las dos asignaciones está en la última instrucción de cada
una, y es la pregunta que puede caer:**

| Destino | Cómo se escribe | Por qué |
| --- | --- | --- |
| Local `x@3` | `-4(%rbp)` | Vive en el **marco de pila**, que cambia de dirección en cada llamada. Se accede con un **desplazamiento relativo a `%rbp`** |
| Global `g@1` | `g(%rip)` | Vive en `.data`, en una dirección **fija del ejecutable**. Se accede por **nombre** |

**`g(%rip)` es direccionamiento relativo al contador de programa.** No usa la
dirección absoluta de `g`, sino "tantos bytes más adelante de donde estoy
ejecutando". Eso permite cargar el ejecutable en cualquier dirección de memoria
(*código independiente de la posición*, PIC), que es el modo por defecto de los
enlazadores actuales.

**Y el símbolo es `g`, no `g@1`.** El sufijo del TAC se descarta al emitir:
`getGlobalName` devuelve el `id` original. El `@uid` existía para desambiguar
nombres repetidos, y **una global no puede estar repetida** porque el ámbito
global es uno solo. Las locales sí conservan el sufijo en el TAC, pero nunca se
vuelven símbolos: viven en offsets, que no tienen nombre en el objeto.

---

## 8. Invocación a método

### El programa

```c
int doble(int n) {
    return n * 2;
}
void nada() {
}
void main() {
    int r;
    r = doble(4);
    nada();
}
```

Dos llamadas distintas a propósito: una **como expresión** (con argumento y con
resultado) y otra **como sentencia** (sin argumentos y sin resultado).

### Etapa 1 — Análisis léxico

```
[Linea 8] ID           r
[Linea 8] '='          =
[Linea 8] ID           doble
[Linea 8] '('          (
[Linea 8] NUMBER       4
[Linea 8] ')'          )
[Linea 8] ';'          ;
[Linea 9] ID           nada
[Linea 9] '('          (
[Linea 9] ')'          )
[Linea 9] ';'          ;
```

**`doble` es un `ID` igual que `r`.** El lexer no sabe que es un método; lo que lo
convierte en una llamada es el paréntesis que viene después, y eso lo ve la
gramática.

### Etapa 2 — Análisis sintáctico

```
MethodCall   : ID '(' ')'                { newMethodCallNode($1, NULL); }
             | ID '(' ArgumentList ')'   { newMethodCallNode($1, $3); }

ArgumentList : Expression                { appendNode(NULL, $1); }
             | ArgumentList ',' Expression { appendNode($1, $3); }

Statement    : MethodCall ';'            ← como SENTENCIA
Expression   : MethodCall                ← como EXPRESIÓN
```

**El mismo no terminal aparece en dos lugares.** Es la forma de permitir que un
método con resultado se invoque como sentencia (descartando el valor), como pide
la letra. **El nodo es el mismo**; lo que cambia es quién lo consume.

**Los argumentos son los hijos del nodo**, en orden:

```
BLOQUE
├── METODO  doble : INT
│   ├── PARAMETROS
│   │   └── DECL_VAR  n : INT
│   └── BLOQUE
│       └── RETURN
│           └── OPERADOR  *
│               ├── ID  n
│               └── CONSTANTE  INT  2
├── METODO  nada : VOID
│   ├── PARAMETROS            ← sin hijos
│   └── BLOQUE                ← sin hijos
└── METODO  main : VOID
    ├── PARAMETROS
    └── BLOQUE
        ├── DECL_VAR  r : INT
        ├── ASIGNACION  destino = r
        │   └── LLAMADA  doble         ← como EXPRESIÓN: cuelga de la asignación
        │       └── CONSTANTE  INT  4  ← hijo [0] = primer argumento
        └── LLAMADA  nada              ← como SENTENCIA: cuelga del bloque, sin hijos
```

La diferencia entre las dos llamadas **se ve en el árbol**: una cuelga de una
asignación, la otra directamente del bloque.

```
  |   | Bloque
  |   |   | Declaracion Variable: r #0 (Tipo INT)
  |   |   | Asignacion a: r #0 (Tipo VOID)
  |   |   |   | Llamada a Metodo: doble
  |   |   |   |   | Constante INT: 4
  |   |   | Llamada a Metodo: nada
```

### Etapa 3 — Análisis semántico

`analyzeMethodCall` recibe un `bool isExpression` justamente para distinguir los
dos casos, y valida **cinco cosas en orden**:

| # | Qué verifica | Error |
| --- | --- | --- |
| 1 | Que el método exista | `Método no declarado: f` |
| 2 | Que el nombre sea de un método y no de una variable | `El simbolo encontrado pertenece a una variable…` |
| 3 | **Si se usa como expresión, que no sea `void`** | `El metodo 'g' es void y no puede usarse como expresion` |
| 4 | Que la **cantidad** de argumentos coincida | `Se esperaban 2 pero se pasaron 1` |
| 5 | Que **cada argumento** sea asignable a su parámetro | `Parámetro incompatible en la posición 1` |

**El chequeo 3 es asimétrico a propósito:** usar un método `void` como expresión
es error, pero usar uno con resultado como sentencia **es legal** — el valor
simplemente se descarta, como pide la letra.

Los tipos esperados salen de `method->parameters->children[i]->type`, o sea **del
nodo de la declaración del método**. Funciona porque el símbolo del método guarda
un puntero a su nodo `PARAMETROS`.

**El árbol de ámbitos:**

```
ÁMBITO 0 — global                     parent = NULL
│   doble#1 : método INT, 1 parámetro
│   nada#3  : método VOID, 0 parámetros
│   main#4  : método VOID, 0 parámetros
│
├── ÁMBITO 1 — cuerpo de doble        parent = ámbito 0 · function = doble#1
│       n#2 : INT
│
├── ÁMBITO 2 — cuerpo de nada         parent = ámbito 0 · function = nada#3
│       (vacío)
│
└── ÁMBITO 3 — cuerpo de main         parent = ámbito 0 · function = main#4
        r#5 : INT
```

Las llamadas de `main` resuelven `doble` y `nada` **subiendo al ámbito 0**: desde
el ámbito 3 no se ven, pero el padre sí los tiene. **Y los dos ya estaban
registrados** cuando se analiza `main`, porque el recorrido es de arriba hacia
abajo: si `main` estuviera escrito primero, los dos serían *"Método no
declarado"*.

### Etapa 4 — Código intermedio

```
begin_func doble, 1
param_decl n@2, INT
t0 = n@2 * 2
ret t0
end_func doble
begin_func nada, 0
ret
end_func nada
begin_func main, 0
local r@5, INT
param 4              ← 1. apilar el argumento
t0 = call doble, 1   ← 2. llamar, con aridad 1, y guardar el resultado
r@5 = t0             ← 3. copiarlo al destino
call nada, 0         ← llamada SIN resultado
ret
end_func main
```

| Instrucción | Por qué |
| --- | --- |
| `param 4` | **Un `param` por argumento**, emitidos en orden de índice creciente, o sea **de izquierda a derecha** como exige la letra |
| `t0 = call doble, 1` | El `1` es la **aridad**: le dice al generador de objeto cuántos `param` pendientes consumir. Tiene destino porque la llamada se usa como expresión |
| `call nada, 0` | **Sin destino**: es una sentencia, el resultado no existe |

**Por qué `param` separado y no los argumentos dentro del `call`.** Una cuádrupla
tiene lugar para dos operandos: una llamada de cinco argumentos no entra. La
secuencia `param…param + call n` es la representación estándar y además refleja
el orden real de evaluación.

**Por qué el `call` lleva la aridad** si los `param` ya están ahí: porque con
llamadas anidadas —`f(g(1), h(2))`— los `param` de las dos llamadas se intercalan,
y la aridad es lo que dice **cuántos de los pendientes** le tocan a cada `call`.

### Etapa 5 — Código objeto

```asm
.globl doble
.type doble, @function
doble:
    pushq %rbp
    movq %rsp, %rbp
    subq $16, %rsp         # 2 ranuras (n, t0) = 8 → 16
    movl %edi, -4(%rbp)    # param_decl n : PRIMER registro de argumento → ranura
    movl -4(%rbp), %eax    # t0 = n * 2 : cargar n
    movl $2, %edx          #              cargar el 2
    imull %edx, %eax       #              multiplicar
    movl %eax, -8(%rbp)    #              guardar t0
    movl -8(%rbp), %eax    # ret t0 : el valor de retorno va en %eax
    jmp .L_doble_epilogue
.L_doble_epilogue:
    leave
    ret

.globl nada
.type nada, @function
nada:
    pushq %rbp
    movq %rsp, %rbp        # sin subq: nada no tiene nada
    jmp .L_nada_epilogue
.L_nada_epilogue:
    leave
    ret

.globl main
.type main, @function
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $16, %rsp         # 3 ranuras (r, t0, + 1 de argumento pendiente) = 12 → 16
    movl $4, %eax          # param 4 : cargar el argumento
    movl %eax, -12(%rbp)   #           y GUARDARLO EN UNA RANURA, no en el registro
    movl -12(%rbp), %edi   # call : recién ACÁ se carga el registro de argumento
    call doble             #        llamar
    movl %eax, -8(%rbp)    #        el resultado vuelve en %eax → ranura de t0
    movl -8(%rbp), %eax    # r@5 = t0
    movl %eax, -4(%rbp)
    call nada              # call nada, 0 : sin argumentos y sin resultado
    jmp .L_main_epilogue
```

Lo que hay que poder explicar de este bloque:

**La convención de llamada (ABI System V AMD64):**

| Posición del argumento | Registro |
| :---: | :---: |
| 1 | `%edi` |
| 2 | `%esi` |
| 3 | `%edx` |
| 4 | `%ecx` |
| 5 | `%r8d` |
| 6 | `%r9d` |
| 7 en adelante | por la **pila**, en orden **inverso** |

Y el resultado vuelve siempre en **`%eax`**.

**Por qué el `param` guarda en una ranura en vez de cargar el registro directo.**
Es la decisión más interesante de esta etapa. Los argumentos se evalúan de a uno,
y evaluar el segundo puede requerir **una llamada interna** —`f(g(1), h(2))`— que
pisaría el registro ya cargado. Guardándolos primero en ranuras del marco y
cargando los registros **justo antes del `call`**, las llamadas anidadas
funcionan sin ningún caso especial.

Esas ranuras son las **"de argumentos pendientes"**, y se cuentan en la pasada de
dimensionamiento tomando **el pico**: se simula el apilado (`param` suma, `call n`
resta `n`) y se reserva el máximo alcanzado. Acá el pico es 1, por eso hay una
sola ranura extra.

**Si hubiera más de 6 argumentos**, los extras irían por la pila con `pushq`, **en
orden inverso** (porque la pila crece hacia abajo y el séptimo tiene que quedar en
la dirección más baja), y si la cantidad apilada fuera **impar** habría un
`subq $8, %rsp` extra antes del `call`: la ABI exige `%rsp` alineado a 16 bytes en
el momento de llamar.

**`call nada` no tiene ni una instrucción alrededor**: sin argumentos no hay
registros que cargar, y sin resultado no hay nada que leer de `%eax`.

**`call` hace dos cosas**: apila la dirección de retorno y salta. Por eso el
prólogo del llamado encuentra en `+8(%rbp)` la dirección de retorno y en
`+16(%rbp)` el primer argumento que vino por pila.

---

## 9. `return`

### El programa

```c
int signo(int n) {
    if (n > 0) {
        return 1;
    } else {
        return 0;
    }
}
void main() {
    int s;
    s = signo(5);
}
```

El `if`/`else` está a propósito: es el caso donde **los dos caminos** tienen que
retornar.

### Etapa 1 — Análisis léxico

```
[Linea 2] IF           if
[Linea 2] '('          (
[Linea 2] ID           n
[Linea 2] '>'          >
[Linea 2] NUMBER       0
[Linea 2] ')'          )
[Linea 2] '{'          {
[Linea 3] RETURN       return
[Linea 3] NUMBER       1
[Linea 3] ';'          ;
[Linea 4] '}'          }
[Linea 4] ELSE         else
[Linea 4] '{'          {
[Linea 5] RETURN       return
[Linea 5] NUMBER       0
[Linea 5] ';'          ;
[Linea 6] '}'          }
```

`return`, `if` y `else` son palabras reservadas con su token propio.

### Etapa 2 — Análisis sintáctico

```
Statement : RETURN Expression ';'   { newReturnNode($2); }
          | RETURN ';'              { newReturnNode(NULL); }
```

**Dos producciones, y el nodo tiene cero o un hijo.** Ésa es toda la diferencia
entre `return expr;` y `return;` a nivel de árbol.

**El árbol:**

```
BLOQUE
├── METODO  signo : INT
│   ├── PARAMETROS
│   │   └── DECL_VAR  n : INT
│   └── BLOQUE  (cuerpo)
│       └── IF-ELSE                       ← TRES hijos
│           ├── OPERADOR  >               ← [0] condición
│           │   ├── ID  n
│           │   └── CONSTANTE  INT  0
│           ├── BLOQUE  (rama then)       ← [1]
│           │   └── RETURN
│           │       └── CONSTANTE  INT  1 ← UN hijo: el valor
│           └── BLOQUE  (rama else)       ← [2]
│               └── RETURN
│                   └── CONSTANTE  INT  0
└── METODO  main : VOID
    ├── PARAMETROS
    └── BLOQUE
        ├── DECL_VAR  s : INT
        └── ASIGNACION  destino = s
            └── LLAMADA  signo
                └── CONSTANTE  INT  5
```

**Cada rama del `if` es un `BLOQUE` propio**, aunque tenga una sola sentencia:
la gramática exige llaves, así que siempre hay un nodo bloque en el medio.

```
  | Metodo: signo (Retorna INT)
  |   | Parametros
  |   |   | Declaracion Variable: n #0 (Tipo INT)
  |   | Bloque
  |   |   | If-Else
  |   |   |   | Operacion: >
  |   |   |   |   | ID: n #0 (Tipo VOID)
  |   |   |   |   | Constante INT: 0
  |   |   |   | Bloque
  |   |   |   |   | Return
  |   |   |   |   |   | Constante INT: 1
  |   |   |   | Bloque
  |   |   |   |   | Return
  |   |   |   |   |   | Constante INT: 0
```

### Etapa 3 — Análisis semántico

`analyzeReturnNode` necesita saber **en qué método está**, y para eso usa
`currentScope->function`, el campo que el ámbito del método guardó al crearse. Es
exactamente la razón por la que ese campo existe.

| Caso | Resultado |
| --- | --- |
| `return;` en método `void` | OK |
| `return;` en método con tipo | Error: *"espera un valor de retorno de tipo INT"* |
| `return expr;` en método `void` | Error: *"es void y no deberia retornar un valor"* |
| `return expr;` con tipo incompatible | Error, usando `isAssignable` (un `int` en un método `float` se acepta) |
| `return` fuera de todo método | Error: *"fuera del cuerpo de una funcion"* |

Y aparte, en `analyzeMethodNode`, la verificación de que **todos los caminos**
retornen:

| Nodo | ¿Garantiza retorno? |
| --- | --- |
| `return` | Sí |
| Bloque | Sí si **alguna** de sus sentencias lo garantiza |
| `if` **sin** `else` | **No** (la rama falsa puede no ejecutarse) |
| `if`/`else` | Sí **solo si las dos ramas** lo garantizan |
| `while` | **No** |

Acá, `signo` pasa porque es un `if`/`else` **con las dos ramas retornando**. Si
faltara el `else`, el error sería: *"El metodo 'signo' no es void: todos sus
caminos de ejecucion deben terminar en un return"*.

**Un `while(true)` con `return` adentro se rechaza** aunque en ejecución siempre
retorne: decidir eso requiere evaluar la condición, que es indecidible en
general. **Es una aproximación conservadora**, y ésa es la respuesta correcta si
lo preguntan.

**El árbol de ámbitos:**

```
ÁMBITO 0 — global                     parent = NULL
│   signo#1 : método INT, 1 parámetro
│   main#3  : método VOID
│
├── ÁMBITO 1 — cuerpo de signo        parent = ámbito 0 · function = signo#1
│   │   n#2 : INT
│   │
│   ├── ÁMBITO — rama then            parent = ámbito 1 · function = signo#1
│   │       (vacío: no declara nada)
│   └── ÁMBITO — rama else            parent = ámbito 1 · function = signo#1
│           (vacío)
│
└── ÁMBITO 2 — cuerpo de main         parent = ámbito 0 · function = main#3
        s#4 : INT
```

**Cada rama del `if` crea su propio ámbito**, aunque no declare nada: el
recorrido ve un `BLOQUE` y crea un `Scope`. Eso es lo que permitiría declarar una
variable adentro del `if` sin chocar con una de afuera. Y los dos heredan
`function = signo#1`, que es lo que hace que los `return` de adentro sepan que
tienen que devolver un `int`.

### Etapa 4 — Código intermedio

```
begin_func signo, 1
param_decl n@2, INT
t0 = n@2 > 0         ← la condición en un temporal
if t0 false goto L0  ← si es falsa, ir al else
ret 1                ← rama THEN
goto L1              ← saltear el else
L0:                  ← rama ELSE
ret 0
L1:                  ← fin del if
end_func signo
begin_func main, 0
local s@4, INT
param 5
t0 = call signo, 1
s@4 = t0
ret                  ← implícito: main es void
end_func main
```

| Instrucción | Por qué |
| --- | --- |
| `ret 1` | El operando es **qué** se devuelve. Es una constante: no hizo falta temporal |
| `goto L1` | Salta el `else`. **Acá es inalcanzable**, porque la rama `then` ya retornó… pero el generador no lo sabe: emite el esquema del `if`/`else` siempre igual. Es exactamente lo que `-opt dead-code` elimina |
| `ret` (en `main`) | **Implícito**: `main` es `void` y su cuerpo no terminaba en `return` |

**Por qué el `ret` implícito.** Para que **toda función termine en `ret`**, que es
una garantía de la que depende el generador de objeto: sin ella, la ejecución se
caería del final de una función al principio de la siguiente.

### Etapa 5 — Código objeto

```asm
.globl signo
.type signo, @function
signo:
    pushq %rbp
    movq %rsp, %rbp
    subq $16, %rsp         # 2 ranuras (n, t0) = 8 → 16
    movl %edi, -4(%rbp)    # param_decl n
    movl -4(%rbp), %eax    # t0 = n > 0 : cargar n
    movl $0, %edx          #              cargar el 0
    cmpl %edx, %eax        #              comparar
    setg %al               #              1 o 0
    movzbl %al, %eax       #              extender a 32 bits
    movl %eax, -8(%rbp)    #              guardar t0
    movl -8(%rbp), %eax    # if t0 false goto L0 : releer t0
    cmpl $0, %eax          #                       ¿es falso?
    je .L0                 #                       si sí, al else
    movl $1, %eax          # ret 1 : el valor de retorno a %eax
    jmp .L_signo_epilogue  #         y al epílogo
    jmp .L1                # goto L1 : INALCANZABLE (ver abajo)
.L0:
    movl $0, %eax          # ret 0
    jmp .L_signo_epilogue
.L1:
.L_signo_epilogue:         # el epílogo común: UNA sola copia del desarmado
    leave
    ret
```

- **Los dos `return` se volvieron `jmp .L_signo_epilogue`.** El valor se deja en
  `%eax` —el registro de retorno que define la ABI— y el desarmado del marco se
  escribe **una sola vez**. Con dos `return` ya se nota: sin el epílogo común
  habría dos `leave`/`ret` duplicados.
- **El `jmp .L1` es código muerto, y se ve.** Viene del esquema del `if`/`else`,
  que siempre emite el salto que saltea el `else`. Como la rama `then` terminó en
  un `jmp` al epílogo, nunca se llega. **Es exactamente lo que elimina
  `-opt dead-code`**: "todo lo que sigue a un salto incondicional, hasta la
  próxima etiqueta, es inalcanzable".
- **`.L1:` queda pegada al epílogo**, sin ninguna instrucción en el medio: la
  etiqueta de fin del `if` coincide con el final de la función.
- **En `main`**, el resultado de `call signo` se lee de `%eax` y se guarda en la
  ranura de `t0`, y recién después se copia a `s`. Dos instrucciones donde
  alcanzaría una, otra vez por la regla de que nada vive en registros entre
  cuádruplas.

---

## 10. `if` y `if`/`else`

### El programa

```c
void main() {
    int x;
    x = 5;
    if (x > 3) {
        x = 1;
    }
    if (x == 1) {
        x = 2;
    } else {
        x = 3;
    }
}
```

Los dos casos juntos, para ver la diferencia.

### Etapa 1 — Análisis léxico

```
[Linea 4] IF           if
[Linea 4] '('          (
[Linea 4] ID           x
[Linea 4] '>'          >
[Linea 4] NUMBER       3
[Linea 4] ')'          )
[Linea 4] '{'          {
[Linea 5] ID           x
[Linea 5] '='          =
[Linea 5] NUMBER       1
[Linea 5] ';'          ;
[Linea 6] '}'          }
[Linea 7] IF           if
...
[Linea 9] '}'          }
[Linea 9] ELSE         else
[Linea 9] '{'          {
```

`==` es **un token de dos caracteres** (`EQ`), escrito **antes** que la regla de
`=`. Flex usa la coincidencia más larga, así que `==` nunca se parte en dos `=`
aunque la regla de `=` esté antes en el archivo.

### Etapa 2 — Análisis sintáctico

```
Statement : IF '(' Expression ')' Block
          | IF '(' Expression ')' Block ELSE Block
```

**Pregunta casi segura: ¿y el *dangling else*?** El conflicto clásico —en
`if (a) if (b) S else T`, ¿a cuál `if` pertenece el `else`?— **no existe en esta
gramática**, porque el cuerpo de un `if` es un `Block` y `Block` **exige llaves**.
Las llaves desambiguan en el texto, así que la gramática no tiene qué
desambiguar: `bison -Wall` reporta **cero conflictos**.

Si el cuerpo fuera `Statement` en vez de `Block`, habría un conflicto
desplazar/reducir y habría que resolverlo (lo habitual: que gane el
desplazamiento, o sea que el `else` se asocie al `if` más cercano).

**El árbol:**

```
BLOQUE
└── METODO  main : VOID
    ├── PARAMETROS
    └── BLOQUE  (cuerpo)
        ├── DECL_VAR  x : INT
        ├── ASIGNACION  destino = x
        │   └── CONSTANTE  INT  5
        ├── IF-ELSE                        ← DOS hijos: sin else
        │   ├── OPERADOR  >                ← [0] condición
        │   │   ├── ID  x
        │   │   └── CONSTANTE  INT  3
        │   └── BLOQUE  (then)             ← [1]
        │       └── ASIGNACION  destino = x
        │           └── CONSTANTE  INT  1
        └── IF-ELSE                        ← TRES hijos: con else
            ├── OPERADOR  ==               ← [0] condición
            │   ├── ID  x
            │   └── CONSTANTE  INT  1
            ├── BLOQUE  (then)             ← [1]
            │   └── ASIGNACION  destino = x
            │       └── CONSTANTE  INT  2
            └── BLOQUE  (else)             ← [2]
                └── ASIGNACION  destino = x
                    └── CONSTANTE  INT  3
```

**El mismo tipo de nodo sirve para los dos casos**: `IF_ELSE_NODE` con dos o tres
hijos. Preguntar si hay `else` es preguntar si existe el hijo `[2]`
(`GET_ELSE_BLOCK(node) != NULL`).

```
  |   |   | If-Else
  |   |   |   | Operacion: >
  |   |   |   |   | ID: x #0 (Tipo VOID)
  |   |   |   |   | Constante INT: 3
  |   |   |   | Bloque
  |   |   |   |   | Asignacion a: x #0 (Tipo VOID)
  |   |   |   |   |   | Constante INT: 1
  |   |   | If-Else
  |   |   |   | Operacion: ==
  |   |   |   |   | ID: x #0 (Tipo VOID)
  |   |   |   |   | Constante INT: 1
  |   |   |   | Bloque
  |   |   |   |   | Asignacion a: x #0 (Tipo VOID)
  |   |   |   |   |   | Constante INT: 2
  |   |   |   | Bloque
  |   |   |   |   | Asignacion a: x #0 (Tipo VOID)
  |   |   |   |   |   | Constante INT: 3
```

### Etapa 3 — Análisis semántico

`analyzeIfElseNode` hace **una sola verificación propia** y después delega:

```c
analyzeBooleanCondition(node, currentScope, "if");   // la condición debe ser BOOL
analyzeNodeSemantics(GET_IF_BLOCK(node), currentScope);
analyzeNodeSemantics(GET_ELSE_BLOCK(node), currentScope);  // acepta NULL
```

**`if (x)` con `x` entero es error**: *"La condicion del if debe ser de tipo BOOL,
pero es INT"*. C-TDS no tiene semántica *truthy*.

Que `analyzeNodeSemantics` acepte `NULL` es lo que hace que el caso sin `else` no
necesite un `if` especial.

**El árbol de ámbitos:**

```
ÁMBITO 0 — global                        parent = NULL
│   main#1 : método VOID
│
└── ÁMBITO 1 — cuerpo de main            parent = ámbito 0 · function = main#1
    │   x#2 : INT
    │
    ├── ÁMBITO 2 — then del primer if    parent = ámbito 1 · function = main#1
    │       (vacío)
    ├── ÁMBITO 3 — then del segundo if   parent = ámbito 1
    │       (vacío)
    └── ÁMBITO 4 — else del segundo if   parent = ámbito 1
            (vacío)
```

**Cada bloque de cada rama crea un ámbito**, aunque esté vacío. Los tres son
hermanos, hijos del cuerpo de `main`, y los tres ven `x#2` subiendo al padre: por
eso las tres asignaciones resuelven a la misma variable.

```
  |   |   | Asignacion a: x #2 (Tipo INT)   ← las tres apuntan al MISMO símbolo
```

### Etapa 4 — Código intermedio

```
begin_func main, 0
local x@2, INT
x@2 = 5

t0 = x@2 > 3         ← IF SIN ELSE
if t0 false goto L0  ← si NO se cumple, saltear el cuerpo
x@2 = 1
L0:                  ← una sola etiqueta

t1 = x@2 == 1        ← IF CON ELSE
if t1 false goto L1  ← si NO se cumple, ir al else
x@2 = 2
goto L2              ← al terminar el then, SALTEAR el else
L1:
x@2 = 3
L2:                  ← dos etiquetas
ret
end_func main
```

| Forma | Etiquetas | Esquema |
| --- | :---: | --- |
| `if` sin `else` | **1** | `if_false → fin` · cuerpo · `fin:` |
| `if` con `else` | **2** | `if_false → else` · then · `goto fin` · `else:` · else · `fin:` |

| Instrucción | Por qué |
| --- | --- |
| `t0 = x@2 > 3` | La condición se evalúa a un temporal: el salto necesita un valor que mirar |
| `if t0 false goto L0` | **La condición se NIEGA.** El `if` del lenguaje dice "si es verdadero, hacé el cuerpo"; el TAC dice "si es **falso**, saltá el cuerpo" |
| `goto L2` | **La pieza que más se olvida.** Sin él, el flujo caería del `then` al `else` y se ejecutarían los dos |
| `L2:` | Donde se juntan los dos caminos |

**Por qué se niega la condición.** Porque así el camino verdadero **sigue en línea
recta** y no cuesta ningún salto; solo el camino falso paga. Es el esquema
estándar, y además es el que deja el código del `then` pegado a la condición, que
es mejor para la predicción de saltos del procesador.

**Las etiquetas se piden al empezar el `if`**, antes de generar el cuerpo. Si
adentro hubiera otro `if`, sus etiquetas tendrían números **mayores**, lo que
permite anidar sin colisiones.

### Etapa 5 — Código objeto

```asm
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $16, %rsp         # 3 ranuras (x, t0, t1) = 12 → 16
    movl $5, %eax          # x@2 = 5
    movl %eax, -4(%rbp)

    movl -4(%rbp), %eax    # ---- primer if: t0 = x > 3
    movl $3, %edx
    cmpl %edx, %eax
    setg %al
    movzbl %al, %eax
    movl %eax, -8(%rbp)
    movl -8(%rbp), %eax    # if t0 false goto L0
    cmpl $0, %eax
    je .L0
    movl $1, %eax          # x@2 = 1  (el cuerpo del then)
    movl %eax, -4(%rbp)
.L0:                       # fin del if sin else

    movl -4(%rbp), %eax    # ---- segundo if: t1 = x == 1
    movl $1, %edx
    cmpl %edx, %eax
    sete %al               # sete, no setg: es una igualdad
    movzbl %al, %eax
    movl %eax, -12(%rbp)
    movl -12(%rbp), %eax   # if t1 false goto L1
    cmpl $0, %eax
    je .L1
    movl $2, %eax          # x@2 = 2  (then)
    movl %eax, -4(%rbp)
    jmp .L2                # saltear el else
.L1:
    movl $3, %eax          # x@2 = 3  (else)
    movl %eax, -4(%rbp)
.L2:
    jmp .L_main_epilogue
```

- **Cada etiqueta del TAC se vuelve una etiqueta del assembly con un punto
  adelante** (`L0` → `.L0`): eso la marca como **local al archivo** y evita que
  entre en la tabla de símbolos del objeto.
- **`setg` contra `sete`** es lo único que cambia entre las dos condiciones: el
  mnemónico de `setCC` depende del operador relacional. `<` → `setl`, `>` →
  `setg`, `==` → `sete`.
- **Seis instrucciones por condición.** Se calcula el booleano, se guarda en su
  ranura, se relee, se compara contra cero y se salta. Es la consecuencia directa
  de que el TAC separe "calcular la comparación" de "saltar según el resultado", y
  de que nada viva en un registro entre cuádruplas.
- **Con `-opt peephole`, las cuatro del medio desaparecen.** Si una comparación es
  consumida inmediatamente por un salto, se emiten **fusionadas**:

```asm
    movl -4(%rbp), %eax
    movl $3, %edx
    cmpl %edx, %eax
    jle .L0                # en vez de setg + movzbl + cmpl $0 + je
```

  La negación se resuelve **eligiendo el mnemónico opuesto**: `jle` es la negación
  de `jg`. Son cuatro instrucciones menos por cada condición del programa.

---

## 11. `while`

### El programa

```c
void main() {
    int x;
    x = 0;
    while (x < 3) {
        x = x + 1;
    }
}
```

### Etapa 1 — Análisis léxico

```
[Linea 4] WHILE        while
[Linea 4] '('          (
[Linea 4] ID           x
[Linea 4] '<'          <
[Linea 4] NUMBER       3
[Linea 4] ')'          )
[Linea 4] '{'          {
[Linea 5] ID           x
[Linea 5] '='          =
[Linea 5] ID           x
[Linea 5] '+'          +
[Linea 5] NUMBER       1
[Linea 5] ';'          ;
[Linea 6] '}'          }
```

`<` y `>` salen de la regla `"<"|">"` y vuelven como el carácter mismo; `while`
es palabra reservada con token propio.

### Etapa 2 — Análisis sintáctico

```
Statement : WHILE '(' Expression ')' Block   { newWhileNode($3, $5); }
```

**El árbol:**

```
BLOQUE
└── METODO  main : VOID
    ├── PARAMETROS
    └── BLOQUE  (cuerpo)
        ├── DECL_VAR  x : INT
        ├── ASIGNACION  destino = x
        │   └── CONSTANTE  INT  0
        └── WHILE                          ← DOS hijos
            ├── OPERADOR  <                ← [0] condición
            │   ├── ID  x
            │   └── CONSTANTE  INT  3
            └── BLOQUE  (cuerpo del ciclo) ← [1]
                └── ASIGNACION  destino = x
                    └── OPERADOR  +
                        ├── ID  x
                        └── CONSTANTE  INT  1
```

**El árbol del `while` es idéntico al de un `if` sin `else`**: condición y cuerpo.
Lo que los diferencia es el `nodeType`, y toda la diferencia de comportamiento
aparece recién al generar código.

```
  |   |   | While
  |   |   |   | Operacion: <
  |   |   |   |   | ID: x #0 (Tipo VOID)
  |   |   |   |   | Constante INT: 3
  |   |   |   | Bloque
  |   |   |   |   | Asignacion a: x #0 (Tipo VOID)
  |   |   |   |   |   | Operacion: +
  |   |   |   |   |   |   | ID: x #0 (Tipo VOID)
  |   |   |   |   |   |   | Constante INT: 1
```

### Etapa 3 — Análisis semántico

Igual que el `if`: una sola verificación propia, que **la condición sea
`boolean`**, y después el cuerpo.

```c
analyzeBooleanCondition(node, currentScope, "while");
analyzeNodeSemantics(GET_WHILE_BLOCK(node), currentScope);
```

**El semántico no sabe que esto se repite.** Para él, un `while` es una condición
más un bloque. La repetición es una propiedad de la traducción, no de los tipos.
Por eso `guaranteesReturn` responde **no** para un `while`: no puede saber si el
cuerpo se ejecuta siquiera una vez.

**El árbol de ámbitos:**

```
ÁMBITO 0 — global                    parent = NULL
│   main#1 : método VOID
│
└── ÁMBITO 1 — cuerpo de main        parent = ámbito 0 · function = main#1
    │   x#2 : INT
    │
    └── ÁMBITO 2 — cuerpo del while  parent = ámbito 1 · function = main#1
            (vacío)
```

**El cuerpo del ciclo crea un ámbito**, y **uno solo**, aunque el ciclo se ejecute
muchas veces: los ámbitos son una estructura **estática**, del texto del programa,
no de la ejecución. Si el cuerpo declarara una variable, habría **un** símbolo en
la tabla, no uno por iteración.

### Etapa 4 — Código intermedio

```
begin_func main, 0
local x@2, INT
x@2 = 0
L0:                   ← etiqueta de INICIO: acá vuelve cada iteración
t0 = x@2 < 3          ← la condición se reevalúa en CADA vuelta
if t0 false goto L1   ← salida del ciclo
t1 = x@2 + 1          ← cuerpo
x@2 = t1
goto L0               ← VOLVER a evaluar la condición
L1:                   ← etiqueta de FIN
ret
end_func main
```

| Instrucción | Por qué |
| --- | --- |
| `L0:` | Se emite **antes de la condición**, porque la condición se reevalúa en cada vuelta. Ésa es **toda** la diferencia con el `if` |
| `t0 = x@2 < 3` | La condición, a un temporal, igual que en el `if` |
| `if t0 false goto L1` | La salida. Misma negación que el `if`: se salta cuando la condición **no** se cumple |
| `goto L0` | **El salto de vuelta.** Es lo que convierte el esquema del `if` en un ciclo |
| `L1:` | Donde cae la salida |

**Las dos etiquetas se piden juntas al empezar** (`L0` y `L1`), antes de generar
nada del cuerpo. Por eso están numeradas consecutivamente aunque `L1:` aparezca
mucho más abajo. Si en el cuerpo hubiera otro `while`, sus etiquetas tendrían
números **mayores**, y eso es lo que permite anidar sin colisiones.

**Comparación directa con el `if` sin `else`:**

```
IF                          WHILE
   t = cond                    L0:
   if t false goto L0          t = cond
   <cuerpo>                    if t false goto L1
L0:                            <cuerpo>
                               goto L0
                            L1:
```

**Dos diferencias y nada más**: la etiqueta de inicio antes de la condición, y el
`goto` de vuelta al final.

### Etapa 5 — Código objeto

```asm
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $16, %rsp         # 3 ranuras (x, t0, t1) = 12 → 16
    movl $0, %eax          # x@2 = 0
    movl %eax, -4(%rbp)
.L0:                       # ← vuelve acá en cada iteración
    movl -4(%rbp), %eax    # t0 = x < 3 : cargar x (RECARGADO cada vuelta)
    movl $3, %edx
    cmpl %edx, %eax
    setl %al               # setl: menor que
    movzbl %al, %eax
    movl %eax, -8(%rbp)    # guardar t0
    movl -8(%rbp), %eax    # if t0 false goto L1
    cmpl $0, %eax
    je .L1                 # salir del ciclo
    movl -4(%rbp), %eax    # t1 = x + 1 : cuerpo
    movl $1, %edx
    addl %edx, %eax
    movl %eax, -12(%rbp)
    movl -12(%rbp), %eax   # x@2 = t1
    movl %eax, -4(%rbp)
    jmp .L0                # ← VOLVER al inicio
.L1:
    jmp .L_main_epilogue
```

- **`.L0` está antes de la carga de `x`**, así que `x` se **relee de memoria en
  cada iteración**. Es correcto —el cuerpo la modifica— y además es obligatorio
  con este diseño, porque ningún valor vive en un registro entre cuádruplas. Un
  compilador optimizador la mantendría en un registro durante todo el ciclo.
- **`jmp .L0` es un salto incondicional hacia atrás**: es lo que hace que esto sea
  un ciclo y no una condición.
- **Las dos ranuras de temporales (`t0` y `t1`) se reutilizan en cada
  iteración**: hay **una** ranura para `t0`, no una por vuelta. Los temporales son
  posiciones fijas del marco, no valores nuevos cada vez.
- **El marco se calcula una sola vez** aunque el ciclo itere mil veces: las
  ranuras se reservan en el prólogo, antes de entrar.

---

## 12. Bloques y ocultamiento de nombres

### El programa

```c
int x;
void main() {
    int x;
    x = 1;
    {
        int x;
        x = 2;
    }
}
```

Tres variables **con el mismo nombre**, en tres ámbitos distintos.

### Etapa 1 — Análisis léxico

```
[Linea 1] INT          int
[Linea 1] ID           x
[Linea 1] ';'          ;
[Linea 2] VOID         void
[Linea 2] ID           main
[Linea 2] '('          (
[Linea 2] ')'          )
[Linea 2] '{'          {
[Linea 3] INT          int
[Linea 3] ID           x
[Linea 3] ';'          ;
[Linea 4] ID           x
[Linea 4] '='          =
[Linea 4] NUMBER       1
[Linea 4] ';'          ;
[Linea 5] '{'          {
[Linea 6] INT          int
[Linea 6] ID           x
[Linea 6] ';'          ;
[Linea 7] ID           x
[Linea 7] '='          =
[Linea 7] NUMBER       2
[Linea 7] ';'          ;
[Linea 8] '}'          }
[Linea 9] '}'          }
```

**Para el lexer las tres `x` son idénticas**: el mismo token, el mismo texto. No
tiene ninguna noción de ámbito. Las llaves son solo delimitadores.

### Etapa 2 — Análisis sintáctico

```
Block     : '{' VariableDeclarations Statements '}'   { newBlockNode(...); }
Statement : Block                                      { $$ = $1; }
```

**Un bloque es una sentencia**, y por eso se puede escribir un `{ ... }` suelto
adentro de otro bloque. La producción `Statement : Block` es la que lo permite.

**El árbol:**

```
BLOQUE                                  ← raíz
├── DECL_VAR  x : INT                   ← la GLOBAL
└── METODO  main : VOID
    ├── PARAMETROS
    └── BLOQUE  (cuerpo de main)
        ├── DECL_VAR  x : INT           ← la LOCAL de main
        ├── ASIGNACION  destino = x
        │   └── CONSTANTE  INT  1
        └── BLOQUE                      ← el bloque ANIDADO, suelto
            ├── DECL_VAR  x : INT       ← la LOCAL del bloque
            └── ASIGNACION  destino = x
                └── CONSTANTE  INT  2
```

**El parser no se queja de las tres `x`** porque no sabe nada de ámbitos: para él
son tres declaraciones y dos asignaciones, cada una con su símbolo provisorio
independiente.

```
Bloque
  | Declaracion Variable: x #0 (Tipo INT)
  | Metodo: main (Retorna VOID)
  |   | Parametros
  |   | Bloque
  |   |   | Declaracion Variable: x #0 (Tipo INT)
  |   |   | Asignacion a: x #0 (Tipo VOID)
  |   |   |   | Constante INT: 1
  |   |   | Bloque
  |   |   |   | Declaracion Variable: x #0 (Tipo INT)
  |   |   |   | Asignacion a: x #0 (Tipo VOID)
  |   |   |   |   | Constante INT: 2
```

Todas `#0`: cinco símbolos provisorios distintos, todos sin registrar.

### Etapa 3 — Análisis semántico

Acá es donde pasa todo. Cada `BLOCK_NODE` que aparece como sentencia dispara
`analyzeSemantics(node, currentScope)`, que **crea un ámbito nuevo colgado del
actual**.

Las tres `x` se registran en ámbitos distintos, así que **no hay error de
redeclaración**: la verificación usa `lookupSymbolInScope`, que mira **solo el
ámbito actual**.

**El árbol de ámbitos — el punto de esta sección:**

```
ÁMBITO 0 — global                     parent = NULL · level 0
│   x#1    : INT  (isGlobal = TRUE)
│   main#2 : método VOID
│
└── ÁMBITO 1 — cuerpo de main         parent = ámbito 0 · function = main#2 · level 1
    │   x#3 : INT  (isGlobal = false)
    │
    └── ÁMBITO 2 — bloque anidado     parent = ámbito 1 · function = main#2 · level 2
            x#4 : INT  (isGlobal = false)
```

**Cómo resuelve cada uso:**

| Uso | Dónde busca | Encuentra |
| --- | --- | --- |
| `x = 1` (línea 4) | ámbito 1 | `x#3` — **la encuentra en el primer intento** |
| `x = 2` (línea 7) | ámbito 2 | `x#4` — también en el primero |
| Si hubiera un `x` en otra función | ámbito de esa función → ámbito 0 | `x#1`, la global |

**La global `x#1` queda inaccesible desde adentro de `main`**: `lookupSymbol`
devuelve la primera coincidencia subiendo, y la local la tapa. Eso **es** el
ocultamiento, y sale gratis de la estructura de árbol más la búsqueda ascendente.

El `.sem` muestra las tres resueltas a símbolos distintos:

```
Bloque
  | Declaracion Variable: x #1 (Tipo INT)
  | Metodo: main (Retorna VOID)
  |   | Parametros
  |   | Bloque
  |   |   | Declaracion Variable: x #3 (Tipo INT)
  |   |   | Asignacion a: x #3 (Tipo INT)      ← la del medio
  |   |   |   | Constante INT: 1
  |   |   | Bloque
  |   |   |   | Declaracion Variable: x #4 (Tipo INT)
  |   |   |   | Asignacion a: x #4 (Tipo INT)  ← la interna
  |   |   |   |   | Constante INT: 2
```

**Por qué el `uid` 2 se lo llevó `main`**: el recorrido registra la global `x` (1),
después el método `main` (2), después entra y registra la local (3), después el
bloque anidado (4).

### Etapa 4 — Código intermedio

```
global x@1, INT      ← la global va a .data
begin_func main, 0
local x@3, INT       ← la local de main: su ranura
x@3 = 1
local x@4, INT       ← la local del bloque: OTRA ranura
x@4 = 2
ret
end_func main
```

**Acá se ve para qué sirve el `@uid`.** Las tres variables se llaman `x`, pero en
la lista plana son `x@1`, `x@3` y `x@4`: **tres nombres distintos**. Sin el
sufijo, el generador de objeto les daría la misma ranura y el programa calcularía
mal.

| Instrucción | Por qué |
| --- | --- |
| `global x@1` | Es la del ámbito 0, y `isGlobal` es true |
| `local x@3` / `local x@4` | Dos ranuras distintas en el **mismo** marco |

**El bloque anidado no generó ninguna instrucción propia.** No hay `begin_block`
ni nada parecido: los bloques existen en el árbol y en la tabla de símbolos, y en
el TAC **se aplanan**. El ámbito ya hizo su trabajo —desambiguar los nombres— y a
partir de ahí no hace falta.

**Y el `local x@4` aparece en el medio del código, no al principio.** El TAC
respeta el orden del programa; lo que reordena es la pasada de dimensionamiento
del generador de objeto, que las cuenta todas antes de emitir nada.

### Etapa 5 — Código objeto

```asm
.data
.globl x
.align 4
x:
    .long 0                # la global, por nombre

.text

.globl main
.type main, @function
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $16, %rsp         # 2 ranuras (x@3 y x@4) = 8 → 16
    movl $1, %eax          # x@3 = 1
    movl %eax, -4(%rbp)    #   ranura de la local de main
    movl $2, %eax          # x@4 = 2
    movl %eax, -8(%rbp)    #   ranura DISTINTA, la del bloque anidado
    jmp .L_main_epilogue
.L_main_epilogue:
    movl $0, %eax
    leave
    ret
```

- **Las dos locales tienen ranuras distintas (`-4` y `-8`)** aunque se llamen
  igual, porque el generador las busca por el nombre calificado, que es distinto.
- **La global ni se toca**, porque el programa nunca la usa: quedó declarada en
  `.data` y nada más. Si `main` dijera `x = 1` sin declarar la local, esa misma
  instrucción sería `movl $1, x(%rip)`.
- **Las dos ranuras coexisten durante toda la función**, aunque la del bloque
  interno solo "exista" conceptualmente dentro de las llaves. El marco se
  dimensiona una vez, con **todas** las ranuras de la función: no se reutiliza el
  espacio al salir de un bloque. Es más simple y desperdicia unos bytes.

---

## 13. El marco de pila

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
O se hacen dos pasadas, o habría que emitir a un buffer y parchear el número.

### Cómo se ve el marco en memoria

```
            direcciones ALTAS
        ┌─────────────────────────┐
        │  argumento 8 (si hay)   │  +24(%rbp)
        │  argumento 7 (si hay)   │  +16(%rbp)
        ├─────────────────────────┤
        │  dirección de retorno   │   +8(%rbp)   ← la apiló el 'call'
        ├─────────────────────────┤
 %rbp → │  %rbp del llamador      │    0(%rbp)   ← lo apiló 'pushq %rbp'
        ├─────────────────────────┤
        │  primera ranura         │   -4(%rbp)
        │  segunda ranura         │   -8(%rbp)
        │  ...                    │
        │  ranuras de argumentos  │              ← las últimas, para las llamadas
        │  pendientes             │
 %rsp → └─────────────────────────┘  -frameSize(%rbp)
            direcciones BAJAS
```

**Todas las ranuras tienen offset negativo** respecto de `%rbp`, porque la pila
crece hacia abajo. Los **argumentos que llegaron por la pila** son la excepción:
están en `+16(%rbp)` y para arriba (8 de la dirección de retorno + 8 del `%rbp`
guardado).

**Cada ranura ocupa 4 bytes aunque el tipo sea `boolean`**: alinear todo a 4 evita
cualquier cuenta de relleno, y la memoria desperdiciada es irrelevante.

### Ejemplo verificado: el `while` de §11

| Ranura | Qué es | Offset | De dónde salió |
| --- | --- | --- | --- |
| 1 | `x@2` | `-4(%rbp)` | el `local x@2` |
| 2 | `t0` | `-8(%rbp)` | resultado de `t0 = x@2 < 3` |
| 3 | `t1` | `-12(%rbp)` | resultado de `t1 = x@2 + 1` |

3 ranuras × 4 bytes = **12** → alineado a 16 → **16**. Y el assembly real dice
`subq $16, %rsp`. ✔

### Ejemplo verificado: la expresión de §4

| Ranura | Qué es | Offset |
| --- | --- | --- |
| 1 | `x@2` | `-4(%rbp)` |
| 2 | **`t1`** | `-8(%rbp)` |
| 3 | **`t0`** | `-12(%rbp)` |

**`t1` se llevó la ranura de más arriba aunque tenga el número mayor**, porque las
ranuras se asignan **en el orden en que las instrucciones aparecen en el TAC**, y
`t1 = 3 * 4` se emitió primero. Es el detalle que demuestra que entendiste cómo se
arma el marco.

---

## 14. El optimizador

No se aplica salvo que se pase `-opt`. Son **siete optimizaciones**, seis sobre el
TAC y una sobre el assembly:

| Flag | Qué hace |
| --- | --- |
| `const-fold` | `2 + 3` → `5` en tiempo de compilación |
| `const-prop` | Si `x = 5`, reemplaza los usos de `x` por `5` |
| `const-branch` | Un salto con condición constante → se borra o se vuelve `goto` |
| `dead-code` | Borra lo inalcanzable (tras un `return`/`goto`) y las asignaciones que nadie lee |
| `self-assign` | Borra `x = x` |
| `peephole` | Borra un `goto L` seguido de `L:`, y **fusiona comparación + salto** en el assembly |
| `strength-reduce` | `x * 8` → `sall $3` (un desplazamiento) |

Se aplican **en un bucle hasta punto fijo** (máximo 16 vueltas), porque una
habilita a otra: plegar crea una constante, que la propagación lleva más lejos, lo
que puede volver constante una condición, lo que vuelve inalcanzable un bloque.
**Termina** porque ninguna pasada agrega instrucciones.

La única que necesita un **análisis** previo es `dead-code`, que se apoya en
**análisis de variables vivas**: se parte el TAC en bloques básicos, se arma el
grafo de flujo de control (CFG) y se resuelven las ecuaciones

```
LIVE_OUT(B) = unión de LIVE_IN(S) para cada sucesor S
LIVE_IN(B)  = USE(B) ∪ (LIVE_OUT(B) − DEF(B))
```

hasta que nada cambia. Es un **análisis hacia atrás** (la información fluye del
final hacia el principio), y los conjuntos se representan como **máscaras de
bits** (`uint64_t`): la unión es un `or`, la diferencia un `and` con negación y la
pertenencia un desplazamiento.

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

### D. Tabla resumen: qué nodo tiene qué hijos

| Nodo | Hijos | Dónde va el nombre |
| --- | --- | --- |
| `BLOQUE` | N sentencias y declaraciones | — |
| `DECL_VAR` | ninguno | `symbol` |
| `METODO` | `[0]` PARAMETROS · `[1]` BLOQUE | `symbol` |
| `PARAMETROS` | N declaraciones | — |
| `ASIGNACION` | `[0]` la expresión | `symbol` = **el destino** |
| `LLAMADA` | N argumentos | `symbol` = el método |
| `RETURN` | 0 o 1 (la expresión) | — |
| `IF-ELSE` | `[0]` cond · `[1]` then · `[2]` else (opcional) | — |
| `WHILE` | `[0]` cond · `[1]` cuerpo | — |
| `OPERADOR` | 1 (unario) o 2 (binario) | `operationType` = el operador |
| `ID` | ninguno | `symbol` |
| `CONSTANTE` | ninguno | `symbol` (sin `id`, con `value`) |

### E. Checklist: "me dieron un programa, ¿qué hago?"

1. **Tokens.** Palabra por palabra. Las reservadas antes que los identificadores.
   Los literales llevan su valor aparte. Los delimitadores y los operadores de un
   carácter vuelven como el carácter mismo.
2. **Árbol.** De abajo hacia arriba. Precedencia alta = más abajo en el árbol. El
   destino de una asignación **no es un hijo**. Cada bloque con llaves es un nodo.
   `PARAMETROS` existe aunque esté vacío.
3. **Tabla de símbolos.** Un ámbito por método y **uno por cada bloque con
   llaves**. Numerar los `uid` **en el orden en que aparecen las declaraciones al
   recorrer el árbol de arriba hacia abajo**, sin olvidar que **los métodos también
   consumen un número**.
4. **Tipos.** De las hojas hacia la raíz. Las constantes ya vienen tipadas.
5. **TAC.** Postorden: operandos antes que la operación. Un temporal por
   operación, **reiniciando en cada método**. Las etiquetas se piden por par en
   `if`/`while` y de a una en el corto circuito, y **no se reinician**.
6. **Assembly.** Contar las ranuras (parámetros + locales + temporales +
   argumentos pendientes), × 4, alinear a 16. Después, cada cuádrupla:
   **cargar, operar, guardar**.

### F. Las cinco frases que conviene tener listas

- *"El parser no sabe de tipos ni de ámbitos: solo construye el árbol."*
- *"El `uid` lo asigna el semántico al registrar la declaración; por eso el TAC
  puede ser una lista plana sin tabla de símbolos."*
- *"No hay opcodes `and` ni `or` porque el corto circuito es control de flujo, no
  una operación."*
- *"Ningún valor sobrevive en un registro entre dos cuádruplas: por eso el código
  es largo y por eso es verificable."*
- *"El marco se arma en dos pasadas porque el `subq` del prólogo necesita un
  número que recién se conoce al final."*
