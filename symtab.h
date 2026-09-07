#ifndef SYMTAB_H
#define SYMTAB_H

#define MAX_SYMBOLS 256

// Códigos de estado para las operaciones
#define SYMTAB_SUCCESS 0
#define SYMTAB_ERR_NOT_FOUND 1
#define SYMTAB_ERR_EXISTS 2
#define SYMTAB_ERR_UNINITIALIZED 3

// Estructura de un símbolo
typedef struct {
    char* name;
    int type;           // 0 para INT, 1 para BOOL (según tu gramática)
    int value;          // El valor numérico actual
    int initialized;    // 0 si solo fue declarada, 1 si ya se le asignó un valor
} Symbol;

// Funciones principales
void init_symtab(void);
int insert_symbol(const char* name, int type);
int update_symbol(const char* name, int value);
int find_symbol(const char* name, int* out_value);
void free_symtab(void);

#endif // SYMTAB_H