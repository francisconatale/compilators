# TODO List: Semantic Analyzer

De acuerdo a la nueva definición, el análisis semántico se limita exclusivamente a:

- [x] Validar variables no declaradas y redeclaraciones en el mismo scope.
- [x] Análisis de tipos en expresiones binarias matemáticas y condicionales.
- [x] Validación de firmas en llamadas a funciones (cantidad y tipos de parámetros).
- [x] Validación de compatibilidad de tipos en la asignación (`isAssignable`).

**Pasos pendientes a implementar:**

- [ ] **Sentencias de control (IF / WHILE):**
  - Evitar que pasen por el `default:` de `analyzeNodeSemantics`.
  - Extraer la expresión condicional y llamar a `analyzeExpression()`.
  - Validar que el tipo resultante de la condición sea estrictamente `TYPE_BOOL`.

- [x] **Validación de la sentencia `RETURN`:**
  - Implementar `analyzeReturnNode` para que llame a `analyzeExpression()` sobre el valor de retorno.
  - Comparar el tipo devuelto con el `returnType` esperado por el método contenedor.

- [ ] **Verificación Estructural del Programa:**
  - Chequear que el programa defina el método `main` y que su firma sea la correcta.
  - Evitar definiciones duplicadas de funciones globales.

- [ ] **Reglas de control de flujo:**
  - Detectar si una función cuyo tipo de retorno es distinto de `VOID` llega al final de su ejecución sin ejecutar un `return`.

- [ ] **Pruebas y Reportes:**
  - Escribir pruebas en `tests/` que verifiquen fallos de tipo (`if (3+4)`, devolver `float` en método `int`, parámetros erróneos).
