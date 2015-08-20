#include "../test/helpers.h"

static void
data_cb(void *data) {
    ok1(streq("1", (char *)data));
}

int main(void) {
    struct strgrp *ctx;

    plan_tests(1);
    create(ctx, DEFAULT_SIMILARITY);
    strgrp_add(ctx, "a", (void *)"1");
    strgrp_free_cb(ctx, &data_cb);
    return exit_status();
}
