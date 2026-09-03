#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unity/unity.h"
#include "../ast.h"

extern FILE *yyin;
extern int yyparse(void);
extern void yyrestart(FILE *input_file);

void setUp(void) {
}

void tearDown(void) {
}

int parse_string(const char* code) {
    FILE* original_stderr = stderr;
    stderr = fopen("/dev/null", "w");
    FILE* original_stdout = stdout;
    stdout = fopen("/dev/null", "w");

    yyin = fmemopen((void*)code, strlen(code), "r");
    if (!yyin) {
        TEST_FAIL_MESSAGE("fmemopen failed");
        return -1;
    }
    
    yyrestart(yyin);
    int result = yyparse();
    fclose(yyin);
    
    fclose(stderr);
    stderr = original_stderr;
    fclose(stdout);
    stdout = original_stdout;
    
    return result;
}

void test_Valido_TodoOk(void) {
    int res = parse_string("void main() { int x; x = 1; }");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, res, "El parseo deberia ser exitoso");
}

void test_Valido_Booleanos(void) {
    int res = parse_string("bool main() { bool a; a = true; return a; }");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, res, "El parseo de booleanos deberia ser exitoso");
}

void test_Valido_Math(void) {
    int res = parse_string("void main() { int p; p = (2 + 3) * 4; }");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, res, "El parseo de expresiones matematicas deberia ser exitoso");
}

void test_Valido_SinCodigo(void) {
    int res = parse_string("void main(){}");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, res, "El parseo de una funcion vacia deberia ser exitoso");
}

void test_Invalido_AsignacionEnDeclaracion(void) {
    int res = parse_string("void main() { int x = 1; }");
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(0, res, "Deberia fallar por asignacion durante la declaracion");
}

void test_Invalido_FaltaPuntoYComa(void) {
    int res = parse_string("void main() { int x }");
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(0, res, "Deberia fallar por falta de punto y coma");
}

void test_Invalido_SinFuncion(void) {
    int res = parse_string("int x; x=1;");
    TEST_ASSERT_NOT_EQUAL_INT_MESSAGE(0, res, "Deberia fallar porque no hay funcion main");
}

int main(void) {
    UNITY_BEGIN();
    
    RUN_TEST(test_Valido_TodoOk);
    RUN_TEST(test_Valido_Booleanos);
    RUN_TEST(test_Valido_Math);
    RUN_TEST(test_Valido_SinCodigo);
    RUN_TEST(test_Invalido_AsignacionEnDeclaracion);
    RUN_TEST(test_Invalido_FaltaPuntoYComa);
    RUN_TEST(test_Invalido_SinFuncion);
    
    return UNITY_END();
}
