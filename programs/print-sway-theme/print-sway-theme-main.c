/*
 * Prints the colors and font of the running sway bar as a theme style sheet,
 * for the login screen, which starts before sway does and cannot ask it:
 *
 *   desktop-shell-print-sway-theme | sudo tee /etc/xdg/desktop-shell-components/shared-theme.css
 */

#include <stdio.h>
#include <stdlib.h>

#include <glib.h>

#include "compositor-bar-appearance.h"

struct theme_printer {
    GMainLoop *main_loop;
    int        exit_status;
};

static void
ignore_event(
    void     *user_data,
    uint32_t  event_type,
    JsonNode *event_payload
)
{}

static void
stop_on_connection_lost(
    void *user_data
)
{
    struct theme_printer *printer = user_data;

    g_main_loop_quit(printer->main_loop);
}

static const struct compositor_ipc_client_handler printer_handler = {
    .handle_event           = ignore_event,
    .handle_connection_lost = stop_on_connection_lost,
};

static void
print_style_sheet(
    struct theme_printer             *printer,
    struct compositor_bar_appearance *appearance
)
{
    g_autofree char *style_sheet = compositor_bar_appearance_to_style_sheet(appearance);

    printf("/* The colors and font of the sway bar, printed by desktop-shell-print-sway-theme. */\n%s", style_sheet);
    compositor_bar_appearance_destroy(appearance);

    printer->exit_status = EXIT_SUCCESS;
}

static void
print_theme(
    struct compositor_bar_appearance *appearance,
    void                             *user_data
)
{
    struct theme_printer *printer = user_data;

    if (appearance == NULL) {
        fprintf(stderr, "sway has no bar to take colors from\n");
    } else {
        print_style_sheet(printer, appearance);
    }

    g_main_loop_quit(printer->main_loop);
}

static struct compositor_ipc_client *
connect_to_sway(
    struct theme_printer *printer
)
{
    const char *socket_path = g_getenv("SWAYSOCK");

    if (socket_path == NULL) {
        fprintf(stderr, "SWAYSOCK is not set; run this inside sway\n");
        return NULL;
    }

    return compositor_ipc_client_connect(socket_path, &printer_handler, printer, NULL);
}

static void
request_theme_and_wait(
    struct theme_printer         *printer,
    struct compositor_ipc_client *client
)
{
    if (client == NULL) {
        return;
    }

    compositor_bar_appearance_request(client, print_theme, printer);
    g_main_loop_run(printer->main_loop);
}

int
main(void)
{
    struct theme_printer          printer = {g_main_loop_new(NULL, FALSE), EXIT_FAILURE};
    struct compositor_ipc_client *client  = connect_to_sway(&printer);

    request_theme_and_wait(&printer, client);
    compositor_ipc_client_destroy(client);
    g_main_loop_unref(printer.main_loop);

    return printer.exit_status;
}
