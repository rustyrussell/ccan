#include "../test/helpers.h"

int main(void) {
    struct strgrp *ctx;
    create(ctx, 0.50);
    return two_groups_from_two(ctx, "abcde", NULL, "zyxab", NULL);
}
