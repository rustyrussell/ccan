#ifndef TDB2_TEST_LAYOUT_H
#define TDB2_TEST_LAYOUT_H
#include <ccan/tdb2/private.h>

struct tdb_layout *new_tdb_layout(void);
void tdb_layout_add_free(struct tdb_layout *layout, tdb_len_t len);
void tdb_layout_add_used(struct tdb_layout *layout,
			 TDB_DATA key, TDB_DATA data,
			 tdb_len_t extra);
void tdb_layout_add_hashtable(struct tdb_layout *layout,
			      unsigned int hash_bits,
			      tdb_len_t extra);
void tdb_layout_add_freetable(struct tdb_layout *layout,
			      unsigned int num_zones,
			      unsigned int zone_bits,
			      unsigned int num_buckets,
			      tdb_len_t extra);
struct tdb_context *tdb_layout_get(struct tdb_layout *layout);

enum layout_type {
	FREE, DATA, HASHTABLE, FREETABLE,
};

/* Shared by all union members. */
struct tle_base {
	enum layout_type type;
	tdb_off_t off;
};

struct tle_free {
	struct tle_base base;
	tdb_len_t len;
};

struct tle_used {
	struct tle_base base;
	TDB_DATA key;
	TDB_DATA data;
	tdb_len_t extra;
};

struct tle_hashtable {
	struct tle_base base;
	unsigned hash_bits;
	tdb_len_t extra;
};

struct tle_freetable {
	struct tle_base base;
	unsigned int num_zones;
	unsigned int zone_bits;
	unsigned int num_buckets;
	tdb_len_t extra;
};

union tdb_layout_elem {
	struct tle_base base;
	struct tle_free free;
	struct tle_used used;
	struct tle_freetable freetable;
	struct tle_hashtable hashtable;
};

struct tdb_layout {
	unsigned int num_elems;
	union tdb_layout_elem *elem;
	unsigned int ftable, htable;
};
#endif /* TDB2_TEST_LAYOUT_H */
