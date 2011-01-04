#ifndef CCANLINT_BUILD_H
#define CCANLINT_BUILD_H
char *build_module(struct manifest *m, bool keep, char **errstr);
char *build_submodule(struct manifest *m);
#endif /* CCANLINT_BUILD_H */
