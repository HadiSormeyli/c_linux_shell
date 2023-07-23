#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>

#define AUTHORS "HADI"
#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64
#define MAX_BACKGROUND_PROCESSES 10

struct background_process {
    pid_t pid;
    char command[MAX_COMMAND_LENGTH];
};

struct background_process background_processes[MAX_BACKGROUND_PROCESSES];
int num_background_processes = 0;

pthread_mutex_t mutex;

void* check_background_process(void* arg) {
    pthread_mutex_lock(&mutex);

    while(1) {

         for(int i = 0;i < num_background_processes;i++) {
                int status = check_process_status(background_processes[i].pid);

                if(status == 0) {
                    printf("\nChild process with PID %d exited with status: %d\n", background_processes[i].pid, WEXITSTATUS(status));
                    remove_background_process(i);
                }
            }
    }
    pthread_mutex_unlock(&mutex);

    return NULL;
}

void print_background_processes() {
    printf("Background Processes:\n");
    for (int i = 0; i < num_background_processes; i++) {
        printf("[%d] %d %s\n", i + 1, background_processes[i].pid, background_processes[i].command);
    }
}

void add_background_process(pid_t pid, char *command) {
    if (num_background_processes < MAX_BACKGROUND_PROCESSES) {
        background_processes[num_background_processes].pid = pid;
        strncpy(background_processes[num_background_processes].command, command, MAX_COMMAND_LENGTH);
        num_background_processes++;
    } else {
        printf("Maximum background processes reached.\n");
    }
}

void remove_background_process(int index) {
    for (int i = index; i < num_background_processes - 1; i++) {
        background_processes[i] = background_processes[i + 1];
    }
    num_background_processes--;
}

int check_process_status(int p_id) {
    int status;
    waitpid(p_id, &status, 0);
    if (WIFEXITED(status)) {
        return 0;
    } else if (WIFSIGNALED(status)) {
        return 1;
    }
    return 0;
}

char* get_output_file(char* command) {
    char *output_file = NULL;
    char *output_symbol = strchr(command, '>');
    if (output_symbol != NULL) {
        output_symbol++;
        if (*output_symbol == ' ') {
            output_symbol++;
        }
        output_file = strtok(output_symbol, " \n");

        command = strtok(command, ">");
        return output_file;
    }
    return NULL;
}

int exec(char *command, int background) {
    char* output_file = get_output_file(command);

    char *token = strtok(command, " \n");
    char *args[MAX_ARGS];
    int arg_count = 0;

    while (token != NULL) {
        args[arg_count++] = token;
        token = strtok(NULL, " \n");
    }

    args[arg_count] = NULL;

    pid_t child_pid = fork();

    if(child_pid < 0) {
        perror("Fork failed");
        return -1;
    }

    if(child_pid == 0) {
        if (output_file != NULL) {
            FILE *file = freopen(output_file, "a", stdout);
            if (file == NULL) {
                perror("Failed to redirect output");
                exit(EXIT_FAILURE);
            }
            printf("%s", stdout);
        } else if(background) {
            freopen("/dev/null", "w", stdout);
            freopen("/dev/null", "w", stderr);
        }

        execvp(args[0], args) ;
        perror("Command execution failed");
        exit(0);
    }

    if (background) {
        printf("Background process started with PID %d\n", child_pid);
        add_background_process(child_pid, command);
    } else {
        int status = check_process_status(child_pid);
        if (status == 0) {
            printf("Child process with PID %d exited with status: %d\n", child_pid, WEXITSTATUS(status));
        } else if (status == 1) {
            printf("Child process with PID %d was terminated by signal: %d\n", child_pid, WTERMSIG(status));
        }
    }

    return 0;
}

void global_usage(char* command) {
    char des[100];
    snprintf(des, sizeof(des), "IMCSH Version 1.1 created by <%s>\n", AUTHORS);
    char* output_file = get_output_file(command);

    if (output_file != NULL) {
        FILE *file = fopen(output_file, "a");
        if (file == NULL) {
            perror("Failed to redirect output");
            exit(EXIT_FAILURE);
        }

        fputs(des, file);
        fclose(file);
    } else
        printf("%s", des);
}

int main() {

    pthread_t thread;
    pthread_mutex_init(&mutex, NULL);
    pthread_create(&thread, NULL, check_background_process, NULL);
    pthread_mutex_lock(&mutex);
    pthread_mutex_unlock(&mutex);


    char command[MAX_COMMAND_LENGTH];
    int running = 1;

    while (running) {


        printf("%s> ", AUTHORS);
        fgets(command, sizeof(command), stdin);

        command[strcspn(command, "\n")] = '\0';

        if (strcmp(command, "quit") == 0) {
            if (num_background_processes > 0) {
                printf("The following processes are running, are you sure you want to quit? [Y/n]\n");

                print_background_processes();
                char response[2];
                fgets(response, sizeof(response), stdin);
                if (response[0] == 'Y') {
                    running = 0;
                    for (int i = 0; i < num_background_processes; i++) {
                        kill(background_processes[i].pid, SIGTERM);
                    }
                }else if(response[0] == 'n') {
                    continue;
                }
            } else {
                running = 0;
            }
        } else if (strncmp(command, "globalusage", 11) == 0) {
            global_usage(command);
        } else if (strncmp(command, "exec", 4) == 0) {
            int background = 0;

            if (command[strlen(command) - 1] == '&') {
                background = 1;
                command[strlen(command) - 1] = '\0';
            }

            exec(command + 5, background);
        } else {
            printf("Unrecognized command: %s\n", command);

        }
    }

    return 0;
}
