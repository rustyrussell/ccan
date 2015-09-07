#include "../test/helpers.h"

int main(void) {
    const char k[] = "a";
    struct strgrp *ctx;
    const struct strgrp_grp *grp;
    struct strgrp_grp_iter *iter;
    const struct strgrp_item *item;

    plan_tests(5);
    create(ctx, DEFAULT_SIMILARITY);
    grp = strgrp_add(ctx, k, NULL);
    ok1(streq(k, strgrp_grp_key(grp)));
    iter = strgrp_grp_iter_new(grp);
    item = strgrp_grp_iter_next(iter);
    ok1(item);
    ok1(streq(k, strgrp_item_key(item)));
    ok1(!strgrp_item_value(item));
    ok1(!strgrp_grp_iter_next(iter));
    strgrp_grp_iter_free(iter);
    strgrp_free(ctx);
    return exit_status();
}
