#include <stdio.h>
#include "parser.h"




int main(void) {
    while (1) {
        tline *line= tokenize("hola");
        char buf[512];
        printf("msh> ");
        fgets(buf,512,stdin);
        if (fgets(buf,512,stdin) == NULL) {
            printf("ERROR, se ha producido un error");
            break;
        }

    }







    return 0;
}
