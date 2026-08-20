# Gramática Inicial

```bnf
p -> e
e -> e+e | e*e | (e) | numero
numero -> [0-9]+
```

**Ejemplos de cadenas aceptadas:**
- `12+(4)`
- `(12+12)*(4)`
- `(12*4+2)`
- `2`

---

# Extensión de la Gramática

## 1. Declaración de Variables

**Restricciones:**
- Las variables deben comenzar con una letra y seguir con letras o números.

**Ejemplos de cadenas aceptadas:**
- `int a;`
- `int a123;`
- `bool a1;`
- `bool a;`

**Ejemplos de cadenas no aceptadas:**
- `int 1;`
- `int 1a;`
- `bool 1a;`
- `bool1;`

**Expresión regular propuesta (para Lex/Flex):**
```regex
decl = (bool|int)[ \t]+[a-zA-Z][a-zA-Z0-9]*";"
```
*(En Flex debería devolver un token `decl`)*

## 2. Retornos (`return`)

**Ejemplos de cadenas aceptadas:**
- `return;`
- `return a;`
- `return true;`
- `return false;`
- `return 1+1;`
- `return 1;`

**Expresión regular propuesta:**
```regex
ret = "return"[ \t]+ e ";"
```
*(En Flex debería devolver un token `ret`)*

---

## Lenguaje Extendido Formalizado

Por lo tanto, el lenguaje extendido viene dado por la **gramática inicial** más las siguientes reglas y definiciones:

```bnf
boolean  -> "true" | "false"
type     -> "bool" | "int"
number   -> 0 | 1 | ... | 9
chars    -> a | b | ... | z | A | B | ... | Z
var_name -> chars (chars | number)*
return   -> "return" (var_name | boolean | number)? ";"
decl     -> "bool" var_name "=" boolean | "int" var_name "=" number+
assign   -> var_name "=" (number | chars | boolean)+
```
