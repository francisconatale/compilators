#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/*
 * newSymbol
 * ---------
 * Crea un Symbol reservado dinamicamente con los valores dados.
 *
 * Parametros:
 *   id    - identificador o lexema asociado (por ejemplo el nombre de
 *           una variable). Puede ser NULL si el nodo no necesita id.
 *   value - valor asociado (por ejemplo el valor literal de una
 *           constante). Puede ser NULL si todavia no se conoce.
 *
 * Devuelve:
 *   Puntero a un Symbol nuevo. La implementacion es responsable de
 *   copiar las cadenas recibidas (no quedarse con el puntero original)
 *   para que el Symbol sea independiente del buffer de yytext, que
 *   flex reutiliza en cada token.
 *
 * Dueño de la memoria: el llamador es dueño del Symbol devuelto.
 */
Symbol *newSymbol(const char *id, const char *value) {
    Symbol *symbol = malloc(sizeof(Symbol));
    // Por si se queda sin memoria
    if (symbol == NULL) {
        return NULL;
    }

    // Para evitar comportamientos indefinidos como strdup(NULL)
    symbol->id = (id != NULL) ? strdup(id) : NULL;
    symbol->value = (value != NULL) ? strdup(value) : NULL;

    return symbol;
}

/*
 * newNode
 * -------
 * Crea un NodeAST reservado dinamicamente.
 *
 * Parametros:
 *   nodeType - tipo de nodo (ver enum NodeType).
 *   symbol   - symbol asociado al nodo, o NULL si no aplica.
 *   left     - hijo izquierdo, o NULL.
 *   right    - hijo derecho, o NULL.
 *
 * Devuelve:
 *   Puntero a un NodeAST nuevo, con children == NULL y childCount == 0
 *   (se completan aparte con attachChildren). El campo type se inicializa
 *   por defecto con TYPE_VOID, ya que el tipo real de la
 *   mayoria de los nodos se resuelve recien en el analisis semantico.
 *
 * Dueño de la memoria:
 *   El llamador es dueño del NodeAST devuelto. left, right y symbol
 *   quedan referenciados (no copiados): el nodo padre pasa a ser
 *   responsable de esos punteros tambien.
 */
NodeAST *newNode(NodeType nodeType, Symbol *symbol, NodeAST *left, NodeAST *mid, NodeAST *right) {
    NodeAST *node = malloc(sizeof(NodeAST));
    // Por si se queda sin memoria
    if (node == NULL) {
        return NULL;
    }

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

    // Por si no se pudo crear el nodo debido a falta de memoria (newNode retorno NULL)
    if (node == NULL) {
        freeSymbol(symbol);
        return NULL;
    }

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

/*
 * freeSymbol
 * ----------
 * Libera un Symbol reservado con newSymbol, incluyendo las cadenas
 * copiadas internamente (id y value).
 *
 * Parametros:
 *   symbol - symbol a liberar. Puede ser NULL (no hace nada).
 *
 * Efecto:
 *   Libera symbol->id, symbol->value y symbol mismo. El puntero queda
 *   invalido despues de esta llamada.
 */
void freeSymbol(Symbol *symbol) {
    if (symbol == NULL) {
        return;
    }

    free(symbol->id);
    free(symbol->value);
    free(symbol);
}