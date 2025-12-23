/*
 * Shell - Simple command-line interface
 * Reads from USB Serial, displays on OLED
 */

#include "shell.h"
#include "ssd1306.h"
#include "console.h"
#include <string.h>

// Shell state
static char input_buffer[SHELL_MAX_LINE_LENGTH];
static uint8_t input_pos = 0;

// Output line buffer (8 lines on 64-pixel display with 8-pixel font height)
#define MAX_LINES 8
static char display_lines[MAX_LINES][22];  // 21 chars per line (128px / 6px per char) + null terminator
static uint8_t current_line = 0;

// Simple string utilities
static void str_copy(char *dest, const char *src, int max_len) {
    int i = 0;
    while (src[i] && i < max_len - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

static int str_len(const char *s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static bool str_equals(const char *s1, const char *s2) {
    int i = 0;
    while (s1[i] && s2[i]) {
        if (s1[i] != s2[i]) return false;
        i++;
    }
    return s1[i] == s2[i];
}

// Print a line to the display buffer
static void shell_print(const char *text) {
    // Echo to serial console for debugging
    console_puts(text);
    console_puts("\n");

    // Add to display buffer
    if (current_line >= MAX_LINES) {
        // Scroll up: shift all lines up by one
        for (int i = 0; i < MAX_LINES - 1; i++) {
            str_copy(display_lines[i], display_lines[i + 1], 22);
        }
        current_line = MAX_LINES - 1;
    }

    str_copy(display_lines[current_line], text, 22);
    current_line++;

    shell_refresh_display();
}

// Clear the display
static void shell_clear(void) {
    for (int i = 0; i < MAX_LINES; i++) {
        display_lines[i][0] = '\0';
    }
    current_line = 0;
    shell_refresh_display();
}

// Command: help
static void cmd_help(int argc, char **argv) {
    shell_print("Available commands:");
    shell_print("  help  - Show help");
    shell_print("  clear - Clear screen");
    shell_print("  echo  - Echo text");
}

// Command: clear
static void cmd_clear(int argc, char **argv) {
    shell_clear();
}

// Command: echo
static void cmd_echo(int argc, char **argv) {
    if (argc < 2) {
        shell_print("Usage: echo <text>");
        return;
    }

    // Reconstruct the message from all arguments
    char message[SHELL_MAX_LINE_LENGTH];
    int pos = 0;

    for (int i = 1; i < argc && pos < SHELL_MAX_LINE_LENGTH - 1; i++) {
        int arg_len = str_len(argv[i]);
        for (int j = 0; j < arg_len && pos < SHELL_MAX_LINE_LENGTH - 2; j++) {
            message[pos++] = argv[i][j];
        }
        if (i < argc - 1 && pos < SHELL_MAX_LINE_LENGTH - 1) {
            message[pos++] = ' ';  // Space between arguments
        }
    }
    message[pos] = '\0';

    shell_print(message);
}

// Command table
typedef struct {
    const char *name;
    void (*handler)(int argc, char **argv);
} command_t;

static const command_t commands[] = {
    {"help", cmd_help},
    {"clear", cmd_clear},
    {"echo", cmd_echo},
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

// Parse command line and execute
void shell_execute(const char *cmdline) {
    // Echo the command
    char prompt_line[SHELL_MAX_LINE_LENGTH];
    prompt_line[0] = '>';
    prompt_line[1] = ' ';
    str_copy(prompt_line + 2, cmdline, SHELL_MAX_LINE_LENGTH - 2);
    shell_print(prompt_line);

    // Skip leading whitespace
    while (*cmdline == ' ') cmdline++;

    // Empty command
    if (*cmdline == '\0') {
        return;
    }

    // Parse arguments
    static char args_buffer[SHELL_MAX_LINE_LENGTH];
    str_copy(args_buffer, cmdline, SHELL_MAX_LINE_LENGTH);

    char *argv[SHELL_MAX_ARGS];
    int argc = 0;

    char *p = args_buffer;
    while (*p && argc < SHELL_MAX_ARGS) {
        // Skip whitespace
        while (*p == ' ') p++;
        if (*p == '\0') break;

        // Start of argument
        argv[argc++] = p;

        // Find end of argument
        while (*p && *p != ' ') p++;
        if (*p) {
            *p = '\0';
            p++;
        }
    }

    if (argc == 0) return;

    // Find and execute command
    bool found = false;
    for (int i = 0; i < NUM_COMMANDS; i++) {
        if (str_equals(argv[0], commands[i].name)) {
            commands[i].handler(argc, argv);
            found = true;
            break;
        }
    }

    if (!found) {
        // Check if error message fits on one line (21 chars max per line)
        const char *prefix = "command unknown: ";
        int prefix_len = str_len(prefix);
        int cmd_len = str_len(argv[0]);
        int total_len = prefix_len + cmd_len;

        if (total_len <= 21) {
            // Fits on one line
            char err_msg[SHELL_MAX_LINE_LENGTH];
            str_copy(err_msg, prefix, SHELL_MAX_LINE_LENGTH);
            str_copy(err_msg + prefix_len, argv[0], SHELL_MAX_LINE_LENGTH - prefix_len);
            shell_print(err_msg);
        } else {
            // Split across two lines
            shell_print(prefix);
            shell_print(argv[0]);
        }
    }
}

// Initialize shell
void shell_init(void) {
    input_pos = 0;
    current_line = 0;

    // Clear display lines
    for (int i = 0; i < MAX_LINES; i++) {
        display_lines[i][0] = '\0';
    }

    // Show welcome message
    shell_print("RISC-V Shell v1.0");
    shell_print("Type 'help'");
    shell_print(">");
}

// Process incoming character from serial
void shell_process_char(char c) {
    // Handle backspace
    if (c == '\b' || c == 127) {  // Backspace or DEL
        if (input_pos > 0) {
            input_pos--;
            input_buffer[input_pos] = '\0';
            console_putc('\b');
            console_putc(' ');
            console_putc('\b');
        }
        return;
    }

    // Handle newline
    if (c == '\n' || c == '\r') {
        console_putc('\n');
        input_buffer[input_pos] = '\0';

        if (input_pos > 0) {
            shell_execute(input_buffer);
        }

        // Show prompt after command execution
        shell_print(">");

        input_pos = 0;
        input_buffer[0] = '\0';
        return;
    }

    // Handle printable characters
    if (c >= 32 && c <= 126) {
        if (input_pos < SHELL_MAX_LINE_LENGTH - 1) {
            input_buffer[input_pos++] = c;
            input_buffer[input_pos] = '\0';
            console_putc(c);  // Echo to serial
        }
    }
}

// Refresh OLED display with current buffer
void shell_refresh_display(void) {
    ssd1306_clear();

    // Draw each line
    for (int i = 0; i < MAX_LINES; i++) {
        if (display_lines[i][0] != '\0') {
            ssd1306_draw_string(0, i * 8, display_lines[i]);
        }
    }

    ssd1306_display();
}
