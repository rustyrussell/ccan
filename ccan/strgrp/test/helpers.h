#ifndef STRGRP_TEST_HELPERS
#define STRGRP_TEST_HELPERS
#include <stdbool.h>
#include "ccan/str/str.h"
#include "ccan/tap/tap.h"
#include "../strgrp.h"

#define DEFAULT_SIMILARITY 0.85

#define create(dst, similarity) \
    do { \
        dst = strgrp_new(similarity); \
        if (!dst) { \
            fail("strgrp_new() returned NULL reference"); \
            return 1; \
        } \
    } while (0)

int
one_group_from_two(struct strgrp *ctx,
        const char *const k1, void *v1,
        const char *const k2, void *v2);

int
two_groups_from_two(struct strgrp *ctx,
        const char *const k1, void *v1,
        const char *const k2, void *v2);

#endif /* STRGRP_TEST_HELPERS */
