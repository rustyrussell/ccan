#ifndef NTDB_TEST_LAYOUT_H
#define NTDB_TEST_LAYOUT_H
#include "private.h"

struct ntdb_layout *new_ntdb_layout(void);
void ntdb_layout_add_freetable(struct ntdb_layout *layout);
void ntdb_layout_add_free(struct ntdb_layout *layout, ntdb_len_t len,
			 unsigned ftable);
void ntdb_layout_add_used(struct ntdb_layout *layout,
			 NTDB_DATA key, NTDB_DATA data,
			 ntdb_len_t extra);
void ntdb_layout_add_capability(struct ntdb_layout *layout,
			       uint64_t type,
			       bool write_breaks,
			       bool check_breaks,
			       bool open_breaks,
			       ntdb_len_t extra);

#if 0 /* FIXME: Allow allocation of subtables */
void ntdb_layout_add_hashtable(struct ntdb_layout *layout,
			      int htable_parent, /* -1 == toplevel */
			      unsigned int bucket,
			      ntdb_len_t extra);
#endif
/* freefn is needed if we're using failtest_free. */
struct ntdb_context *ntdb_layout_get(struct ntdb_layout *layout,
				   void (*freefn)(void *),
				   union ntdb_attribute *attr);
void ntdb_layout_write(struct ntdb_layout *layout, void (*freefn)(void *),
		       union ntdb_attribute *attr, const char *filename);

void ntdb_layout_free(struct ntdb_layout *layout);

enum layout_type {
	FREETABLE, FREE, DATA, CAPABILITY
};

/* Shared by all union members. */
struct tle_base {
	enum layout_type type;
	ntdb_off_t off;
};

struct tle_freetable {
	struct tle_base base;
};

struct tle_free {
	struct tle_base base;
	ntdb_len_t len;
	unsigned ftable_num;
};

struct tle_used {
	struct tle_base base;
	NTDB_DATA key;
	NTDB_DATA data;
	ntdb_len_t extra;
};

struct tle_capability {
	struct tle_base base;
	uint64_t type;
	ntdb_len_t extra;
};

union ntdb_layout_elem {
	struct tle_base base;
	struct tle_freetable ftable;
	struct tle_free free;
	struct tle_used used;
	struct tle_capability capability;
};

struct ntdb_layout {
	unsigned int num_elems;
	union ntdb_layout_elem *elem;
};
#endif /* NTDB_TEST_LAYOUT_H */
