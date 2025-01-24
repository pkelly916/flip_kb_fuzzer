#include "usb_hid_keyboard.h"
#include "usb_hid_keyboard_map.h"
#include <furi.h>
#include <gui/elements.h>

// pjk
/* TODO
 * write to a log on the SD card
 * bonus: add third button to just loop over every possible key + modifier combo, then finish
 */



// flipper res
#define FLIPPER_W 128
#define FLIPPER_H 64

// button layout
#define BUTTON_WIDTH_PERCENT 80 
#define BUTTON_HEIGHT_PERCENT 20
#define BUTTON_PADDING 1 
#define ROW_COUNT 2
#define LOWER_LIMIT_STRING_LENGTH 1
#define UPPER_LIMIT_STRING_LENGTH 20

struct UsbHidKeyboard {
    View* view;
};

typedef struct {
    uint8_t y;
    bool ok_pressed;
    bool connected;
    char button_string[5];
} UsbHidKeyboardModel;

enum Buttons {
    PUMP,
    FLOOD
};

typedef struct {
    enum Buttons button;
    char* label;
    char* alt_label;
    uint8_t enabled;
    uint8_t width;
} UsbHidKeyFuzzerButton;

const int BUTTON_WIDTH = BUTTON_WIDTH_PERCENT * FLIPPER_W / 100;
const int BUTTON_HEIGHT = BUTTON_HEIGHT_PERCENT * FLIPPER_H / 100;

const int MARGIN_LEFT = (FLIPPER_W - BUTTON_WIDTH) / 2;

// definition of buttons 
UsbHidKeyFuzzerButton usb_hid_key_fuzzer_buttons[ROW_COUNT] = {
    {.button = PUMP, .label = "Pump!", .alt_label = "", .enabled = 0, .width = BUTTON_WIDTH},
    {.button = FLOOD, .label = "Flood", .alt_label = "Stop", .enabled = 0, .width = BUTTON_WIDTH},
};


// global thread variables
FuriThread* kf_flood_thread;
uint8_t RUN_THREAD = 0; // TODO: use thread flags

/* send keystroke */
/* param is send string */
bool send_string(const uint16_t* param) {
    uint32_t i = 0;
    while (param[i] != '\0') {
        uint16_t keycode = param[i];
        if (keycode != HID_KEYBOARD_NONE) {
            furi_hal_hid_kb_press(keycode);
            furi_hal_hid_kb_release(keycode);
        }
        i++;
    }
    return true;
}

// randomly selects between 0 and 4 modifiers out of 
// HidKeyboardMods enum (left only)
// builds and returns modifier mask
uint16_t get_rand_modifier() {
    // there is a 1/8 chance there will not be a modifier
    uint16_t modifier = (rand() % 8) << 8;
    // there is now a 50% chance there will not be a modifier
    // note: extra call to rand
    return modifier * (rand() % 2);
}

/* name is as does, populates str parameter and returns it */
static uint16_t* create_rand_string(uint16_t* str, size_t size)
{
    if (size) {
        --size;
        for (size_t n = 0; n < size; n++) {
            uint16_t modifier = get_rand_modifier();
            int key = rand() % (int)(sizeof hid_full_asciimap - 1);
            str[n] = hid_full_asciimap[key] | modifier;
            
        }
        str[size] = '\0';
    }
    return str;
}

/* button ui */
static void usb_hid_keyboard_draw_button(
    Canvas* canvas,
    UsbHidKeyboardModel* model,
    uint8_t y,
    UsbHidKeyFuzzerButton button,
    bool selected) 
{
    canvas_set_color(canvas, ColorBlack);
    if(selected) {
        // Draw a filled box
        elements_slightly_rounded_box(
            canvas,
            MARGIN_LEFT, 
            y * (BUTTON_HEIGHT + BUTTON_PADDING),
            BUTTON_WIDTH,
            BUTTON_HEIGHT);
        canvas_set_color(canvas, ColorWhite);
    } else {
        // Draw a framed box
        elements_slightly_rounded_frame(
            canvas,
            MARGIN_LEFT, 
            y * (BUTTON_HEIGHT + BUTTON_PADDING),
            BUTTON_WIDTH,
            BUTTON_HEIGHT);
    }

    // change to alt_label when enabled 
    strcpy(model->button_string, (button.enabled != 0) ? button.alt_label : button.label);

    canvas_draw_str_aligned(
        canvas,
        MARGIN_LEFT + BUTTON_WIDTH / 2 + 1,
        y * (BUTTON_HEIGHT + BUTTON_PADDING) + BUTTON_HEIGHT / 2,
        AlignCenter,
        AlignCenter,
        model->button_string);
}

/* button hover ui */
static void usb_hid_keyboard_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    UsbHidKeyboardModel* model = context;

    canvas_set_font(canvas, FontKeyboard);
    // Start shifting all buttons up if on the next row (Scrolling)
    // note: scrolling functionality was preserved but is currently unused (untested)
    uint8_t initY = model->y - 4 > 0 ? model->y - 4 : 0;
    for(uint8_t y = initY; y < ROW_COUNT; y++) {
        const UsbHidKeyFuzzerButton currentButton = usb_hid_key_fuzzer_buttons[y];
        // Select when the button is hovered
        // Deselect when the button clicked or not hovered
        bool buttonSelected = y == model->y;
        usb_hid_keyboard_draw_button(
            canvas,
            model,
            y - initY,
            currentButton,
            (!model->ok_pressed && buttonSelected)
        );
    }
}

/* used for filling in currently selected button */
static void usb_hid_key_fuzzer_get_select_button(UsbHidKeyboardModel* model, int rowDelta) {
    // wrap around at top
    if(model->y == 0 && rowDelta < 0) 
      model->y = ROW_COUNT - 1;
    // wrap around at bottom
    else if (model->y + rowDelta > ROW_COUNT - 1)
      model->y = 0;
    else
      model->y += rowDelta;
}

static void send_rand_string() {
    int random_length = (rand() % (UPPER_LIMIT_STRING_LENGTH - LOWER_LIMIT_STRING_LENGTH + 1)) + LOWER_LIMIT_STRING_LENGTH;
    uint16_t* ptr_rand_string = malloc(random_length * (sizeof(uint16_t)));

    if (ptr_rand_string != NULL) {
        // send random string as hid keyboard
        send_string(create_rand_string(ptr_rand_string, random_length));
        free(ptr_rand_string);
    }
    furi_hal_hid_kb_release_all();
}

/* main flood thread function */
static int32_t fuzzer_flood_worker(void *context) {
    UNUSED(context);

    while (RUN_THREAD) {

        // usb_hid_key_fuzzer_buttons FLOOD button idx = 1
        if (usb_hid_key_fuzzer_buttons[1].enabled) {
            send_rand_string();
            furi_delay_ms(100); // 10 a second
        }
        else {
            furi_delay_ms(500);
        }
    }

    return 0;
}

/* main rand string generator and sender */
static void usb_hid_key_fuzzer_process(UsbHidKeyboard* usb_hid_keyboard, InputEvent* event) {
    with_view_model(
        usb_hid_keyboard->view,
        UsbHidKeyboardModel * model,
        { // core of key press processing
            // run selected button action
            if(event->key == InputKeyOk) {
                if(event->type == InputTypePress) {
                    model->ok_pressed = true;
                } 
                else if(event->type == InputTypeLong || event->type == InputTypeShort) {

                    // get selected button and run its action
                    enum Buttons selectedButton = usb_hid_key_fuzzer_buttons[model->y].button;

                    switch (selectedButton) {
                    case PUMP:
                        usb_hid_key_fuzzer_buttons[1].enabled = 0; // stop flooding if its running
                        send_rand_string();
                        goto default_label;
               
                     case FLOOD:
                        FuriThreadState kf_flood_thread_state = furi_thread_get_state(kf_flood_thread);
                        if (kf_flood_thread_state == FuriThreadStateStopped) {
                           RUN_THREAD = 1;
                           furi_thread_start(kf_flood_thread);
                        }

                        usb_hid_key_fuzzer_buttons[model->y].enabled = !usb_hid_key_fuzzer_buttons[model->y].enabled;
                        goto default_label;

                     default:
                        default_label:
                        furi_hal_hid_kb_press(HID_KEYBOARD_RETURN);
                        furi_hal_hid_kb_release_all();
                    }

                }
                else if(event->type == InputTypeRelease) {
                    // release happens after short and long presses
                    model->ok_pressed = false;
                }
            }
            // cycle the selected keys
            else if(event->type == InputTypePress || event->type == InputTypeRepeat) {
                if(event->key == InputKeyUp) {
                    usb_hid_key_fuzzer_get_select_button(model, -1);
                } else if (event->key == InputKeyDown) {
                    usb_hid_key_fuzzer_get_select_button(model, 1);
                } 
            }

        },
        true
   );
}

/* button get pressed, this get called */
static bool usb_hid_keyboard_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    UsbHidKeyboard* usb_hid_keyboard = context;
    bool consumed = false;

    // quit app
    if(event->key == InputKeyBack) {
        furi_hal_hid_kb_release_all();
    // process flipper keypress
    } else {
        usb_hid_key_fuzzer_process(usb_hid_keyboard, event);
        consumed = true;
    }

    return consumed;
}

UsbHidKeyboard* usb_hid_keyboard_alloc() {
    UsbHidKeyboard* usb_hid_keyboard = malloc(sizeof(UsbHidKeyboard));

    usb_hid_keyboard->view = view_alloc();
    view_set_context(usb_hid_keyboard->view, usb_hid_keyboard);
    view_allocate_model(usb_hid_keyboard->view, ViewModelTypeLocking, sizeof(UsbHidKeyboardModel));
    view_set_draw_callback(usb_hid_keyboard->view, usb_hid_keyboard_draw_callback);
    view_set_input_callback(usb_hid_keyboard->view, usb_hid_keyboard_input_callback);

    with_view_model(usb_hid_keyboard->view, UsbHidKeyboardModel * model, { model->connected = true; }, true);

    kf_flood_thread = furi_thread_alloc();
    furi_thread_set_stack_size(kf_flood_thread, 1024);
    furi_thread_set_callback(kf_flood_thread, fuzzer_flood_worker);

    return usb_hid_keyboard;
}

void usb_hid_keyboard_free(UsbHidKeyboard* usb_hid_keyboard) {
    FuriThreadState kf_flood_thread_state = furi_thread_get_state(kf_flood_thread);
    if (kf_flood_thread_state != FuriThreadStateStopped) {
        RUN_THREAD = 0;
        furi_thread_join(kf_flood_thread);
    }
    furi_thread_free(kf_flood_thread);

    furi_assert(usb_hid_keyboard);
    view_free(usb_hid_keyboard->view);
    free(usb_hid_keyboard);
}

View* usb_hid_keyboard_get_view(UsbHidKeyboard* usb_hid_keyboard) {
    furi_assert(usb_hid_keyboard);
    return usb_hid_keyboard->view;
}
