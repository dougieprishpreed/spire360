/* The translated module's data segments, read from a file beside the
 * executable instead of held in it (srt_data.c). */
#ifndef SRT_DATA_H
#define SRT_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

/* Where the data file is (before the module is instantiated). */
void srt_data_path(const char* path);
const char* srt_data_where(void);
/* 0 when the file holds the segments this executable expects (table: the C
 * stage's spire_logic_data_table); otherwise -1, with a line in the log. */
int srt_data_check(const unsigned* table);
/* One segment, decoded into dst (the generated C calls it while the module
 * is instantiated); 0, or -1 with a line in the log. */
int srt_data_load(unsigned index, unsigned stored, unsigned fnv, unsigned char* dst, unsigned size);
/* Closes the file once the module is instantiated. */
void srt_data_close(void);

#ifdef __cplusplus
}
#endif

#endif
