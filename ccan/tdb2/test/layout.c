/* TDB tools to create various canned database layouts. */
#include "layout.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <err.h>
#include "logging.h"

struct tdb_layout *new_tdb_layout(const char *filename)
{
	struct tdb_layout *layout = malloc(sizeof(*layout));
	layout->filename = filename;
	layout->num_elems = 0;
	layout->elem = NULL;
	return layout;
}

static void add(struct tdb_layout *layout, union tdb_layout_elem elem)
{
	layout->elem = realloc(layout->elem,
			       sizeof(layout->elem[0])
			       * (layout->num_elems+1));
	layout->elem[layout->num_elems++] = elem;
}

void tdb_layout_add_freetable(struct tdb_layout *layout)
{
	union tdb_layout_elem elem;
	elem.base.type = FREETABLE;
	add(layout, elem);
}

void tdb_layout_add_free(struct tdb_layout *layout, tdb_len_t len,
			 unsigned ftable)
{
	union tdb_layout_elem elem;
	elem.base.type = FREE;
	elem.free.len = len;
	elem.free.ftable_num = ftable;
	add(layout, elem);
}

static struct tdb_data dup_key(struct tdb_data key)
{
	struct tdb_data ret;
	ret.dsize = key.dsize;
	ret.dptr = malloc(ret.dsize);
	memcpy(ret.dptr, key.dptr, ret.dsize);
	return ret;
}

void tdb_layout_add_used(struct tdb_layout *layout,
			 TDB_DATA key, TDB_DATA data,
			 tdb_len_t extra)
{
	union tdb_layout_elem elem;
	elem.base.type = DATA;
	elem.used.key = dup_key(key);
	elem.used.data = dup_key(data);
	elem.used.extra = extra;
	add(layout, elem);
}

static tdb_len_t free_record_len(tdb_len_t len)
{
	return sizeof(struct tdb_used_record) + len;
}

static tdb_len_t data_record_len(struct tle_used *used)
{
	tdb_len_t len;
	len = sizeof(struct tdb_used_record)
		+ used->key.dsize + used->data.dsize + used->extra;
	assert(len >= sizeof(struct tdb_free_record));
	return len;
}

static tdb_len_t hashtable_len(struct tle_hashtable *htable)
{
	return sizeof(struct tdb_used_record)
		+ (sizeof(tdb_off_t) << TDB_SUBLEVEL_HASH_BITS)
		+ htable->extra;
}

static tdb_len_t freetable_len(struct tle_freetable *ftable)
{
	return sizeof(struct tdb_freetable);
}

static void set_free_record(void *mem, tdb_len_t len)
{
	/* We do all the work in add_to_freetable */
}

static void set_data_record(void *mem, struct tdb_context *tdb,
			    struct tle_used *used)
{
	struct tdb_used_record *u = mem;

	set_header(tdb, u, TDB_USED_MAGIC, used->key.dsize, used->data.dsize,
		   used->key.dsize + used->data.dsize + used->extra,
		   tdb_hash(tdb, used->key.dptr, used->key.dsize));
	memcpy(u + 1, used->key.dptr, used->key.dsize);
	memcpy((char *)(u + 1) + used->key.dsize,
	       used->data.dptr, used->data.dsize);
}

static void set_hashtable(void *mem, struct tdb_context *tdb,
			  struct tle_hashtable *htable)
{
	struct tdb_used_record *u = mem;
	tdb_len_t len = sizeof(tdb_off_t) << TDB_SUBLEVEL_HASH_BITS;

	set_header(tdb, u, TDB_HTABLE_MAGIC, 0, len, len + htable->extra, 0);
	memset(u + 1, 0, len);
}

static void set_freetable(void *mem, struct tdb_context *tdb,
			 struct tle_freetable *freetable, struct tdb_header *hdr,
			 tdb_off_t last_ftable)
{
	struct tdb_freetable *ftable = mem;
	memset(ftable, 0, sizeof(*ftable));
	set_header(tdb, &ftable->hdr, TDB_FTABLE_MAGIC, 0,
			sizeof(*ftable) - sizeof(ftable->hdr),
			sizeof(*ftable) - sizeof(ftable->hdr), 0);

	if (last_ftable) {
		ftable = (struct tdb_freetable *)((char *)hdr + last_ftable);
		ftable->next = freetable->base.off;
	} else {
		hdr->free_table = freetable->base.off;
	}
}

static void add_to_freetable(struct tdb_context *tdb,
			     tdb_off_t eoff,
			     tdb_off_t elen,
			     unsigned ftable,
			     struct tle_freetable *freetable)
{
	tdb->ftable_off = freetable->base.off;
	tdb->ftable = ftable;
	add_free_record(tdb, eoff, sizeof(struct tdb_used_record) + elen);
}

static tdb_off_t hbucket_off(tdb_off_t group_start, unsigned ingroup)
{
	return group_start
		+ (ingroup % (1 << TDB_HASH_GROUP_BITS)) * sizeof(tdb_off_t);
}

/* Get bits from a value. */
static uint32_t bits(uint64_t val, unsigned start, unsigned num)
{
	assert(num <= 32);
	return (val >> start) & ((1U << num) - 1);
}

/* We take bits from the top: that way we can lock whole sections of the hash
 * by using lock ranges. */
static uint32_t use_bits(uint64_t h, unsigned num, unsigned *used)
{
	*used += num;
	return bits(h, 64 - *used, num);
}

static tdb_off_t encode_offset(tdb_off_t new_off, unsigned bucket,
			       uint64_t h)
{
	return bucket
		| new_off
		| ((uint64_t)bits(h, 64 - TDB_OFF_UPPER_STEAL_EXTRA,
				  TDB_OFF_UPPER_STEAL_EXTRA)
		   << TDB_OFF_HASH_EXTRA_BIT);
}

/* FIXME: Our hash table handling here is primitive: we don't expand! */
static void add_to_hashtable(struct tdb_context *tdb,
			     tdb_off_t eoff,
			     struct tdb_data key)
{
	uint64_t h = tdb_hash(tdb, key.dptr, key.dsize);
	tdb_off_t b_off, group_start;
	unsigned i, group, in_group;
	unsigned used = 0;

	group = use_bits(h, TDB_TOPLEVEL_HASH_BITS-TDB_HASH_GROUP_BITS, &used);
	in_group = use_bits(h, TDB_HASH_GROUP_BITS, &used);

	group_start = offsetof(struct tdb_header, hashtable)
		+ group * (sizeof(tdb_off_t) << TDB_HASH_GROUP_BITS);

	for (i = 0; i < (1 << TDB_HASH_GROUP_BITS); i++) {
		unsigned bucket = (in_group + i) % (1 << TDB_HASH_GROUP_BITS);

		b_off = hbucket_off(group_start, bucket);		
		if (tdb_read_off(tdb, b_off) == 0) {
			tdb_write_off(tdb, b_off,
				      encode_offset(eoff, bucket, h));
			return;
		}
	}
	abort();
}

static struct tle_freetable *find_ftable(struct tdb_layout *layout, unsigned num)
{
	unsigned i;

	for (i = 0; i < layout->num_elems; i++) {
		if (layout->elem[i].base.type != FREETABLE)
			continue;
		if (num == 0)
			return &layout->elem[i].ftable;
		num--;
	}
	abort();
}

/* FIXME: Support TDB_CONVERT */
struct tdb_context *tdb_layout_get(struct tdb_layout *layout)
{
	unsigned int i;
	tdb_off_t off, len, last_ftable;
	char *mem;
	struct tdb_context *tdb;

	off = sizeof(struct tdb_header);

	/* First pass of layout: calc lengths */
	for (i = 0; i < layout->num_elems; i++) {
		union tdb_layout_elem *e = &layout->elem[i];
		e->base.off = off;
		switch (e->base.type) {
		case FREETABLE:
			len = freetable_len(&e->ftable);
			break;
		case FREE:
			len = free_record_len(e->free.len);
			break;
		case DATA:
			len = data_record_len(&e->used);
			break;
		case HASHTABLE:
			len = hashtable_len(&e->hashtable);
			break;
		default:
			abort();
		}
		off += len;
	}

	mem = malloc(off);
	/* Fill with some weird pattern. */
	memset(mem, 0x99, off);
	/* Now populate our header, cribbing from a real TDB header. */
	tdb = tdb_open(NULL, TDB_INTERNAL, O_RDWR, 0, &tap_log_attr);
	memcpy(mem, tdb->map_ptr, sizeof(struct tdb_header));

	/* Mug the tdb we have to make it use this. */
	free(tdb->map_ptr);
	tdb->map_ptr = mem;
	tdb->map_size = off;

	last_ftable = 0;
	for (i = 0; i < layout->num_elems; i++) {
		union tdb_layout_elem *e = &layout->elem[i];
		switch (e->base.type) {
		case FREETABLE:
			set_freetable(mem + e->base.off, tdb, &e->ftable,
				     (struct tdb_header *)mem, last_ftable);
			last_ftable = e->base.off;
			break;
		case FREE:
			set_free_record(mem + e->base.off, e->free.len);
			break;
		case DATA:
			set_data_record(mem + e->base.off, tdb, &e->used);
			break;
		case HASHTABLE:
			set_hashtable(mem + e->base.off, tdb, &e->hashtable);
			break;
		}
	}
	/* Must have a free table! */
	assert(last_ftable);

	/* Now fill the free and hash tables. */
	for (i = 0; i < layout->num_elems; i++) {
		union tdb_layout_elem *e = &layout->elem[i];
		switch (e->base.type) {
		case FREE:
			add_to_freetable(tdb, e->base.off, e->free.len,
					 e->free.ftable_num,
					 find_ftable(layout, e->free.ftable_num));
			break;
		case DATA:
			add_to_hashtable(tdb, e->base.off, e->used.key);
			break;
		default:
			break;
		}
	}

	tdb->ftable_off = find_ftable(layout, 0)->base.off;

	/* Get physical if they asked for it. */
	if (layout->filename) {
		int fd = open(layout->filename, O_WRONLY|O_TRUNC|O_CREAT,
			      0600);
		if (fd < 0)
			err(1, "opening %s for writing", layout->filename);
		if (write(fd, tdb->map_ptr, tdb->map_size) != tdb->map_size)
			err(1, "writing %s", layout->filename);
		close(fd);
		tdb_close(tdb);
		/* NOMMAP is for lockcheck. */
		tdb = tdb_open(layout->filename, TDB_NOMMAP, O_RDWR, 0,
			       &tap_log_attr);
	}

	return tdb;
}

void tdb_layout_free(struct tdb_layout *layout)
{
	unsigned int i;

	for (i = 0; i < layout->num_elems; i++) {
		if (layout->elem[i].base.type == DATA) {
			free(layout->elem[i].used.key.dptr);
			free(layout->elem[i].used.data.dptr);
		}
	}
	free(layout->elem);
	free(layout);
}
