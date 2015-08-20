#include "../test/helpers.h"

int main(void) {
    struct strgrp *ctx;

    plan_tests(1);
    create(ctx, DEFAULT_SIMILARITY);
    strgrp_free(ctx);
    pass("Successfully initialised strgrp instance");
    return exit_status();
}
