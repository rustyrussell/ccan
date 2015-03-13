/* NTDB tools to create various canned database layouts. */
#include "layout.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ccan/err/err.h>
#include "logging.h"

struct ntdb_layout *new_ntdb_layout(void)
{
	struct ntdb_layout *layout = malloc(sizeof(*layout));
	layout->num_elems = 0;
	layout->elem = NULL;
	return layout;
}

static void add(struct ntdb_layout *layout, union ntdb_layout_elem elem)
{
	layout->elem = realloc(layout->elem,
			       sizeof(layout->elem[0])
			       * (layout->num_elems+1));
	layout->elem[layout->num_elems++] = elem;
}

void ntdb_layout_add_freetable(struct ntdb_layout *layout)
{
	union ntdb_layout_elem elem;
	elem.base.type = FREETABLE;
	add(layout, elem);
}

void ntdb_layout_add_free(struct ntdb_layout *layout, ntdb_len_t len,
			 unsigned ftable)
{
	union ntdb_layout_elem elem;
	elem.base.type = FREE;
	elem.free.len = len;
	elem.free.ftable_num = ftable;
	add(layout, elem);
}

void ntdb_layout_add_capability(struct ntdb_layout *layout,
			       uint64_t type,
			       bool write_breaks,
			       bool check_breaks,
			       bool open_breaks,
			       ntdb_len_t extra)
{
	union ntdb_layout_elem elem;
	elem.base.type = CAPABILITY;
	elem.capability.type = type;
	if (write_breaks)
		elem.capability.type |= NTDB_CAP_NOWRITE;
	if (open_breaks)
		elem.capability.type |= NTDB_CAP_NOOPEN;
	if (check_breaks)
		elem.capability.type |= NTDB_CAP_NOCHECK;
	elem.capability.extra = extra;
	add(layout, elem);
}

static NTDB_DATA dup_key(NTDB_DATA key)
{
	NTDB_DATA ret;
	ret.dsize = key.dsize;
	ret.dptr = malloc(ret.dsize);
	memcpy(ret.dptr, key.dptr, ret.dsize);
	return ret;
}

void ntdb_layout_add_used(struct ntdb_layout *layout,
			 NTDB_DATA key, NTDB_DATA data,
			 ntdb_len_t extra)
{
	union ntdb_layout_elem elem;
	elem.base.type = DATA;
	elem.used.key = dup_key(key);
	elem.used.data = dup_key(data);
	elem.used.extra = extra;
	add(layout, elem);
}

static ntdb_len_t free_record_len(ntdb_len_t len)
{
	return sizeof(struct ntdb_used_record) + len;
}

static ntdb_len_t data_record_len(struct tle_used *used)
{
	ntdb_len_t len;
	len = sizeof(struct ntdb_used_record)
		+ used->key.dsize + used->data.dsize + used->extra;
	assert(len >= sizeof(struct ntdb_free_record));
	return len;
}

static ntdb_len_t capability_len(struct tle_capability *cap)
{
	return sizeof(struct ntdb_capability) + cap->extra;
}

static ntdb_len_t freetable_len(struct tle_freetable *ftable)
{
	return sizeof(struct ntdb_freetable);
}

static void set_free_record(void *mem, ntdb_len_t len)
{
	/* We do all the work in add_to_freetable */
}

static void add_zero_pad(struct ntdb_used_record *u, size_t len, size_t extra)
{
	if (extra)
		((char *)(u + 1))[len] = '\0';
}

static void set_data_record(void *mem, struct ntdb_context *ntdb,
			    struct tle_used *used)
{
	struct ntdb_used_record *u = mem;

	set_header(ntdb, u, NTDB_USED_MAGIC, used->key.dsize, used->data.dsize,
		   used->key.dsize + used->data.dsize + used->extra);
	memcpy(u + 1, used->key.dptr, used->key.dsize);
	memcpy((char *)(u + 1) + used->key.dsize,
	       used->data.dptr, used->data.dsize);
	add_zero_pad(u, used->key.dsize + used->data.dsize, used->extra);
}

static void set_capability(void *mem, struct ntdb_context *ntdb,
			   struct tle_capability *cap, struct ntdb_header *hdr,
			   ntdb_off_t last_cap)
{
	struct ntdb_capability *c = mem;
	ntdb_len_t len = sizeof(*c) - sizeof(struct ntdb_used_record) + cap->extra;

	c->type = cap->type;
	c->next = 0;
	set_header(ntdb, &c->hdr, NTDB_CAP_MAGIC, 0, len, len);

	/* Append to capability list. */
	if (!last_cap) {
		hdr->capabilities = cap->base.off;
	} else {
		c = (struct ntdb_capability *)((char *)hdr + last_cap);
		c->next = cap->base.off;
	}
}

static void set_freetable(void *mem, struct ntdb_context *ntdb,
			 struct tle_freetable *freetable, struct ntdb_header *hdr,
			 ntdb_off_t last_ftable)
{
	struct ntdb_freetable *ftable = mem;
	memset(ftable, 0, sizeof(*ftable));
	set_header(ntdb, &ftable->hdr, NTDB_FTABLE_MAGIC, 0,
			sizeof(*ftable) - sizeof(ftable->hdr),
			sizeof(*ftable) - sizeof(ftable->hdr));

	if (last_ftable) {
		ftable = (struct ntdb_freetable *)((char *)hdr + last_ftable);
		ftable->next = freetable->base.off;
	} else {
		hdr->free_table = freetable->base.off;
	}
}

static void add_to_freetable(struct ntdb_context *ntdb,
			     ntdb_off_t eoff,
			     ntdb_off_t elen,
			     unsigned ftable,
			     struct tle_freetable *freetable)
{
	ntdb->ftable_off = freetable->base.off;
	ntdb->ftable = ftable;
	add_free_record(ntdb, eoff, sizeof(struct ntdb_used_record) + elen,
			NTDB_LOCK_WAIT, false);
}

/* Get bits from a value. */
static uint32_t bits(uint64_t val, unsigned start, unsigned num)
{
	assert(num <= 32);
	return (val >> start) & ((1U << num) - 1);
}

static ntdb_off_t encode_offset(const struct ntdb_context *ntdb,
				ntdb_off_t new_off, uint32_t hash)
{
	ntdb_off_t extra;

	assert((new_off & (1ULL << NTDB_OFF_CHAIN_BIT)) == 0);
	assert((new_off >> (64 - NTDB_OFF_UPPER_STEAL)) == 0);
	/* We pack extra hash bits into the upper bits of the offset. */
	extra = bits(hash, ntdb->hash_bits, NTDB_OFF_UPPER_STEAL);
	extra <<= (64 - NTDB_OFF_UPPER_STEAL);

	return new_off | extra;
}

static ntdb_off_t hbucket_off(ntdb_len_t idx)
{
	return sizeof(struct ntdb_header) + sizeof(struct ntdb_used_record)
		+ idx * sizeof(ntdb_off_t);
}

/* FIXME: Our hash table handling here is primitive: we don't expand! */
static void add_to_hashtable(struct ntdb_context *ntdb,
			     ntdb_off_t eoff,
			     NTDB_DATA key)
{
	ntdb_off_t b_off;
	uint32_t h = ntdb_hash(ntdb, key.dptr, key.dsize);

	b_off = hbucket_off(h & ((1 << ntdb->hash_bits)-1));
	if (ntdb_read_off(ntdb, b_off) != 0)
		abort();

	ntdb_write_off(ntdb, b_off, encode_offset(ntdb, eoff, h));
}

static struct tle_freetable *find_ftable(struct ntdb_layout *layout, unsigned num)
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

/* FIXME: Support NTDB_CONVERT */
struct ntdb_context *ntdb_layout_get(struct ntdb_layout *layout,
				   void (*freefn)(void *),
				   union ntdb_attribute *attr)
{
	unsigned int i;
	ntdb_off_t off, hdrlen, len, last_ftable, last_cap;
	char *mem;
	struct ntdb_context *ntdb;

	/* Now populate our header, cribbing from a real NTDB header. */
	ntdb = ntdb_open("layout", NTDB_INTERNAL, O_RDWR, 0, attr);

	off = sizeof(struct ntdb_header) + sizeof(struct ntdb_used_record)
		+ (sizeof(ntdb_off_t) << ntdb->hash_bits);
	hdrlen = off;

	/* First pass of layout: calc lengths */
	for (i = 0; i < layout->num_elems; i++) {
		union ntdb_layout_elem *e = &layout->elem[i];
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
		case CAPABILITY:
			len = capability_len(&e->capability);
			break;
		default:
			abort();
		}
		off += len;
	}

	mem = malloc(off);
	/* Fill with some weird pattern. */
	memset(mem, 0x99, off);
	memcpy(mem, ntdb->file->map_ptr, hdrlen);

	/* Mug the ntdb we have to make it use this. */
	freefn(ntdb->file->map_ptr);
	ntdb->file->map_ptr = mem;
	ntdb->file->map_size = off;

	last_ftable = 0;
	last_cap = 0;
	for (i = 0; i < layout->num_elems; i++) {
		union ntdb_layout_elem *e = &layout->elem[i];
		switch (e->base.type) {
		case FREETABLE:
			set_freetable(mem + e->base.off, ntdb, &e->ftable,
				     (struct ntdb_header *)mem, last_ftable);
			last_ftable = e->base.off;
			break;
		case FREE:
			set_free_record(mem + e->base.off, e->free.len);
			break;
		case DATA:
			set_data_record(mem + e->base.off, ntdb, &e->used);
			break;
		case CAPABILITY:
			set_capability(mem + e->base.off, ntdb, &e->capability,
				       (struct ntdb_header *)mem, last_cap);
			last_cap = e->base.off;
			break;
		}
	}
	/* Must have a free table! */
	assert(last_ftable);

	/* Now fill the free and hash tables. */
	for (i = 0; i < layout->num_elems; i++) {
		union ntdb_layout_elem *e = &layout->elem[i];
		switch (e->base.type) {
		case FREE:
			add_to_freetable(ntdb, e->base.off, e->free.len,
					 e->free.ftable_num,
					 find_ftable(layout, e->free.ftable_num));
			break;
		case DATA:
			add_to_hashtable(ntdb, e->base.off, e->used.key);
			break;
		default:
			break;
		}
	}

	ntdb->ftable_off = find_ftable(layout, 0)->base.off;
	return ntdb;
}

void ntdb_layout_write(struct ntdb_layout *layout, void (*freefn)(void *),
		       union ntdb_attribute *attr, const char *filename)
{
	struct ntdb_context *ntdb = ntdb_layout_get(layout, freefn, attr);
	int fd;

	fd = open(filename, O_WRONLY|O_TRUNC|O_CREAT,  0600);
	if (fd < 0)
		err(1, "opening %s for writing", filename);
	if (write(fd, ntdb->file->map_ptr, ntdb->file->map_size)
	    != ntdb->file->map_size)
		err(1, "writing %s", filename);
	close(fd);
	ntdb_close(ntdb);
}

void ntdb_layout_free(struct ntdb_layout *layout)
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
