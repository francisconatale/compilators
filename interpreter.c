#include <stdio.h>
#include <stdlib.h>
#include "interpreter.h"
#include "symtab.h"

// Declaraciones anticipadas (Forward declarations) de funciones privadas
static void execute_statement(ASTNode* node);
static int evaluate_expression(ASTNode* expr);

void interpret(ASTNode* node) {
    if (node == NULL) return;

    if (node->type == PROGRAM_NODE) {
        // El programa es solo un envoltorio, pasamos a la lista de sentencias
        interpret(node->as.program.statements);
    } 
    else if (node->type == STATEMENT_LIST_NODE) {
        // 1. Ejecutamos la instrucción actual
        execute_statement(node->as.statement_list.statement);
        
        // 2. Llamamos recursivamente para ejecutar la siguiente instrucción de la cadena
        interpret(node->as.statement_list.next);
    }
}

static void execute_statement(ASTNode* node) {
    if (node == NULL) return;

    int status;
    int resolved_value;

    switch (node->type) {
        case DECLARATION_NODE:
            // Intentamos insertar la variable en la tabla de símbolos
            status = insert_symbol(node->as.declaration.var_name, node->as.declaration.data_type);
            
            // Manejo de Error Semántico: Redeclaración
            if (status == SYMTAB_ERR_EXISTS) {
                fprintf(stderr, "Error Semantico: La variable '%s' ya fue declarada previamente.\n", node->as.declaration.var_name);
                exit(1);
            }
            break;

        case ASSIGN_NODE:
            // 1. Primero resolvemos matemáticamente todo lo que está a la derecha del '='
            resolved_value = evaluate_expression(node->as.assignment.expression);
            
            // 2. Intentamos guardar ese resultado numérico en la variable
            status = update_symbol(node->as.assignment.var_name, resolved_value);
            
            // Manejo de Error Semántico: Asignación a variable inexistente
            if (status == SYMTAB_ERR_NOT_FOUND) {
                fprintf(stderr, "Error Semantico: Intento de asignacion a la variable '%s' no declarada.\n", node->as.assignment.var_name);
                exit(1);
            }
            break;

        case RETURN_NODE:
            // En este compilador de juguete, el return simplemente finaliza e imprime el resultado
            if (node->as.return_stmt.expression != NULL) {
                resolved_value = evaluate_expression(node->as.return_stmt.expression);
                printf("=> Ejecucion finalizada. Retorno: %d\n", resolved_value);
            } else {
                printf("=> Ejecucion finalizada. (void)\n");
            }
            break;
            
        default:
            fprintf(stderr, "Error Interno: Nodo no reconocido como sentencia.\n");
            exit(1);
    }
}

static int evaluate_expression(ASTNode* expr) {
    if (expr == NULL) return 0;

    int stored_value;
    int status;
    int left_val, right_val;

    switch (expr->type) {
        case CONSTANT_NODE:
            // Es un número literal (ej. 42), simplemente extraemos y devolvemos su valor
            return expr->as.constant.value;

        case ID_NODE:
            // Es una variable (ej. 'x'). Debemos buscar en la tabla cuánto vale actualmente
            status = find_symbol(expr->as.id.var_name, &stored_value);
            
            // Manejo de Error Semántico: Variable no declarada
            if (status == SYMTAB_ERR_NOT_FOUND) {
                fprintf(stderr, "Error Semantico: Variable '%s' usada en una expresion no ha sido declarada.\n", expr->as.id.var_name);
                exit(1);
            } 
            // Manejo de Error Semántico: Variable sin valor asignado
            else if (status == SYMTAB_ERR_UNINITIALIZED) {
                fprintf(stderr, "Error Semantico: Variable '%s' usada sin haberle asignado un valor previo.\n", expr->as.id.var_name);
                exit(1);
            }
            
            // Si superó los controles, devolvemos el valor seguro que encontramos en la tabla
            return stored_value;

        case BINOP_NODE:
            // Es una operación matemática binaria. Resolvemos recursivamente ambas ramas.
            left_val = evaluate_expression(expr->as.binop.left);
            right_val = evaluate_expression(expr->as.binop.right);
            
            // Ejecutamos la instrucción matemática del procesador
            switch (expr->as.binop.op) {
                case OP_ADD: return left_val + right_val;
                case OP_SUB: return left_val - right_val;
                case OP_MUL: return left_val * right_val;
                case OP_DIV: 
                    if (right_val == 0) {
                        fprintf(stderr, "Error de Ejecucion: Division por cero.\n");
                        exit(1);
                    }
                    return left_val / right_val;
            }
            break;
            
        default:
            fprintf(stderr, "Error Interno: Nodo no reconocido en la expresion.\n");
            exit(1);
    }
    return 0; // Retorno de seguridad
}