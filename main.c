#include <stdio.h>
#include <string.h>

#include "parser.h"


void executeLine(tline *line)
{
    printf(":)\n");
}

int main(void) {
    while (1) {

        char buf[512];
        printf("msh> ");


        if (fgets(buf,512,stdin) == NULL) {
            printf("ERROR, se ha producido un error");
            break;
        }
        if (strlen(buf) == 0)
        {
            continue;
        }

        tline *line = tokenize(buf);

        if (line)
        {
            executeLine(line);
        }




    }







    return 0;
}
