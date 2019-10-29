/*
 * ux_blorb.h
 *
 * Blorb related functions specific to the Curses interface.
 *
 */

#include "../blorb/blorb.h"
#include "../blorb/blorblow.h"

typedef struct sampledata_struct {
	unsigned short channels;
	unsigned long samples;
	unsigned short bits;
	double rate;
} sampledata_t;


/*
 * The bb_result_t struct lacks a few members that would make things a
 * bit easier.  The myresource struct takes encapsulates the bb_result_t
 * struct and adds a type member and a filepointer.  I would like to
 * convince Andrew Plotkin to make a change in the reference Blorb code
 * to add these members.
 *
 */
typedef struct {
	bb_result_t bbres;
	/* ulong type; */
	unsigned long type;
	FILE *fp;
} myresource;

extern bb_err_t		blorb_err;
extern bb_map_t		*blorb_map;
extern bb_result_t	blorb_res;
extern FILE *blorb_fp;

bb_err_t ux_blorb_init(char *);
void ux_blorb_stop(void);

uint16 ux_get_release_num(bb_map_t *map);
bb_err_t ux_count_resources(bb_map_t *map, uint32 usage, int *num, int *min, int *max);
bb_err_t ux_load_resource(bb_map_t *, int, bb_result_t *, uint32, int);
bb_err_t ux_unload_chunk(bb_map_t *map, int chunknum);
bb_err_t ux_load_chunk_by_type(bb_map_t *, int, bb_result_t *, uint32, int);
bb_err_t ux_create_map(FILE *file, bb_map_t **newmap);
