/* Objects to link with; ctype is variant for test helpers and other modules,
   own_ctype is (if link_with_module) for this module's objects. */
char *test_obj_list(const struct manifest *m, bool link_with_module,
		    enum compile_type ctype, enum compile_type own_ctype);
/* Library list as specified by ctype variant of _info. */
char *test_lib_list(const struct manifest *m, enum compile_type ctype);
