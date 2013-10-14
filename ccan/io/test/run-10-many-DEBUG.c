#define DEBUG
#define PORT "64010"
#define main real_main
int real_main(void);
#include "run-10-many.c"
#undef main
/* We stack overflow if we debug all of them! */
static bool debug_one(struct io_conn *conn)
{
	return conn == buf[1].reader;
}
int main(void) { io_debug_conn = debug_one; return real_main(); }
