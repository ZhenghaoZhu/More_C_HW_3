#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "tish.h"

bool cliRunning = true;

int main(void){
    signal(SIGINT, sigint_handler);
    char *cliArgs = NULL;
    size_t cliLen;
    int charCount;
    int commandStatus;
    debug("%s", "Huh");
    while(cliRunning){
        fprintf(stdout , "%s", "tish> ");
        fflush(stdout);
        charCount = getline(&cliArgs, &cliLen, stdin);
        if(charCount != -1){
            if(!strncmp(cliArgs, "exit", 4)){
                cliRunning = false;
            } else {
                printf("You inputted: %s", cliArgs);
            }
        } else {
            printf("No \n");
            cliRunning = false;
        }
    }
    if(cliArgs != NULL){
        free(cliArgs);
    }
    return 1;
}

void sigint_handler(int sigNum){
  printf("\nInside handler function\n");
  cliRunning = false;
  return;
}