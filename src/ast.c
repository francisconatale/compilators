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

/*
 * newNodeList
 * -----------
 * Crea (o extiende) una lista enlazada de nodos hermanos, usada
 * unicamente durante la construccion del AST en bison.y.
 *
 * Parametros:
 *   node - nodo a envolver en esta celda de la lista.
 *   next - resto de la lista (NULL si este es el ultimo elemento).
 *
 * Devuelve:
 *   Puntero a la nueva celda NodeList, con node y next asignados
 *   tal cual se recibieron.
 *
 * Dueño de la memoria:
 *   El llamador es dueño de la celda devuelta. Es una estructura
 *   transitoria: no queda colgada del AST final (ver attachChildren).
 */
NodeList *newNodeList(NodeAST *node, NodeList *next) {
    NodeList *list = malloc(sizeof(NodeList));
    // Por si se queda sin memoria
    if (list == NULL) {
        return NULL;
    }

    list->node = node;
    list->next = next;

    return list;
}

/*
 * newLiteralNode
 * --------------
 * Crea un nodo CONSTANT_NODE para un literal (numerico, booleano o
 * de punto flotante) encontrado en una expresion.
 *
 * Parametros:
 *   type  - tipo semantico del literal (TYPE_INT, TYPE_BOOL o
 *           TYPE_FLOAT).
 *   value - valor del literal como cadena, o NULL si no se conserva
 *           el valor textual.
 *
 * Devuelve:
 *   Puntero a un NodeAST de tipo CONSTANT_NODE con su campo type ya
 *   fijado, o NULL si no se pudo reservar memoria.
 *
 * Dueño de la memoria:
 *   El llamador es dueño del NodeAST devuelto (y del Symbol interno
 *   que queda colgado de node->symbol).
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

    // Por si no se pudo crear el nodo debido a falta de memoria (newNode retorno NULL)
    if (node == NULL) {
        freeSymbol(symbol);
        return NULL;
    }

    node->type = type;

    return node;
}

/*
 * attachChildren
 * --------------
 * Vuelca una NodeList (acumulada durante el parsing) al arreglo
 * children[] de un NodeAST ya existente, fijando tambien childCount.
 *
 * Parametros:
 *   parent - nodo al que se le asignan los children. Se espera que
 *            parent->children sea NULL antes de llamar a esta funcion.
 *   list   - lista de nodos hijos, en el orden en que deben quedar en
 *            children[]. Puede ser NULL (cero hijos).
 *
 * Efecto:
 *   Modifica parent->children y parent->childCount in-place.
 *
 * Dueño de la memoria:
 *   Las celdas de list dejan de ser necesarias una vez volcado su
 *   contenido a children[], y se liberan aqui mismo
 */
void attachChildren(NodeAST *parent, NodeList *list) {
    // Contar nodos en lista
    int count = 0;
    for (NodeList *current = list; current != NULL; current = current->next) {
        count++;
    }

    // No hay hijos (list era NULL)
    if (count == 0) {
        parent->children = NULL;
        parent->childCount = 0;
        return;
    }

    /* Reservamos el arreglo children[] de una sola vez, ya con el
     * tamaño final (count). Es un array de punteros a NodeAST, no
     * de NodeAST completos: cada slot apunta al mismo nodo que ya
     * estaba colgado en la NodeList, no se copia nada. */
    NodeAST **children = malloc(count * sizeof(NodeAST *));
    // Por si se queda sin memoria. No tocamos parent porque la precondicion era que sea NULL
    if (children == NULL) {
        return;
    }

    /* Recorremos la lista de nuevo, esta vez volcando
     * cada nodo al arreglo y liberando la celda de NodeList que ya
     * cumplio su funcion (es una estructura transitoria, no forma
     * parte del AST final; el NodeAST que contenia sigue vivo,
     * ahora referenciado desde children[]). */
    int i = 0;
    NodeList *current = list;
    while (current != NULL) {
        children[i] = current->node;
        i++;

        // Guardamos el puntero a la celda actual antes de avanzar
        NodeList *toFree = current;
        current = current->next;
        free(toFree);
    }

    parent->children = children;
    parent->childCount = count;
}

/*
 * mergeNodeLists
 * --------------
 * Concatena dos NodeList en una sola, uniendo list2 al final de list1.
 * Se usa para combinar listas de hijos que se arman por separado
 * durante el parsing (por ejemplo, declaraciones de variables y
 * sentencias dentro de un mismo Block) en una unica lista antes de
 * volcarla con attachChildren.
 *
 * Parametros:
 *   list1 - primera lista (o NULL).
 *   list2 - segunda lista, que queda enganchada al final de list1
 *           (o NULL).
 *
 * Devuelve:
 *   Puntero a la lista resultante. Si alguna de las dos es NULL,
 *   devuelve la otra tal cual (no reserva memoria nueva).
 *
 * Dueño de la memoria:
 *   No reserva celdas nuevas; reutiliza las de list1 y list2,
 *   modificando el ultimo next de list1 para que apunte a list2.
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
 * ----------------------------
 * Desarma el azucar sintactico de una declaracion con varios
 * identificadores en una sola linea (por ejemplo "int x, y, z;") en
 * una lista de declaraciones unitarias equivalentes, una por cada
 * identificador, todas con el mismo Type.
 *
 * Parametros:
 *   dataType    - nodo Type (ya creado) compartido por todas las
 *                 declaraciones generadas.
 *   identifiers - lista de nodos ID_NODE, uno por cada identificador
 *                 declarado.
 *
 * Devuelve:
 *   Lista de nodos VARIABLE_DECLARATION_NODE, uno por cada elemento
 *   de identifiers, listos para colgarse como hijos de Program o de
 *   un Block via attachChildren.
 *
 * Dueño de la memoria:
 *   El llamador es dueño de la lista devuelta y de cada
 *   VARIABLE_DECLARATION_NODE que contiene. dataType queda
 *   referenciado (no copiado) desde cada declaracion generada: los
 *   distintos VARIABLE_DECLARATION_NODE comparten el mismo puntero a
 *   dataType, no tienen cada uno su propia copia.
 */
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