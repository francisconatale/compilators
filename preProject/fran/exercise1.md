## GRAMATICA INICIAL
p -> e
e -> e+e | e*e | (e) | numero, donde numero [0-9]+

cosas que se aceptan:
12+(4), (12+12)*(4), (12*4+2), 2

## EXTENSION DE LA GRAMATICA

restricciones para la declaracion de variables:
1. variables comienzan con letra y siguen con un numero

cosas que se aceptan:
int a;, int a123;, bool a1;, bool a;

cosas que no se aceptan:
int 1;, int 1a;, bool 1a;, bool1;

regex propuesto:
decl = (bool|int)[ \t]+[a-zA-z][a-zA-Z0-9]";"
en flex deberia devolver un token decl;

2. returns

return; return a; return true; return false; return 1+1; return 1;

regex propuesto:
ret = "return"[ \t]+ e ";"

en flex deberia devolver un token ret;

por lo tanto el lenguaje extendido viene dado por 
gramatica inicial + 
{boolean -> "true" | "false"
type -> "bool"| "int"
number -> 0 | ... | 9
chars -> (a | ... | z)
var_name -> (chars) (chars | number) +
return -> "return" (var_name|boolean|number)
decl -> bool var_name = boolean | int var_name = number+
assign -> var_name "=" (numbers|chars|boolean)+
}
