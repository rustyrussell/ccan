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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#if !defined(__APPLE__)
/* Apple gets what it needs for malloc from stdlib.h */
#include <malloc.h>
#endif

#include <vorbis/vorbisfile.h>

#define DEFINE_OGG_TO_PCM_GLOBALS
#include "ogg_to_pcm.h"

static const int bits = 16;

/* Reads an ogg vorbis file, infile, and dumps the data into
   a big buffer, *pcmbuffer (which it allocates via malloc)
   and returns the number of samples in *nsamples, and the
   samplesize in *samplesize. and etc.
*/
int ogg_to_pcm(char *infile, int16_t **pcmbuffer,
	int *samplesize, int *sample_rate, int *nchannels,
	uint64_t *nsamples)
{
	FILE *in;
	OggVorbis_File vf;
	char buf[8192];
	unsigned char *bufferptr;
	int link, ret, chainsallowed = 0, bs = 0;

	/* how to do this portably at compile time? */
	const uint32_t dummy = 0x01020304;
	const unsigned char *endian = (unsigned char *) &dummy;

	in = fopen(infile, "r");
	if (in == NULL) {
		fprintf(stderr, "%s:%d ERROR: Failed to open '%s' for read: '%s'\n",
			__FILE__, __LINE__, infile, strerror(errno));
		return -1;
	}
	if (ov_open(in, &vf, NULL, 0) < 0) {
		fprintf(stderr, "%s:%d: ERROR: Failed to open '%s' as vorbis\n",
			__FILE__, __LINE__, infile);
		fclose(in);
		return -1;
	}
	if (!ov_seekable(&vf)) {
		fprintf(stderr, "%s:%d: %s is not seekable.\n",
			__FILE__, __LINE__, infile);
		fclose(in);
		return -1;
	}

	*nchannels = ov_info(&vf,0)->channels;
	*sample_rate = ov_info(&vf,0)->rate;

	for (link = 0; link < ov_streams(&vf); link++) {
		if (ov_info(&vf, link)->channels == *nchannels &&
		    ov_info(&vf, link)->rate == *sample_rate) {
			chainsallowed = 1;
		}
	}

	if (chainsallowed)
		*nsamples = ov_pcm_total(&vf, -1);
	else
		*nsamples = ov_pcm_total(&vf, 0);

	*pcmbuffer = (void *) malloc(sizeof(int16_t) * *nsamples * *nchannels);
	memset(*pcmbuffer, 0, sizeof(int16_t) * *nsamples * *nchannels);
	if (*pcmbuffer == NULL) {
		fprintf(stderr, "%s:%d: Failed to allocate memory for '%s'\n",
			__FILE__, __LINE__, infile);
		fclose(in);
		return -1;
	}
	bufferptr = (unsigned char *) *pcmbuffer;

	while ((ret = ov_read(&vf, buf, sizeof(buf), endian[0] == 0x01, bits/8, 1, &bs)) != 0) {
		if (bs != 0) {
			vorbis_info *vi = ov_info(&vf, -1);
			if (*nchannels != vi->channels || *sample_rate != vi->rate) {
				fprintf(stderr, "%s:%d: Logical bitstreams with changing "
					"parameters are not supported\n",
					__FILE__, __LINE__);
				break;
			}
		}

		if(ret < 0 ) {
			fprintf(stderr, "%s:%d: Warning: hole in data (%d)\n",
				__FILE__, __LINE__, ret);
			continue;
		}

		/* copy the data to the pcmbuffer. */
		memcpy(bufferptr, buf, ret);
		bufferptr += ret;
	}

	/* ov_clear closes the file, so don't fclose here, even though we fopen()ed.
	 * libvorbis is weird that way.
 	 */
	ov_clear(&vf);

	return 0;
}
