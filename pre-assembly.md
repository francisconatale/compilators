ltabla_offsets = mapa vacío (nombre -> offset)
offset_actual = 0

recorrer_prepass(nodo):
    switch nodo.tipo:
        caso DECLARACION:
            offset_actual = offset_actual + 8
            tabla_offsets[nodo.nombre] = offset_actual   // sobrescribe si ya existía
        caso ASIGNACION:
            // no declara nada, pero puede tener otra decl anidada si tu gramática lo permite
            // normalmente no hace falta bajar a nodo.valor acá
        default:
            // BINOP, LITERAL, VARIABLE: no declaran nada, no se recursa en el pre-pass

para cada statement en AST.statements:
    recorrer_prepass(statement)

total_bytes = offset_actual
// total_bytes = round_up(total_bytes, 16)   // opcional, si hay calls



emitir("push rbp")
emitir("mov rbp, rsp")
emitir("sub rsp, " + total_bytes)


gen_expr(nodo):
    switch nodo.tipo:
        caso LITERAL:
            emitir("mov rax, " + nodo.valor)

        caso VARIABLE:
            offset = tabla_offsets[nodo.nombre]
            si offset no existe:
                error("variable no declarada: " + nodo.nombre)
            emitir("mov rax, [rbp-" + offset + "]")

        caso BINOP:
            gen_expr(nodo.izq)          // resultado en rax
            emitir("push rax")          // apilo el resultado de la izquierda
            gen_expr(nodo.der)          // resultado en rax (pisa lo anterior)
            emitir("mov rbx, rax")      // muevo el resultado de la derecha a rbx
            emitir("pop rax")           // recupero el de la izquierda, apilado "debajo"
            emitir(op_asm(nodo.operador) + " rax, rbx")
            // resultado final de este subárbol queda en rax

        default:
            error("nodo inesperado en expresión: " + nodo.tipo)


// op_asm mapea el operador del nodo a la instrucción:
op_asm(operador):
    switch operador:
        caso "+": return "add"
        caso "-": return "sub"
        caso "*": return "imul"
        // "/" requiere manejo especial (idiv usa rdx:rax), ver nota abajo


gen_stmt(nodo):
    switch nodo.tipo:
        caso DECLARACION:
            // nada, el offset ya fue reservado en el pre-pass
            continuar

        caso ASIGNACION:
            offset = tabla_offsets[nodo.nombre]
            gen_expr(nodo.valor)              // resuelve todo el árbol, deja resultado en rax
            emitir("mov [rbp-" + offset + "], rax")

para cada statement en AST.statements:
    gen_stmt(statement)



emitir("mov rsp, rbp")
emitir("pop rbp")
emitir("ret")
