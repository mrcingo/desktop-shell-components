# Development tools

Format the code:

    development-tools/format-all-source-files.sh

Check it (layout, function size, plain ASCII):

    meson test -C build

Test the programs in a headless sway, away from your desktop:

    meson compile -C build send-virtual-key-presses
    development-tools/test-window-components-in-headless-compositor.sh build OUTPUT_DIRECTORY

Retake the README images:

    meson compile -C build send-virtual-key-presses login-form-widget-preview
    development-tools/take-documentation-screenshots.sh build
