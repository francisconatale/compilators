#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "ast.h"

void free_program_node(ASTNode* node);
void free_statement_list_node(ASTNode* node);
void free_declaration_node(ASTNode* node);
void free_assignment_node(ASTNode* node);
void free_return_node(ASTNode* node);
void free_binop_node(ASTNode* node);
void free_constant_node(ASTNode* node);
void free_id_node(ASTNode* node);

ASTNode* create_program_node(ASTNode* statements) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = PROGRAM_NODE;
    node->free_node = free_program_node;
    node->as.program.statements = statements;
    return node;
}

ASTNode* create_statement_list_node(ASTNode* statement, ASTNode* next) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = STATEMENT_LIST_NODE;
    node->free_node = free_statement_list_node;
    node->as.statement_list.statement = statement;
    node->as.statement_list.next = next;
    return node;
}

ASTNode* create_declaration_node(int data_type, char* var_name) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = DECLARATION_NODE;
    node->free_node = free_declaration_node;
    node->as.declaration.data_type = data_type;
    node->as.declaration.var_name = var_name;
    return node;
}

ASTNode* create_assignment_node(char* var_name, ASTNode* expression) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = ASSIGN_NODE;
    node->free_node = free_assignment_node;
    node->as.assignment.var_name = var_name;
    node->as.assignment.expression = expression;
    return node;
}

ASTNode* create_return_node(ASTNode* expression) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = RETURN_NODE;
    node->free_node = free_return_node;
    node->as.return_stmt.expression = expression;
    return node;
}

ASTNode* create_binop_node(ASTNode* left, Operator op, ASTNode* right) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = BINOP_NODE;
    node->free_node = free_binop_node;
    node->as.binop.left = left;
    node->as.binop.op = op;
    node->as.binop.right = right;
    return node;
}

ASTNode* create_constant_node(int value) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = CONSTANT_NODE;
    node->free_node = free_constant_node;
    node->as.constant.value = value;
    return node;
}

ASTNode* create_id_node(char* var_name) {
    ASTNode* node = (ASTNode*) malloc(sizeof(ASTNode));
    node->type = ID_NODE;
    node->free_node = free_id_node;
    node->as.id.var_name = var_name;
    return node;
}

void free_program_node(ASTNode* node) {
    free_ast(node->as.program.statements);
}

void free_statement_list_node(ASTNode* node) {
    free_ast(node->as.statement_list.statement);
    free_ast(node->as.statement_list.next);
}

void free_declaration_node(ASTNode* node) {
    free(node->as.declaration.var_name);
}

void free_assignment_node(ASTNode* node) {
    free(node->as.assignment.var_name);
    free_ast(node->as.assignment.expression);
}

void free_return_node(ASTNode* node) {
    free_ast(node->as.return_stmt.expression);
}

void free_binop_node(ASTNode* node) {
    free_ast(node->as.binop.left);
    free_ast(node->as.binop.right);
}

void free_constant_node(ASTNode* node) {
}

void free_id_node(ASTNode* node) {
    free(node->as.id.var_name);
}

void free_ast(ASTNode* node) {
    if (node == NULL) {
        return;
    }
    
    assert(node->free_node != NULL && "El nodo no tiene definida su función free_node");
    
    node->free_node(node);
    free(node); 
}