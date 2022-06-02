/* MIT (BSD) license - see LICENSE file for details */
#include <ccan/ungraph/ungraph.h>
#include <ccan/tal/str/str.h>

struct xy {
	size_t x, y;
};

struct text {
	struct xy xy;
	size_t width;
	const char *text;
	/* If it's a node, this is non-NULL */
	void *node;
	/* NULL if none found, edge it one found, self if > 1 */
	struct edge *nearest_edge;
};

struct edge {
	struct text *src, *dst;
	bool bidir;
	const char **labels;
};

/* This means we actually found two "nearest_edge" */
static struct edge fake_edge;

#define EDGES "+/-\\|"
#define ARROWS "<^>v"

enum dir {
	UP,
	UP_RIGHT,
	RIGHT,
	DOWN_RIGHT,
	DOWN,
	DOWN_LEFT,
	LEFT,
	UP_LEFT,
	INVALID,
};

static enum dir opposite_dir(enum dir dir)
{
	return (dir + 4) % 8;
}

static enum dir clockwise(enum dir dir)
{
	return (dir + 1) % 8;
}

static enum dir anticlockwise(enum dir dir)
{
	return (dir + 7) % 8;
}

static enum dir dir_away(const struct text *t, struct xy xy)
{
	int xdir, ydir;
	enum dir dirs[3][3] = {{UP_LEFT, UP, UP_RIGHT},
			       {LEFT, INVALID, RIGHT},
			       {DOWN_LEFT, DOWN, DOWN_RIGHT}};
	
	if (xy.y < t->xy.y)
		ydir = -1;
	else if (xy.y > t->xy.y)
		ydir = 1;
	else
		ydir = 0;
	if (xy.x >= t->xy.x + t->width)
		xdir = 1;
	else if (xy.x < t->xy.x)
		xdir = -1;
	else
		xdir = 0;

	return dirs[ydir+1][xdir+1];
}

static char line_for_dir(enum dir dir)
{
	switch (dir) {
	case UP:
	case DOWN:
		return '|';
	case UP_RIGHT:
	case DOWN_LEFT:
		return '/';
	case RIGHT:
	case LEFT:
		return '-';
	case DOWN_RIGHT:
	case UP_LEFT:
		return '\\';
	case INVALID:
		break;
	}
	abort();
}

static char arrow_for_dir(enum dir dir)
{
	switch (dir) {
	case UP:
	case UP_RIGHT:
	case UP_LEFT:
		return '^';
	case DOWN:
	case DOWN_RIGHT:
	case DOWN_LEFT:
		return 'v';
	case LEFT:
		return '<';
	case RIGHT:
		return '>';
	case INVALID:
		break;
	}
	abort();
}

static struct xy move_in_dir(struct xy xy, enum dir dir)
{
	switch (dir) {
	case UP:
		xy.y = xy.y - 1;
		return xy;
	case DOWN:
		xy.y = xy.y + 1;
		return xy;
	case UP_RIGHT:
		xy.x = xy.x + 1;
		xy.y = xy.y - 1;
		return xy;
	case DOWN_LEFT:
		xy.x = xy.x - 1;
		xy.y = xy.y + 1;
		return xy;
	case RIGHT:
		xy.x = xy.x + 1;
		return xy;
	case LEFT:
		xy.x = xy.x - 1;
		return xy;
	case DOWN_RIGHT:
		xy.x = xy.x + 1;
		xy.y = xy.y + 1;
		return xy;
	case UP_LEFT:
		xy.x = xy.x - 1;
		xy.y = xy.y - 1;
		return xy;
	case INVALID:
		break;
	}
	abort();
}

static char *sqc(char **sq, struct xy xy)
{
	return &sq[xy.y][xy.x];
}

/* Try straight ahead first, then a bit to either side, then
 * finally left and right */
static struct xy scan_move_next(struct xy xy, enum dir start, enum dir *cur)
{
	if (*cur == start)
		*cur = clockwise(start);
	else if (*cur == clockwise(start))
		*cur = anticlockwise(start);
	else if (*cur == anticlockwise(start))
		*cur = anticlockwise(anticlockwise(start));
	else if (*cur == anticlockwise(anticlockwise(start)))
		*cur = clockwise(clockwise(start));
	else {
		*cur = INVALID;
		return xy;
	}
	return move_in_dir(xy, *cur);
}

static void start_perimeter(struct xy *xyp, enum dir *dirp, struct xy xy)
{
	*dirp = RIGHT;
	xyp->x = xy.x - 1;
	xyp->y = xy.y - 1;
}

static void next_perimeter(struct xy *xyp, enum dir *dirp, struct xy xy, size_t width)
{
	*xyp = move_in_dir(*xyp, *dirp);
	if (*dirp == RIGHT && xyp->x == xy.x + width)
		*dirp = DOWN;
	else if (*dirp == DOWN && xyp->y == xy.y + 1)
		*dirp = LEFT;
	else if (*dirp == LEFT && xyp->x == xy.x - 1)
		*dirp = UP;
	else if (*dirp == UP && xyp->y == xy.y - 2)
		*dirp = INVALID;
}

/* Useful iterators. */
#define for_each_scan_dir(xyp, dirp, xy, dir)			\
	for (*dirp = dir, *xyp = move_in_dir(xy, *dirp);	\
	     *dirp != INVALID;					\
	     *xyp = scan_move_next(xy, dir, dirp))

#define for_each_perimeter(xyp, dirp, xy, width)	\
	for (start_perimeter(xyp, dirp, xy);		\
	     *dirp != INVALID;				\
	     next_perimeter(xyp, dirp, xy, width))

/* Canonicalizes str into array of characters, finds text strings. */
static char **square(const tal_t *ctx,
		     const char *str,
		     struct text **texts)
{
	size_t width = 0, height = 0;
	size_t line_len = 0;
	char **sq;
	struct text *cur_text;
	size_t strlen;

	*texts = tal_arr(ctx, struct text, 0);

	strlen = 0;
	for (size_t i = 0; str[i]; i++) {
		if (str[i] == '\n') {
			height++;
			line_len = 0;
		} else {
			line_len++;
			if (line_len > width)
				width = line_len;
		}
		strlen++;
	}

	/* If didn't end in \n, it's implied */
	if (line_len != 0) {
		height++;
		strlen++;
	}

	/* For analysis simplicity, create a blank border. */
	sq = tal_arr(ctx, char *, height + 2);
	for (size_t i = 0; i < height + 2; i++) {
		sq[i] = tal_arr(sq, char, width + 2);
		memset(sq[i], ' ', width + 2);
	}

	/* Copy across and find text */
	cur_text = NULL;
	width = height = 1;
	for (size_t i = 0; i < strlen; i++) {
		bool end_text;
		bool eol;

		eol = (str[i] == '\n' || str[i] == '\0');
		if (!eol)
			sq[height][width] = str[i];

		/* v by itself handled separately below */
		if (strchr(EDGES ARROWS "\n", str[i]) && str[i] != 'v') {
			end_text = (cur_text != NULL);
		} else if (cur_text) {
			/* Two spaces ends text */
			end_text = (str[i] == ' ' && str[i-1] == ' ') || eol;
		} else if (str[i] != ' ') {
			size_t num_texts = tal_count(*texts);
			tal_resize(texts, num_texts+1);
			cur_text = &(*texts)[num_texts];
			cur_text->xy.x = width;
			cur_text->xy.y = height;
			cur_text->width = 0;
			cur_text->node = NULL;
			cur_text->nearest_edge = NULL;
			end_text = false;
		} else
			end_text = false;

		if (end_text) {
			/* Trim final space */
			if (sq[cur_text->xy.y][cur_text->xy.x + cur_text->width-1] == ' ')
				cur_text->width--;
			/* Ignore lone 'v' */
			if (cur_text->width == 1 && sq[cur_text->xy.y][cur_text->xy.x] == 'v')
				tal_resize(texts, tal_count(*texts)-1);
			else {
				cur_text->text = tal_strndup(ctx, &sq[cur_text->xy.y][cur_text->xy.x],
							     cur_text->width);
			}
			cur_text = NULL;
		}

		if (cur_text)
			cur_text->width++;
		if (eol) {
			height++;
			width = 1;
		} else
			width++;
	}

	return sq;
}

/* If text was not previously a node, it is now! */
static const char *text_now_node(const tal_t *ctx,
				 char **sq,
				 struct text *text,
				 void *(*add_node)(const tal_t *ctx,
						   const char *name,
						   const char **errstr,
						   void *arg),
				 void *arg)
{
	const char *err;

	/* Already a node? */
	if (text->node)
		return NULL;

	text->node = add_node(ctx, text->text, &err, arg);
	if (!text->node)
		return err;
	return NULL;
}

static bool correct_line_char(char c, enum dir dir, enum dir *newdir)
{
	if (c == line_for_dir(dir)) {
		*newdir = dir;
		return true;
	} else if (c == line_for_dir(anticlockwise(dir))) {
		*newdir = anticlockwise(dir);
		return true;
	} else if (c == line_for_dir(clockwise(dir))) {
		*newdir = clockwise(dir);
		return true;
	}
	return false;
}

static bool seek_line(char **sq, struct xy *xy, enum dir *dir)
{
	struct xy scan;
	enum dir scandir;

	for_each_scan_dir(&scan, &scandir, *xy, *dir) {
		if (correct_line_char(*sqc(sq, scan), scandir, &scandir))
			goto found;
		/* + in front always works */
		if (*dir == scandir && *sqc(sq, scan) == '+')
			goto found;
	}
	return false;

found:
	*xy = scan;
	*dir = scandir;
	return true;
}

static bool seek_arrowhead(char **sq, struct xy *xy, enum dir *dir)
{
	struct xy scan;
	enum dir scandir;

	for_each_scan_dir(&scan, &scandir, *xy, *dir) {
		if (strchr(ARROWS, *sqc(sq, scan))) {
			*xy = scan;
			*dir = scandir;
			return true;
		}
	}
	return false;
}

static struct text *in_text(struct text *texts, struct xy xy)
{
	for (size_t i = 0; i < tal_count(texts); i++) {
		if (texts[i].xy.y != xy.y)
			continue;
		if (xy.x >= texts[i].xy.x + texts[i].width)
			continue;
		if (xy.x < texts[i].xy.x)
			continue;
		return texts + i;
	}
	return NULL;
}

static struct text *seek_text(struct text *texts,
			      struct xy xy, enum dir dir)
{
	struct xy scan;
	enum dir scandir;

	for_each_scan_dir(&scan, &scandir, xy, dir) {
		struct text *t = in_text(texts, scan);
		if (t)
			return t;
	}
	return NULL;
}

static void erase_line(char **sq,
		       struct xy xy,
		       enum dir dir,
		       enum dir prev_dir)
{
	char c = ' ';

	/* If we go straight through a +, convert for crossover */
	if (prev_dir == dir && *sqc(sq, xy) == '+') {
		if (dir == UP || dir == DOWN)
			c = '-';
		if (dir == LEFT || dir == RIGHT)
			c = '|';
	}
	*sqc(sq, xy) = c;
}

static bool in_nearby(struct text **nearby, const struct text *t)
{
	for (size_t i = 0; i < tal_count(nearby); i++) {
		if (nearby[i] == t)
			return true;
	}
	return false;
}

static void add_nearby(struct text ***nearby,
		       struct text *texts,
		       struct xy xy)
{
	struct xy perim;
	enum dir pdir;
	size_t n = tal_count(*nearby);

	for_each_perimeter(&perim, &pdir, xy, 1) {
		struct text *t = in_text(texts, perim);
		if (!t)
			continue;
		/* Don't care if it's already a node */
		if (t->node)
			continue;
		if (in_nearby(*nearby, t))
			continue;
		tal_resize(nearby, n+1);
		(*nearby)[n++] = t;
	}
}

/* Clears lines as it goes. */
static struct text *follow_line(char **sq,
				struct text *texts,
				struct xy xy,
				enum dir dir,
				bool *arrow_src,
				bool *arrow_dst,
				bool *dangling,
				struct text ***nearby)
{
	char expect_arrow = arrow_for_dir(opposite_dir(dir));
	enum dir prev_dir;

	*nearby = tal_arr(sq, struct text *, 0);

	if (*sqc(sq, xy) == expect_arrow) {
		*arrow_src = true;
	} else if (*sqc(sq, xy) == line_for_dir(dir)) {
		*arrow_src = false;
	} else if (*sqc(sq, xy) == line_for_dir(anticlockwise(dir))) {
		*arrow_src = false;
		dir = anticlockwise(dir);
	} else if (*sqc(sq, xy) == line_for_dir(clockwise(dir))) {
		*arrow_src = false;
		dir = clockwise(dir);
	} else {
		*dangling = false;
		/* No arrow is fine. */
		return NULL;
	}

	erase_line(sq, xy, dir, INVALID);
	add_nearby(nearby, texts, xy);

	*arrow_dst = false;
	prev_dir = dir;
	for (;;) {
		/* Try to continue line */
		if (!*arrow_dst && seek_line(sq, &xy, &dir)) {
			erase_line(sq, xy, dir, prev_dir);
			add_nearby(nearby, texts, xy);
			prev_dir = dir;
			continue;
		}
		/* Look for arrow */
		if (!*arrow_dst && seek_arrowhead(sq, &xy, &dir)) {
			erase_line(sq, xy, dir, prev_dir);
			add_nearby(nearby, texts, xy);
			*arrow_dst = true;
			prev_dir = dir;
			continue;
		}
		break;
	}

	/* Must be in text! */
	*dangling = true;
	return seek_text(texts, xy, dir);
}

static const char *try_create_edge(const tal_t *ctx,
				   char **sq,
				   struct text *texts,
				   struct xy xy,
				   struct text *src,
				   void *(*add_node)(const tal_t *ctx,
						     const char *name,
						     const char **errstr,
						     void *arg),
				   void *arg,
				   struct edge **edge)
{
	struct text *dst;
	bool arrow_src, arrow_dst, dangling;
	struct text **nearby;
	const char *err;

	*edge = NULL;
	if (in_text(texts, xy))
		return NULL;

	dst = follow_line(sq, texts, xy, dir_away(src, xy), &arrow_src, &arrow_dst, &dangling, &nearby);
	if (!dst) {
		if (dangling)
			return tal_fmt(ctx, "Found dangling arrow at (%zu,%zu)", xy.x-1, xy.y-1);
		return NULL;
	}

	/* If you weren't a node before, you are now! */
	err = text_now_node(ctx, sq, src, add_node, arg);
	if (err)
		return err;
	err = text_now_node(ctx, sq, dst, add_node, arg);
	if (err)
		return err;

	/* No arrows equiv to both arrows */
	if (!arrow_src && !arrow_dst)
		arrow_src = arrow_dst = true;

	*edge = tal(NULL, struct edge);
	if (arrow_dst) {
		(*edge)->src = src;
		(*edge)->dst = dst;
		(*edge)->bidir = arrow_src;
	} else {
		(*edge)->src = dst;
		(*edge)->dst = src;
		(*edge)->bidir = false;
	}
	(*edge)->labels = tal_arr(*edge, const char *, 0);

	/* Now record any texts it passed by, in case they're labels */
	for (size_t i = 0; i < tal_count(nearby); i++) {
		/* We might have just made it a node */
		if (nearby[i]->node)
			continue;
		/* Already has an edge?  Mark it as near two, to error
		 * later if it's a label */
		if (nearby[i]->nearest_edge)
			nearby[i]->nearest_edge = &fake_edge;
		else
			nearby[i]->nearest_edge = *edge;
	}
		
	return NULL;
}

static const char *scan_for_unused(const tal_t *ctx,
				   struct text *texts,
				   char **sq)
{
	struct xy xy;
	for (xy.y = 0; xy.y < tal_count(sq); xy.y++) {
		for (xy.x = 0; xy.x < tal_count(sq[xy.y]); xy.x++) {
			if (in_text(texts, xy))
				continue;
			if (*sqc(sq,xy) != ' ')
				return tal_fmt(ctx, "Unused '%c' at (%zu,%zu)",
					       *sqc(sq, xy), xy.x-1, xy.y-1);
		}
	}
	return NULL;
}

static void add_label(struct edge *edge, const struct text *label)
{
	size_t n = tal_count(edge->labels);
	tal_resize(&edge->labels, n+1);
	edge->labels[n] = label->text;
}
	
const char *ungraph_(const tal_t *ctx,
		     const char *str,
		     void *(*add_node)(const tal_t *ctx,
				       const char *name,
				       const char **errstr,
				       void *arg),
		     const char *(*add_edge)(const tal_t *ctx,
					     void *source_node,
					     void *dest_node,
					     bool bidir,
					     const char **labels,
					     void *arg),
		     void *arg)
{
	/* To hold all our temporaries! */
	const tal_t *sub = tal(ctx, char);
	char **sq;
	struct text *texts, *remaining_label;
	const char *err;
	bool progress;
	struct edge **edges = tal_arr(sub, struct edge *, 0);
	size_t num_edges = 0;

	/* We create canonical square, find texts. */
	sq = square(sub, str, &texts);

	/* Now search for arrows around each text, cleaning
	 * as we go! */
	for (size_t i = 0; i < tal_count(texts); i++) {
		struct xy perim;
		enum dir pdir;
		struct text *t = &texts[i];

		for_each_perimeter(&perim, &pdir, t->xy, t->width) {
			struct edge *edge;
			err = try_create_edge(ctx, sq, texts, perim, t, add_node, arg, &edge);
			if (err)
				goto fail;
			if (edge) {
				tal_resize(&edges, num_edges+1);
				edges[num_edges++] = tal_steal(edges, edge);
			}
		}
	}

	/* Now attach any remaining labels */
	for (size_t i = 0; i < tal_count(texts); i++) {
		struct text *t = &texts[i];

		if (t->node)
			continue;
		if (t->nearest_edge == &fake_edge) {
			err = tal_fmt(ctx, "Label at (%zu,%zu) near more than one edge",
				      t->xy.x-1, t->xy.y-1);
			goto fail;
		}
		if (t->nearest_edge)
			add_label(t->nearest_edge, t);
	}

	/* Any remaining labels must be attached to already-attached labels */
	do {
		progress = false;
		remaining_label = NULL;

		for (size_t i = 0; i < tal_count(texts); i++) {
			struct xy perim;
			enum dir pdir;
			struct text *t = &texts[i];

			if (t->node || t->nearest_edge)
				continue;

			remaining_label = t;
			for_each_perimeter(&perim, &pdir, t->xy, t->width) {
				struct text *neighbor = in_text(texts, perim);
				if (!neighbor || neighbor->node || !neighbor->nearest_edge)
					continue;
				t->nearest_edge = neighbor->nearest_edge;
				add_label(t->nearest_edge, t);
				progress = true;
				break;
			}
		}
	} while (progress);

	if (remaining_label) {
		err = tal_fmt(ctx, "Label at (%zu,%zu) not near any edge",
			      remaining_label->xy.x-1,
			      remaining_label->xy.y-1);
		goto fail;
	}

	err = scan_for_unused(ctx, texts, sq);
	if (err)
		goto fail;

	/* Now add edges, complete with labels */
	for (size_t i = 0; i < tal_count(edges); i++) {
		err = add_edge(ctx, edges[i]->src->node, edges[i]->dst->node,
			       edges[i]->bidir, edges[i]->labels, arg);
		if (err)
			goto fail;
	}
	
	tal_free(sub);
	return NULL;

fail:
	tal_free(sub);
	return err;
}
