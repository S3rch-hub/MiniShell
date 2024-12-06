#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "parser.h"
#include <sys/wait.h>

pid_t act,*pids;
int i;

void executeLine(tline *line){

    int fd[line->ncommands-1][2];
    for (i=0; i< line->ncommands-1;i++) {
        if (pipe(fd[i])==-1) {
            printf("error al crear el pipe");
            exit(1);
        }
    }

    pids = malloc(sizeof(pid_t) * line->ncommands);
    if (pids == NULL) {
        perror("Error al reservar memoria");
        exit(1);
    }
    for (i=0; i<line->ncommands; i++) {
        act = fork();
        if (act==-1) {
            printf("error al crear hijo");
            free(pids);
            exit(1);
        }
        if (act==0) {
            if (line->ncommands>1) {
                if (i==0) { //primero
                    close(fd[0][0]);
                    dup2(fd[0][1],STDOUT_FILENO);
                    close(fd[0][1]);
                }
                else if (i==line->ncommands-1) { //ultimo
                    close(fd[i-1][1]);
                    dup2(fd[i-1][0],STDIN_FILENO);
                    close(fd[i-1][0]);

                }
                else { // los del medio
                    close(fd[i-1][1]);
                    close(fd[i][0]);
                    dup2(fd[i-1][0],STDIN_FILENO);
                    dup2(fd[i][1],STDOUT_FILENO);
                    close(fd[i][1]);
                    close(fd[i-1][0]);
                }
                for (int j = 0; j < line->ncommands - 1; j++) { //hay que cerrar todos los pipes en todos los hijos
                    close(fd[j][0]);
                    close(fd[j][1]);
                }
            }
            execvp(line->commands[i].argv[0],line->commands[i].argv);
            printf("Error al ejecutar el comando: %s\n",line->commands[i].argv[0]);
            exit(1);
        }
        if (act>0) {
            pids[i]=act;
        }
    }
    for (i = 0; i < line->ncommands - 1; i++) {
        close(fd[i][0]);
        close(fd[i][1]);
    }

    for (i = 0; i < line->ncommands; i++) {
        wait(NULL);
    }

    free(pids);

}

int main(void) {
    while (1) {

        char buf[1024];
        printf("msh> ");

        if (fgets(buf,1024,stdin) == NULL) {
            printf("ERROR, se ha producido un error");
            break;
        }
        tline *line = tokenize(buf);

        if (line->ncommands==0) {
            continue;
        }
        executeLine(line);

    }

    return 0;
}
