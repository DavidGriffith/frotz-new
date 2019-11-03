/*
 * sbdrv.h - DJGPP interface, sound blaster driver
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
#ifndef _SBDRV_H
#define _SBDRV_H

#include <stdbool.h>
#include <stdint.h>
#include <dpmi.h>
#include <go32.h>

#define DSP_CMD_8BIT_PLAY         0x14
#define DSP_CMD_TIME_CONSTANT     0x40
#define DSP_CMD_8BIT_PAUSE        0xD0
#define DSP_CMD_SPEAKER_ON        0xD1
#define DSP_CMD_SPEAKER_OFF       0xD3
#define DSP_CMD_VERSION           0xE1

#define DSP_CMD_SET_BLOCK_SIZE    0x48
#define DSP_CMD_HISPEED_8BIT_PLAY 0x91

#define DSP_CMD_SET_OUTPUT_RATE   0x41
#define DSP_CMD_16BIT_SINGLE      0xB0
#define DSP_CMD_8BIT_SINGLE       0xC0
#define DSP_CMD_16BIT_PAUSE       0xD5

#define CARD_TYPE_SB    0
#define CARD_TYPE_SB2   1
#define CARD_TYPE_SBPRO 2
#define CARD_TYPE_SB16  3

#define LOBYTE(x) ((uint8_t)(((uint16_t)(x)) &  0xFF))
#define HIBYTE(x) ((uint8_t)(((uint16_t)(x)) >> 8))
#define LOWORD(x) ((uint16_t)(((uint32_t)(x)) & 0xFFFF))
#define HIWORD(x) ((uint16_t)(((uint32_t)(x)) >> 16))

typedef struct _sound_blaster sound_blaster_t;
typedef void (*interrupt_handler)(sound_blaster_t *data);
typedef struct {
	_go32_dpmi_seginfo dma_mem;
	uint32_t addr;
} dma_t;


typedef struct {
	/* This buffer should be locked by caller prior to calling play */
	uint8_t *buffer;
	uint32_t length;
	uint32_t offset;
	uint8_t loop;
} sample_t;


typedef struct {
	void (*frequency)(uint16_t);
	void (*volume)(uint8_t);
	void (*play)(uint16_t);
	void (*stop)(void);
	void (*sign)(bool);
	void (*bits)(uint8_t);
	void (*channels)(uint8_t);
} sound_blaster_ops_t;

struct _sound_blaster {
	uint16_t addr;
	uint16_t irq;
	uint16_t dmalo;
	uint16_t dmahi;
	uint16_t interrupt;
	uint16_t type;
	uint8_t  mode;
	uint8_t  playing;
	uint8_t  version_major;
	uint8_t  version_minor;
	dma_t dma_buffer;
	sample_t sample;
	uint16_t dma_port_address[8];
	uint16_t dma_port_count[8];
	uint16_t dma_port_page[8];
	/* The data these point to need to be locked by the user */
	interrupt_handler handler;
	void* user_data;
};

bool sound_blaster_init(uint16_t base, uint16_t irq, uint16_t dmalo, uint16_t dmahi);
void sound_blaster_deinit(void);
void sound_blaster_stop(void);
void sound_blaster_play(uint8_t *buffer, uint32_t size, uint16_t freq, uint8_t loop);
void sound_blaster_speaker_on(void);
void sound_blaster_speaker_off(void);
void sound_blaster_version(uint8_t *major, uint8_t *minor);
void sound_blaster_volume(uint8_t volume);
void sound_blaster_bits(uint8_t bits);
void sound_blaster_sign(bool enable);
void sound_blaster_channels(uint8_t channels);
void sound_blaster_frequency(uint16_t freq);
void sound_blaster_callback(interrupt_handler handler, void *user_data);
#endif
