#include "../test/helpers.h"

int main(void) {
    struct strgrp *ctx;
    create(ctx, 0.25);
    return one_group_from_two(ctx, "abcde", NULL, "zyxab", NULL);
}
