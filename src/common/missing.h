/*
 * missing.h
 *
 * Declarations and definitions for standard things that may be missing
 * on older systems.
 *
 */

#ifndef MISSING_H
#define MISSING_H

#ifdef NO_MEMMOVE
void *my_memmove(void *, const void *, size_t);
#define memmove my_memmove
#endif


#endif /* MISSING_H */
