/* CC0 license (public domain) - see LICENSE file for details */
#ifndef CCAN_ORDER_H
#define CCAN_ORDER_H

#include <stdint.h>
#include <assert.h>

#include <ccan/typesafe_cb/typesafe_cb.h>

typedef int (*_total_order_cb)(const void *, const void *, void *);
typedef int (*total_order_noctx_cb)(const void *, const void *);

#define total_order_cb(_name, _item, _ctx)		\
	int (*_name)(const __typeof__(_item) *,		\
		     const __typeof__(_item) *,		\
		     __typeof__(_ctx))

#define total_order_cast(cmp, item, ctx)				\
	typesafe_cb_cast(_total_order_cb, total_order_cb(, item, ctx),	\
			 (cmp))

struct _total_order {
	_total_order_cb cb;
	void *ctx;
};

#define total_order(_name, _item, _ctx)			\
	struct {					\
		total_order_cb(cb, _item, _ctx);	\
		_ctx ctx;				\
	} _name

#endif /* CCAN_ORDER_H */
