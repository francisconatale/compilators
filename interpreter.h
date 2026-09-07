#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "ast.h"

// Inicia la ejecución del programa recorriendo el AST desde la raíz
void interpret(ASTNode* root);

#endif // INTERPRETER_H