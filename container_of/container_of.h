#ifndef CCAN_CONTAINER_OF_H
#define CCAN_CONTAINER_OF_H
#include <stddef.h>

#include "config.h"
#include "check_type/check_type.h"

#define container_of(member_ptr, containing_type, member)		\
	 ((containing_type *)						\
	  ((char *)(member_ptr) - offsetof(containing_type, member))	\
	  - check_types_match(*(member_ptr), ((containing_type *)0)->member))


#endif /* CCAN_CONTAINER_OF_H */
