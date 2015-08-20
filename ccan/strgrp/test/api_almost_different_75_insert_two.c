#include "../test/helpers.h"

int main(void) {
    struct strgrp *ctx;
    create(ctx, 0.50);
    return one_group_from_two(ctx, "abcde", NULL, "zabcd", NULL);
}
