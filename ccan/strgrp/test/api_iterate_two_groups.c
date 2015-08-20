#include "../test/helpers.h"

int main(void) {
    struct strgrp *ctx;
    struct strgrp_iter *iter;

    plan_tests(3);
    create(ctx, DEFAULT_SIMILARITY);
    strgrp_add(ctx, "a", NULL);
    strgrp_add(ctx, "b", NULL);
    iter = strgrp_iter_new(ctx);
    ok1(strgrp_iter_next(iter));
    ok1(strgrp_iter_next(iter));
    ok1(!strgrp_iter_next(iter));
    strgrp_iter_free(iter);
    strgrp_free(ctx);
    return exit_status();
}
