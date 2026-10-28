#include "commands.h"
#include <stddef.h>
typedef command_result_t (*handler_t)(const command_t *);
typedef struct {uint16_t opcode;handler_t handler;} entry_t;
static command_result_t ping(const command_t *command) {
    (void)command;return COMMAND_READ_ONLY_OK;
}
#if defined(__GNUC__) && !defined(HOST_TEST)
#define COMMAND_TABLE __attribute__((section(".command_table"),used))
#else
#define COMMAND_TABLE
#endif
/* Dedicated linker region begins at 0x080C0000. No mutating command yet. */
static const entry_t registry[] COMMAND_TABLE={{0x0001u,ping}};
command_result_t command_dispatch(const command_t *command) {
    size_t i;
    for(i=0;i<sizeof registry/sizeof registry[0];i++)
        if(registry[i].opcode==command->opcode)return registry[i].handler(command);
    return COMMAND_UNSUPPORTED;
}
