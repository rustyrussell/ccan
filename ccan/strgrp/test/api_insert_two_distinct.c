#include "../test/helpers.h"

int main(void) {
    struct strgrp *ctx;
    create(ctx, DEFAULT_SIMILARITY);
    return two_groups_from_two(ctx, "a", NULL, "b", NULL);
}
