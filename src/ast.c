#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/*
 * newSymbol
 * Crea un Symbol dinámico. Copia las cadenas 'id' y 'value' para ser
 * independiente de yytext. Retorna un puntero al Symbol (el llamador
 * es dueño de la memoria). Los parámetros pueden ser NULL.
 */
Symbol *newSymbol(const char *id, const char *value) {
    Symbol *symbol = malloc(sizeof(Symbol));
    if (symbol == NULL) return NULL;

    symbol->id = (id != NULL) ? strdup(id) : NULL;
    symbol->value = (value != NULL) ? strdup(value) : NULL;
    return symbol;
}

/*
 * newNode
 * Crea un NodeAST dinámico (tipo por defecto TYPE_VOID, sin hijos).
 * El llamador es dueño del nodo. 'left', 'mid', 'right' y 'symbol'
 * quedan referenciados y el nodo asume su propiedad.
 */
NodeAST *newNode(NodeType nodeType, Symbol *symbol, NodeAST *left, NodeAST *mid, NodeAST *right) {
    NodeAST *node = malloc(sizeof(NodeAST));
    if (node == NULL) return NULL;

    node->nodeType = nodeType;
    node->symbol = symbol;
    node->left = left;
    node->mid = mid;
    node->right = right;
    node->type = TYPE_VOID;
    node->children = NULL;
    node->childCount = 0;

    return node;
}

/*
 * newNodeList
 * Crea o extiende una lista enlazada de nodos (transitoria para bison).
 * El llamador es dueño de la celda devuelta.
 */
NodeList *newNodeList(NodeAST *node, NodeList *next) {
    NodeList *list = malloc(sizeof(NodeList));
    if (list == NULL) return NULL;

    list->node = node;
    list->next = next;

    return list;
}

/*
 * newLiteralNode
 * Crea un CONSTANT_NODE para un literal con su tipo semántico.
 * El llamador es dueño del nodo y de su Symbol interno.
 */
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

    if (node == NULL) {
        freeSymbol(symbol);
        return NULL;
    }

    node->type = type;

    return node;
}

/*
 * attachChildren
 * Vuelca los nodos de 'list' al arreglo 'children[]' de 'parent' 
 * (asumiendo children == NULL). Modifica in-place y libera 'list'.
 */
void attachChildren(NodeAST *parent, NodeList *list) {
    int count = 0;
    for (NodeList *current = list; current != NULL; current = current->next) {
        count++;
    }

    if (count == 0) {
        parent->children = NULL;
        parent->childCount = 0;
        return;
    }

    // reservamos el arreglo de punteros NodeAST de una vez
    NodeAST **children = malloc(count * sizeof(NodeAST *));
    if (children == NULL) return;

    // volcamos cada nodo al arreglo y liberamos la lista transitoria
    int i = 0;
    NodeList *current = list;
    while (current != NULL) {
        children[i] = current->node;
        i++;

        NodeList *toFree = current;
        current = current->next;
        free(toFree);
    }

    parent->children = children;
    parent->childCount = count;
}

/*
 * mergeNodeLists
 * Concatena 'list2' al final de 'list1' reutilizando las celdas
 * existentes sin reservar memoria nueva. Retorna la lista unida.
 */
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

/*
 * flattenVariableDeclarations
 * Convierte múltiples identificadores (ej. "int x, y;") en una lista
 * de declaraciones unitarias (VARIABLE_DECLARATION_NODE). Todas
 * comparten el puntero 'dataType'. El llamador es dueño de la lista.
 * los ifs nulls devuelven lo armado si nos quedamos sin memoria
 */
NodeList *flattenVariableDeclarations(NodeAST *dataType, NodeList *identifiers) {
    NodeList *declarationList = NULL;
    NodeList **tail = &declarationList;
    NodeList *currentId = identifiers;
    
    while (currentId != NULL) {
        NodeAST *individualDecl = newNode(VARIABLE_DECLARATION_NODE, NULL, dataType, currentId->node, NULL);        
        if (individualDecl == NULL) return declarationList;

        NodeList *newCell = newNodeList(individualDecl, NULL);
        if (newCell == NULL) return declarationList;
        
        *tail = newCell;
        tail = &newCell->next;
        
        currentId = currentId->next;
    }
    
    return declarationList;
}

/*
 * freeSymbol
 * Libera un Symbol y sus cadenas internas (id y value).
 */
void freeSymbol(Symbol *symbol) {
    if (symbol == NULL) return;

    free(symbol->id);
    free(symbol->value);
    free(symbol);
}