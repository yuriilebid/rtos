#include "header.h"

/*
 * Determines the type of command
 * to be executed.
 */

#define EN_AMP                  23

static void no_such_command_error() {
    char *msg = "\e[31mNo such command.\e[36mWrite \e[32mhelp \e[36mto list all supported commands.\e[0m\n\r";
    uart_write_bytes(UART_PORT, msg, strlen(msg));
}



// Determines type of led cmd.
static void led_commands(char **cmd, int len) {
    if (cmd[1] && !strcmp(cmd[1], "on"))
        led_on(cmd, len);
    else if  (cmd[1] && !strcmp(cmd[1], "off"))
        led_off(cmd, len);
    else if  (cmd[1] && !strcmp(cmd[1], "pulse"))
        led_pulse(cmd, len);
    else
        no_such_command_error();
}



static void sound_command(char **cmd, int len) {
    char *msg = "\e[31mWrong syntax\e[36m sound [on/off]\e[0m\n\r";

    if (len != 2)
        uart_write_bytes(UART_PORT, msg, strlen(msg));
    else if (!strcmp("on", cmd[1]) || !strcmp("off", cmd[1])) {
        if (!strcmp("off", cmd[1]))
            i2s_stop(0);
        else if (!strcmp("on", cmd[1]))
            i2s_start(0);
    }
    else
        uart_write_bytes(UART_PORT, msg, strlen(msg));
}



void execute(char **cmd, int len) {

    if (len == 0) {
        // pass
    }
    else if (!strcmp(cmd[0], "led"))
        led_commands(cmd, len);
    else if (!strcmp(cmd[0], "help"))
        help_command();
    else if (!strcmp(cmd[0], "tehu"))
        tehu(cmd);
    else if (!strcmp(cmd[0], "time"))
        time_command(cmd);
    else if (!strcmp(cmd[0], "sound"))
        sound_command(cmd, len);
    else 
        no_such_command_error();

    const char *prompt = "Enter your command : ";
    uart_write_bytes(UART_PORT, prompt, strlen(prompt));
}

