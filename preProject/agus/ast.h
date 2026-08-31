/*

Con Flex y Bison, definiamos cuales eran los tokens del programa parseado. Los tokens son los átomos
del programa, por ejemplo un número, un identificador, un operador, una palabra reservada, etc.

Es decir, una expresion como "int x = 3" tiene distintos tokens: "int", "x", "=", "3"

Eso es util para parsear el programa y validar su sintaxis.

En cambio, los AST (Abstract Syntax Tree) son una representación estructurada del programa, donde los nodos 
representan construcciones sintácticas relevantes del lenguaje en lugar de tokens individuales (por ejemplo,
una declaración completa, una sentencia completa, etc. Eso nos permite después poder definir
el orden y prioridad de las operaciones.

Entonces, definimos nodos para cada unidad "logica" del programa:
    - Un nodo raiz que es el Program Node
    - De ahí, varios nodos para cada sentencia o declaración
    - De esas sentencias, un nodo para las declaraciones de variables (completas, por ej., "int x;"), 
      un nodo para las asignaciones (completas, por ej., "x =3;"), etc.

De manera similar a como hacemos arboles sintacticos para definir la prioridad de operadores en expresiones
aritméticas o lógicas, construimos un árbol para el programa. Eso nos permite más adelante hacer un análisis
semántico y determinar si el programa es correcto (por ej., si todas las variables están definidas antes de ser
usadas, etc.), determinar el orden en el que se deben ejecutar las instrucciones, generar el código
intermedio, o el código máquina, etc.


En este archivo definimos los tipos de nodos, su estructura, y los perfiles de las funciones que usaremos en Bison para construir el AST

*/

typedef enum Node_Type {
    PROGRAM_NODE,        // Represents the root of the program (main function)
    STATEMENT_LIST_NODE, // Represents a sequence of sentences (Code block)
    DECLARATION_NODE,    // Represents a variable declaration (int x;)
    ASSIGN_NODE,         // Represents an assignment (x = 5;)
    RETURN_NODE,         // Represents a return statement (return x;)
    BINOP_NODE,          // Represents a binary operation (+, -, *)
    CONSTANT_NODE,       // Represents a literal value (number or boolean)
    ID_NODE              // Represents a variable identifier used in an expression

    // Hay algunos nodos que no hacen falta
    // No son necesarios nodos de tipo: el tipo sera un atributo de un nodo general
    // No son necesarios nodos distintos para "return ALGO" o simplemente "return": en el segundo caso el hijo del nodo sera null
    // etc.

} Node_Type;


// Esta es la estructura de un nodo cualquiera
// Cada campo de este struct permite almacenar la información necesaria para cada tipo de nodo
// El union significa que un ASTNode sera alguno de todos los structs incluidos en el union.
typedef struct ASTNode {
    Node_Type type;

    union {
        struct {
            struct ASTNode* statements;
        } program;

        struct {
            struct ASTNode* statement;
            struct ASTNode* next;
        } statement_list; // secuencia de sentencias

        struct {
            int data_type; // por ej., 0 para int, 1 para bool
            char* var_name;
        } declaration;
        
        struct {
            char* var_name;
            struct ASTNode* expression;
        } assignment;
        
        struct {
            struct ASTNode* expression; // puede ser NULL si es solo "return;"
        } return_stmt; // return statement

        struct {
            struct ASTNode* left;
            char op; // Usamos 'char' en lugar de 'char*' porque es solo '+', '-', '*'
            struct ASTNode* right;
        } binop; // binary operation

        struct {
            int value; // contiene el numero o el valor booleano (0 o 1)
        } constant; // numero o bool

        struct {
            char* var_name;
        } id; // variable usada dentro de una expresion

    } data; // le damos un nombre al union (como 'data') para acceder a sus campos

} ASTNode; // cerramos el struct correctamente

ASTNode* create_program_node(ASTNode* statements);
ASTNode* create_statement_list_node(ASTNode* statement, ASTNode* next);
ASTNode* create_declaration_node(int data_type, char* var_name);
ASTNode* create_assignment_node(char* var_name, ASTNode* expression);
ASTNode* create_return_node(ASTNode* expression);
ASTNode* create_binop_node(ASTNode* left, char op, ASTNode* right);
ASTNode* create_constant_node(int value);
ASTNode* create_id_node(char* var_name);
void free_ast(ASTNode* node);
