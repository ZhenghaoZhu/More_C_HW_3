#define KMAG "\033[1;36m"
#define KNRM "\033[0m"
#define NL "\n"

#define MAX_ARGS 20
#define ARG_MAX_LENGTH 20

#define debug(S, ...)                                                          \
  do {                                                                         \
    fprintf(stdout, KMAG "DEBUG: %s:%s:%d " KNRM S NL, __extension__ __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);                \
  } while (0)


int tish_parseArgs(char** cliArgs);
int tish_pwd(char** curArgs, int curArgsCount);
int tish_cd(char** curArgs, int curArgsCount);
int tish_echo_env(char** curArgs, int curArgsCount);
int tish_all_else(char** curArgs, int curArgsCount);
int tish_reset_fd();
void tish_quit(char** cliArgs);

// Signal Handlers //
void sigint_handler(int sigNum);