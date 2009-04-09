#ifndef _WWVIAUDIO_H_
#define _WWVIAUDIO_H_
/*
    (C) Copyright 2007,2008, Stephen M. Cameron.

    This file is part of wordwarvi.

    wordwarvi is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    wordwarvi is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with wordwarvi; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

 */

#ifdef WWVIAUDIO_DEFINE_GLOBALS
#define GLOBAL
#else
#define GLOBAL extern
#endif

#define WWVIAUDIO_MUSIC_SLOT (0)
#define WWVIAUDIO_SAMPLE_RATE   (44100)
#define WWVIAUDIO_ANY_SLOT (-1)

/*
 *             Configuration functions.
 */
/* Disables the music channel.  Meant to be called prior to
 * wwviaudio_initialize_portaudio
 */
GLOBAL void wwviaudio_set_nomusic(void);

/* Set the audio device number to the given value
 * This is meant to be called prior to calling
 * wwviaudio_initialize_portaudio.  If you don't use
 * this function, portaudio's default device will be
 * used.  This function provides a way to use a
 * device other than the default
 */
GLOBAL int wwviaudio_set_sound_device(int device);

/* Initialize portaudio and start the audio engine.
 * Space will be allocated to allow for the specified
 * number of concurrently playing sounds.  The 2nd parameter
 * indicates how many different sound clips are to be held
 * in memory at once.  e.g. if your app has five different
 * sounds that it plays, then this would be 5.
 * 0 is returned on success, -1 otherwise.
 */
GLOBAL int wwviaudio_initialize_portaudio(int maximum_concurrent_sounds,
	int maximum_sound_clips);

/* Stop portaudio and the audio engine. Space allocated
 * during initialization is freed.
 */
GLOBAL void wwviaudio_stop_portaudio(void);

/*
 *             Audio data functions
 */

/* Read and decode an ogg vorbis audio file into a numbered buffer
 * The sound_number parameter is used later with wwviaudio_play_music and
 * wwviaudio_add_sound.  0 is returned on success, -1 otherwise.
 * Audio files should be 44100Hz, MONO.  The sound number is one you
 * provide which will then be associated with that sound.
 */
GLOBAL int wwviaudio_read_ogg_clip(int sound_number, char *filename);

/*
 *             Global sound control functions.
 */

/* Suspend all audio playback.  Silence is output. */
GLOBAL void wwviaudio_pause_audio(void);

/* Resume all previously playing audio from whence it was previusly paused. */
GLOBAL void wwviaudio_resume_audio(void);

/*
 *             Music channel related functions
 */

/* Begin playing a numbered buffer into the mix on the music channel
 * The channel number of the music channel is returned.
 */
GLOBAL int wwviaudio_play_music(int sound_number);

/* Output silence on the music channel (pointer still advances though.) */
GLOBAL void wwviaudio_silence_music(void);

/* Unsilence the music channel */
GLOBAL void wwviaudio_resume_music(void);

/* Silence or unsilence the music channe. */
GLOBAL void wwviaudio_toggle_music(void);

/* Stop playing the playing buffer from the music channel */
GLOBAL void wwviaudio_cancel_music(void);

/*
 *             Sound effect (not music) related functions
 */

/* Begin playing a sound on a non-music channel.  The channel is returned.
 * sound_number refers to a sound previously associated with the number by
 * wwviaudio_read_ogg_clip()
 */
GLOBAL /* channel */ int wwviaudio_add_sound(int sound_number);

/* Begin playing a sound on a non-music channel.  The channel is returned.
 * If fewer than five channels are open, the sound is not played, and -1
 * is returned.
 */
GLOBAL void wwviaudio_add_sound_low_priority(int sound_number);

/* Silence all channels but the music channel (pointers still advance though) */
GLOBAL void wwviaudio_silence_sound_effects(void);

/* Unsilence all channels but the music channel */
GLOBAL void wwviaudio_resume_sound_effects(void);

/* Either silence or unsilence all but the music channel */
GLOBAL void wwviaudio_toggle_sound_effects(void);

/* Stop playing the playing buffer from the given channel */
GLOBAL void wwviaudio_cancel_sound(int channel);


/* Stop playing the playing buffer from all channels */
GLOBAL void wwviaudio_cancel_all_sounds(void);

/*
	Example usage, something along these lines:

	if (wwviaudio_initialize_portaudio() != 0)
		bail_out_and_die();

	You would probably use #defines or enums rather than bare ints...
	wwviaudio_read_ogg_clip(1, "mysound1.ogg");
	wwviaudio_read_ogg_clip(2, "mysound2.ogg");
	wwviaudio_read_ogg_clip(3, "mysound3.ogg");
	wwviaudio_read_ogg_clip(4, "mymusic.ogg");

	...

	wwviaudio_play_music(4); <-- begins playing music in background, returns immediately

	while (program isn't done) {
		do_stuff();
		if (something happened)
			wwviaudio_add_sound(1);
		if (something else happened)
			wwviaudio_add_sound(2);
		time_passes();
	}
	
	wwviaudio_cancel_all_sounds();
	wwviaduio_stop_portaudio();
*/
#undef GLOBAL
#endif
