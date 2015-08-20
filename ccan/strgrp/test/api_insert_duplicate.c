#include "../test/helpers.h"

int main(void) {
    struct strgrp *ctx;
    create(ctx, DEFAULT_SIMILARITY);
    return one_group_from_two(ctx, "a", (void *)"1", "a", (void *)"2");
}
