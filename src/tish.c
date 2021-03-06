#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <glib.h>
#include "tish.h"

GSList* jobList = NULL;
sig_atomic_t cliRunning = true;
struct rusage useStart = {};
struct rusage useEnd = {};
char *cliArgs = NULL;
int lastCommandExit = 0;
bool debugOutput = false;
bool timeOutput = false;
bool putInBackground = false;

void sigint_handler(int sigNum){
    cliRunning = false;
    fprintf(stdout, "\n");
    tish_quit(1);
}

void sigchld_handler(int sigNum){
    pid_t curPID;
    int status;
    if((curPID = waitpid(-1, &status, WNOHANG)) != -1){
        bool found = false;
        GSList* iterator = NULL;
        JOB* curJob = NULL;
        for (iterator = jobList; iterator; iterator = iterator->next) {
            curJob = (JOB*)iterator->data;
            if(curPID == curJob->pgid){
                for(int i = 0; i < curJob->argCount; i++){
                    free(curJob->args[i]);
                }
                jobList = g_slist_remove_link(jobList, iterator);
                break;
            }
        }
    }
}

int main(int argc, char const *argv[], char *envp[]){
    FILE* curStdIn = stdin;
    bool nonInteractive = false;
    getrusage(RUSAGE_SELF, &useStart);
    getrusage(RUSAGE_SELF, &useEnd);
    signal(SIGINT, sigint_handler);
    signal(SIGCHLD, sigchld_handler);
    for(int i = 1; i < argc; i++){ // Run through flags and files passed
        #ifdef EXTRA_CREDIT
        if(strncmp(argv[i], "-t", 2) == 0){
            timeOutput = true;
        }
        #endif
        if(strncmp(argv[i], "-d", 2) == 0){ // Debug Option
            debugOutput = true;
        } else if(access(argv[i], F_OK) == 0){ // Non interactive option
            int newStdIn = open(argv[i], O_RDONLY, 0640);
            if(dup2(newStdIn, fileno(curStdIn)) == -1){ // Set stdin to file passed
                fprintf(stderr, "Error ocured with non-interactive functionality.\n");
                tish_quit(1);
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
            if(strncmp(cliArgs, "exit", 4) == 0){ // Check for exit command else begin parsing arguments
                if(cliArgs != NULL){
                    free(cliArgs);
                    cliArgs = NULL;
                }
                tish_quit(0);
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

void tish_running_cmd(char* curCmd){ // For RUNNING debug
    if(debugOutput){
        fprintf(stderr, "RUNNING: %s\n", curCmd);
    }
    return;
}

void tish_ending_cmd(char* curCmd, int exitStatus){ // For ENDED debug
    if(debugOutput){
        fprintf(stderr, "ENDED: %s (ret=%d)\n", curCmd, exitStatus);
    }
    return;
}

void tish_time_cmd(double realTime, double userTime, double sysTime){ // FOR TIME EXTRA CREDIT
    #ifdef EXTRA_CREDIT
    if(timeOutput){
        fprintf(stderr, "TIMES: real=%fs user=%fs sys=%fs\n", realTime, userTime, sysTime);
    }
    #endif
    return;
}

void tish_update_times(clock_t* curRealTime, double* curUserTime, double* curSysTime, bool start){
    *curRealTime = clock();
    if(start){
        getrusage(RUSAGE_SELF, &useStart);
        *curUserTime = (useStart.ru_utime.tv_sec * CLOCKS_PER_SEC + useStart.ru_utime.tv_usec);
        *curSysTime = (useStart.ru_stime.tv_sec * CLOCKS_PER_SEC + useStart.ru_stime.tv_usec);
    } else {
        getrusage(RUSAGE_SELF, &useEnd);
        *curUserTime = (useEnd.ru_utime.tv_sec * CLOCKS_PER_SEC + useEnd.ru_utime.tv_usec);
        *curSysTime = (useEnd.ru_stime.tv_sec * CLOCKS_PER_SEC + useEnd.ru_stime.tv_usec);
    }
    
}

int tish_parseArgs(char** cliArgs){
    char* parsedArgs[MAX_ARGS];
    char* token = NULL;
    char* setEnvStr = NULL;
    const char delim[2] = " ";
    int argCount = 0;
    int totalArgs = 0;
    int newFd = 0;
    int oldStdIn = dup(STDIN_FILENO);
    int oldStdOut = dup(STDOUT_FILENO);
    int oldStdErr = dup(STDERR_FILENO);
    bool redIn = false, redOut = false, redErr = false;
    bool cdCmd = false, pwdCmd = false, envCmd = false, echoCmd = false, killCmd = false, fgCmd = false, comment = false;
    token = strtok(*cliArgs, delim);

    while(token != NULL) {
        // debug("len :  %ld, str: %s", strlen(token), token);
        totalArgs += 1;
        if(redIn){
            size_t tokenLen = strlen(token);
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
            if(dup2(newFd, fileno(stdin)) == -1){
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
            if(dup2(newFd, fileno(stdout)) == -1){
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
            if(dup2(newFd, fileno(stderr)) == -1){
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
            tish_running_cmd("Setting ENV VAR");
            if(setenv(nameEnv, setEnvStr, 1) == -1){
                free(nameEnv);
                tish_ending_cmd("Setting ENV VAR", 1);
                fprintf(stderr, "Error setting environment variable.\n");
                return 1;
            }
            tish_ending_cmd("Setting ENV VAR", 0);
            free(nameEnv);
            return 0;
        } else if(token[0] == '&'){
            putInBackground = true;
        } else if(token[0] == '$'){
            envCmd = true;
            parsedArgs[argCount++] = token;
        } else if(strcmp(token, "<") == 0){
            redIn = true;
        } else if(strcmp(token, ">") == 0){
            redOut = true;
        } else if(strcmp(token, "2>") == 0){
            redErr = true;
        } else if(strcmp(token, "echo") == 0){
            parsedArgs[argCount++] = token;
            echoCmd = true;
        } else if(strncmp(token, "jobs", 4) == 0){
            tish_print_jobs();
            return 0;
        } else if(strncmp(token, "kill", 4) == 0){
            killCmd = true;
        } else if(strncmp(token, "fg", 2) == 0){
            fgCmd = true;
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

    if(argCount > 0 && argCount == totalArgs){ // Take into account null terminator of last argument
        size_t lastArgLen = strlen(parsedArgs[argCount - 1]);
        parsedArgs[argCount - 1][lastArgLen - 1] = '\0';
    }
    parsedArgs[argCount] = NULL;

    if(cdCmd + pwdCmd + killCmd > 1) {
        fprintf(stderr, "Too many commands given, please try again.\n");
    } else {
        if(fgCmd){
            tish_fg(parsedArgs, argCount);
        }
        else if(cdCmd){
            tish_cd(parsedArgs, argCount);
        } else if(pwdCmd) {
            tish_pwd(parsedArgs, argCount);
        } else if(killCmd) {
            tish_kill(parsedArgs, argCount);
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

    tish_reset_fd(oldStdIn, oldStdOut, oldStdErr); // Reset to old FDs

    return 0;
}

int tish_pwd(char** curArgs, int curArgsCount){
    if(curArgsCount > 2){
        fprintf(stderr, "Too many arguments given, please try again.\n");
        lastCommandExit = 1;
        tish_ending_cmd("pwd", lastCommandExit);
        return 1;
    }
    clock_t start, end;
    double userStart, userEnd, sysStart, sysEnd;
    double realTime = 0, userTime = 0, sysTime = 0;
    tish_update_times(&start, &userStart, &sysStart, true);
    tish_running_cmd("pwd");
    int curPathSz = 100;
    char curPath[curPathSz];
    if(getcwd(curPath, curPathSz) == NULL){
        lastCommandExit = 1;
        tish_update_times(&end, &userEnd, &sysEnd, false);
        realTime = ((double) (end - start)) / CLOCKS_PER_SEC;
        userTime = userEnd - userStart;
        sysTime = sysEnd - sysStart;
        tish_time_cmd(realTime, userTime, sysTime);
        tish_ending_cmd("pwd", lastCommandExit);
        fprintf(stderr, "Pwd command failed, please try again. \n");
        return 1;
    }
    printf("%s\n", curPath);
    lastCommandExit = 0;
    tish_update_times(&end, &userEnd, &sysEnd, false);
    realTime = ((double)(end - start)) / CLOCKS_PER_SEC;
    userTime = userEnd - userStart;
    sysTime = sysEnd - sysStart;
    tish_time_cmd(realTime, userTime, sysTime);
    tish_ending_cmd("pwd", lastCommandExit);
    return 0;
}

int tish_fg(char** curArgs, int curArgsCount){
    GSList* iterator = NULL;
    JOB* curJob = NULL;
    char* pidPassed;
    int pidInt = strtol(curArgs[0], &pidPassed, 10);
    for (iterator = jobList; iterator; iterator = iterator->next) {
        curJob = (JOB*)iterator->data;
        if(curJob->pgid == pidInt){
            kill(curJob->pgid, SIGCONT);
            curJob->stopped = false;
            return 0;
        }
    }
    return 0;
}

int tish_kill(char** curArgs, int curArgsCount){
    if(curArgsCount < 2){
        fprintf(stderr, "Wrong number of arguments.\n");
        return 1;
    }
    GSList* iterator = NULL;
    JOB* curJob = NULL;
    char* signalArr;
    int signalInt = -1*strtol(curArgs[0], &signalArr, 10);
    if(signalInt <= 0){
        fprintf(stderr, "Signal passed not valid.\n");
        return 1;
    }
    char* pidPassed;
    int pidInt = strtol(curArgs[1], &pidPassed, 10);
    for (iterator = jobList; iterator; iterator = iterator->next) {
        curJob = (JOB*)iterator->data;
        if(curJob->pgid == pidInt){
            kill(curJob->pgid, signalInt);
            if(signalInt == 9){
                jobList = g_slist_remove_link(jobList, iterator);
            }
            if(signalInt == SIGSTOP){
                curJob->stopped = true;
            }
            return 0;
        }
    }
    return 0;
}

int tish_cd(char** curArgs, int curArgsCount){
    if(curArgsCount < 2){
        fprintf(stderr, "cd command not given a specific directory, please try again.\n");
        lastCommandExit = 1;
        tish_ending_cmd("cd", lastCommandExit);
        return 1;
    }
    clock_t start, end;
    double userStart, userEnd, sysStart, sysEnd;
    double realTime = 0, userTime = 0, sysTime = 0;
    tish_update_times(&start, &userStart, &sysStart, true);
    tish_running_cmd("cd");
    char * destDirArr = calloc(sizeof(char), 50);
    strncpy(destDirArr, curArgs[1], strlen(curArgs[1]));
    if(chdir(destDirArr) == -1){
        lastCommandExit = 1;
        tish_update_times(&end, &userEnd, &sysEnd, false);
        realTime = ((double) (end - start)) / CLOCKS_PER_SEC;
        userTime = userEnd - userStart;
        sysTime = sysEnd - sysStart;
        tish_time_cmd(realTime, userTime, sysTime);
        tish_ending_cmd("cd", lastCommandExit);
        fprintf(stderr, "cd command failed, please try again. \n");
        free(destDirArr);
        return 1;
    }
    free(destDirArr);
    lastCommandExit = 0;
    tish_update_times(&end, &userEnd, &sysEnd, false);
    realTime = ((double) (end - start)) / CLOCKS_PER_SEC;
    userTime = userEnd - userStart;
    sysTime = sysEnd - sysStart;
    tish_time_cmd(realTime, userTime, sysTime);
    tish_ending_cmd("cd", lastCommandExit);
    return 0;
}

int tish_echo_env(char** curArgs, int curArgsCount){
    if(curArgsCount < 1){
        printf("\n");
        return 0;
    }
    clock_t start, end;
    double userStart, userEnd, sysStart, sysEnd;
    double realTime = 0, userTime = 0, sysTime = 0;
    tish_update_times(&start, &userStart, &sysStart, true);
    tish_running_cmd("echo");
    char* cutArg = curArgs[1] + sizeof(char)*1; // Take out "$"
    char* getEnvRet = NULL;
    if(strcmp(cutArg, "?") == 0){
        printf("%i\n", lastCommandExit);
        lastCommandExit = 0;
        tish_update_times(&end, &userEnd, &sysEnd, false);
        realTime = ((double) (end - start)) / CLOCKS_PER_SEC;
        userTime = userEnd - userStart;
        sysTime = sysEnd - sysStart;
        tish_time_cmd(realTime, userTime, sysTime);
        tish_ending_cmd(curArgs[0], lastCommandExit);
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
        tish_update_times(&end, &userEnd, &sysEnd, false);
        realTime = ((double) (end - start)) / CLOCKS_PER_SEC;
        userTime = userEnd - userStart;
        sysTime = sysEnd - sysStart;
        tish_time_cmd(realTime, userTime, sysTime);
        tish_ending_cmd(curArgs[0], lastCommandExit);
        return 1;
    }
    tish_update_times(&end, &userEnd, &sysEnd, false);
    realTime = ((double) (end - start)) / CLOCKS_PER_SEC;
    userTime = userEnd - userStart;
    sysTime = sysEnd - sysStart;
    tish_time_cmd(realTime, userTime, sysTime);
    tish_ending_cmd(curArgs[0], lastCommandExit);
    lastCommandExit = 0;
    return 0;
}

int tish_all_else(char** curArgs, int curArgsCount){
    int execRet = 0;
    pid_t childPID = fork();
    pid_t waitPID;
    int status;
    clock_t start, end;
    double userStart, userEnd, sysStart, sysEnd;
    double realTime = 0, userTime = 0, sysTime = 0;
    tish_update_times(&start, &userStart, &sysStart, true);
    if(childPID == 0){ // Child
        tish_running_cmd(curArgs[0]);
        execRet = execvp(curArgs[0], curArgs); // Execute command, wait and all of that in parent
        if(execRet == -1){
            lastCommandExit = 1;
            fprintf(stderr, "execvp error.\n");
            tish_quit(1);
        }
    } else {
            if(putInBackground && execRet != -1){ // Background process
                int mainIn = fileno(stdin), mainOut = fileno(stdout), mainErr = fileno(stderr);
                int newProcCount = g_slist_length(jobList) + 1;
                fprintf(stdout, "[%i] %i\n", newProcCount, childPID);
                tish_add_to_list(curArgs, curArgsCount, childPID, mainIn, mainOut, mainErr);
                putInBackground = false;
            } else { // Foreground process
                waitPID = waitpid(childPID, &status, WUNTRACED | WCONTINUED);
                if (waitPID == -1) {
                    perror("waitpid");
                    exit(EXIT_FAILURE);
                }
                if(putInBackground && execRet != -1){
                    tish_job_completed(curArgs);
                }
                tish_update_times(&end, &userEnd, &sysEnd, false);
                realTime = ((double) (end - start)) / CLOCKS_PER_SEC;
                userTime = userEnd - userStart;
                sysTime = sysEnd - sysStart;
                if (WIFEXITED(status)) {
                    lastCommandExit = WEXITSTATUS(status);
                }
                tish_time_cmd(realTime, userTime, sysTime);
                tish_ending_cmd(curArgs[0], lastCommandExit);
            }
    }
    // while((waitPID = wait(&status)) > 0);
    return 0;
}

void tish_job_completed(char **curArgs){ // Change status after job is completed
    bool found = false;
    GSList* iterator = NULL;
    JOB* curJob = NULL;
    for (iterator = jobList; iterator; iterator = iterator->next) {
        curJob = (JOB*)iterator->data;
        found = true;
        for(int i = 0; i < curJob->argCount; i++){
            if(strcmp(curArgs[i], curJob->args[i]) != 0){
                found = false;
                break;
            }            
        }
        if(found){
            curJob->completed = true;
        }
    }
    return;
}

void tish_print_jobs(){
    GSList* iterator = NULL;
    int count = 0;
    JOB* curJob = NULL;
    for (iterator = jobList; iterator; iterator = iterator->next) {
        curJob = (JOB*)iterator->data;
        count += 1;
        fprintf(stdout, "[%i] ", count);
        if(!(curJob->stopped) && !(curJob->completed)){ // Check if it's running or stopped
            fprintf(stdout, "%s \t\t", "Running");
        } else if(curJob->stopped) {
            fprintf(stdout, "%s \t\t", "Stopped");
        } else {
            fprintf(stdout, "%s \t\t", "Done");
        }
        fprintf(stdout, "%i \t\t", curJob->pgid);
        for(int i = 0; i < curJob->argCount; i++){
            fprintf(stdout, "%s ", curJob->args[i]);
        }
        fprintf(stdout, "\n");
        fflush(stdout);
    }
    return;
}

void tish_add_to_list(char** curArgs, int curArgsCount, pid_t curPgid, int stdIn, int stdOut, int stdErr){
    JOB* curJob = g_new(JOB, 1); // Create new job
    if(curJob == NULL){
        return;
    }
    curJob->pgid = curPgid;
    for(int i = 0; i < curArgsCount; i++){ // Copy all args
        curJob->args[i] = calloc(sizeof(char), strlen(curArgs[i]) + 1);
        strcpy(curJob->args[i], curArgs[i]);
    }
    curJob->argCount = curArgsCount;
    curJob->completed = false;
    curJob->stopped = false;
    curJob->curStdIn = stdIn;
    curJob->curStdOut = stdOut;
    curJob->curStdErr = stdErr;
    jobList = g_slist_append(jobList, curJob); // Append job to list
    return;
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

void tish_quit(int exitStatus){
    if(jobList != NULL){ // Free jobList
        GSList* iterator = NULL;
        JOB* curJob = NULL;
        for (iterator = jobList; iterator; iterator = iterator->next) {
            curJob = (JOB*)iterator->data;
            for(int i = 0; i < curJob->argCount; i++){ // Free all args from each JOB struct
                free(curJob->args[i]);
            }
            jobList = g_slist_remove_link(jobList, iterator);
            break;
        }
        g_slist_free(jobList);
    }
    cliRunning = false;
    exit(exitStatus);
}