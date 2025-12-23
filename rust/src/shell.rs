//! Shell - Simple command-line interface
//! Reads from USB Serial, displays on OLED

use crate::console;
use crate::ssd1306;

// Shell configuration
pub const SHELL_MAX_LINE_LENGTH: usize = 64;
pub const SHELL_MAX_ARGS: usize = 8;

// Shell state
static mut INPUT_BUFFER: [u8; SHELL_MAX_LINE_LENGTH] = [0; SHELL_MAX_LINE_LENGTH];
static mut INPUT_POS: usize = 0;

// Output line buffer (8 lines on 64-pixel display with 8-pixel font height)
const MAX_LINES: usize = 8;
static mut DISPLAY_LINES: [[u8; 22]; MAX_LINES] = [[0; 22]; MAX_LINES];  // 21 chars + null
static mut CURRENT_LINE: usize = 0;

// Simple string utilities
fn str_copy(dest: &mut [u8], src: &[u8], max_len: usize) {
    let copy_len = src.len().min(max_len - 1);
    for i in 0..copy_len {
        if src[i] == 0 {
            dest[i] = 0;
            return;
        }
        dest[i] = src[i];
    }
    dest[copy_len] = 0;
}

fn str_len(s: &[u8]) -> usize {
    for (i, &c) in s.iter().enumerate() {
        if c == 0 {
            return i;
        }
    }
    s.len()
}

fn str_equals(s1: &[u8], s2: &[u8]) -> bool {
    let len1 = str_len(s1);
    let len2 = str_len(s2);
    if len1 != len2 {
        return false;
    }
    for i in 0..len1 {
        if s1[i] != s2[i] {
            return false;
        }
    }
    true
}

fn bytes_to_str(bytes: &[u8]) -> &str {
    let len = str_len(bytes);
    // Safety: we're assuming the bytes are valid ASCII
    unsafe { core::str::from_utf8_unchecked(&bytes[..len]) }
}

// Print a line to the display buffer
fn shell_print(text: &str) {
    // Echo to serial console for debugging
    console::puts(text);
    console::puts("\n");

    unsafe {
        // Add to display buffer
        if CURRENT_LINE >= MAX_LINES {
            // Scroll up: shift all lines up by one
            for i in 0..(MAX_LINES - 1) {
                DISPLAY_LINES[i] = DISPLAY_LINES[i + 1];
            }
            CURRENT_LINE = MAX_LINES - 1;
        }

        let text_bytes = text.as_bytes();
        str_copy(&mut DISPLAY_LINES[CURRENT_LINE], text_bytes, 22);
        CURRENT_LINE += 1;
    }

    refresh_display();
}

// Clear the display
fn shell_clear() {
    unsafe {
        for i in 0..MAX_LINES {
            DISPLAY_LINES[i][0] = 0;
        }
        CURRENT_LINE = 0;
    }
    refresh_display();
}

// Command: help
fn cmd_help(_argc: usize, _argv: &[&[u8]]) {
    shell_print("Available commands:");
    shell_print("  help  - Show help");
    shell_print("  clear - Clear screen");
    shell_print("  echo  - Echo text");
}

// Command: clear
fn cmd_clear(_argc: usize, _argv: &[&[u8]]) {
    shell_clear();
}

// Command: echo
fn cmd_echo(argc: usize, argv: &[&[u8]]) {
    if argc < 2 {
        shell_print("Usage: echo <text>");
        return;
    }

    // Reconstruct the message from all arguments
    let mut message: [u8; SHELL_MAX_LINE_LENGTH] = [0; SHELL_MAX_LINE_LENGTH];
    let mut pos: usize = 0;

    for i in 1..argc {
        let arg = argv[i];
        let arg_len = str_len(arg);
        for j in 0..arg_len {
            if pos < SHELL_MAX_LINE_LENGTH - 2 {
                message[pos] = arg[j];
                pos += 1;
            }
        }
        if i < argc - 1 && pos < SHELL_MAX_LINE_LENGTH - 1 {
            message[pos] = b' ';
            pos += 1;
        }
    }
    message[pos] = 0;

    shell_print(bytes_to_str(&message));
}

// Command table entry
struct Command {
    name: &'static [u8],
    handler: fn(usize, &[&[u8]]),
}

static COMMANDS: [Command; 3] = [
    Command { name: b"help\0", handler: cmd_help },
    Command { name: b"clear\0", handler: cmd_clear },
    Command { name: b"echo\0", handler: cmd_echo },
];

// Parse command line and execute
fn shell_execute(cmdline: &[u8]) {
    // Echo the command
    let mut prompt_line: [u8; SHELL_MAX_LINE_LENGTH] = [0; SHELL_MAX_LINE_LENGTH];
    prompt_line[0] = b'>';
    prompt_line[1] = b' ';
    str_copy(&mut prompt_line[2..], cmdline, SHELL_MAX_LINE_LENGTH - 2);
    shell_print(bytes_to_str(&prompt_line));

    // Skip leading whitespace
    let mut start = 0;
    while start < cmdline.len() && cmdline[start] == b' ' {
        start += 1;
    }

    // Empty command
    if start >= cmdline.len() || cmdline[start] == 0 {
        return;
    }

    // Parse arguments
    let mut args_buffer: [u8; SHELL_MAX_LINE_LENGTH] = [0; SHELL_MAX_LINE_LENGTH];
    str_copy(&mut args_buffer, &cmdline[start..], SHELL_MAX_LINE_LENGTH);

    let mut argv: [&[u8]; SHELL_MAX_ARGS] = [&[]; SHELL_MAX_ARGS];
    let mut argc: usize = 0;

    let mut p: usize = 0;
    while p < args_buffer.len() && argc < SHELL_MAX_ARGS {
        // Skip whitespace
        while p < args_buffer.len() && args_buffer[p] == b' ' {
            p += 1;
        }
        if p >= args_buffer.len() || args_buffer[p] == 0 {
            break;
        }

        // Start of argument
        let arg_start = p;

        // Find end of argument
        while p < args_buffer.len() && args_buffer[p] != 0 && args_buffer[p] != b' ' {
            p += 1;
        }

        argv[argc] = &args_buffer[arg_start..p];
        argc += 1;

        if p < args_buffer.len() && args_buffer[p] == b' ' {
            // Mark end of this arg for string comparison
            // Note: We can't modify the buffer here in safe Rust
            p += 1;
        }
    }

    if argc == 0 {
        return;
    }

    // Find and execute command
    let mut found = false;
    for cmd in &COMMANDS {
        if str_equals(argv[0], cmd.name) {
            (cmd.handler)(argc, &argv);
            found = true;
            break;
        }
    }

    if !found {
        let prefix = "command unknown: ";
        let cmd_str = bytes_to_str(argv[0]);
        let total_len = prefix.len() + cmd_str.len();

        if total_len <= 21 {
            // Fits on one line
            let mut err_msg: [u8; SHELL_MAX_LINE_LENGTH] = [0; SHELL_MAX_LINE_LENGTH];
            for (i, &c) in prefix.as_bytes().iter().enumerate() {
                err_msg[i] = c;
            }
            for (i, &c) in argv[0].iter().enumerate() {
                if c == 0 { break; }
                err_msg[prefix.len() + i] = c;
            }
            shell_print(bytes_to_str(&err_msg));
        } else {
            // Split across two lines
            shell_print(prefix);
            shell_print(cmd_str);
        }
    }
}

/// Initialize shell
pub fn init() {
    unsafe {
        INPUT_POS = 0;
        CURRENT_LINE = 0;

        // Clear display lines
        for i in 0..MAX_LINES {
            DISPLAY_LINES[i][0] = 0;
        }
    }

    // Show welcome message
    shell_print("RISC-V Shell v1.0");
    shell_print("Type 'help'");
    shell_print(">");
}

/// Process incoming character from serial
pub fn process_char(c: char) {
    let c = c as u8;

    unsafe {
        // Handle backspace
        if c == 0x08 || c == 127 {  // Backspace or DEL
            if INPUT_POS > 0 {
                INPUT_POS -= 1;
                INPUT_BUFFER[INPUT_POS] = 0;
                console::putc('\x08');
                console::putc(' ');
                console::putc('\x08');
            }
            return;
        }

        // Handle newline
        if c == b'\n' || c == b'\r' {
            console::putc('\n');
            INPUT_BUFFER[INPUT_POS] = 0;

            if INPUT_POS > 0 {
                shell_execute(&INPUT_BUFFER);
            }

            // Show prompt after command execution
            shell_print(">");

            INPUT_POS = 0;
            INPUT_BUFFER[0] = 0;
            return;
        }

        // Handle printable characters
        if c >= 32 && c <= 126 {
            if INPUT_POS < SHELL_MAX_LINE_LENGTH - 1 {
                INPUT_BUFFER[INPUT_POS] = c;
                INPUT_POS += 1;
                INPUT_BUFFER[INPUT_POS] = 0;
                console::putc(c as char);  // Echo to serial
            }
        }
    }
}

/// Refresh OLED display with current buffer
pub fn refresh_display() {
    ssd1306::clear();

    unsafe {
        // Draw each line
        for i in 0..MAX_LINES {
            if DISPLAY_LINES[i][0] != 0 {
                ssd1306::draw_string(0, (i * 8) as i32, bytes_to_str(&DISPLAY_LINES[i]));
            }
        }
    }

    ssd1306::display();
}
