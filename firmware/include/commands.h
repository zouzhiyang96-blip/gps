#ifndef GPS_COMMANDS_H
#define GPS_COMMANDS_H
#include <stdint.h>
typedef struct {uint32_t id;uint16_t opcode;char argument[49];} command_t;
typedef enum { COMMAND_UNSUPPORTED, COMMAND_READ_ONLY_OK } command_result_t;
command_result_t command_dispatch(const command_t *command);
#endif
