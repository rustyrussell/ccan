/* Licensed under LGPLv2.1+ - see LICENSE file for details */
#ifndef CCAN_JACOBSON_KARELS_H
#define CCAN_JACOBSON_KARELS_H

#include "config.h"

#include <ccan/minmax/minmax.h>

#define JACOBSON_KARELS(_name, _type, _a1, _a2, _b1, _b2, _g, _k) \
	struct _name##_state { \
		_type rtt, variance; \
	}; \
	static inline void _name##_init(struct _name##_state *s, \
				 _type rtt0, _type var0)  \
	{ \
		s->rtt = rtt0; \
		s->variance = var0; \
	} \
	static inline void _name##_update(struct _name##_state *s, _type sample) \
	{ \
		_type diff = sample - s->rtt; \
		s->rtt += (_a2) * diff / ((_a1) + (_a2)); \
		diff = (diff < 0) ? -diff : diff; \
		s->variance = ((_b1)*s->variance + (_b2) * diff) \
			/ ((_b1) + (_b2));			 \
	} \
	static inline _type _name##_timeout(struct _name##_state *s, \
				     _type tmin, _type tmax)  \
	{ \
		return clamp((_g) * s->rtt + (_k)*s->variance, tmin, tmax); \
	}

JACOBSON_KARELS(jacobson_karels, unsigned long, 7, 1, 3, 1, 1, 4)

#endif /* CCAN_JACOBSON_KARELS_H */
