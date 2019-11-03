/*
 * sbdrv.c - DJGPP interface, sound blaster driver
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
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <dpmi.h>
#include <go32.h>
#include "sbdrv.h"

#define DMA_BUF_SZ  0x10000L

/* Sound Blaster 1.0 and 2.0 */
static void default_frequency(uint16_t freq);
static void default_play(uint16_t size);
static void default_stop(void);
static void sb2_volume(uint8_t volume);

/* Sound Blaster Pro */
static void sbpro_frequency(uint16_t freq);
static void sbpro_volume(uint8_t volume);
static void sbpro_play(uint16_t size);
static void sbpro_stop(void);
static void sbpro_channels(uint8_t channels);

/* Sound Blaster 16 */
static void sb16_frequency(uint16_t freq);
static void sb16_volume(uint8_t volume);
static void sb16_play(uint16_t size);
static void sb16_stop(void);
static void sb16_sign(bool enable);
static void sb16_bits(uint8_t bits);
static void sb16_channels(uint8_t channels);

static sound_blaster_t sound_blaster = {
	.dma_port_address = {0x00, 0x02, 0x04, 0x06, 0xC0, 0xC4, 0xC8, 0xCC},
	.dma_port_count = {0x01, 0x03, 0x05, 0x07, 0xC2, 0xC6, 0xCA, 0xCE},
	.dma_port_page = {0x87, 0x83, 0x81, 0x82, 0x8F, 0x8B, 0x89, 0x8A},
};

static sound_blaster_ops_t ops[4] = {
	/* Original Sound Blaster */
	{
		.frequency = default_frequency,
		.play = default_play,
		.stop = default_stop,
	},
	/* Sound Blaster 2.0 */
	{
		.frequency = default_frequency,
		.volume = sb2_volume,
		.play = default_play,
		.stop = default_stop,
	},
	/* Sound Blaster Pro */
	{
		.frequency = sbpro_frequency,
		.volume = sbpro_volume,
		.play = sbpro_play,
		.stop = sbpro_stop,
		.channels = sbpro_channels,
	},
	/* Sound Blaster 16 */
	{
		.frequency = sb16_frequency,
		.volume = sb16_volume,
		.play = sb16_play,
		.stop = sb16_stop,
		.sign = sb16_sign,
		.bits = sb16_bits,
		.channels = sb16_channels,
	}
};

static _go32_dpmi_seginfo old_vector;
static _go32_dpmi_seginfo int_vector;

static void WRITE_DSP(uint8_t byte)
{
	while (inportb(sound_blaster.addr + 12) & 0x80);
	outportb(sound_blaster.addr + 12, byte);
}

static uint8_t READ_DSP(void)
{
	while (!(inportb(sound_blaster.addr + 14) & 0x80));
	return inportb(sound_blaster.addr + 10);
}

static void WRITE_MIXER(uint8_t reg, uint8_t value)
{
	outportb(sound_blaster.addr + 4, reg);
	outportb(sound_blaster.addr + 5, value);
}

static uint8_t READ_MIXER(uint8_t reg)
{
	outportb(sound_blaster.addr + 4, reg);
	return inportb(sound_blaster.addr + 5);
}

static bool sound_blaster_alloc_dma_buffer(uint32_t size)
{
	uint32_t bytes;

	sound_blaster.dma_buffer.dma_mem.size = (size + 15) >> 4;
	if (_go32_dpmi_allocate_dos_memory(&sound_blaster.dma_buffer.dma_mem) != 0) {
		return false;
	}
	bytes = (sound_blaster.dma_buffer.dma_mem.size) << 4;
	sound_blaster.dma_buffer.addr = sound_blaster.dma_buffer.dma_mem.rm_segment << 4;
	if (HIWORD(sound_blaster.dma_buffer.addr + bytes) != HIWORD(sound_blaster.dma_buffer.addr)) {
		_go32_dpmi_free_dos_memory(&sound_blaster.dma_buffer.dma_mem);
		bytes <<= 1;
		sound_blaster.dma_buffer.dma_mem.size <<= 1;
		if (_go32_dpmi_allocate_dos_memory(&sound_blaster.dma_buffer.dma_mem) != 0) {
			return false;
		}
		sound_blaster.dma_buffer.addr = sound_blaster.dma_buffer.dma_mem.rm_segment << 4;
		sound_blaster.dma_buffer.addr = (sound_blaster.dma_buffer.addr + 0xFFFF) & 0xFFFF0000;
	}
	return true;
}

static uint32_t sound_blaster_copy_next_sample()
{
	uint8_t *buffer = sound_blaster.sample.buffer + sound_blaster.sample.offset;
	uint32_t size = sound_blaster.sample.length - sound_blaster.sample.offset;

	if (sound_blaster.sample.length == sound_blaster.sample.offset) {
		return 0;
	}

	size = ((size > DMA_BUF_SZ) ? DMA_BUF_SZ : size);
	dosmemput(buffer, size, sound_blaster.dma_buffer.addr);
	sound_blaster.sample.offset += size;

	return size;
}

static void play_sample(uint32_t size)
{
	uint16_t dma;
	uint16_t address, page;
	uint32_t phys = sound_blaster.dma_buffer.addr;

	dma = ((sound_blaster.mode & 0x80) ? sound_blaster.dmahi : sound_blaster.dmalo) & 7;
	if (dma & 4) size >>= 1;
	--size;
	page = HIWORD(phys);
	if (dma & 4) phys >>= 1;
	address = LOWORD(phys);

	/* Set up DMA chip */
	outportb(dma < 4 ? 0x0a : 0xd4, 0x04 | (dma & 3));
	outportb(dma < 4 ? 0x0c : 0xd8, 0x00);
	outportb(dma < 4 ? 0x0b : 0xd6, 0x48 | (dma & 3));
	outportb(sound_blaster.dma_port_address[dma], LOBYTE(address));
	outportb(sound_blaster.dma_port_address[dma], HIBYTE(address));
	outportb(sound_blaster.dma_port_page[dma], LOBYTE(page));
	outportb(sound_blaster.dma_port_count[dma], LOBYTE(size));
	outportb(sound_blaster.dma_port_count[dma], HIBYTE(size) & 0xFF);
	outportb(dma < 4 ? 0xa : 0xd4, dma & 3);

	if (ops[sound_blaster.type].play) {
		ops[sound_blaster.type].play(LOWORD(size));
	}
}

static bool sound_blaster_reset()
{
	WRITE_MIXER(0x00, 0x00);

	outportb(sound_blaster.addr + 6, 1);
	delay(75);
	outportb(sound_blaster.addr + 6, 0);
	delay(75);
	return (inportb(sound_blaster.addr + 0xA) == 0xAA);
}

static void default_stop(void)
{
	WRITE_DSP(DSP_CMD_8BIT_PAUSE);
}

static void default_frequency(uint16_t freq)
{
	WRITE_DSP(DSP_CMD_TIME_CONSTANT);
	WRITE_DSP(256 - 1000000L / freq);
}

static void default_play(uint16_t size)
{
	WRITE_DSP(DSP_CMD_8BIT_PLAY);
	WRITE_DSP(LOBYTE(size));
	WRITE_DSP(HIBYTE(size));
}

static void sb2_volume(uint8_t volume)
{
	volume = (volume != 255) ? (volume & 7) << 1 : 14;
	WRITE_MIXER(0x02, volume);
	WRITE_MIXER(0x0A, 0x06);
}

static void sbpro_volume(uint8_t volume)
{
	volume = (volume != 255) ? 7 + volume : 15;
	WRITE_MIXER(0x04, (volume << 4) | volume);
	WRITE_MIXER(0x22, 0xFF);
}

static void sbpro_frequency(uint16_t freq)
{
	uint8_t channels = (READ_MIXER(0x0E) & 0x02) ? 2 : 1;
	WRITE_DSP(DSP_CMD_TIME_CONSTANT);
	WRITE_DSP(HIBYTE(0x10000 - 256000000 / (channels * freq)));
}

static void sbpro_channels(uint8_t channels)
{
	if (channels == 2) {
		WRITE_MIXER(0x0E, READ_MIXER(0x0E) | 0x02);
	} else {
		WRITE_MIXER(0x0E, READ_MIXER(0x0E) & 0xFD);
	}
}

static void sbpro_play(uint16_t size)
{
	WRITE_DSP(DSP_CMD_SET_BLOCK_SIZE);
	WRITE_DSP(LOBYTE(size));
	WRITE_DSP(HIBYTE(size));
	WRITE_DSP(DSP_CMD_HISPEED_8BIT_PLAY);
}

static void sbpro_stop(void)
{
	outportb(sound_blaster.addr + 6, 1);
	delay(75);
	outportb(sound_blaster.addr + 6, 0);
	delay(75);
}

static void sb16_volume(uint8_t volume)
{
	volume = (volume != 255) ? 127 + 16 * volume : 255;
	WRITE_MIXER(0x32, volume);
	WRITE_MIXER(0x33, volume);
	WRITE_MIXER(0x30, 0xFF);
	WRITE_MIXER(0x31, 0xFF);
}

static void sb16_frequency(uint16_t freq)
{
	WRITE_DSP(DSP_CMD_SET_OUTPUT_RATE);
	WRITE_DSP(HIBYTE(freq));
	WRITE_DSP(LOBYTE(freq));
}

static void sb16_play(uint16_t size)
{
	if (sound_blaster.mode & 0x80) {
		WRITE_DSP(DSP_CMD_16BIT_SINGLE);
	} else {
		WRITE_DSP(DSP_CMD_8BIT_SINGLE);
	}
	WRITE_DSP(sound_blaster.mode & 0x30);
	WRITE_DSP(LOBYTE(size));
	WRITE_DSP(HIBYTE(size));
}

static void sb16_stop(void)
{
	if (sound_blaster.mode & 0x80) {
		WRITE_DSP(DSP_CMD_16BIT_PAUSE);
	} else {
		WRITE_DSP(DSP_CMD_8BIT_PAUSE);
	}
}

static void sb16_channels(uint8_t channels)
{
	if (channels == 2) {
		sound_blaster.mode |= 0x20;
	} else {
		sound_blaster.mode &= 0xDF;
	}
}

static void sb16_bits(uint8_t bits)
{
	if (bits == 16) {
		sound_blaster.mode |= 0x80;
	} else {
		sound_blaster.mode &= 0x7F;
	}
}

static void sb16_sign(bool enable)
{
	if (enable) {
		sound_blaster.mode |= 0x10;
	} else {
		sound_blaster.mode &= 0xEF;
	}
}

void sound_blaster_interrupt(void)
{
	uint32_t size = sound_blaster_copy_next_sample();
	if (size > 0) {
		play_sample(size);
	} else {
		if (sound_blaster.sample.loop > 0) {
			sound_blaster.sample.offset = 0;
			size = sound_blaster_copy_next_sample();
			play_sample(size);
			if (sound_blaster.sample.loop != 255)
				sound_blaster.sample.loop -= 1;
		} else {
			if (sound_blaster.handler) {
				sound_blaster.handler(&sound_blaster);
			}
			sound_blaster.playing = false;
		}
	}

	if (sound_blaster.type == CARD_TYPE_SB16 && READ_MIXER(0x82) & 0x02) {
		inportb(sound_blaster.addr + 15);
	} else {
		inportb(sound_blaster.addr + 14);
	}

	/* Tell interrupt controller(s) + sound board we are done */
	outportb(0x20, 0x20);

	if (sound_blaster.irq >= 8)
		outportb(0xa0, 0x20);
}

bool sound_blaster_init(uint16_t base, uint16_t irq, uint16_t dmalo, uint16_t dmahi)
{
	sound_blaster.addr = base;
	sound_blaster.irq = irq;
	sound_blaster.dmalo = dmalo;
	sound_blaster.dmahi = dmahi;
	sound_blaster.interrupt = (irq < 8) ? (0x08 + irq) : (0x68 + irq);

	if (!sound_blaster_reset()) {
		goto nosound;
	}

	if (!sound_blaster_alloc_dma_buffer(DMA_BUF_SZ)) {
		goto nosound;
	}

	int_vector.pm_selector = _my_cs();
	int_vector.pm_offset = (long)sound_blaster_interrupt;
	_go32_dpmi_get_protected_mode_interrupt_vector(sound_blaster.interrupt, &old_vector);

	if (_go32_dpmi_allocate_iret_wrapper(&int_vector) != 0)
		goto nosound;

	_go32_dpmi_lock_code (WRITE_DSP, (uint32_t)sound_blaster_init - (uint32_t)WRITE_DSP);
	_go32_dpmi_lock_data((void *)&sound_blaster, sizeof(sound_blaster));
	_go32_dpmi_lock_data((void *)&ops, sizeof(ops));

	_go32_dpmi_set_protected_mode_interrupt_vector(sound_blaster.interrupt, &int_vector);

	if (irq < 8) {
		outportb(0x21, inportb(0x21) & ~(1 << (irq & 7)));
	} else {
		outportb(0xA1, inportb(0xA1) & ~(1 << (irq & 7)));
	}

	WRITE_DSP(DSP_CMD_VERSION);
	sound_blaster.version_major = READ_DSP();
	sound_blaster.version_minor = READ_DSP();

	if (sound_blaster.version_major >= 4) {
		sound_blaster.type = CARD_TYPE_SB16;
	} else if (sound_blaster.version_major == 3) {
		sound_blaster.type = CARD_TYPE_SBPRO;
	} else if (sound_blaster.version_major == 2 && sound_blaster.version_minor > 0) {
		sound_blaster.type = CARD_TYPE_SB2;
	} else {
		sound_blaster.type = CARD_TYPE_SB;
	}

	outportb(0x20, 0x20);

	if (irq >= 8)
		outportb(0xa0, 0x20);

	sound_blaster_speaker_on();
	sound_blaster_volume(5);
	return true;
nosound:
	return false;
}

void sound_blaster_deinit(void)
{
	sound_blaster_reset();
	if (sound_blaster.irq < 8) {
		outportb(0x21, inportb(0x21) | (1 << (sound_blaster.irq & 7)));
	} else {
		outportb(0xA1, inportb(0xA1) | (1 << (sound_blaster.irq & 7)));
	}
	if (int_vector.pm_offset != 0) {
		_go32_dpmi_set_protected_mode_interrupt_vector(sound_blaster.interrupt, &old_vector);
		_go32_dpmi_free_iret_wrapper(&int_vector);
		memset(&int_vector, 0, sizeof(int_vector));
	}
	_go32_dpmi_free_dos_memory(&sound_blaster.dma_buffer.dma_mem);
}

void sound_blaster_version(uint8_t *major, uint8_t *minor)
{
	*major = sound_blaster.version_major;
	*minor = sound_blaster.version_minor;
}

void sound_blaster_callback(interrupt_handler handler, void *user_data)
{
	sound_blaster.handler = handler;
	sound_blaster.user_data = user_data;
}

void sound_blaster_frequency(uint16_t freq)
{
	if (ops[sound_blaster.type].frequency) {
		ops[sound_blaster.type].frequency(freq);
	}
}

void sound_blaster_volume(uint8_t volume)
{
	if (ops[sound_blaster.type].volume) {
		ops[sound_blaster.type].volume(volume);
	}
}

void sound_blaster_channels(uint8_t channels)
{
	if (ops[sound_blaster.type].channels) {
		ops[sound_blaster.type].channels(channels);
	}
}

void sound_blaster_sign(bool enable)
{
	if (ops[sound_blaster.type].sign) {
		ops[sound_blaster.type].sign(enable);
	}
}

void sound_blaster_bits(uint8_t bits)
{
	if (ops[sound_blaster.type].bits) {
		ops[sound_blaster.type].bits(bits);
	}
}

void sound_blaster_play(uint8_t *buffer, uint32_t size, uint16_t freq, uint8_t loop)
{
	if (buffer == NULL || size == 0) {
		return;
	}

	if (sound_blaster.playing) {
		return;
	}

	sound_blaster.sample.buffer = buffer;
	sound_blaster.sample.length = size;
	sound_blaster.sample.offset = 0;
	sound_blaster.sample.loop = loop;

	sound_blaster_frequency(freq);

	size = sound_blaster_copy_next_sample();

	play_sample(size);

	sound_blaster.playing = true;
}

void sound_blaster_stop(void)
{
	if (!sound_blaster.playing) {
		return;
	}

	if (ops[sound_blaster.type].stop) {
		ops[sound_blaster.type].stop();
		sound_blaster.playing = false;
	}
}

void sound_blaster_speaker_on(void)
{
	WRITE_DSP(DSP_CMD_SPEAKER_ON);
}

void sound_blaster_speaker_off(void)
{
	WRITE_DSP(DSP_CMD_SPEAKER_OFF);
}
