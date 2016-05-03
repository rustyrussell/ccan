/*
	Copyright (C) 2016 Stephen M. Cameron
	Author: Stephen M. Cameron

	This file is part of Spacenerds In Space.

	Spacenerds in Space is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	Spacenerds in Space is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Spacenerds in Space; if not, write to the Free Software
	Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <ccan/a_star/a_star.h>
#include <ccan/a_star/a_star.c>

/* Here is the maze we want to solve with A* algorithm */
static char maze[] =
	"##########@################x#################\n"
	"#                  #                        #\n"
	"#                  #                        #\n"
	"#  ###########     ###################      #\n"
	"#  #        #      #                 #      #\n"
	"####        #      #                 #      #\n"
	"#     #            #   ########      #      #\n"
	"#     #    #########   #      #      #      #\n"
	"#     #            #          #             #\n"
	"#     #            #          #             #\n"
	"#     #            #####   ##################\n"
	"#     ##########   #                        #\n"
	"#     #        #   #                        #\n"
	"#              #########################    #\n"
	"#          #           #           #        #\n"
	"############           #     #     #        #\n"
	"#                  #         #              #\n"
	"#############################################\n";

static const char solution[] =
	"##########@################x#################\n"
	"#         .....    #       .                #\n"
	"#             .    #       ............     #\n"
	"#  ###########.    ###################.     #\n"
	"#  #        # .    #                 #.     #\n"
	"#### .......#..    #  ............   #.     #\n"
	"#    .#    ...     #  .########  ..  #.     #\n"
	"#    .#    #########  .#      #   .  #.     #\n"
	"#    .#            #  .....   #   .....     #\n"
	"#    .#            #      .   #             #\n"
	"#    .#            #####  .##################\n"
	"#    .##########   #      ..                #\n"
	"#    .#        #   #       ..............   #\n"
	"#    ..........#########################.   #\n"
	"#          #  .........#.........  #.....   #\n"
	"############          .#.    #  ...#.       #\n"
	"#                  #  ...    #    ...       #\n"
	"#############################################\n";


/* Return the width of the maze */
static int maze_width(char *maze)
{
	char *n;

	n = strchr(maze, '\n');
	return 1 + n - maze;
}

/* offsets for 4 points north, east south, west */
static int xoff[] = { 0, 1, 0, -1 };
static int yoff[] = { 1, 0, -1, 0 };

/* Convert x,y coords into a pointer to an element in the maze */
static char *maze_node(char *maze, int x, int y)
{
	return maze + y * maze_width(maze) + x;
}

/* Return ptr to nth traversable neighbor of a node p, where 0 <= n <= 3, NULL if n out of range */
static void *nth_neighbor(void *context, void *p, int n)
{
	char *maze = context;
	char *c = p;
	int i, x, y, offset = c - maze;

	/* convert ptr to x,y coords */
	x = offset % maze_width(maze);
	y = offset / maze_width(maze);

	for (i = n; i < 4; i++) {
		int tx = x + xoff[i];
		int ty = y + yoff[i];
		if (tx < 0 || ty < 0) /* x,y out of range? */
			continue;
		if (ty * maze_width(maze) + tx > strlen(maze)) /* out of range? */
			continue;
		c = maze_node(maze, x + xoff[i], y + yoff[i]);
		if (*c != '#') /* traversable? */
			return c;
	}
	return NULL;
}

static float maze_cost(void *context, void *first, void *second)
{
	char *maze = context;
	char *f = first;
	char *s = second;
	int sx, sy, fx, fy;
	float d, dx, dy;

	int fp = f - maze;
	int sp = s - maze;

	sx = sp % maze_width(maze);
	sy = sp / maze_width(maze);
	fx = fp % maze_width(maze);
	fy = fp / maze_width(maze);

	dx = (float) sx - fx;
	dy = (float) sy - fy;
	d = (float) (abs(dx) + abs(dy)); /* manhattan distance */
	return d;
}

int main(int argc, char *argv[])
{
	static int maxnodes, i;
	char *start, *goal;
	struct a_star_path *path;

	start = strchr(maze, '@');
	goal = strchr(maze, 'x');
	maxnodes = strlen(maze);

	path = a_star((void *) maze, start, goal, maxnodes, maze_cost, maze_cost, nth_neighbor);
	if (!path) {
		printf("a_star() failed to return a path.\n");
		return 0;
	}
	for (i = 0; i < path->node_count; i++) {
		char *p = path->path[i];
		*p = '.';
	}
	*goal = 'x';
	*start = '@';

	printf("%s\n", maze);

	free(path);

	if (strcmp(solution, maze) == 0)
		return 0;
	else
		return 1;
}
