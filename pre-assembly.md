# Generación de Pseudo-Assembly (Código de 3 Direcciones)

### 1. La regla de las "3 direcciones"
Se llama así porque cada instrucción matemática está estrictamente limitada a tener, como máximo, tres operandos (direcciones de memoria o registros):
* **Dos operandos de origen:** los que se van a calcular.
* **Un operando de destino:** donde se guarda el resultado.

> **Ejemplo:** `SUM op1 op2 destino`

Esta restricción es vital porque emula cómo funcionan las Unidades Aritmético Lógicas (ALU) en los procesadores reales. Básicamente, obliga al compilador a "masticar" ecuaciones gigantes en operaciones diminutas que la CPU pueda tragar de a una.

### 2. Los actores del Pseudo-Assembly
En los programas de nuestro C--, el pseudo-assembly va a escupir líneas donde interactúan tres tipos de "actores" (los operandos):

* **Constantes (Números puros):** Valores literales que escribimos en el código (ej. `42` o `1`). En el futuro assembly real, se convertirán en valores "inmediatos" inyectados directo en la instrucción.
* **Variables reales (`x`, `y`):** Son los nombres originales que pasaron la validación de nuestra tabla de símbolos. En esta etapa las llamamos por su nombre, pero en el assembly final (x86), el compilador las reemplazará por direcciones de memoria RAM reales (ej. `[RBP - 4]`).
* **Variables temporales (`T1`, `T2`, `T3`...):** Son la magia del compilador. Son variables invisibles para quien programa en C--, creadas automáticamente para guardar resultados intermedios. En un procesador real, estos `T` representan los **registros de la CPU** (como `EAX`, `EBX`, `ECX`). Como la CPU tiene pocos registros, crear infinitos temporales nos permite armar la lógica sin preocuparnos por quedarnos sin memoria todavía.

### 3. El set de instrucciones (Operadores)
Basado en nuestra gramática, el pseudo-assembly tiene un set de instrucciones muy conciso.

**Instrucciones aritméticas (3 direcciones completas):**
Toman dos valores, aplican la matemática y la guardan en el temporal.
* `SUM left right T_res`
* `RES left right T_res`
* `MUL left right T_res`

**Instrucción de movimiento (2 direcciones):**
No todas las instrucciones necesitan tres partes. La asignación usa solo dos: el origen (que puede ser un número, una variable o un temporal) y el destino final.
* `ASIG origen destino` (ej: `ASIG T3 x`)

**Instrucciones de control (1 o 0 direcciones):**
Sirven para cerrar la función.
* `RETURN origen` (ej: `RETURN T4`)
* `RETURN` (para retornos vacíos).

---

### 4. Un ejemplo visual en C--
Supongamos el siguiente código:

```c
int x;
x = 2 + 3 * 4;
return x;
```

El AST arma el árbol y, gracias a las reglas de precedencia de Bison, sabe que el * está más abajo en el árbol que el +. Al recorrerlo con el módulo híbrido que armamos, el compilador generará esto en consola:
```text
MUL 3 4 T1
SUM 2 T1 T2 
ASIG T2 x 
RETURN x
```

**¿Qué pasó acá?**

1. Como la multiplicación tiene prioridad, el motor resolvió primero ese subárbol. Agarró las constantes `3` y `4`, operó e inventó `T1` para guardar el resultado (`12`).
2. El motor subió un nivel en el árbol hacia la suma. Tomó la constante `2`, tomó el temporal `T1` que venía de abajo, operó, e inventó `T2` para guardar el nuevo resultado (`14`).
3. El motor subió al nodo de asignación (`=`). Tomó el temporal final `T2` y lo movió a la memoria de la variable real `x`.
4. Finalmente, ejecutó el retorno leyendo directamente `x`.
