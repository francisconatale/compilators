#ifndef AST_H
#define AST_H

/* Tipos semanticos */
typedef enum {
    TYPE_INT,
    TYPE_BOOL,
    TYPE_FLOAT,
    TYPE_VOID
} DataType;

typedef enum {
    TYPE_NODE,
    VARIABLE_DECLARATION_NODE,
    ID_NODE,
    ASSIGNMENT_NODE,
    CONSTANT_NODE,
    RETURN_NODE,
    IF_ELSE_NODE,
    WHILE_NODE,
    BLOCK_NODE,
    METHOD_CALL_NODE
} NodeType;

typedef struct Symbol {
    char *id;
    char *value;
} Symbol;

/* NodeAST
 * left/right para nodos binarios (ASSIGNMENT_NODE). children/childCount
 * para nodos con una cantidad variable de hijos (VARIABLE_DECLARATION_NODE, ver
 * IdList en bison.y). */
typedef struct NodeAST {
    NodeType nodeType;
    DataType type;
    Symbol *symbol;
    int line;
    struct NodeAST **children;
    int childCount;
} NodeAST;

#define GET_LEFT(node)  ((node)->childCount > 0 ? (node)->children[0] : NULL)
#define GET_RIGHT(node) ((node)->childCount > 1 ? (node)->children[1] : NULL)

#define GET_CONDITION(node)  ((node)->childCount > 0 ? (node)->children[0] : NULL)
#define GET_IF_BLOCK(node)   ((node)->childCount > 1 ? (node)->children[1] : NULL)
#define GET_ELSE_BLOCK(node) ((node)->childCount > 2 ? (node)->children[2] : NULL)

typedef struct NodeList {
    NodeAST *node;
    struct NodeList *next;
} NodeList;

/* Crea un Symbol reservado dinamicamente copiando id y value. */
Symbol *newSymbol(const char *id, const char *value);

/* Crea un NodeAST. left, mid y right se empaquetan en children. */
NodeAST *newNode(NodeType nodeType, Symbol *symbol, NodeAST *left, NodeAST *mid, NodeAST *right);

/* Crea una lista enlazada transitoria para el parsing en bison.y. */
NodeList *newNodeList(NodeAST *node, NodeList *next);

/* Vuelca los nodos de NodeList al arreglo children[] de parent y libera la lista. */
void attachChildren(NodeAST *parent, NodeList *list);

/* Concatena dos NodeList sin reservar nueva memoria. */
NodeList *mergeNodeLists(NodeList *list1, NodeList *list2);

/* Desarma declaraciones multiples en unitarias (ej: int x, y; -> int x; int y;). 
 * [MODIFICADO]: Ahora recibe 'DataType type' en lugar de un NodeAST para evitar 
 * crear nodos (TYPE_NODE) innecesarios en el árbol. */
NodeList *flattenVariableDeclarations(DataType type, NodeList *identifiers);

/* Crea un CONSTANT_NODE para un literal numérico/booleano. */
NodeAST *newLiteralNode(DataType type, const char *value);

/* Libera un Symbol y sus cadenas internas. */
void freeSymbol(Symbol *symbol);

#endif /* AST_H */
