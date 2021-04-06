#define KMAG "\033[1;36m"
#define KNRM "\033[0m"
#define NL "\n"

#define MAX_ARGS 20
#define ARG_MAX_LENGTH 20

#define debug(S, ...)                                                          \
  do {                                                                         \
    fprintf(stderr, KMAG "DEBUG: %s:%s:%d " KNRM S NL, __extension__ __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);                \
  } while (0)


void tish_parseArgs(char** cliArgs);
void tish_pwd();
void tish_cd(char* destDir);
void tish_ls();
void tish_quit(char** cliArgs);

// Signal Handlers //
void sigint_handler(int sigNum);