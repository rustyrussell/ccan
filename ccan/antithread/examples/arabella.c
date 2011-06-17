/* Antithread example to approximate picture with triangles using
 * genetic algorithm.   Example for a 2 cpu system:
 *	./arabella arabella.jpg out out.save 2
 */
#include <stdio.h>
#include <jpeglib.h>
#include <ccan/talloc/talloc.h>
#include <err.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <sys/select.h>
#include <ccan/str/str.h>
#include <ccan/antithread/antithread.h>
#include <sys/types.h>
#include <unistd.h>

// Define this to run 100 times without dumping state
//#define BENCHMARK

/* How many drawings in entire population. */
#define POPULATION_SIZE 1000

/* How many generations without 1% improvement before we terminate. */
#define PLATEAU_GENS 200

/* An image buffer to render into. */
struct image {
	unsigned height, width;
	unsigned int stride;
	/* RGB data */
	unsigned char *buffer;
};

/* A drawing is a (fixed) number of triangles. */
struct drawing {
	struct triangle *tri;
	unsigned int num_tris;
	unsigned long score;
};

/* Here's a triangle. */
struct triangle {
	struct {
		unsigned int x, y;
	} coord[3];

	/* RGBA */
	unsigned char color[4];
	unsigned char mult;
	uint16_t add[3];
};

/* Get pointer into image at a specific location. */
static unsigned char *image_at(struct image *image,
			       unsigned int x, unsigned int y)
{
	return image->buffer + y * image->stride + x * 3;
}

/* Blend a dot into this location. */
static void add_dot(unsigned char *buf,
		    unsigned char mult, const uint16_t add[])
{
	unsigned int c;
	/* /256 isn't quite right, but it's much faster than /255 */
	for (c = 0; c < 3; c++)
		buf[c] = (buf[c] * mult + add[c]) / 256;
}

/* Code based on example taken from:
 * http://www.devmaster.net/forums/showthread.php?t=1094 */
static void add_flat_triangle(struct image *image,
			      int x0, int y0, int x1, int y1, int x2, int y2,
			      unsigned char mult, const uint16_t add[])
{
	unsigned char *buf;
	unsigned int i, j;

        // compute slopes for the two triangle legs
        float dx0 = (float)(x2 - x0) / (y2 - y0);
        float dx1 = (float)(x2 - x1) / (y2 - y1);
        
        int yRange = 0;
        
        float lx = (float) x0, rx = (float) x1;
        
        if (y0 < y2) { 
		yRange = y2 - y0;
		buf = image_at(image, 0, y0);
	} else {
		yRange = y0 - y2;
		buf = image_at(image, 0, y2);
		lx = rx = (float)x2;
	}

        for (i=0; i < yRange; ++i) {
		for (j=(int)(lx); j<(int)((rx) + 1.0f); ++j)
			add_dot(buf + 3 * j, mult, add);
        
		lx  += dx0;
		rx  += dx1;
		buf += image->stride;
        }
}

static void swap(int *a, int *b)
{
	int tmp = *a;
	*a = *b;
	*b = tmp;
}

static void paint_triangle(struct image *image, const struct triangle *tri)
{
	int i0 = 0, i1 = 1, i2 = 2;
	int x0, y0, x1, y1, x2, y2;

	/* Could do this on triangle creation. */
        // sort verts by height
        if (tri->coord[i0].y > tri->coord[i1].y) swap(&i0, &i1);
        if (tri->coord[i0].y > tri->coord[i2].y) swap(&i0, &i2);
        if (tri->coord[i1].y > tri->coord[i2].y) swap(&i1, &i2);

	x0 = tri->coord[i0].x, y0 = tri->coord[i0].y;
        x1 = tri->coord[i1].x, y1 = tri->coord[i1].y;
        x2 = tri->coord[i2].x, y2 = tri->coord[i2].y;
        
        // test for easy cases, else split trinagle in two and render both halfs
        if (y1 == y2) {
 		if (x1 > x2) swap(&x1, &x2);
		add_flat_triangle(image, x1, y1, x2, y2, x0, y0,
				  tri->mult, tri->add);
        } else if (y0 == y1) {
 		if (x0 > x1) swap(&x0, &x1);
		add_flat_triangle(image, x0, y0, x1, y1, x2, y2,
				  tri->mult, tri->add);
        } else {
 		// compute x pos of the vert that builds the splitting line with x1
		int tmp_x = x0 + (int)(0.5f + (float)(y1-y0) * (float)(x2-x0) / (float)(y2-y0));
 
		if (x1 > tmp_x) swap(&x1, &tmp_x);
		add_flat_triangle(image, x1, y1, tmp_x, y1, x0, y0,
				  tri->mult, tri->add);
		add_flat_triangle(image, x1, y1, tmp_x, y1, x2, y2,
				  tri->mult, tri->add);
        }
}

/* Create a new image, allocated off context. */
static struct image *new_image(const void *ctx,
			       unsigned width, unsigned height, unsigned stride)
{
	struct image *image = talloc(ctx, struct image);

	image->width = width;
	image->height = height;
	image->stride = stride;
	image->buffer = talloc_zero_array(image, unsigned char,
					  stride * height);
	return image;
}

/* Taken from JPEG example code.  Quality is 1 to 100. */
static void write_jpeg_file(const struct image *image,
			    const char *filename, int quality)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE *outfile;
	JSAMPROW row_pointer[1];
	int row_stride;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	if ((outfile = fopen(filename, "wb")) == NULL)
		err(1, "can't open %s", filename);

	jpeg_stdio_dest(&cinfo, outfile);

	cinfo.image_width = image->width;
	cinfo.image_height = image->height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);

	jpeg_start_compress(&cinfo, TRUE);
	row_stride = image->width * 3;

	while (cinfo.next_scanline < cinfo.image_height) {
		row_pointer[0] = image->buffer + cinfo.next_scanline*row_stride;
		(void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	fclose(outfile);

	jpeg_destroy_compress(&cinfo);
}

static struct image *read_jpeg_file(const void *ctx, const char *filename)
{
	struct jpeg_decompress_struct cinfo;
	struct image *image;
	struct jpeg_error_mgr jerr;
	FILE *infile;
	int row_stride;

	if ((infile = fopen(filename, "rb")) == NULL)
		err(1, "can't open %s", filename);

	cinfo.err = jpeg_std_error(&jerr);

	jpeg_create_decompress(&cinfo);

	jpeg_stdio_src(&cinfo, infile);

	jpeg_read_header(&cinfo, TRUE);

	jpeg_start_decompress(&cinfo);
	row_stride = cinfo.output_width * cinfo.output_components;

	image = new_image(ctx,
			  cinfo.output_width, cinfo.output_height, row_stride);
	while (cinfo.output_scanline < cinfo.output_height) {
		JSAMPROW row = &image->buffer[cinfo.output_scanline*row_stride];
		jpeg_read_scanlines(&cinfo, &row, 1);
	}

	(void) jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	fclose(infile);
	return image;
}

/* Higher means closer to perfect match.  We assume images same size. */
static unsigned long compare_images(const struct image *a,
				    const struct image *b)
{
	unsigned long result = 0;
	unsigned i;

	/* Huge images won't work here.  We'd need to get cleverer. */
	assert(a->height * a->stride < ULONG_MAX / 8);

	for (i = 0; i < a->height * a->stride; i++) {
		if (a->buffer[i] > b->buffer[i])
			result += a->buffer[i] - b->buffer[i];
		else
			result += b->buffer[i] - a->buffer[i];
	}
	return result;
}

/* Precalculate the alpha adds and multiplies for this color/alpha combo. */
static void calc_multipliers(struct triangle *tri)
{
	/* Multiply by 255 - alpha. */
	tri->mult = (255 - tri->color[3]);
	/* Add alpha * color */
	tri->add[0] = (tri->color[0] * tri->color[3]);
	tri->add[1] = (tri->color[1] * tri->color[3]);
	tri->add[2] = (tri->color[2] * tri->color[3]);
}

/* Render the image of this drawing, and return it. */
static struct image *image_of_drawing(const void *ctx,
				      const struct drawing *drawing,
				      const struct image *master)
{
	struct image *image;
	unsigned int i;

	image = new_image(ctx, master->width, master->height, master->stride);

	for (i = 0; i < drawing->num_tris; i++)
		paint_triangle(image, &drawing->tri[i]);
	return image;
}

/* Render the image and compare with the master. */
static void score_drawing(struct drawing *drawing,
			  const struct image *master)
{
	struct image *image;

	/* We don't allocate it off the drawing, since we don't need
	 * it inside the shared area. */
	image = image_of_drawing(NULL, drawing, master);
	drawing->score = compare_images(image, master);
	talloc_free(image);
}

/* Create a new drawing allocated off context (which is the antithread
 * pool context, so this is all allocated inside the pool so the
 * antithreads can access it).
 */
static struct drawing *new_drawing(const void *ctx, unsigned int num_tris)
{
	struct drawing *drawing = talloc_zero(ctx, struct drawing);
	drawing->num_tris = num_tris;
	drawing->tri = talloc_array(drawing, struct triangle, num_tris);
	return drawing;
}

/* Make a random change to the drawing: frob one triangle. */
static void mutate_drawing(struct drawing *drawing,
			   const struct image *master)
{
	unsigned int i, r = random();
	struct triangle *tri = &drawing->tri[r % drawing->num_tris];

	r /= drawing->num_tris;
	r %= 12;
	if (r < 6) {
		/* Move one corner in x or y dir. */
		if (r % 2)
			tri->coord[r/2].x = random() % master->width;
		else
			tri->coord[r/2].y = random() % master->height;
	} else if (r < 10) {
		/* Change one aspect of color. */
		tri->color[r - 6] = random() % 256;
	} else if (r == 10) {
		/* Completely move a triangle. */
		for (i = 0; i < 3; i++) {
			tri->coord[i].x = random() % master->width;
			tri->coord[i].y = random() % master->height;
		}
	} else {
		/* Completely change a triangle's colour. */
		for (i = 0; i < 4; i++)
			tri->color[i] = random() % 256;
	}
	calc_multipliers(tri);
}

/* Breed two drawings together, and throw in a mutation. */
static struct drawing *breed_drawing(const void *ctx,
				     const struct drawing *a,
				     const struct drawing *b,
				     const struct image *master)
{
	unsigned int i;
	struct drawing *drawing;
	unsigned int r = random(), randmax = RAND_MAX;

	assert(a->num_tris == b->num_tris);
	drawing = new_drawing(ctx, a->num_tris);

	for (i = 0; i < a->num_tris; i++) {
		switch (r & 1) {
		case 0:
			/* Take from A. */
			drawing->tri[i] = a->tri[i];
			break;
		case 1:
			/* Take from B. */
			drawing->tri[i] = b->tri[i];
			break;
		}
		r >>= 1;
		randmax >>= 1;
		if (randmax == 0) {
			r = random();
			randmax = RAND_MAX;
		}
	}
	mutate_drawing(drawing, master);
	score_drawing(drawing, master);
	return drawing;
}

/* This is our anti-thread.  It does the time-consuming operation of
 * breeding two drawings together and scoring the result. */
static void *breeder(struct at_pool *atp, struct image *master)
{
	const struct drawing *a, *b;

	/* For simplicity, controller just hands us two pointers in
	 * separate writes.  It could put them in the pool for us to
	 * read. */
	while ((a = at_read_parent(atp)) != NULL) {
		struct drawing *child;
		b = at_read_parent(atp);

		child = breed_drawing(at_pool_ctx(atp), a, b, master);
		at_tell_parent(atp, child);
	}
	/* Unused: we never exit. */
	return NULL;
}

/* We keep a very rough count of how much work each athread has.  This
 * works fine since fairly all rendering takes about the same time.
 *
 * Better alternative would be to put all the pending work somewhere
 * in the shared area and notify any idle thread.  The threads would
 * keep looking in that shared area until they can't see any more
 * work, then they'd at_tell_parent() back. */
struct athread_work {
	struct athread *at;
	unsigned int pending;
};

/* It's assumed that there isn't more than num results pending. */
static unsigned gather_results(struct athread_work *athreads,
			       unsigned int num_threads,
			       struct drawing **drawing,
			       unsigned int num,
			       bool block)
{
	unsigned int i, maxfd = 0, done = 0;
	struct timeval zero = { .tv_sec = 0, .tv_usec = 0 };

	/* If it mattered, we could cache this fd mask and maxfd. */
	for (i = 0; i < num_threads; i++) {
		if (at_fd(athreads[i].at) > maxfd)
			maxfd = at_fd(athreads[i].at);
	}

	do {
		fd_set set;
		FD_ZERO(&set);
		for (i = 0; i < num_threads; i++)
			FD_SET(at_fd(athreads[i].at), &set);

		if (select(maxfd+1, &set, NULL, NULL, block ? NULL : &zero) < 0)
			err(1, "Selecting on antithread results");

		for (i = 0; i < num_threads; i++) {
			if (!FD_ISSET(at_fd(athreads[i].at), &set))
				continue;
			*drawing = at_read(athreads[i].at);
			if (!*drawing)
				err(1, "Error with thread %u", i);
			drawing++;
			num--;
			athreads[i].pending--;
			done++;
		}
	} while (block && num);

	return done;
}

/* Hand work to an antithread to breed these two together. */
static void tell_some_breeder(struct athread_work *athreads,
			      unsigned int num_threads,
			      const struct drawing *a, const struct drawing *b)
{
	unsigned int i, best = 0;

	/* Find least loaded thread. */
	for (i = 1; i < num_threads; i++) {
		if (athreads[i].pending < athreads[best].pending)
			best = i;
	}

	at_tell(athreads[best].at, a);
	at_tell(athreads[best].at, b);
	athreads[best].pending++;
}

/* We seed initial triangles colours from the master image. */
static const unsigned char *initial_random_color(const struct image *master)
{
	return master->buffer + (random() % (master->height * master->width))*3;
}

/* Create an initial random drawing. */
static struct drawing *random_drawing(const void *ctx,
				      const struct image *master,
				      unsigned int num_tris)
{
	struct drawing *drawing = new_drawing(ctx, num_tris);
	unsigned int i;

	for (i = 0; i < drawing->num_tris; i++) {
		unsigned int c;
		struct triangle *tri = &drawing->tri[i];
		for (c = 0; c < 3; c++) {
			tri->coord[c].x = random() % master->width;
			tri->coord[c].y = random() % master->height;
		}
		memcpy(tri->color, initial_random_color(master), 3);
		tri->color[3] = (random() % 255) + 1;
		calc_multipliers(tri);
	}
	score_drawing(drawing, master);
	return drawing;
}

/* Read in a drawing from the saved state file. */
static struct drawing *read_drawing(const void *ctx, FILE *in,
				    const struct image *master,
				    unsigned int *generation)
{
	struct drawing *drawing;
	unsigned int i;

	if (fscanf(in, "%u triangles, generation %u:\n", &i, generation) != 2)
		errx(1, "Reading saved state");
	drawing = new_drawing(ctx, i);
	for (i = 0; i < drawing->num_tris; i++) {
		unsigned int color[4];
		if (fscanf(in, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
			   &drawing->tri[i].coord[0].x,
			   &drawing->tri[i].coord[0].y,
			   &drawing->tri[i].coord[1].x,
			   &drawing->tri[i].coord[1].y,
			   &drawing->tri[i].coord[2].x,
			   &drawing->tri[i].coord[2].y,
			   &color[0], &color[1], &color[2], &color[3]) != 10)
			errx(1, "Reading saved state");
		drawing->tri[i].color[0] = color[0];
		drawing->tri[i].color[1] = color[1];
		drawing->tri[i].color[2] = color[2];
		drawing->tri[i].color[3] = color[3];
		calc_multipliers(&drawing->tri[i]);
	}
	score_drawing(drawing, master);
	return drawing;
}

/* Comparison function for sorting drawings best to worst. */
static int compare_drawing_scores(const void *_a, const void *_b)
{
	struct drawing **a = (void *)_a, **b = (void *)_b;

	return (*a)->score - (*b)->score;
}

/* Save one drawing to state file */
static void dump_drawings(struct drawing **drawing, const char *outname,
			  unsigned int generation)
{
	FILE *out;
	unsigned int i, j;
	char *tmpout = talloc_asprintf(NULL, "%s.tmp", outname);

	out = fopen(tmpout, "w");
	if (!out)
		err(1, "Opening %s", tmpout);
	fprintf(out, "POPULATION_SIZE=%u\n", POPULATION_SIZE);
	for (i = 0; i < POPULATION_SIZE; i++) {
		fprintf(out, "%u triangles, generation %u:\n",
			drawing[i]->num_tris, generation);
		for (j = 0; j < drawing[i]->num_tris; j++) {
			fprintf(out, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
				drawing[i]->tri[j].coord[0].x,
				drawing[i]->tri[j].coord[0].y,
				drawing[i]->tri[j].coord[1].x,
				drawing[i]->tri[j].coord[1].y,
				drawing[i]->tri[j].coord[2].x,
				drawing[i]->tri[j].coord[2].y,
				drawing[i]->tri[j].color[0],
				drawing[i]->tri[j].color[1],
				drawing[i]->tri[j].color[2],
				drawing[i]->tri[j].color[3]);
		}
	}
	if (fclose(out) != 0)
		err(1, "Failure closing %s", tmpout);

	if (rename(tmpout, outname) != 0)
		err(1, "Renaming %s over %s", tmpout, outname);
	talloc_free(tmpout);
}

/* Save state file. */
static void dump_state(struct drawing *drawing[POPULATION_SIZE],
		       const struct image *master,
		       const char *outpic,
		       const char *outstate,
		       unsigned int gen)
{
	char *out = talloc_asprintf(NULL, "%s.%08u.jpg", outpic, gen);
	struct image *image;
	printf("Dumping gen %u to %s & %s\n", gen, out, outstate);
	dump_drawings(drawing, outstate, gen);
	image = image_of_drawing(out, drawing[0], master);
	write_jpeg_file(image, out, 80);
	talloc_free(out);
}

/* Biassed coinflip moves us towards top performers.  I didn't spend
 * too much time on it, but 1/32 seems to give decent results (see
 * breeding-algos.gnumeric). */
static struct drawing *select_good_drawing(struct drawing *drawing[],
					   unsigned int population)
{
	if (population == 1)
		return drawing[0];
	if (random() % 32)
		return select_good_drawing(drawing, population/2);
	return select_good_drawing(drawing + population/2, population/2);
}

static void usage(void)
{
	errx(1, "usage: <infile> <outfile> <statefile> <numtriangles> <numcpus> [<instatefile>]");
}

int main(int argc, char *argv[])
{
	struct image *master;
	unsigned int gen, since_prev_best, num_threads, i;
	struct drawing *drawing[POPULATION_SIZE];
	unsigned long prev_best, poolsize;
	struct at_pool *atp;
	struct athread_work *athreads;

	if (argc != 6 && argc != 7)
		usage();

	/* Room for triangles and master image, with some spare.
	 * We should really read master image header first, so we can be
	 * more precise than "about 3MB".  ccan/alloc also needs some 
	 * more work to be more efficient. */
	poolsize = (POPULATION_SIZE + POPULATION_SIZE/4) * (sizeof(struct drawing) + atoi(argv[4]) * sizeof(struct triangle)) * 2 + 1000 * 1000 * 3;
	atp = at_pool(poolsize);
	if (!atp)
		err(1, "Creating pool of %lu bytes", poolsize);

	/* Auto-free the pool and anything hanging off it (eg. threads). */
	talloc_steal(talloc_autofree_context(), atp);

	/* Read in file */
	master = read_jpeg_file(at_pool_ctx(atp), argv[1]);

	if (argc == 6) {
		printf("Creating initial population");
		fflush(stdout);
		for (i = 0; i < POPULATION_SIZE; i++) {
			drawing[i] = random_drawing(at_pool_ctx(atp),
						    master, atoi(argv[4]));
			printf(".");
			fflush(stdout);
		}
		printf("\n");
		gen = 0;
	} else {
		FILE *state;
		char header[100];
		state = fopen(argv[6], "r");
		if (!state)
			err(1, "Opening %s", argv[6]);
		fflush(stdout);
		fgets(header, 100, state);
		printf("Loading initial population from %s: %s", argv[6],
			header);
		for (i = 0; i < POPULATION_SIZE; i++) {
			drawing[i] = read_drawing(at_pool_ctx(atp),
						  state, master, &gen);
			gen++;	/* We start working on the _next_ gen */
			printf(".");
			fflush(stdout);
		}
	}

	num_threads = atoi(argv[5]);
	if (!num_threads)
		usage();

	/* Hang the threads off the pool (not *in* the pool). */
	athreads = talloc_array(atp, struct athread_work, num_threads);
	for (i = 0; i < num_threads; i++) {
		athreads[i].pending = 0;
		athreads[i].at = at_run(atp, breeder, master);
		if (!athreads[i].at)
			err(1, "Creating antithread %u", i);
	}

	since_prev_best = 0;
	/* Worse than theoretically worst case. */
	prev_best = master->height * master->stride * 256;

	while (since_prev_best < PLATEAU_GENS) {
		unsigned int j, done = 0;
		struct drawing *new[POPULATION_SIZE/4];

		qsort(drawing, POPULATION_SIZE, sizeof(drawing[0]),
		      compare_drawing_scores);

		printf("Best %lu, worst %lu\n",
		       drawing[0]->score, drawing[POPULATION_SIZE-1]->score);

		/* Probability of being chosen to breed depends on
		 * rank.  We breed over lowest 1/4 population. */
		for (j = 0; j < POPULATION_SIZE / 4; j++) {
			struct drawing *best1, *best2;

			best1 = select_good_drawing(drawing, POPULATION_SIZE);
			best2 = select_good_drawing(drawing, POPULATION_SIZE);

			tell_some_breeder(athreads, num_threads, best1, best2);

			/* We reap during loop, so return pipes don't fill.
			 * See "Better alternative" above. */
			done += gather_results(athreads, num_threads,
					       new + done, j - done, false);
		}

		/* Collate final results. */
		gather_results(athreads, num_threads, new+done, j-done, true);

		/* Overwrite bottom 1/4 */
		for (j = POPULATION_SIZE * 3 / 4; j < POPULATION_SIZE; j++) {
			talloc_free(drawing[j]);
			drawing[j] = new[j - POPULATION_SIZE * 3 / 4];
		}

		/* We dump on every 1% improvement in score. */
		if (drawing[0]->score < prev_best * 0.99) {
#ifndef BENCHMARK
			dump_state(drawing, master, argv[2], argv[3], gen);
#endif
			prev_best = drawing[0]->score;
			since_prev_best = 0;
		} else
			since_prev_best++;

#ifdef BENCHMARK
		if (gen == 100)
			exit(0);
#endif
		gen++;
	}

	/* Dump final state */
	printf("No improvement over %lu for %u gens\n",
	       prev_best, since_prev_best);
	dump_state(drawing, master, argv[2], argv[3], gen);
	return 0;
}		     
