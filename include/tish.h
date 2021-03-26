#define KMAG "\033[1;36m"
#define KNRM "\033[0m"
#define NL "\n"

#define debug(S, ...)                                                          \
  do {                                                                         \
    fprintf(stderr, KMAG "DEBUG: %s:%s:%d " KNRM S NL, __extension__ __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);                \
  } while (0)



void sigint_handler(int sigNum);