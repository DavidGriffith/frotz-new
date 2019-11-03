/*
 * djsample.c - DJGPP interface, input functions
 *
 * This file is part of Frotz.
 *
 * Frotz is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Frotz is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 * Or visit http://www.fsf.org/
 */

#include <pc.h>
#include <dos.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "frotz.h"
#include "djfrotz.h"
#include "sbdrv.h"

#include <dpmi.h>
#include <go32.h>

#define SWAP_BYTES(v)	v = v << 8 | v >> 8;

extern void end_of_sound(void);

static struct {
	word prefix;
	byte repeats;
	byte base_note;
	word frequency;
	word unused;
	word length;
} sheader;

volatile int end_of_sound_flag = 0;

static int current_sample = 0;

static byte *sample_data = NULL;


void end_of_sound_cb(sound_blaster_t *device)
{
	end_of_sound_flag = 1;
}

/* calls end_of_sound if flag is set. */
void check_end_of_sound(void)
{
#ifdef SOUND_SUPPORT
	if (end_of_sound_flag) {
		end_of_sound_flag = 0;
		end_of_sound();
	}
#endif /* SOUND_SUPPORT */
}

/*
 * os_init_sound
 *
 * Initialise the sound board and various sound related variables.
 *
 */
void os_init_sound(void)
{
#ifdef SOUND_SUPPORT
	const char *settings;
	int irq, adr, dmalo, dmahi;

	/* Read the IRQ, port address, DMA channel High DMA channel */
	if ((settings = getenv("BLASTER")) == NULL) {
		goto nosound;
	}

	irq =   dectoi(strchr(settings, 'I') + 1);
	adr =   hextoi(strchr(settings, 'A') + 1);
	dmalo = dectoi(strchr(settings, 'D') + 1);
	dmahi = dectoi(strchr(settings, 'H') + 1);

	/* Allocate 64KB RAM for sample data */
	if ((sample_data = (byte *) malloc(0x10000L)) == NULL) {
		goto nosound;
	}

	if (!sound_blaster_init(adr, irq, dmalo, dmahi)) {
		goto nosound;
	}
	sound_blaster_callback(end_of_sound_cb, NULL);
	/* Indicate success */
	return;

nosound:
	cleanup_sound();
#endif /* SOUND_SUPPORT */
	if (z_header.version == V3 && (z_header.flags & OLD_SOUND_FLAG)) {
			z_header.flags &= ~OLD_SOUND_FLAG;
	}
	if (z_header.flags & SOUND_FLAG) {
			z_header.flags &= ~SOUND_FLAG;
	}
	f_setup.sound = FALSE;

} /* os_init_sound */

/*
 * cleanup_sound
 *
 * Free resources allocated for playing samples.
 *
 */
void cleanup_sound(void)
{
	os_stop_sample(0);

	if (sample_data != NULL) {
		free(sample_data);
		sample_data = NULL;
	}
	sound_blaster_deinit();
} /* cleanup_sound */

/*
 * os_beep
 *
 * Play a beep sound. Ideally, the sound should be high- (number == 1)
 * or low-pitched (number == 2).
 *
 */
void os_beep(int number)
{
	word T = 888 * number;

	outportb(0x43, 0xb6);
	outportb(0x42, lo(T));
	outportb(0x42, hi(T));
	outportb(0x61, inportb(0x61) | 3);

	delay(75);

	outportb(0x61, inportb(0x61) & ~3);
} /* os_beep */

/*
 * os_prepare_sample
 *
 * Load the sample from the disk.
 *
 */
void os_prepare_sample(int number)
{
#ifdef SOUND_SUPPORT

	if (!f_setup.sound) {
		return;
	}

	os_stop_sample(0);

	/* Exit if the sound board isn't set up properly */
	if (sample_data == NULL) {
		return;
	}

	/* Continue only if the desired sample is not already present */
	if (current_sample != number) {

		char sample_name[MAX_FILE_NAME + 1];
		char numstr[2];
		FILE *fp;

		/* Build sample file name */
		strcpy(sample_name, "sound/");

		numstr[0] = '0' + number / 10;
		numstr[1] = '0' + number % 10;

		strncat(sample_name, f_setup.story_name, 6);
		strncat(sample_name, numstr, 2);
		strncat(sample_name, ".snd", 4);

		/* Open sample file */
		if ((fp = fopen(sample_name, "rb")) == NULL)
			return;

		/* Load header and sample data */
		fread(&sheader, sizeof(sheader), 1, fp);

		SWAP_BYTES(sheader.frequency);
		SWAP_BYTES(sheader.length);
		fread(sample_data, 1, sheader.length, fp);
		current_sample = number;

		/* Close sample file */
		fclose(fp);

	}
#endif /* SOUND_SUPPORT */
} /* os_prepare_sample */


/*
 * os_start_sample
 *
 * Play the given sample at the given volume (ranging from 1 to 8 and
 * 255 meaning a default volume). The sound is played once or several
 * times in the background (255 meaning forever). The end_of_sound
 * function is called as soon as the sound finishes.
 *
 */
void os_start_sample(int number, int volume, int repeats, zword eos)
{
#ifdef SOUND_SUPPORT
	eos = eos;		/* not used in DOS Frotz */

	if (!f_setup.sound) {
		return;
	}

	os_stop_sample(0);

	/* Exit if the sound board isn't set up properly */
	if (sample_data == NULL) {
		return;
	}

	/* Load new sample */
	os_prepare_sample(number);

	/* Continue only if the sample's in memory now */
	if (current_sample == number) {
		repeats = ((repeats & 0xFF) == 255 ? 255 : repeats - 1);
		sound_blaster_bits(8);
		sound_blaster_channels(1);
		sound_blaster_sign(FALSE);
		sound_blaster_volume(volume);
		sound_blaster_play(sample_data, sheader.length, sheader.frequency, repeats);
	}

#endif /* SOUND_SUPPORT */
} /* os_start_sample */


/*
 * os_stop_sample
 *
 * Turn off the current sample.
 *
 */
void os_stop_sample(int UNUSED(id))
{
#ifdef SOUND_SUPPORT

	if (!f_setup.sound) {
		return;
	}
	/* Exit if the sound board isn't set up properly */
	if (sample_data == NULL) {
		return;
	}
	sound_blaster_stop();
#endif /* SOUND_SUPPORT */
} /* os_stop_sample */


/*
 * os_finish_with_sample
 *
 * Remove the current sample from memory (if any).
 *
 */
void os_finish_with_sample(int UNUSED(id))
{
#ifdef SOUND_SUPPORT
	os_stop_sample(0);	/* we keep 64KB allocated all the time */
#endif /* SOUND_SUPPORT */
} /* os_finish_with_sample */
