#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
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

void tish_parseArgs(char** cliArgs){
    char* parsedArgs[MAX_ARGS];
    char* token;
    const char delim[2] = " ";
    int argCount = 0;
    bool redIn = false;
    bool redOut = false;
    bool redErr = false;
    token = strtok(*cliArgs, delim);
    // TODO  First set all of the redirections then go through commands in separate while loop
    /* walk through other tokens */
    while(token != NULL) {
        if(redIn){
            if(freopen(token, "w", stdin) == NULL){
                fprintf(stderr, "freopen failed\n");
                break;
            }
            redIn = false;
        } else if(redOut){
            if(freopen(token, "w", stdout) == NULL){
                fprintf(stderr, "freopen failed\n");
                break;
            }
            redOut = false;
        } else if(redErr){
            if(freopen(token, "w", stderr) == NULL){
                fprintf(stderr, "freopen failed\n");
                break;
            }
        }
         else if(token[0] == '$'){
             // TODO  Add 'echo' check
            char* skipPointer = token + sizeof(char)*1;
            char* envContainer = malloc(strlen(skipPointer) - 1);
            if(envContainer == NULL){
                fprintf(stderr, "malloc failed\n");
                break;
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
                token = strtok(NULL, delim);
                printf("CD TOKEN : %s", token);
                tish_cd(token);
            } else if(strcmp(token, "ls")){
                tish_ls();
            } else if(strcmp(token, "pwd")){
                tish_pwd();
            } else {
                printf("Other stuff");
            }
            parsedArgs[argCount++] = token;
        }
        token = strtok(NULL, delim);
    }

    // for(int i = 0; i < argCount; i++){
    //     printf("what %s\n", parsedArgs[i]);
    // }
    if(argCount > 0){
        printf("%s", parsedArgs[0]);
    }
}

void tish_pwd(){
    return;
}

void tish_cd(char* destDir){
    return;
}

void tish_ls(){
    return;
}

void tish_quit(char** cliArgs){
    free(*cliArgs);
    *cliArgs = NULL;
    cliRunning = false;
    exit(0);
}