#ifndef CCANLINT_BUILD_H
#define CCANLINT_BUILD_H
char *build_module(struct manifest *m, enum compile_type ctype, char **errstr);
char *build_submodule(struct manifest *m, const char *flags,
		      enum compile_type ctype);
void build_objects(struct manifest *m,
		   struct score *score, const char *flags,
		   enum compile_type ctype);
#endif /* CCANLINT_BUILD_H */
