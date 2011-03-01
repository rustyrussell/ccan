#ifndef CCANLINT_BUILD_H
#define CCANLINT_BUILD_H
char *build_module(struct manifest *m, bool keep, char **errstr);
char *build_submodule(struct manifest *m);
void build_objects(struct manifest *m,
		   bool keep, struct score *score, const char *flags);
#endif /* CCANLINT_BUILD_H */
