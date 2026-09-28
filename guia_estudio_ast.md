# Guía de Estudio: Listas, `NodeList` y `children` en el AST

Esta guía resume cómo se manejan los elementos repetibles (como listas de sentencias o argumentos) durante la construcción de un Abstract Syntax Tree (AST) usando Bison y C.

---

## 1. Conceptos Fundamentales

*   **Parseo Bottom-Up (Bison):** Bison lee el código desde las "hojas" hacia la "raíz". Descubre primero las partes pequeñas (ej. identificadores individuales) antes de descubrir la estructura completa que los contiene (ej. una lista de declaraciones).
*   **Aridad:** Cantidad de hijos que tiene un nodo.
    *   **Aridad Fija:** Nodos que siempre tienen la misma cantidad de hijos (ej. `+` siempre tiene lado izquierdo y derecho). Usan los punteros `left`, `right`.
    *   **Aridad Variable (N-ario):** Nodos que pueden tener de 0 a $N$ hijos (ej. un bloque de código `{ ... }`). Usan el arreglo `children`.

---

## 2. El problema: Acumular elementos en Bison

Cuando Bison se encuentra con reglas como `Statements` o `ListArguments`, no sabe de antemano cuántos elementos habrá. Necesita una forma de "acumularlos" uno tras otro temporalmente hasta que se cierre la estructura contenedora (por ejemplo, hasta encontrar la llave de cierre `}`).

### Solución: `%type <list>` y `NodeList`

*   **¿Qué es `NodeList`?** Es una **lista enlazada auxiliar y temporal**. Solo existe mientras Bison está leyendo el archivo. NO forma parte del AST definitivo.
*   **¿Qué es `%type <list>`?** Es la instrucción en `bison.y` que le dice a Bison: *"Cuando resuelvas reglas como `Statements` o `ListArguments`, el resultado (`$$`) será un puntero a una lista enlazada temporal (`NodeList*`)"*.
*   **Metáfora:** Imagina que `NodeList` es un **"carrito de compras"**. Vas metiendo las sentencias una a una mientras recorres los pasillos del código.

---

## 3. La solución definitiva: `children` en `NodeAST`

Una vez que Bison termina de leer toda la lista de elementos (ej. se termina el bloque de sentencias), necesitamos guardar esos elementos en el AST de forma eficiente. Las listas enlazadas son malas para buscar datos rápido; los arreglos son mejores.

*   **¿Qué es `children`?** Es un **arreglo dinámico de punteros** (`NodeAST **children`) que vive dentro de los nodos definitivos del árbol.
*   **¿Cuándo se usa?** **Exclusivamente en nodos de aridad variable** (que pueden tener 0, 1 o muchos hijos).

---

## 4. El Ciclo de Vida (El "Paso a Paso")

Así es como un elemento pasa de ser texto a estar en el AST:

1.  **Recolección (`newNodeList`):** Bison lee reglas recursivas (ej. `Statement Statements`). Usa `newNodeList()` para ir enlazando temporalmente cada sentencia leída en un "carrito de compras" (`NodeList`).
2.  **Unión opcional (`mergeNodeLists`):** Si hay listas separadas que van al mismo lugar (ej. Declaraciones de variables + Sentencias de un bloque), se unen en una sola lista enlazada.
3.  **Vaciado del carrito (`attachChildren`):** Cuando Bison crea el nodo padre definitivo (ej. `BLOCK_NODE`), llama a `attachChildren(parent, list)`.
    *   Esta función cuenta cuántos elementos hay en la lista enlazada.
    *   Reserva memoria (`malloc`) para el arreglo `children` con ese tamaño exacto.
    *   Copia los punteros de la lista al arreglo.
    *   **Destruye la lista enlazada `NodeList`** (libera su memoria porque ya no sirve).
    *   Actualiza `parent->childCount`.

---

## 5. Casos de Uso (Ejemplos de Parcial)

Si te preguntan *"¿En qué casos un nodo debería usar `children`?"*, aquí tienes las respuestas clave:

| Nodo / Contexto | ¿Por qué usa `children`? (Aridad variable) | Ejemplo |
| :--- | :--- | :--- |
| **`BLOCK_NODE`** (Bloques de código) | Un bloque `{ }` puede tener $0$ a $N$ instrucciones adentro. | `{ x = 1; y = 2; z = 3; }` (3 hijos) |
| **`METHOD_CALL_NODE`** (Llamadas a función) | Una función puede recibir de $0$ a $N$ argumentos. | `sumar(a, b, c)` (3 hijos) |
| **Raíz del Programa** | Un archivo fuente puede tener $0$ a $N$ declaraciones globales y funciones. | `int a; void main() { }` (2 hijos) |
| **`IdentifierList`** (Variables en línea) | Se pueden declarar múltiples variables en la misma línea. | `int x, y, z;` (Se aplana a 3 nodos hijos del bloque) |

---
*Tip de estudio: Diferencia siempre las estructuras **auxiliares/temporales** (`NodeList`) de las estructuras **persistentes** (el arreglo `children` dentro de `NodeAST`).*
