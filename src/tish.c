#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include "tish.h"

sig_atomic_t cliRunning = true;
char *cliArgs = NULL;
// TODO  Next line for '$?' support
// int lastCommandExit = 0;
// TODO  Next for line for '-t' EC
// bool timeCounting = false;

void sigint_handler(int sigNum){
    debug("%i", sigNum);
    cliRunning = false;
    return;
}

int main(int argc, char const *argv[], char *envp[]){
    signal(SIGINT, sigint_handler);
    cliArgs = NULL;
    size_t cliLen;
    int charCount;
    int commandStatus;
    debug("%s", "Huh");
    while(cliRunning){
        printf("%s", "tish> ");
        fflush(stdout);
        charCount = getline(&cliArgs, &cliLen, stdin);
        if(!cliRunning){
            break;
        }
        if(charCount != -1){
            if(!strncmp(cliArgs, "exit", 4)){
                tish_quit(&cliArgs);
            } else {
                tish_parseArgs(&cliArgs);
            }
        } else {
            printf("No \n");
            cliRunning = false;
        }
    }

    if(cliArgs != NULL){
        free(cliArgs);
        cliArgs = NULL;
    }

    return 0;
}

int tish_parseArgs(char** cliArgs){
    char* parsedArgs[MAX_ARGS];
    char* token;
    char* fileNameArr;
    const char delim[2] = " ";
    int argCount = 0;
    int totalArgs = 0;
    int oldStdIn = dup(STDIN_FILENO);
    int oldStdOut = dup(STDOUT_FILENO);
    int oldStdErr = dup(STDERR_FILENO);
    FILE* fdPointer = NULL;
    int newFd = 0;
    bool redIn = false;
    bool redOut = false;
    bool redErr = false;
    bool cdCmd = false, pwdCmd = false;
    token = strtok(*cliArgs, delim);
    // TODO  First set all of the redirections then go through commands in separate while loop
    /* walk through other tokens */
    // TODO  Have a count first? To know which null terminator take out from last
    while(token != NULL) {
        totalArgs += 1;
        if(redIn){
            size_t tokenLen = strlen(token);
            debug("%s", token);
            if(access(token, F_OK) != 0 ) {
                token[tokenLen - 1] = '\0';
                if(access(token, F_OK) != 0){
                    fprintf(stderr, "File given doesn't exist, please try again.\n");
                    return 1;
                }
            }
            fdPointer = fopen(token, "r");
            if(fdPointer == NULL){
                fprintf(stderr, "open failed\n");
                return 1;
            }
            newFd = fileno(fdPointer);
            if(dup2(newFd, STDIN_FILENO) == -1){
                fprintf(stderr, "File redirection failed, please try again.\n");
            }
            close(newFd);
            redIn = false;
        } else if(redOut){
            token[strlen(token) - 1] = '\0';
            newFd = open(token, O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if(newFd == -1){
                fprintf(stderr, "open failed\n");
                return 1;
            }
            if(dup2(newFd, STDOUT_FILENO) == -1){
                fprintf(stderr, "File redirection failed, please try again.\n");
            }
            close(newFd);
            redOut = false;
        } else if(redErr){
            newFd = open(token, O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if(newFd == -1){
                fprintf(stderr, "open failed\n");
                return 1;
            }
            if(dup2(newFd, STDERR_FILENO) == -1){
                fprintf(stderr, "File redirection failed, please try again.\n");
            }
            close(newFd);
            redErr = false;
        }
         else if(token[0] == '$'){
             // TODO  Add 'echo' check
            char* skipPointer = token + sizeof(char)*1;
            char* envContainer = malloc(strlen(skipPointer) - 1);
            if(envContainer == NULL){
                fprintf(stderr, "malloc failed\n");
                return 1;
            }
            strncpy(envContainer, skipPointer,strlen(skipPointer) - 1);
            printf("%s\n", getenv(envContainer));
            free(envContainer);
        } else if(strcmp(token, "<") == 0){
            redIn = true;
        } else if(strcmp(token, ">") == 0){
            redOut = true;
        } else if(strcmp(token, "2>") == 0){
            redErr = true;
        } else {
            if(strcmp(token, "cd") == 0){
                cdCmd = true;
            } else if(strncmp(token, "pwd", 3) == 0){
                pwdCmd = true;
            }
            parsedArgs[argCount++] = token;
        }
        token = strtok(NULL, delim);
    }

    size_t lastArgLen = strlen(parsedArgs[argCount - 1]);
    if(totalArgs == argCount){
        parsedArgs[argCount - 1][lastArgLen - 1] = '\0';
    }
    parsedArgs[argCount] = NULL;

    if(cdCmd + pwdCmd > 1) {
        fprintf(stderr, "Too many commands given, please try again. \n");
    } else {
        if(cdCmd){
            tish_cd(parsedArgs, argCount);
        } else if(pwdCmd) {
            tish_pwd(parsedArgs, argCount);
        } else {
            if(argCount == 0){
                fprintf(stderr, "Please provide the command you wish to run. \n");
            } else {
                tish_all_else(parsedArgs, argCount);
            }
        }
    }

    tish_reset_fd(oldStdIn, oldStdOut, oldStdErr);

    return 0;
}

int tish_pwd(char** curArgs, int curArgsCount){
    int curPathSz = 100;
    char curPath[curPathSz];
    if(getcwd(curPath, curPathSz) == NULL){
        fprintf(stderr, "Pwd command failed, please try again. \n");
    }
    printf("%s\n", curPath);
    return 0;
}

int tish_cd(char** curArgs, int curArgsCount){
    if(curArgsCount < 2){
        fprintf(stderr, "cd command not given a specific directory, please try again.\n");
        return 1;
    }
    char * destDirArr = malloc(50);
    debug("str : %s len: %ld \n", curArgs[1], strlen(curArgs[1]));
    strncpy(destDirArr, curArgs[1], strlen(curArgs[1]));
    if(chdir(destDirArr) == -1){
        fprintf(stderr, "cd command failed, please try again. \n");
    }
    free(destDirArr);
    return 0;
}

int tish_all_else(char** curArgs, int curArgsCount){
    pid_t childPID = fork();
    pid_t waitPID;
    int status;
    if(childPID == 0){ // Child
        if(execvp(curArgs[0], curArgs) == -1){
            fprintf(stderr, "execvp error.\n");
            exit(1);
        }
    }
    while((waitPID = wait(&status)) > 0);
    return 0;
}

int tish_reset_fd(int oldStdIn, int oldStdOut, int oldStdErr){
    dup2(oldStdIn, STDIN_FILENO);
    dup2(oldStdOut, STDOUT_FILENO);
    dup2(oldStdErr, STDERR_FILENO);
    close(oldStdIn);
    close(oldStdOut);
    close(oldStdErr);
    return 0;
}

void tish_quit(char** cliArgs){
    free(*cliArgs);
    *cliArgs = NULL;
    cliRunning = false;
    exit(0);
}