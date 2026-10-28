#ifndef GPS_JOURNAL_H
#define GPS_JOURNAL_H
#include "state.h"
typedef struct { int active; uint32_t generation; } journal_t;
typedef enum { JOURNAL_OK, JOURNAL_EMPTY, JOURNAL_CORRUPT, JOURNAL_IO } journal_result_t;
journal_result_t journal_load(journal_t *journal,persisted_t *state);
bool journal_save(journal_t *journal,const persisted_t *state);
uint32_t crc32_bytes(const uint8_t *data,unsigned length);
#endif
