#ifndef APP_CONSOLE_PRIVATE_H
#define APP_CONSOLE_PRIVATE_H

typedef int (*app_console_subcommand_handler_t)(int argc, char **argv);

typedef struct {
    const char                   *command;
    app_console_subcommand_handler_t handler;
} app_console_subcommand_t ;


#endif //APP_CONSOLE_PRIVATE_H
