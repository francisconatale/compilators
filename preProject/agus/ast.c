#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

// Este archivo contiene las funciones auxiliares que permiten crear los nodos definidos en ast.h
// Estas funciones las vamos a usar en Bison para construir el arbol sintactico a medida que parseamos
// el programa

// Por ej: para el nodo del programa, le pasamos los nodos de los statements
// Creamos el nodo raiz del programa, le asignamos su tipo, y le asignamos la lista de statements
ASTNode* create_program_node(ASTNode* statements) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = PROGRAM_NODE;
    node->data.program.statements = statements;
    return node;
}

ASTNode* create_statement_list_node(ASTNode* statement, ASTNode* next) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = STATEMENT_LIST_NODE;
    node->data.statement_list.statement = statement;
    node->data.statement_list.next = next;
    return node;
}

ASTNode* create_declaration_node(int data_type, char* var_name) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = DECLARATION_NODE;
    node->data.declaration.data_type = data_type;
    node->data.declaration.var_name = var_name;
    return node;
}

ASTNode* create_assignment_node(char* var_name, ASTNode* expression) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = ASSIGN_NODE;
    node->data.assignment.var_name = var_name;
    node->data.assignment.expression = expression;
    return node;
}

ASTNode* create_return_node(ASTNode* expression) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = RETURN_NODE;
    node->data.return_stmt.expression = expression;
    return node;
}

ASTNode* create_binop_node(ASTNode* left, char op, ASTNode* right) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = BINOP_NODE;
    node->data.binop.left = left;
    node->data.binop.op = op;
    node->data.binop.right = right;
    return node;
}

ASTNode* create_constant_node(int value) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = CONSTANT_NODE;
    node->data.constant.value = value;
    return node;
}

ASTNode* create_id_node(char* var_name) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = ID_NODE;
    node->data.id.var_name = var_name;
    return node;
}

// Por último, definimos la funcion de liberar memoria
void free_ast(ASTNode* node) {
    if (node == NULL) {
        return;
    }
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