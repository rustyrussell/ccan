#include "../test/helpers.h"

int main(void) {
    struct strgrp *ctx;
    create(ctx, 1.0);
    return one_group_from_two(ctx, "a", NULL, "a", NULL);
}
