#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "parser.h"
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

// Definimos la estructura para el comando jobs
typedef struct job{
    int job_id; // Id del trabajo
    pid_t pid; // Id del proceso
    char *status; // Estado del proceso (Running, stopped, finished)
    char *command;
    struct job *next; // Puntero al siguiente trabajo

}t_Job;
pid_t act;
pid_t *pids = NULL;
int i;   // Variable auxiliar

// Creamos la lista de trabajos
t_Job *jobs_list = NULL;

void cleanJobs(int sig) {
    pid_t pid;
    int status;

    while((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        t_Job *current=jobs_list;
        t_Job *prev=NULL;
        while (current != NULL) {
            if (current->pid == pid) {

                if (prev == NULL) {
                    jobs_list = current->next;
                } else {
                    prev->next = current->next;
                }
                printf("[%d] done \t%s",current->job_id,current->command);
                free(current->command);
                free(current->status);
                free(current);
                break;
            }
            else {
                prev = current;
                current = current->next;
            }

        }
    }
}


// Funcion para mostrar los jobs
void printJobs() {
    if (jobs_list != NULL) {
        t_Job *current = jobs_list;
        while (current != NULL)
        {
            printf("[%d] %s \t%s\n",current->job_id,current->status,current->command );
            current = current->next;
        }
    }
}

// Funcion para agregar un proceso a la lista de jobs
void addJob(tline *line, pid_t pod) {
    static int current = 1;
    t_Job *newJob = (t_Job *)malloc(sizeof(t_Job));
    newJob->job_id = current;
    newJob->pid = pod;
    newJob->status = strdup("Running");

    // Construir el comando completo como una cadena
    int total_length = 0;
    for (int i = 0; i < line->ncommands; i++) {
        tcommand cmd = line->commands[i];
        total_length += strlen(cmd.filename) + 1; // Nombre del comando + espacio
        for (int j = 0; j < cmd.argc; j++) {
            total_length += strlen(cmd.argv[j]) + 1; // Argumentos + espacio
        }
    }

    // Reservar memoria para la cadena completa
    char *command = (char *)malloc(total_length + 1);
    command[0] = '\0'; // Inicializar la cadena vacía

    // Concatenar cada comando y sus argumentos
    for (int i = 0; i < line->ncommands; i++) {
        tcommand cmd = line->commands[i];
        strcat(command, cmd.filename); // Añadir el nombre del comando
        strcat(command, " "); // Añadir un espacio
        for (int j = 0; j < cmd.argc; j++) {
            strcat(command, cmd.argv[j]); // Añadir cada argumento
            strcat(command, " "); // Añadir un espacio
        }
        if (i < line->ncommands - 1) {
            strcat(command, "| "); // Añadir un símbolo de pipe si no es el último comando
        }
    }
    newJob->command = command;
    newJob->next = jobs_list;
    jobs_list = newJob;
    current++;
}


void executeBG(tline *line) {
    pid_t *process_pids = malloc(sizeof(pid_t) * line->ncommands);
    int fd[line->ncommands - 1][2]; // Creamos una matriz de n-1 X 2 para los descriptores de fichero

    for (i = 0; i < line->ncommands - 1; i++) {
        if (pipe(fd[i]) == -1) {
            perror("Error al crear el pipe");
            exit(1);
        }
    }

    for (i = 0; i < line->ncommands; i++) {
        act = fork();
        if (act == -1) {
            perror("Error al crear hijo");
            free(process_pids);
            exit(1);
        }

        if (act == 0) { // Proceso hijo
            signal(SIGINT, SIG_DFL);

            if (line->redirect_input != NULL) {
                int in_file = open(line->redirect_input, O_RDONLY);
                if (in_file == -1) {
                    perror(line->redirect_input);
                    exit(1);
                }
                dup2(in_file, STDIN_FILENO);
                close(in_file);
            }
            if (line->redirect_output != NULL) {
                int out_file = open(line->redirect_output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (out_file == -1) {
                    perror(line->redirect_output);
                    exit(1);
                }
                dup2(out_file, STDOUT_FILENO);
                close(out_file);
            }

            if (line->ncommands > 1) {
                if (i == 0) { // Primer comando
                    close(fd[0][0]);
                    dup2(fd[0][1], STDOUT_FILENO);
                    close(fd[0][1]);
                } else if (i == line->ncommands - 1) { // Último comando
                    close(fd[i - 1][1]);
                    dup2(fd[i - 1][0], STDIN_FILENO);
                    close(fd[i - 1][0]);
                } else { // Comandos intermedios
                    close(fd[i - 1][1]);
                    close(fd[i][0]);
                    dup2(fd[i - 1][0], STDIN_FILENO);
                    dup2(fd[i][1], STDOUT_FILENO);
                    close(fd[i][1]);
                    close(fd[i - 1][0]);
                }
            }

            execvp(line->commands[i].argv[0], line->commands[i].argv);
            perror("Error al ejecutar el comando");
            exit(1);
        }

        if (act > 0) {
            process_pids[i] = act; // Guardar el PID del hijo
        }
    }

    for (i = 0; i < line->ncommands - 1; i++) {
        close(fd[i][0]);
        close(fd[i][1]);
    }

    addJob(line, process_pids[0]);
    free(process_pids);
}




void fg(int id) {

    t_Job *current = jobs_list;
    t_Job *previous = NULL;
    if (id == -1) {
        if (current == NULL) {
            fprintf(stderr, "fg: No hay jobs en segundo plano\n");
            return;
        }
    }
    else {
        while (current != NULL) {
            if (current->job_id == id) {
                break;
            }
            previous = current;
            current = current->next;
        }
        if (current == NULL) {
            fprintf(stderr, "fg: No existe el job con ID %d\n", id);
            return;
        }
    }
    kill(current->pid,SIGCONT);
    waitpid(current->pid,NULL,0);
    if (previous == NULL) {
        jobs_list = current->next;
    }
    else {
        previous->next=current->next;
    }
    free(current->command);
    free(current->status);
    free(current);
}




// Funcion manejador para la señal Ctr+C
void handle_sig(int sig) {
    if (pids != NULL) // Si el array tiene hijos, mandamos la señal para que terminen su ejecucion
    {
        for (int k =0;pids[k] != 0;k++)
        {
            kill(pids[k],SIGINT);
        }
    }
}


void executeCD(char *directorio) {
    if (strcmp(directorio,"~")== 0 || strcmp(directorio,"$HOME") == 0) { // Si la entrada es ~ o es $HOME, el directorio es home
        directorio =getenv("HOME");;
    }else if (directorio[0] == '~') {
        char *dir = getenv("HOME"); // Si la entrada empieza por ~ y luego seguido el nombre de otro directorio
        strcat(dir,directorio+1);
        directorio = dir;
    }
    if (chdir(directorio) == -1) { // Si falla el cambiar de directorio

        fprintf(stderr, "Error: No se ha encontrado el directorio '%s'\n", directorio);
        fflush(stderr);
    }

}


void executeLine(tline *line){
    if (line->background == 1)
    {
        executeBG(line);
        return;
    }

    if (strcmp(line->commands[0].argv[0], "jobs") == 0) {
        printJobs();
        return;
    }
    if (strcmp(line->commands[0].argv[0], "fg") == 0) {
        int job_id = -1;
        if (line->commands[0].argc > 1) {
            job_id = atoi(line->commands[0].argv[1]);
        }
        fg(job_id);
        return;
    }

    if (strcmp(line->commands[0].argv[0],"cd")==0) // Verificar si el comando es cd
    {
        char *nuevoDirectorio = line->commands[0].argv[1];
        if (nuevoDirectorio == NULL) // Si no introduce nada el usuario, el directorio es home
        {
            nuevoDirectorio= "~";
        }
        executeCD(nuevoDirectorio);
        return ;

    }


    int fd[line->ncommands-1][2]; // Creamos una matriz de n-1 X 2 para los descriptores de fichero
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
            signal(SIGINT,SIG_DFL); // Ver si funciona

            if (line->redirect_input!=NULL) {
                int in_file = open(line->redirect_input,O_RDONLY);
                if (in_file==-1) {
                    fprintf(stderr,"%s",line->redirect_input);
                    exit(1);
                }
                dup2(in_file,STDIN_FILENO);
                close(in_file);
            }
            if (line->redirect_output!=NULL) {
                int in_file = open(line->redirect_output,O_WRONLY | O_CREAT | O_CREAT, 0644);
                if (in_file==-1) {
                    fprintf(stderr,"%s",line->redirect_output);
                    exit(1);
                }
                dup2(in_file,STDOUT_FILENO);
                close(in_file);
            }
            if (line->redirect_error!=NULL) {
                int in_file = open(line->redirect_error,O_WRONLY | O_CREAT | O_TRUNC,0644);
                if (in_file==-1) {
                    fprintf(stderr,"%s",line->redirect_error);
                    exit(1);
                }
                dup2(in_file,STDERR_FILENO);
                close(in_file);
            }

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
    pids = NULL;

}

int main(void) {


    signal(SIGINT,SIG_IGN);
    signal(SIGCHLD,cleanJobs);
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