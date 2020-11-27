#include "header.h"

#define LF_ASCII_CODE 0xA
#define CR_ASCII_CODE 0xD // \r the same
#define BACK_SPACE    127

#define NOT_SUPPORT_ARROWS "Arrows are not supported"
#define PROMPT             "Enter your command : "
#define LENGTH_ERR         "Command can`t be longer than 30 symbols!"


void  uart_print(char *msg, bool newline) {
    if (newline) uart_write_bytes(UART_PORT, "\r\n", 2);
    uart_write_bytes(UART_PORT, msg, strlen(msg));  
    if (newline) uart_write_bytes(UART_PORT, "\r\n", 2);
}



void inline erase_char() {
    char c = 8;
    char *tmp = &c;
    uart_write_bytes(UART_PORT, tmp, 1);
    uart_write_bytes(UART_PORT, " ", 1);
    uart_write_bytes(UART_PORT, tmp, 1);
}



static int arrow_checker(uint8_t *buf) {
    for (int i = 0; buf[i]; ++i) {
        if (iscntrl(buf[i]) && buf[i] == 27) {
            uart_print(NOT_SUPPORT_ARROWS, 1);
            return 1;
        }
    }
    return 0;
}



/*
 * Reads user`s input from UART2 and stores it in comman_Line string.
 * When user press enter string command_line is sending to cmd_handler
 * task through queue.
 */
void user_input() {
    uart_event_t event;
    uint8_t command_line[COMMAND_LINE_MAX_LENGTH];
    size_t buf_size    = 0;
    uint8_t *buf       = NULL;
    int index          = 0;

    uart_write_bytes(UART_PORT, PROMPT, strlen(PROMPT));
    uart_print(PROMPT, 0);
    while(1) {
        bzero(command_line, COMMAND_LINE_MAX_LENGTH);
        while (1) {
            if (xQueueReceive(uart0_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
                if (event.type == UART_DATA) {
                    uart_get_buffered_data_len(UART_PORT, &buf_size);
                    if (buf_size > 30 || index > 30) {
                        uart_print(LENGTH_ERR, 1);
                        uart_print(PROMPT, 0);
                        break;
                    }
                    buf =  malloc(sizeof(uint8_t) * (buf_size + 1));
                    if (buf == NULL) 
                        break;
                    bzero(buf, buf_size + 1);
                    uart_read_bytes(UART_PORT, buf, buf_size + 1, buf_size);
                    if (arrow_checker(buf)) {
                        uart_print(PROMPT, 0);
                        break;
                    }
                    if (buf[0] == CR_ASCII_CODE && buf_size == 1) {
                        uart_write_bytes(UART_PORT, "\n\r", 2);
                        if (!xQueueSend(global_input_queue, command_line, (200 / portTICK_PERIOD_MS)))
                            printf("Failed to send data in queue\n");
                        break;
                    }
                    else if (buf[0] == BACK_SPACE && buf_size == 1) {
                        if (index > 0) {
                            erase_char();
                            command_line[index - 1] = '\0';
                            index -= 1;
                        }
                    }
                    uart_print((char *)buf, 0);
                    for (int i = 0; buf[i]; ++i) {
                        if (buf[i] != BACK_SPACE) {
                            command_line[index] = buf[i];
                            index++;
                        }
                    }
                    free(buf);
                }
            }
        }
        index    = 0;
        buf_size = 0;
    }
}


char *upper_to_lower(char *str) {
    int len = strlen((const char *)strlen) + 1;
    char *new_str = mx_strnew(len);
    bzero(new_str, len);

    for (int i = 0; str[i]; ++i) {
        if(str[i] >= 65 && str[i] <= 90)
            new_str[i] = str[i] + 32;
    }
    return new_str;
}






/*
 * Receives user`s input from Queue.
 * Splits user`s input in arr.
 * Calls execute function, which is in charge 
 * of executing command.
 */
void cmd_handler() {
    char result[1000];
    bzero(result, 1000);

    char **cmd = (char **)malloc(100 * sizeof(char *));
    if (cmd == NULL) exit(1);
    while(1) {
        if (xQueueReceive(global_input_queue, result, (200 / portTICK_PERIOD_MS))) {
            for (int i = 0; i < 100; ++i) cmd[i] = NULL;
            // splitting str into arr.
            int index = 0;
            char *p;
            p = strtok(result, " ");
            cmd[index] = p;
            index++;
            while(p != NULL || index < 100) {
                p = strtok(NULL, " ");
                cmd[index] = p;
                index++;
            }
            // 
            int cmd_len = 0;
            while(cmd[cmd_len] && cmd_len < 100) cmd_len++;
            execute(cmd, cmd_len);
        }
    }
}

