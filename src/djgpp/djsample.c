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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "frotz.h"
#include "djfrotz.h"
#include "sbdrv.h"
#include "djblorb.h"

#include <dpmi.h>
#include <go32.h>

extern void end_of_sound(void);

typedef struct {
	uint16_t freq;
	uint16_t channels;
	uint16_t bits;
	uint32_t length;
	uint8_t *samples;
} sound_t;

sound_t snd;

volatile int end_of_sound_flag = 0;

static int current_sample = 0;

long double convert_to_long_double(uint8_t *buffer)
{
		union {
			long double value;
			uint8_t buffer[16];
		} conv;
		conv.buffer[9] = buffer[0];
		conv.buffer[8] = buffer[1];
		conv.buffer[7] = buffer[2];
		conv.buffer[6] = buffer[3];
		conv.buffer[5] = buffer[4];
		conv.buffer[4] = buffer[5];
		conv.buffer[3] = buffer[6];
		conv.buffer[2] = buffer[7];
		conv.buffer[1] = buffer[8];
		conv.buffer[0] = buffer[9];
		return conv.value;
}

uint16_t convert_to_uint16(uint8_t *buffer)
{
		return buffer[1] | (buffer[0] << 8);
}

uint32_t convert_to_uint32(uint8_t *buffer)
{
		return buffer[3] | (buffer[2] << 8) | (buffer[1] << 16) | (buffer[0] << 24);
}

bool loadaiff(int fp, int length, sound_t *snd)
{
	uint8_t buffer[18];
	uint16_t *samples16;;
	int bytes, i;
	uint16_t offset;
	uint32_t chkSz;

	if ((bytes = read(fp, buffer, 12)) < 12) {
		return FALSE;
	}
	if (memcmp(buffer, "FORM", 4) != 0 || memcmp(buffer + 8, "AIFF", 4) != 0) {
		return FALSE;
	}
	length -= bytes;
	while (bytes != 0 && length != 0) {
		chkSz = 0;
		bytes = read(fp, buffer, 8);
		if (bytes == 0) {
			continue;
		} else if (bytes < 8) {
			return FALSE;
		}
		length -= bytes;
		chkSz = convert_to_uint32(buffer+4);
		if (memcmp(buffer, "COMM", 4) == 0) {
			if ((bytes = read(fp, buffer, 18)) < 18) {
				return FALSE;
			}
			length -= bytes;
			snd->channels = convert_to_uint16(buffer);
			snd->bits = (convert_to_uint16(buffer+6) + 7) & ~7;
			snd->freq = (uint16_t)convert_to_long_double(buffer + 8);
			if (snd->bits > 16) {
				return FALSE;
			}
		} else if (memcmp(buffer, "SSND", 4) == 0) {
			if ((bytes = read(fp, buffer, 8)) < 8) {
				return FALSE;
			}
			length -= bytes;
			offset = convert_to_uint32(buffer);
			chkSz -= 8 - offset;
			snd->samples = malloc(chkSz);
			snd->length = chkSz;
			bytes = read(fp, snd->samples, chkSz);
			length -= bytes;
		} else {
			lseek(fp, chkSz, SEEK_CUR);
			length -= chkSz;
		}
	}
	if (snd->bits == 8) {
		for (i = 0; i < snd->length; ++i) {
			snd->samples[i] ^= 0x80;
		}
	} else {
		samples16 = (uint16_t *)snd->samples;
		for (i = 0; i < snd->length / 2; ++i) {
			samples16[i] = (samples16[i] >> 8) | (samples16[i] << 8);
		}
	}
	return TRUE;
}

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

	if (snd.samples) {
		free(snd.samples);
		memset(&snd, 0, sizeof(snd));
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
	bb_result_t resource;

	if (!f_setup.sound) {
		return;
	}

	if (blorb_map == NULL) {
		return;
	}

	os_stop_sample(0);

	/* Continue only if the desired sample is not already present */
	if (current_sample != number) {

		if (snd.samples) {
			free(snd.samples);
			memset(&snd, 0, sizeof(snd));
		}

		if (bb_err_None != bb_load_resource(blorb_map, bb_method_FilePos, &resource, bb_ID_Snd, number)) {
			return;
		}

		switch(blorb_map->chunks[resource.chunknum].type) {
		case bb_ID_FORM:
			lseek(fileno(blorb_map->file), resource.data.startpos, SEEK_SET);
			loadaiff(fileno(blorb_map->file), resource.length, &snd);
			break;
		default:
			return;
		}
		current_sample = number;
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

	/* Load new sample */
	os_prepare_sample(number);

	/* Continue only if the sample's in memory now */
	if (current_sample == number) {
		repeats = ((repeats & 0xFF) == 255 ? 255 : repeats - 1);
		sound_blaster_bits(snd.bits);
		sound_blaster_channels(snd.channels);
		sound_blaster_sign(snd.bits > 8);
		sound_blaster_volume(volume);
		sound_blaster_play(snd.samples, snd.length, snd.freq, repeats);
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
