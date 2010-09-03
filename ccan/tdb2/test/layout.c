/* TDB tools to create various canned database layouts. */
#include "layout.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "logging.h"

struct tdb_layout *new_tdb_layout(void)
{
	struct tdb_layout *layout = malloc(sizeof(*layout));
	layout->num_elems = 0;
	layout->elem = NULL;
	layout->htable = -1;
	return layout;
}

static void add(struct tdb_layout *layout, union tdb_layout_elem elem)
{
	layout->elem = realloc(layout->elem,
			       sizeof(layout->elem[0])
			       * (layout->num_elems+1));
	layout->elem[layout->num_elems++] = elem;
}

void tdb_layout_add_zone(struct tdb_layout *layout,
			 unsigned int zone_bits,
			 bool fill_prev)
{
	union tdb_layout_elem elem;
	if (fill_prev)
		tdb_layout_add_free(layout, 0);
	elem.base.type = ZONE;
	elem.zone.zone_bits = zone_bits;
	add(layout, elem);
}

void tdb_layout_add_free(struct tdb_layout *layout, tdb_len_t len)
{
	union tdb_layout_elem elem;
	elem.base.type = FREE;
	elem.free.len = len;
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

void tdb_layout_add_hashtable(struct tdb_layout *layout,
			      unsigned int hash_bits,
			      tdb_len_t extra)
{
	union tdb_layout_elem elem;
	elem.base.type = HASHTABLE;
	elem.hashtable.hash_bits = hash_bits;
	elem.hashtable.extra = extra;
	assert(layout->htable == -1U);
	layout->htable = layout->num_elems;
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
		+ (sizeof(tdb_off_t) << htable->hash_bits);
}

static tdb_len_t zone_header_len(struct tle_zone *zone)
{
	return sizeof(struct free_zone_header)
		+ sizeof(tdb_off_t) * (BUCKETS_FOR_ZONE(zone->zone_bits)+1);
}

static void set_free_record(void *mem, tdb_len_t len)
{
	/* We do all the work in add_to_freetable */
}

static void set_data_record(void *mem, struct tdb_context *tdb,
			    struct tle_zone *last_zone,
			    struct tle_used *used)
{
	struct tdb_used_record *u = mem;

	set_header(tdb, u, used->key.dsize, used->data.dsize,
		   used->key.dsize + used->data.dsize + used->extra,
		   tdb_hash(tdb, used->key.dptr, used->key.dsize),
		   last_zone->zone_bits);
	memcpy(u + 1, used->key.dptr, used->key.dsize);
	memcpy((char *)(u + 1) + used->key.dsize,
	       used->data.dptr, used->data.dsize);
}

static void set_hashtable(void *mem, struct tdb_context *tdb,
			  struct tle_zone *last_zone,
			  struct tle_hashtable *htable)
{
	struct tdb_used_record *u = mem;
	tdb_len_t len = sizeof(tdb_off_t) << htable->hash_bits;

	set_header(tdb, u, 0, len, len + htable->extra, 0,
		   last_zone->zone_bits);
	memset(u + 1, 0, len);
}

static void set_zone(void *mem, struct tdb_context *tdb,
		     struct tle_zone *zone)
{
	struct free_zone_header *fz = mem;
	memset(fz, 0, zone_header_len(zone));
	fz->zone_bits = zone->zone_bits;
}

static void add_to_freetable(struct tdb_context *tdb,
			     struct tle_zone *last_zone,
			     tdb_off_t eoff,
			     tdb_off_t elen)
{
	add_free_record(tdb, last_zone->zone_bits, eoff,
			sizeof(struct tdb_used_record) + elen);
}

static void add_to_hashtable(struct tdb_context *tdb,
			     tdb_off_t eoff,
			     struct tdb_data key)
{
	uint64_t hash = tdb_hash(tdb, key.dptr, key.dsize);
	tdb_off_t hoff;

	while (tdb_read_off(tdb, hoff = hash_off(tdb, hash)) != 0)
		hash++;

	tdb_write_off(tdb, hoff, eoff);
}

/* FIXME: Support TDB_CONVERT */
struct tdb_context *tdb_layout_get(struct tdb_layout *layout)
{
	unsigned int i;
	tdb_off_t off, len;
	tdb_len_t zone_left;
	struct tdb_header *hdr;
	char *mem;
	struct tdb_context *tdb;
	struct tle_zone *last_zone = NULL;

	assert(layout->htable != -1U);
	assert(layout->elem[0].base.type == ZONE);

	zone_left = 0;
	off = sizeof(struct tdb_header);

	/* First pass of layout: calc lengths */
	for (i = 0; i < layout->num_elems; i++) {
		union tdb_layout_elem *e = &layout->elem[i];
		e->base.off = off;
		switch (e->base.type) {
		case ZONE:
			assert(zone_left == 0);
			len = zone_header_len(&e->zone);
			zone_left = 1ULL << e->zone.zone_bits;
			break;
		case FREE:
			if (e->free.len == 0)
				e->free.len = zone_left
					- sizeof(struct tdb_used_record);
			len = free_record_len(e->free.len);
			break;
		case DATA:
			len = data_record_len(&e->used);
			break;
		case HASHTABLE:
			len = hashtable_len(&e->hashtable);
			break;
		}
		off += len;
		assert(zone_left >= len);
		zone_left -= len;
	}

	/* Fill final zone with free record. */
	if (zone_left != 0) {
		tdb_layout_add_free(layout,
				    zone_left
				    - sizeof(struct tdb_used_record));
		layout->elem[layout->num_elems-1].base.off = off;
		off += zone_left;
	}

	mem = malloc(off+1);
	/* Now populate our header, cribbing from a real TDB header. */
	tdb = tdb_open(NULL, TDB_INTERNAL, O_RDWR, 0, &tap_log_attr);
	hdr = (void *)mem;
	*hdr = tdb->header;
	hdr->v.generation++;
	hdr->v.hash_bits = layout->elem[layout->htable].hashtable.hash_bits;
	hdr->v.hash_off = layout->elem[layout->htable].base.off
		+ sizeof(struct tdb_used_record);

	/* Mug the tdb we have to make it use this. */
	free(tdb->map_ptr);
	tdb->map_ptr = mem;
	tdb->map_size = off+1;
	header_changed(tdb);

	for (i = 0; i < layout->num_elems; i++) {
		union tdb_layout_elem *e = &layout->elem[i];
		switch (e->base.type) {
		case ZONE:
			set_zone(mem + e->base.off, tdb, &e->zone);
			last_zone = &e->zone;
			break;
		case FREE:
			set_free_record(mem + e->base.off, e->free.len);
			break;
		case DATA:
			set_data_record(mem + e->base.off, tdb, last_zone,
					&e->used);
			break;
		case HASHTABLE:
			set_hashtable(mem + e->base.off, tdb, last_zone,
				      &e->hashtable);
			break;
		}
	}

	/* Now fill the free and hash tables. */
	for (i = 0; i < layout->num_elems; i++) {
		union tdb_layout_elem *e = &layout->elem[i];
		switch (e->base.type) {
		case ZONE:
			last_zone = &e->zone;
			break;
		case FREE:
			add_to_freetable(tdb, last_zone,
					 e->base.off, e->free.len);
			break;
		case DATA:
			add_to_hashtable(tdb, e->base.off, e->used.key);
			break;
		default:
			break;
		}
	}

	/* Write tailer. */
	((uint8_t *)tdb->map_ptr)[tdb->map_size-1] = last_zone->zone_bits;
	return tdb;
}
