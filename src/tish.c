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
int lastCommandExit = 0;
bool debugOutput = false;
// TODO  Next for line for '-t' EC
// bool timeCounting = false;

void sigint_handler(int sigNum){
    debug("%i", sigNum);
    cliRunning = false;
    return;
}

int main(int argc, char const *argv[], char *envp[]){
    FILE* curStdIn = stdin;
    bool nonInteractive = false;
    signal(SIGINT, sigint_handler);
    for(int i = 1; i < argc; i++){
        #ifdef EXTRA_CREDIT
        if(strncmp(argv[i], "-t")){

        }
        #endif
        if(strncmp(argv[i], "-d", 2) == 0){ // Debug Option
            debugOutput = true;
        } else if(access(argv[i], F_OK) == 0){ // Non interactive option
            int newStdIn = open(argv[i], O_RDONLY, 0640);
            if(dup2(newStdIn, fileno(curStdIn)) == -1){
                fprintf(stderr, "Error ocured with non-interactive functionality.\n");
                exit(1);
            }
            printf("%s\n", "tish> ");
            nonInteractive = true;
        }
    }
    cliArgs = NULL;
    size_t cliLen;
    int charCount;
    int commandStatus;
    // debug("%s", "Huh");
    while(cliRunning){
        if(!nonInteractive){
            printf("%s", "tish> ");
        }
        fflush(stdout);
        charCount = getline(&cliArgs, &cliLen, curStdIn);
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
            cliRunning = false;
        }
    }

    if(cliArgs != NULL){
        free(cliArgs);
        cliArgs = NULL;
    }

    return 0;
}

void tish_running_cmd(char* curCmd){
    fprintf(stderr, "RUNNING: %s\n", curCmd);
}

void tish_ending_cmd(char* curCmd, int exitStatus){
    fprintf(stderr, "ENDED: %s (ret=%d)\n", curCmd, exitStatus);
}

int tish_parseArgs(char** cliArgs){
    char* parsedArgs[MAX_ARGS];
    char* token = NULL;
    char* fileNameArr = NULL;
    char* setEnvStr = NULL;
    const char delim[2] = " ";
    int argCount = 0;
    int totalArgs = 0;
    int newFd = 0;
    int oldStdIn = dup(STDIN_FILENO);
    int oldStdOut = dup(STDOUT_FILENO);
    int oldStdErr = dup(STDERR_FILENO);
    bool redIn = false, redOut = false, redErr = false;
    bool cdCmd = false, pwdCmd = false, envCmd = false, echoCmd = false, comment = false;
    token = strtok(*cliArgs, delim);

    while(token != NULL) {
        // debug("len :  %ld, str: %s", strlen(token), token);
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
            newFd = open(token, O_RDONLY, 0640);
            if(newFd == -1){
                fprintf(stderr, "stdin open failed\n");
                return 1;
            }
            if(dup2(newFd, STDIN_FILENO) == -1){
                fprintf(stderr, "File redirection failed, please try again.\n");
            }
            close(newFd);
            redIn = false;
        } else if(redOut){
            token[strlen(token) - 1] = '\0';
            // debug("FILE PASSED : %s\n", token);
            newFd = open(token, O_WRONLY | O_CREAT | O_TRUNC, 0640);
            if(newFd == -1){
                fprintf(stderr, "stdout open failed\n");
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
                fprintf(stderr, "stderr open failed\n");
                return 1;
            }
            if(dup2(newFd, STDERR_FILENO) == -1){
                fprintf(stderr, "File redirection failed, please try again.\n");
            }
            close(newFd);
            redErr = false;
        } else if(token[0] == '#'){
            comment = true;
            break; // Break out of loop and run args before command
        } else if((setEnvStr = strchr(token, '=')) != NULL){
            size_t equalToNameOffset = setEnvStr - token;
            char* nameEnv = calloc(sizeof(char), equalToNameOffset + 1);
            if(nameEnv == NULL){
                fprintf(stderr, "Error mallocing.\n");
                return 1;
            }
            strncpy(nameEnv, token, equalToNameOffset);
            setEnvStr = setEnvStr + sizeof(char)*1; // Jump away from '='
            if(setenv(nameEnv, setEnvStr, 1) == -1){
                free(nameEnv);
                fprintf(stderr, "Error setting environment variable.\n");
                return 1;
            }
            free(nameEnv);
            return 0;
        } else if(token[0] == '$'){
            envCmd = true;
            parsedArgs[argCount++] = token;
        } else if(strcmp(token, "<") == 0){
            redIn = true;
        } else if(strcmp(token, ">") == 0){
            redOut = true;
        } else if(strcmp(token, "2>") == 0){
            redErr = true;
        } else if(strcmp(token, "echo") == 0) {
            parsedArgs[argCount++] = token;
            echoCmd = true;
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

    if(argCount > 0 && argCount == totalArgs){
        size_t lastArgLen = strlen(parsedArgs[argCount - 1]);
        parsedArgs[argCount - 1][lastArgLen - 1] = '\0';
    }
    parsedArgs[argCount] = NULL;

    if(cdCmd + pwdCmd > 1) {
        fprintf(stderr, "Too many commands given, please try again.\n");
    } else {
        if(cdCmd){
            tish_cd(parsedArgs, argCount);
        } else if(pwdCmd) {
            tish_pwd(parsedArgs, argCount);
        } else if(envCmd && echoCmd){ 
            tish_echo_env(parsedArgs, argCount);
        }else {
            if(argCount == 0 && !comment){
                fprintf(stderr, "Please provide the command you wish to run.\n");
            } else {
                if(!envCmd){
                    tish_all_else(parsedArgs, argCount);
                }
            }
        }
    }

    tish_reset_fd(oldStdIn, oldStdOut, oldStdErr);

    return 0;
}

int tish_pwd(char** curArgs, int curArgsCount){
    if(curArgsCount > 2){
        fprintf(stderr, "Too many arguments given, please try again.\n");
        return 1;
    }
    int curPathSz = 100;
    char curPath[curPathSz];
    if(getcwd(curPath, curPathSz) == NULL){
        lastCommandExit = 1;
        fprintf(stderr, "Pwd command failed, please try again. \n");
        return 1;
    }
    printf("%s\n", curPath);
    lastCommandExit = 0;
    return 0;
}

int tish_cd(char** curArgs, int curArgsCount){
    if(curArgsCount < 2){
        fprintf(stderr, "cd command not given a specific directory, please try again.\n");
        return 1;
    }
    char * destDirArr = malloc(50);
    strncpy(destDirArr, curArgs[1], strlen(curArgs[1]));
    if(chdir(destDirArr) == -1){
        lastCommandExit = 1;
        fprintf(stderr, "cd command failed, please try again. \n");
        free(destDirArr);
        return 1;
    }
    free(destDirArr);
    lastCommandExit = 0;
    return 0;
}

int tish_echo_env(char** curArgs, int curArgsCount){
    if(curArgsCount < 1){
        printf("\n");
        return 0;
    }
    char* cutArg = curArgs[1] + sizeof(char)*1; // Take out "$"
    // for(int i = 0; i < strlen(cutArg); i++){
    //     debug("%i", cutArg[i]);
    // }
    char* getEnvRet = NULL;
    if(strcmp(cutArg, "?") == 0){
        printf("%i\n", lastCommandExit);
        lastCommandExit = 0;
        return 0;
    }
    getEnvRet = getenv(cutArg); 
    if(getEnvRet != NULL){
        if(strchr(getEnvRet, '\n') != NULL){
            printf("%s", getEnvRet);
        } else {
            printf("%s\n", getEnvRet);
        }
    } else {
        lastCommandExit = 1;
        return 1;
    }
    lastCommandExit = 0;
    return 0;
}

int tish_all_else(char** curArgs, int curArgsCount){
    int execRet = 0;
    pid_t childPID = fork();
    pid_t waitPID;
    int status;
    if(childPID == 0){ // Child
        execRet = execvp(curArgs[0], curArgs);
        if(execRet == -1){
            lastCommandExit = 1;
            fprintf(stderr, "execvp error.\n");
            exit(1);
        }
    } else {
        do {
            waitPID = waitpid(childPID, &status, WUNTRACED | WCONTINUED);
            if (waitPID == -1) {
                perror("waitpid");
                exit(EXIT_FAILURE);
            }

            if (WIFEXITED(status)) {
                lastCommandExit = WEXITSTATUS(status);
            }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
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