#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

ASTNode* create_program_node(ASTNode* statements) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = PROGRAM_NODE;
    node->as.program.statements = statements;
    return node;
}

ASTNode* create_statement_list_node(ASTNode* statement, ASTNode* next) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = STATEMENT_LIST_NODE;
    node->as.statement_list.statement = statement;
    node->as.statement_list.next = next;
    return node;
}

ASTNode* create_declaration_node(int data_type, char* var_name) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = DECLARATION_NODE;
    node->as.declaration.data_type = data_type;
    node->as.declaration.var_name = var_name;
    return node;
}

ASTNode* create_assignment_node(char* var_name, ASTNode* expression) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = ASSIGN_NODE;
    node->as.assignment.var_name = var_name;
    node->as.assignment.expression = expression;
    return node;
}

ASTNode* create_return_node(ASTNode* expression) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = RETURN_NODE;
    node->as.return_stmt.expression = expression;
    return node;
}

ASTNode* create_binop_node(ASTNode* left, char op, ASTNode* right) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = BINOP_NODE;
    node->as.binop.left = left;
    node->as.binop.op = op;
    node->as.binop.right = right;
    return node;
}

ASTNode* create_constant_node(int value) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = CONSTANT_NODE;
    node->as.constant.value = value;
    return node;
}

ASTNode* create_id_node(char* var_name) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = ID_NODE;
    node->as.id.var_name = var_name;
    return node;
}

void free_ast(ASTNode* node) {
    if (node == NULL) {
        return;
    }
    switch (node->type) {
        case PROGRAM_NODE:
            free_ast(node->as.program.statements);
            break;
        case STATEMENT_LIST_NODE:
            free_ast(node->as.statement_list.statement);
            free_ast(node->as.statement_list.next);
            break;
        case DECLARATION_NODE:
            free(node->as.declaration.var_name);
            break;
        case ASSIGN_NODE:
            free(node->as.assignment.var_name);
            free_ast(node->as.assignment.expression);
            break;
        case RETURN_NODE:
            free_ast(node->as.return_stmt.expression);
            break;
        case BINOP_NODE:
            free_ast(node->as.binop.left);
            free_ast(node->as.binop.right);
            break;
        case CONSTANT_NODE:
            break;
        case ID_NODE:
            free(node->as.id.var_name);
            break;
    }
    free(node); 
}