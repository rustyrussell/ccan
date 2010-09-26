#ifndef __OGG_TO_PCM_H__
#define __OGG_TO_PCM_H__
#include <stdint.h>
#ifdef DEFINE_OGG_TO_PCM_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

/* OggDec
 *
 * This program is distributed under the GNU General Public License, version 2.
 * A copy of this license is included with this source.
 *
 * Copyright 2002, Michael Smith <msmith@xiph.org>
 *
 */

/*
 *
 * This code was hacked off of the carcass of oggdec.c, from
 * the vorbistools-1.2.0 package, and is copyrighted as above,
 * with the modifications made by me,
 * (c) Copyright Stephen M. Cameron, 2008,
 * (and of course also released under the GNU General Public License, version 2.)
 *
 */

/* ogg_to_pcm() reads an ogg vorbis audio file, infile, and
 * dumps the data into a big buffer, *pcmbuffer (which it
 * allocates via malloc) and returns the number of samples
 * in *nsamples, and the samplesize in *samplesize. and etc.
 */
GLOBAL int ogg_to_pcm(char *infile, int16_t **pcmbuffer,
	int *samplesize, int *sample_rate, int *nchannels,
	uint64_t *nsamples);

#undef GLOBAL
#endif
