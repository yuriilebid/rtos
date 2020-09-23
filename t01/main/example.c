void user_input() {
    char *msg = "\n\rSorry, but command can`t be longer than 30 symbols.\n\r";
    uart_event_t event;
    const char *prompt = "Enter your command : ";
    uint8_t command_line[COMMAND_LINE_MAX_LENGTH];
    size_t buf_size    = 0;
    uint8_t *buf       = NULL;
    int index          = 0;

    while(1) {
        bzero(command_line, COMMAND_LINE_MAX_LENGTH);
        uart_write_bytes(UART_PORT, prompt, strlen(prompt));

        while (1) {
            if (xQueueReceive(uart0_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
                if (event.type == UART_DATA) {
                    uart_get_buffered_data_len(UART_PORT, &buf_size);
                    if (buf_size > 30 || index > 30) {
                        uart_write_bytes(UART_PORT, msg, strlen(msg));
                        index = 0;
                        break;
                    }
                    buf = malloc(sizeof(uint8_t) * (buf_size + 1));
                    memset(buf, '\0', buf_size + 1);
                    uart_read_bytes(UART_PORT, buf, buf_size + 1, buf_size);
                    if (buf[0] == CR_ASCII_CODE && buf_size == 1) {
                        uart_write_bytes(UART_PORT, "\n\r", strlen("\n\r"));
                        if (!xQueueSend(global_queue_handle, command_line, (200 / portTICK_PERIOD_MS)))
                            printf("Failed to send data in queue\n");
                        index = 0;
                        break;
                    }
                    else if (buf[0] == BACK_SPACE && buf_size == 1) {
                        if (index > 0) {
                            char c = 8;
                            char *tmp = &c;
                            uart_write_bytes(UART_PORT, tmp, 1);
                            uart_write_bytes(UART_PORT, " ", 1);
                            uart_write_bytes(UART_PORT, tmp, 1);
                            command_line[index - 1] = '\0';
                            index -= 1;
                        }
                    }
                    uart_write_bytes(UART_PORT, buf, strlen((char *)buf));
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
    }
}