//#include "../test/helpers.h"
#include "helpers.h"

int
one_group_from_two(struct strgrp *ctx,
        const char *const k1, void *v1,
        const char *const k2, void *v2) {
    const struct strgrp_grp *grp1, *grp2;
    struct strgrp_grp_iter *iter;
    const struct strgrp_item *item;

    plan_tests(10);
    grp1 = strgrp_add(ctx, k1, v1);
    ok1(grp1);
    grp2 = strgrp_add(ctx, k2, v2);
    ok1(grp2);
    ok1(grp1 == grp2);
    iter = strgrp_grp_iter_new(grp1);
    item = strgrp_grp_iter_next(iter);
    ok1(item);
    ok1(streq(k1, strgrp_item_key(item)));
    ok1(v1 == strgrp_item_value(item));
    item = strgrp_grp_iter_next(iter);
    ok1(item);
    ok1(streq(k2, strgrp_item_key(item)));
    ok1(v2 == strgrp_item_value(item));
    ok1(!strgrp_grp_iter_next(iter));
    strgrp_grp_iter_free(iter);
    strgrp_free(ctx);
    return exit_status();
}

int
two_groups_from_two(struct strgrp *ctx,
        const char *const k1, void *v1,
        const char *const k2, void *v2) {
    const struct strgrp_grp *grp1, *grp2;
    struct strgrp_grp_iter *iter;
    const struct strgrp_item *item;

    plan_tests(11);
    grp1 = strgrp_add(ctx, k1, v1);
    ok1(grp1);
    grp2 = strgrp_add(ctx, k2, v2);
    ok1(grp2);
    ok1(grp1 != grp2);
    {
        iter = strgrp_grp_iter_new(grp1);
        item = strgrp_grp_iter_next(iter);
        ok1(item);
        ok1(streq(k1, strgrp_item_key(item)));
        ok1(v1 == strgrp_item_value(item));
        ok1(!strgrp_grp_iter_next(iter));
        strgrp_grp_iter_free(iter);
    }
    {
        iter = strgrp_grp_iter_new(grp2);
        item = strgrp_grp_iter_next(iter);
        ok1(item);
        ok1(streq(k2, strgrp_item_key(item)));
        ok1(v2 == strgrp_item_value(item));
        ok1(!strgrp_grp_iter_next(iter));
        strgrp_grp_iter_free(iter);
    }
    strgrp_free(ctx);
    return exit_status();
}
