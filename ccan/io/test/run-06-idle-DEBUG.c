#define DEBUG
#define PORT "64006"
#define main real_main
int real_main(void);
#include "run-06-idle.c"
#undef main
static bool always_debug(struct io_conn *conn) { return true; }
int main(void) { io_debug_conn = always_debug; return real_main(); }
