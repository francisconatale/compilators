#include <stddef.h>
#include "ast.h"

Symbol *newSymbol(const char *id, const char *value) {
    (void)id;
    (void)value;
    /* TODO hernan jara*/
    return NULL;
}

NodeAST *newNode(NodeType nodeType, Symbol *symbol, NodeAST *left, NodeAST *mid, NodeAST *right) {
    (void)nodeType;
    (void)symbol;
    (void)left;
    (void)mid;
    (void)right;
    /* TODO hernan jara */
    return NULL;
}

NodeList *newNodeList(NodeAST *node, NodeList *next) {
    (void)node;
    (void)next;
    /* TODO hernan jara */
    return NULL;
}

NodeAST *newLiteralNode(DataType type, const char *value)
{
    Symbol *symbol = newSymbol(NULL, value);

    NodeAST *node = newNode(
        CONSTANT_NODE,
        symbol,
        NULL,
        NULL,
        NULL
    );

    node->type = type;

    return node;
}

void attachChildren(NodeAST *parent, NodeList *list) {
    (void)parent;
    (void)list;
    /* TODO hernan jara */
}

NodeList *mergeNodeLists(NodeList *list1, NodeList *list2) {
    if (list1 == NULL) return list2;
    if (list2 == NULL) return list1;
    
    NodeList *current = list1;
    while (current->next != NULL) {
        current = current->next;
    }
    
    current->next = list2;
    return list1;
}

NodeList *flattenVariableDeclarations(NodeAST *dataType, NodeList *identifiers) {
    NodeList *declarationList = NULL;
    NodeList **tail = &declarationList;
    NodeList *currentId = identifiers;
    
    while (currentId != NULL) {
        NodeAST *individualDecl = newNode(VARIABLE_DECLARATION_NODE, NULL, dataType, currentId->node, NULL);        
        NodeList *newCell = newNodeList(individualDecl, NULL);
        
        *tail = newCell;
        tail = &newCell->next;
        
        currentId = currentId->next;
    }
    
    return declarationList;
}
