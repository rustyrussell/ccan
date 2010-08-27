#ifndef CCANLINT_BUILD_COVERAGE_H
#define CCANLINT_BUILD_COVERAGE_H

/* FIXME: gcov dumps a file into a random dir. */
void move_gcov_turd(const char *dir,
		    struct ccan_file *file, const char *extension);

#endif /* CCANLINT_BUILD_COVERAGE_H */
