#include "../test/helpers.h"

int main(void) {
    struct strgrp *ctx;
    struct strgrp_iter *iter;

    plan_tests(1);
    create(ctx, DEFAULT_SIMILARITY);
    iter = strgrp_iter_new(ctx);
    ok1(!strgrp_iter_next(iter));
    strgrp_iter_free(iter);
    strgrp_free(ctx);
    return exit_status();
}
