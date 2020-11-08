#include <fcntl.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
// -------------------------------

#define MAX_LENGTH 100   // Maximum length command
#define MAX_COM 10       // Max number of command
#define MAX_COM_PIPED 2  // Max number of command using pipe
// ------------------------

void clear();
void print_info();
int take_input(char* inputString);
int process_string(char* str, char** args, char** args_piped, int* bgFlag, int* redirectFlag, char** file_name);
int exec_built_in(char** args);
void parseArgs(char* str, char** args, int* bgFlag);
int process_pipe(char* str, char** strpiped);
int process_redirect(char** args, char** file_name);
void exec_simple(char** args, int* bgFlag);
void exec_redirect(char** args, int* redirectFlag, char* file_name);
void exec_piped(char** args, char** args_piped);
int handle_last_cmd(char* str, const char* last_cmd);
// ------------------------------

int main(void) {
    char inputString[MAX_LENGTH];
    char* args[MAX_LENGTH / 2 + 1];
    char* args_piped[MAX_LENGTH / 2 + 1];
    char* file_name;
    int execFlag = 0;
    char last_cmd[MAX_LENGTH] = "";
    int bgFlag = 0;        // flag to know if it's a background command
    int redirectFlag = 0;  // return 1 if have >, return 2 if have <, return 0 if dont have any

    clear();

    while (1) {
        bgFlag = 0;
        // print info before shell line
        print_info();

        // take input
        if (take_input(inputString))
            continue;

        // handle last command, return 0 if no command in history
        if (handle_last_cmd(inputString, last_cmd) == 0)
            continue;

        // Save last command
        strncpy(last_cmd, inputString, MAX_LENGTH);

        // process
        execFlag = process_string(inputString, args, args_piped, &bgFlag, &redirectFlag, &file_name);
        /* execFlag return values:
        - 0 if no command or built-in command
        - 1 if it's a simple command
        - 2 if it's a piped command
        */

        // execute
        if (execFlag == 1 && redirectFlag == 0)
            exec_simple(args, &bgFlag);
        if (execFlag == 1 && redirectFlag != 0)
            exec_redirect(args, &redirectFlag, file_name);
        if (execFlag == 2)
            exec_piped(args, args_piped);

        // save last command
    }
    return 0;
}

// clear the shell using escape sequences
void clear() {
    printf("\033[H\033[J");
}

void print_info() {
    // get current user
    char* username = getenv("USER");
    printf("[@%s]:", username);

    // get current directory
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));

    // get user directory
    char uwd[50] = "/home/";
    strcat(uwd, username);

    // replace user directory with prefix '~'
    if (strstr(cwd, uwd)) {
        strcpy(&cwd[0], "~");
        strcpy(&cwd[1], &cwd[strlen(uwd)]);
    }

    // print shell info line
    printf("%s", cwd);
    // flush the buffer
    fflush(stdout);
}

int handle_last_cmd(char* str, const char* last_cmd) {
    if (strcmp(str, "!!") == 0) {
        if (strcmp(last_cmd, "") == 0) {
            printf("No command in history!\n");
            return 0;
        } else
            strcpy(str, last_cmd);
    }
    return 1;
}

void exec_simple(char** args, int* bgFlag) {
    // Forking a child
    pid_t child = fork();

    if (child == -1) {
        printf("\nFailed forking child..");
        return;
    } else if (child == 0) {
        // child running
        pid_t grandChild = fork();

        if (grandChild == 0) {
            // grand child running
            if (execvp(args[0], args) < 0) {
                printf("Could not execute command..\n");
            }
            exit(0);
        }
        wait(NULL);  //child must wait for grandChild
        exit(0);

    } else {
        // check if it's a background cmd
        if (*bgFlag == 0) {
            // double wait here because last child have not terminated
            wait(NULL);
            wait(NULL);
        }
    }
    return;
}

void exec_redirect(char** args, int* redirectFlag, char* file_name) {
    // Forking a child
    pid_t child = fork();

    if (child == -1) {
        printf("\nFailed forking child..");
        return;
    } else if (child == 0) {
        if (*redirectFlag == 1) {
            int fd = open(file_name, O_WRONLY | O_CREAT | O_TRUNC);
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }
        if (*redirectFlag == 2) {
            int fd = open(file_name, O_RDONLY);
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
        // execute cmd
        if (execvp(args[0], args) < 0) {
            printf("Could not execute redirect command..\n");
            exit(0);
        }
    }

    wait(NULL);

    return;
}

void exec_piped(char** args, char** args_piped) {
    // 0 is read end, 1 is write end
    int pipefd[2];
    pid_t p1, p2;

    if (pipe(pipefd) < 0) {
        printf("\nPipe could not be initialized");
        return;
    }
    p1 = fork();
    if (p1 < 0) {
        printf("\nCould not fork");
        return;
    }

    if (p1 == 0) {
        // Child 1 executing..
        // It only needs to write at the write end
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        if (execvp(args[0], args) < 0) {
            printf("Could not execute command 1..\n");
            exit(0);
        }
    } else {
        // Parent executing
        p2 = fork();

        if (p2 < 0) {
            printf("\nCould not fork");
            return;
        }

        // Child 2 executing..
        // It only needs to read at the read end
        if (p2 == 0) {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            if (execvp(args_piped[0], args_piped) < 0) {
                printf("Could not execute command 2..\n");
                exit(0);
            }
        } else {
            // parent executing, waiting for two children
            wait(NULL);
        }
        wait(NULL);
    }

    return;
}

int take_input(char* inputString) {
    char* buf;

    buf = readline("$ ");
    if (strlen(buf) != 0) {
        // add command to history
        add_history(buf);
        strcpy(inputString, buf);
        // free buffer after used
        free(buf);
        return 0;
    } else {
        return 1;
    }
}

int exec_built_in(char** args) {
    int NoOfBuiltIns = 4, i, switchBuiltIns = 0;
    char* ListOfBuiltIns[NoOfBuiltIns];

    ListOfBuiltIns[0] = "exit";
    ListOfBuiltIns[1] = "cd";
    ListOfBuiltIns[2] = "help";

    for (i = 0; i < NoOfBuiltIns; i++) {
        if (strcmp(args[0], ListOfBuiltIns[i]) == 0) {
            switchBuiltIns = i + 1;
            break;
        }
    }

    switch (switchBuiltIns) {
        case 1:
            printf("Goodbye!\n");
            exit(0);
        case 2:
            chdir(args[1]);
            return 1;
        case 3:
            puts(
                "\n***SIMPLE SHELL HELP MENU***"
                "\nCopyright @GondaKid and @AI005"
                "\nList of implemented Functionalities: "
                "\n>1. Execute all the External commands (ls, clear, vi etc.)"
                "\n>2. Implement Internal commands: cd, pwd"
                "\n>3. Redirection operators: STDIN, STDOUT, STDERR (>>,>,<<,<,2>)"
                "\n>4. Support for history command browse (using arrow keys) and '!!' operator"
                "\n>5. Pipes"
                "\n>Please use it with reponsibility!!!");
            return 1;
        default:
            break;
    }

    return 0;
}

// parsing command tokens
void parseArgs(char* str, char** args, int* bgFlag) {
    int i;
    for (i = 0; i < MAX_COM; i++) {
        args[i] = strsep(&str, " ");

        if (args[i] == NULL)
            break;
        if (strlen(args[i]) == 0)
            i--;
    }

    if (strcmp(args[i - 1], "&") == 0) {
        args[i - 1] = NULL;
        *bgFlag = 1;
    }
}

// finding piped in string and parsed it in array
int process_pipe(char* str, char** strpiped) {
    int i;
    for (i = 0; i < 2; i++) {
        strpiped[i] = strsep(&str, "|");
        if (strpiped[i] == NULL)
            break;
    }

    if (strpiped[1] == NULL)
        return 0;  // returns zero if no pipe is found.
    else {
        return 1;
    }
}

int process_redirect(char** args, char** file_name) {
    int i;
    int j;
    int result = 0;
    int redirected = 0;

    for (i = 0; args[i] != NULL; i++) {
        // Look for the >
        if (args[i][0] == '>') {
            result = 1;
            redirected = 1;
            break;
        } else if (args[i][0] == '<') {
            result = 2;
            redirected = 1;
            break;
        }
    }

    if (redirected) {
        // Read the filename
        if (args[i + 1] != NULL) {
            *file_name = args[i + 1];
        } else {
            return -1;
        }

        // Cut out the rest, just left the command in args
        args[i] = 0x0;
    }

    return result;
}

int process_string(char* str, char** args, char** args_piped, int* bgFlag, int* redirectFlag, char** file_name) {
    char* piped_cmd[MAX_COM_PIPED];
    int piped = 0;

    // args from piped cmd to 2 different simple cmds
    piped = process_pipe(str, piped_cmd);

    if (piped) {
        parseArgs(piped_cmd[0], args, bgFlag);
        parseArgs(piped_cmd[1], args_piped, bgFlag);
    } else {
        parseArgs(str, args, bgFlag);
        *redirectFlag = process_redirect(args, file_name);
    }

    if (exec_built_in(args))
        return 0;
    else
        return 1 + piped;
}
