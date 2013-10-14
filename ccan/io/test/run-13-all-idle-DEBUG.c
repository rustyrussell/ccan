#define DEBUG
#define PORT "64013"
#define main real_main
int real_main(void);
#include "run-13-all-idle.c"
#undef main
static bool always_debug(struct io_conn *conn) { return true; }
int main(void) { io_debug_conn = always_debug; return real_main(); }
