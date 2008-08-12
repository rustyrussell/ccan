/* This is test for grab_file() function */

/*
 * Example:
 * 
 * void *grab_file(const void *ctx, const char *filename)
 *	{
 *	int fd; 
 *	char *buffer;
 * 
 *	if (streq(filename, "-"))
 *		fd = dup(STDIN_FILENO);
 *	else
 *		fd = open(filename, O_RDONLY, 0); 
 *
 *	if (fd < 0)
 *		return NULL; 
 *
 *	buffer = grab_fd(ctx, fd);
 *	close_noerr(fd);
 *	return buffer;
 *	}
 */

/* End of grab_file() test */
