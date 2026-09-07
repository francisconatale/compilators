#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtab.h"

static Symbol symtab[MAX_SYMBOLS];
static int sym_count = 0;

void init_symtab(void) {
    sym_count = 0;
}

int insert_symbol(const char* name, int type) {
    // 1. Verificar si la variable ya existe (Error de re-declaración)
    for (int i = 0; i < sym_count; i++) {
        if (strcmp(symtab[i].name, name) == 0) {
            return SYMTAB_ERR_EXISTS;
        }
    }

    // 2. Verificar que no nos quedemos sin memoria en la tabla
    if (sym_count >= MAX_SYMBOLS) {
        fprintf(stderr, "Error Interno: Desbordamiento de la Tabla de Símbolos.\n");
        exit(1);
    }

    // 3. Insertar el nuevo símbolo
    symtab[sym_count].name = strdup(name); // Duplicamos el string para que la tabla sea dueña de la memoria
    symtab[sym_count].type = type;
    symtab[sym_count].value = 0;
    symtab[sym_count].initialized = 0;     // Nace sin valor asignado
    
    sym_count++;

    return SYMTAB_SUCCESS;
}

int update_symbol(const char* name, int value) {
    // Buscar la variable y actualizar su valor
    for (int i = 0; i < sym_count; i++) {
        if (strcmp(symtab[i].name, name) == 0) {
            symtab[i].value = value;
            symtab[i].initialized = 1; // Marcamos que ya tiene un dato válido
            return SYMTAB_SUCCESS;
        }
    }
    
    // Si terminamos el bucle y no la encontramos, es un error
    return SYMTAB_ERR_NOT_FOUND;
}

int find_symbol(const char* name, int* out_value) {
    // Buscar la variable para leer su valor
    for (int i = 0; i < sym_count; i++) {
        if (strcmp(symtab[i].name, name) == 0) {
            
            // Si existe pero no tiene valor, reportamos error de inicialización
            if (!symtab[i].initialized) {
                return SYMTAB_ERR_UNINITIALIZED;
            }
            
            // Si todo está bien, "devolvemos" el valor a través del puntero
            *out_value = symtab[i].value;
            return SYMTAB_SUCCESS;
        }
    }
    
    return SYMTAB_ERR_NOT_FOUND;
}

void free_symtab(void) {
    // Liberar la memoria de los strings que duplicamos con strdup
    for (int i = 0; i < sym_count; i++) {
        free(symtab[i].name);
    }
    sym_count = 0;
}