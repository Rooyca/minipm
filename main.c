#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Window dimensions
#define WINDOW_WIDTH  343
#define WINDOW_HEIGHT 96

// Colors
#define BG_COLOR     0x222222
#define TEXT_COLOR   0xFFFFFF
#define BORDER_COLOR 0x444444

// Application states
typedef enum {
    STATE_NORMAL = 0,
    STATE_REBOOT_CONFIRM,
    STATE_SHUTDOWN_CONFIRM,
    STATE_SUSPEND_CONFIRM,
    STATE_LOGOUT_CONFIRM,
    STATE_REBOOTING,
    STATE_SHUTTING_DOWN,
    STATE_SUSPENDING,
    STATE_LOGGING_OUT
} app_state_t;

// UI messages for each state
static const char *state_messages[] = {
    "Press: (r)eboot (s)hutdown s(u)spend (l)ogout (q)uit",
    "Press 'r' again to confirm reboot",
    "Press 's' again to confirm shutdown", 
    "Press 'u' again to confirm suspend",
    "Press 'l' again to confirm logout",
    "Rebooting system...",
    "Shutting down system...",
    "Suspending system...",
    "Logging out..."
};

// Power management commands
static const char *power_commands[] = {
    NULL,           // STATE_NORMAL
    "reboot",       // STATE_REBOOT_CONFIRM  
    "poweroff",     // STATE_SHUTDOWN_CONFIRM
    "suspend",      // STATE_SUSPEND_CONFIRM
    "terminate-user", // STATE_LOGOUT_CONFIRM
    "reboot",       // STATE_REBOOTING
    "poweroff",     // STATE_SHUTTING_DOWN
    "suspend",      // STATE_SUSPENDING
    "terminate-user" // STATE_LOGGING_OUT
};

/**
 * Execute a loginctl command in a child process
 */
static void execute_power_command(const char *command) {
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process
        if (strcmp(command, "terminate-user") == 0) {
            // For logout, we need to get the current user ID
            char uid_str[32];
            snprintf(uid_str, sizeof(uid_str), "%d", getuid());
            execlp("loginctl", "loginctl", "terminate-user", uid_str, (char *)NULL);
        } else {
            execlp("loginctl", "loginctl", command, (char *)NULL);
        }
        _exit(127); // Exit if exec fails
    }
    // Parent continues (pid > 0 or pid == -1 for error)
}

/**
 * Initialize X11 window with proper properties
 */
static Window create_main_window(Display *display, int screen) {
    Window root = RootWindow(display, screen);
    
    // Create window
    Window window = XCreateSimpleWindow(
        display, root,
        100, 100,                    // x, y position
        WINDOW_WIDTH, WINDOW_HEIGHT, // width, height
        2,                           // border width
        BORDER_COLOR,                // border color
        BG_COLOR                     // background color
    );
    
    // Set window properties
    XStoreName(display, window, "minipm - Power Manager");
    
    // Set class hints for window manager
    XClassHint *class_hint = XAllocClassHint();
    if (class_hint) {
        class_hint->res_name = "minipm";
        class_hint->res_class = "minipm";
        XSetClassHint(display, window, class_hint);
        XFree(class_hint);
    }
    
    // Set size hints to make window non-resizable
    XSizeHints *size_hints = XAllocSizeHints();
    if (size_hints) {
        size_hints->flags = PMinSize | PMaxSize;
        size_hints->min_width = size_hints->max_width = WINDOW_WIDTH;
        size_hints->min_height = size_hints->max_height = WINDOW_HEIGHT;
        XSetWMNormalHints(display, window, size_hints);
        XFree(size_hints);
    }
    
    // Enable window manager close button
    Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wm_delete, 1);
    
    // Select input events
    XSelectInput(display, window, ExposureMask | KeyPressMask);
    
    return window;
}

/**
 * Draw the current state message on the window
 */
static void draw_interface(Display *display, Window window, GC gc, app_state_t state) {
    // Clear window
    XSetForeground(display, gc, BG_COLOR);
    XFillRectangle(display, window, gc, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    
    // Set text color
    XSetForeground(display, gc, TEXT_COLOR);
    
    // Get message for current state
    const char *message = state_messages[state];
    int text_length = strlen(message);
    
    // Calculate text position (roughly centered)
    int text_x = (WINDOW_WIDTH - (text_length * 6)) / 2;
    int text_y = WINDOW_HEIGHT / 2;
    
    // Draw text
    XDrawString(display, window, gc, text_x, text_y, message, text_length);
}

/**
 * Handle key press events and state transitions
 */
static app_state_t handle_keypress(KeySym key, app_state_t current_state) {
    switch (current_state) {
        case STATE_NORMAL:
            switch (key) {
                case XK_r: return STATE_REBOOT_CONFIRM;
                case XK_s: return STATE_SHUTDOWN_CONFIRM;  
                case XK_u: return STATE_SUSPEND_CONFIRM;
                case XK_l: return STATE_LOGOUT_CONFIRM;
                default: return STATE_NORMAL;
            }
            
        case STATE_REBOOT_CONFIRM:
            if (key == XK_r || key == XK_Return) {
                execute_power_command(power_commands[STATE_REBOOTING]);
                return STATE_REBOOTING;
            }
            return STATE_NORMAL; // Any other key cancels
            
        case STATE_SHUTDOWN_CONFIRM:
            if (key == XK_s || key == XK_Return) {
                execute_power_command(power_commands[STATE_SHUTTING_DOWN]);
                return STATE_SHUTTING_DOWN;
            }
            return STATE_NORMAL;
            
        case STATE_SUSPEND_CONFIRM:
            if (key == XK_u || key == XK_Return) {
                execute_power_command(power_commands[STATE_SUSPENDING]);
                return STATE_SUSPENDING;
            }
            return STATE_NORMAL;
            
        case STATE_LOGOUT_CONFIRM:
            if (key == XK_l || key == XK_Return) {
                execute_power_command(power_commands[STATE_LOGGING_OUT]);
                return STATE_LOGGING_OUT;
            }
            return STATE_NORMAL;
            
        default:
            return STATE_NORMAL;
    }
}

/**
 * Main application entry point
 */
int main(void) {
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Error: Cannot open X11 display\n");
        return EXIT_FAILURE;
    }
    
    // Ignore SIGCHLD to avoid zombie processes
    signal(SIGCHLD, SIG_IGN);
    
    int screen = DefaultScreen(display);
    Window window = create_main_window(display, screen);
    
    // Create graphics context
    GC graphics_context = XCreateGC(display, window, 0, NULL);
    
    // Show window
    XMapWindow(display, window);
    
    // Get WM_DELETE_WINDOW atom for close button handling
    Atom wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    
    app_state_t current_state = STATE_NORMAL;
    XEvent event;
    int running = 1;
    
    // Main event loop
    while (running) {
        XNextEvent(display, &event);
        
        switch (event.type) {
            case Expose:
                // Redraw window content
                draw_interface(display, window, graphics_context, current_state);
                break;
                
            case KeyPress: {
                KeySym key = XLookupKeysym(&event.xkey, 0);
                
                // Handle quit keys
                if (key == XK_q || key == XK_Escape) {
                    running = 0;
                    break;
                }
                
                // Handle state transitions
                app_state_t new_state = handle_keypress(key, current_state);
                if (new_state != current_state) {
                    current_state = new_state;
                    draw_interface(display, window, graphics_context, current_state);
                }
                break;
            }
            
            case ClientMessage:
                // Handle window close button
                if ((Atom)event.xclient.data.l[0] == wm_delete_window) {
                    running = 0;
                }
                break;
        }
    }
    
    // Cleanup
    XFreeGC(display, graphics_context);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
    
    return EXIT_SUCCESS;
}
