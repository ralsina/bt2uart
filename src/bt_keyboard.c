#include "bt_keyboard.h"

#include <btstack.h>
#include <btstack_hid_parser.h>
#include <pico/cyw43_arch.h>
#include <pico/btstack_cyw43.h>
#include <ble/gatt-service/hids_client.h>
#include <ble/sm.h>
#include <hardware/uart.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "pins.h"

// Uncomment to skip scan and connect directly to a known device:
// #define TARGET_BD_ADDR {0x54, 0x46, 0x6E, 0x00, 0x04, 0x8E}

#define HID_KEY_A           0x04
#define HID_KEY_B           0x05
#define HID_KEY_C           0x06
#define HID_KEY_D           0x07
#define HID_KEY_E           0x08
#define HID_KEY_F           0x09
#define HID_KEY_G           0x0A
#define HID_KEY_H           0x0B
#define HID_KEY_I           0x0C
#define HID_KEY_J           0x0D
#define HID_KEY_K           0x0E
#define HID_KEY_L           0x0F
#define HID_KEY_M           0x10
#define HID_KEY_N           0x11
#define HID_KEY_O           0x12
#define HID_KEY_P           0x13
#define HID_KEY_Q           0x14
#define HID_KEY_R           0x15
#define HID_KEY_S           0x16
#define HID_KEY_T           0x17
#define HID_KEY_U           0x18
#define HID_KEY_V           0x19
#define HID_KEY_W           0x1A
#define HID_KEY_X           0x1B
#define HID_KEY_Y           0x1C
#define HID_KEY_Z           0x1D
#define HID_KEY_1           0x1E
#define HID_KEY_2           0x1F
#define HID_KEY_3           0x20
#define HID_KEY_4           0x21
#define HID_KEY_5           0x22
#define HID_KEY_6           0x23
#define HID_KEY_7           0x24
#define HID_KEY_8           0x25
#define HID_KEY_9           0x26
#define HID_KEY_0           0x27
#define HID_KEY_ENTER       0x28
#define HID_KEY_ESCAPE      0x29
#define HID_KEY_BACKSPACE   0x2A
#define HID_KEY_TAB         0x2B
#define HID_KEY_SPACE       0x2C
#define HID_KEY_MINUS       0x2D
#define HID_KEY_EQUAL       0x2E
#define HID_KEY_BRACE_OPEN  0x2F
#define HID_KEY_BRACE_CLOSE 0x30
#define HID_KEY_BACKSLASH   0x31
#define HID_KEY_HASH        0x32
#define HID_KEY_SEMICOLON   0x33
#define HID_KEY_APOSTROPHE  0x34
#define HID_KEY_GRAVE       0x35
#define HID_KEY_COMMA       0x36
#define HID_KEY_PERIOD      0x37
#define HID_KEY_SLASH       0x38
#define HID_KEY_CAPSLOCK    0x39
#define HID_KEY_DELETE      0x4C
#define HID_KEY_FN          0x9D  // Fn key modifier

#define HID_MOD_LCTRL  0x01
#define HID_MOD_LSHIFT 0x02
#define HID_MOD_LALT   0x04
#define HID_MOD_LGUI   0x08
#define HID_MOD_RCTRL  0x10
#define HID_MOD_RSHIFT 0x20
#define HID_MOD_RALT   0x40
#define HID_MOD_RGUI   0x80
#define HID_MOD_FN     0x100 // Virtual modifier for Fn key tracking

#define HID_REPORT_SIZE 8

static const uint8_t hid_to_ascii[0x53][2] = {
    {0, 0}, {0, 0}, {0, 0}, {0, 0},
    {'a', 'A'}, {'b', 'B'}, {'c', 'C'}, {'d', 'D'},
    {'e', 'E'}, {'f', 'F'}, {'g', 'G'}, {'h', 'H'},
    {'i', 'I'}, {'j', 'J'}, {'k', 'K'}, {'l', 'L'},
    {'m', 'M'}, {'n', 'N'}, {'o', 'O'}, {'p', 'P'},
    {'q', 'Q'}, {'r', 'R'}, {'s', 'S'}, {'t', 'T'},
    {'u', 'U'}, {'v', 'V'}, {'w', 'W'}, {'x', 'X'},
    {'y', 'Y'}, {'z', 'Z'},
    {'1', '!'}, {'2', '@'}, {'3', '#'}, {'4', '$'},
    {'5', '%'}, {'6', '^'}, {'7', '&'}, {'8', '*'},
    {'9', '('}, {'0', ')'},
    {0x0D, 0x0D},
    {0, 0},
    {0x08, 0x08},
    {0x09, 0x09},
    {' ', ' '},
    {'-', '_'}, {'=', '+'},
    {'[', '{'}, {']', '}'},
    {'\\', '|'}, {'#', '~'},
    {';', ':'}, {'\'', '"'},
    {'`', '~'},
    {',', '<'}, {'.', '>'}, {'/', '?'},
    {0, 0},
};

#define HID_DESCRIPTOR_STORAGE_SIZE 300
static uint8_t hid_descriptor_storage[HID_DESCRIPTOR_STORAGE_SIZE];
static uint16_t hid_descriptor_len = 0;
static uint16_t hids_cid = 0;
static hci_con_handle_t le_connection_handle = HCI_CON_HANDLE_INVALID;

static uint8_t prev_report[HID_REPORT_SIZE];
static bool has_prev_report = false;
static bool connected = false;
static bd_addr_t target_addr;
static bool has_target = false;
static bool scanning = false;
static bool capslock = false;
static bool fn_pressed = false;
static bool hid_descriptor_available = false;

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t sm_event_callback_registration;

static uint8_t find_in_prev(uint8_t code)
{
    for (int i = 2; i < HID_REPORT_SIZE; i++) {
        if (prev_report[i] == code) return 1;
    }
    return 0;
}

static uint8_t find_in_report(const uint8_t *report, uint8_t code)
{
    for (int i = 2; i < HID_REPORT_SIZE; i++) {
        if (report[i] == code) return 1;
    }
    return 0;
}

static const char* get_key_name_for_code(uint8_t code) {
    // Function keys
    switch (code) {
        case 0x3a: return "F1";
        case 0x3b: return "F2";
        case 0x3c: return "F3";
        case 0x3d: return "F4";
        case 0x3e: return "F5";
        case 0x3f: return "F6";
        case 0x40: return "F7";
        case 0x41: return "F8";
        case 0x42: return "F9";
        case 0x43: return "F10";
        case 0x44: return "F11";
        case 0x45: return "F12";
        // Special keys
        case 0x28: return "Enter";
        case 0x29: return "Escape";
        case 0x2a: return "Backspace";
        case 0x2b: return "Tab";
        case 0x2c: return "Space";
        case 0x4a: return "Home";
        case 0x4b: return "PageUp";
        case 0x4c: return "Delete";
        case 0x4d: return "End";
        case 0x4e: return "PageDown";
        case 0x4f: return "Right";
        case 0x50: return "Left";
        case 0x51: return "Down";
        case 0x52: return "Up";
        default: return NULL;
    }
}

static void inject_key(char key, enum key_state state)
{
    const char *s;
    switch (state) {
        case KEY_STATE_PRESSED:  s = "PRESSED";  break;
        case KEY_STATE_HOLD:     s = "HOLD";     break;
        case KEY_STATE_RELEASED: s = "RELEASED"; break;
        default:                 s = "IDLE";     break;
    }

    const char* key_name = get_key_name_for_code((uint8_t)key);
    if (key_name) {
        printf("key: %s %s\n", key_name, s);
    } else if (key >= 0x20 && key <= 0x7E) {
        printf("key: '%c' (0x%02x) %s\n", key, (uint8_t)key, s);
    } else {
        printf("key: 0x%02x %s\n", (uint8_t)key, s);
    }
}

static char hid_code_to_char(uint8_t code, uint8_t modifiers, bool capslock)
{
    if (code >= 0x53) return 0;
    uint8_t c = hid_to_ascii[code][0];
    if (c == 0) return 0;

    bool shift = (modifiers & (HID_MOD_LSHIFT | HID_MOD_RSHIFT)) != 0;
    bool ctrl = (modifiers & (HID_MOD_LCTRL | HID_MOD_RCTRL)) != 0;

    if (ctrl && c >= 'a' && c <= 'z')
        return c - 'a' + 1;

    if (capslock && c >= 'a' && c <= 'z')
        shift = !shift;

    if (shift)
        c = hid_to_ascii[code][1];

    return c;
}

static void process_hid_report(const uint8_t *report_buf, uint16_t buf_len, uint8_t report_id)
{
    uint8_t offset = (report_id > 0 && buf_len > 0 && report_buf[0] == report_id) ? 1 : 0;
    const uint8_t *report = report_buf + offset;
    uint16_t len = buf_len - offset;

    if (len < HID_REPORT_SIZE) return;

    uint8_t modifiers = report[0];
    uint8_t prev_mod = has_prev_report ? prev_report[0] : 0;

    // Check for Fn key state (0x9D in the key array)
    bool fn_now_pressed = find_in_report(report, HID_KEY_FN);
    bool fn_was_pressed = has_prev_report && find_in_prev(HID_KEY_FN);
    fn_pressed = fn_now_pressed;

    // Handle Fn key combinations by converting them to function keys
    uint8_t modified_report[HID_REPORT_SIZE];
    memcpy(modified_report, report, HID_REPORT_SIZE);

    if (fn_now_pressed && !fn_was_pressed) {
        // Fn was just pressed, look for keys that should become function keys
        for (int i = 2; i < HID_REPORT_SIZE; i++) {
            if (report[i] >= 0x04 && report[i] <= 0x1D) {
                // Letters A-Z: Handle potential Fn shortcuts
                switch(report[i]) {
                    case 0x04: modified_report[i] = 0x3A; break; // A+Fn -> F1
                    case 0x05: modified_report[i] = 0x3B; break; // B+Fn -> F2
                    case 0x06: modified_report[i] = 0x3C; break; // C+Fn -> F3
                    case 0x07: modified_report[i] = 0x3D; break; // D+Fn -> F4
                    case 0x08: modified_report[i] = 0x3E; break; // E+Fn -> F5
                    case 0x09: modified_report[i] = 0x3F; break; // F+Fn -> F6
                    case 0x0A: modified_report[i] = 0x40; break; // G+Fn -> F7
                    case 0x0B: modified_report[i] = 0x41; break; // H+Fn -> F8
                    case 0x0C: modified_report[i] = 0x42; break; // I+Fn -> F9
                    case 0x0D: modified_report[i] = 0x43; break; // J+Fn -> F10
                    case 0x0E: modified_report[i] = 0x44; break; // K+Fn -> F11
                    case 0x0F: modified_report[i] = 0x45; break; // L+Fn -> F12
                }
            } else if (report[i] >= 0x1E && report[i] <= 0x27) {
                // Numbers 1-0: Handle Fn+number -> function keys
                switch(report[i]) {
                    case 0x1E: modified_report[i] = 0x3A; break; // 1+Fn -> F1
                    case 0x1F: modified_report[i] = 0x3B; break; // 2+Fn -> F2
                    case 0x20: modified_report[i] = 0x3C; break; // 3+Fn -> F3
                    case 0x21: modified_report[i] = 0x3D; break; // 4+Fn -> F4
                    case 0x22: modified_report[i] = 0x3E; break; // 5+Fn -> F5
                    case 0x23: modified_report[i] = 0x3F; break; // 6+Fn -> F6
                    case 0x24: modified_report[i] = 0x40; break; // 7+Fn -> F7
                    case 0x25: modified_report[i] = 0x41; break; // 8+Fn -> F8
                    case 0x26: modified_report[i] = 0x42; break; // 9+Fn -> F9
                    case 0x27: modified_report[i] = 0x43; break; // 0+Fn -> F10
                }
            }
        }
        // Remove Fn key from the report since we've processed it
        for (int i = 2; i < HID_REPORT_SIZE; i++) {
            if (modified_report[i] == HID_KEY_FN) {
                modified_report[i] = 0;
                break;
            }
        }
    }

    // Use the modified report for UART transmission and processing
    const uint8_t *final_report = fn_pressed ? modified_report : report;

    printf("report: rid=%u mod=0x%02x keys=", report_id, modifiers);
    for (int i = 2; i < HID_REPORT_SIZE && i < len; i++) {
        if (final_report[i]) printf("0x%02x ", final_report[i]);
    }
    printf("\n");

    // Send (possibly modified) HID report over UART: 0xFE + 8 report bytes
    uint8_t frame[9] = {0xFE};
    memcpy(frame + 1, final_report, HID_REPORT_SIZE);
    uart_write_blocking(UART_ID, frame, 9);

    if (!find_in_prev(HID_KEY_CAPSLOCK) && find_in_report(final_report, HID_KEY_CAPSLOCK)) {
        capslock = !capslock;
        printf("capslock: %s\n", capslock ? "ON" : "OFF");
    }

    uint8_t mod_changes = modifiers ^ prev_mod;
    if (mod_changes & (HID_MOD_LSHIFT | HID_MOD_RSHIFT)) {
        inject_key(KEY_MOD_SHL, modifiers & (HID_MOD_LSHIFT | HID_MOD_RSHIFT)
            ? KEY_STATE_PRESSED : KEY_STATE_RELEASED);
    }
    if (mod_changes & (HID_MOD_LALT | HID_MOD_RALT)) {
        inject_key(KEY_MOD_ALT, modifiers & (HID_MOD_LALT | HID_MOD_RALT)
            ? KEY_STATE_PRESSED : KEY_STATE_RELEASED);
    }
    if (mod_changes & (HID_MOD_LCTRL | HID_MOD_RCTRL)) {
        inject_key(KEY_MOD_SYM, modifiers & (HID_MOD_LCTRL | HID_MOD_RCTRL)
            ? KEY_STATE_PRESSED : KEY_STATE_RELEASED);
    }

    for (int i = 2; i < HID_REPORT_SIZE; i++) {
        uint8_t code = final_report[i];
        if (code == 0 || code == HID_KEY_CAPSLOCK) continue;
        if (has_prev_report && find_in_prev(code)) continue;

        // Handle special keys directly (Enter, Escape, Backspace, Tab, Space, Function keys, Navigation keys, etc.)
        if ((code >= 0x28 && code <= 0x2c) || (code >= 0x3a && code <= 0x65)) {
            inject_key(code, KEY_STATE_PRESSED);
            continue;
        }

        uint8_t ch = hid_code_to_char(code, modifiers, capslock);
        if (ch == 0) continue;

        inject_key((char)ch, KEY_STATE_PRESSED);
    }

    if (has_prev_report) {
        for (int i = 2; i < HID_REPORT_SIZE; i++) {
            uint8_t code = prev_report[i];
            if (code == 0 || code == HID_KEY_CAPSLOCK) continue;
            if (find_in_report(report, code)) continue;

            // Handle special keys directly (Enter, Escape, Backspace, Tab, Space, Function keys, Navigation keys, etc.)
            if ((code >= 0x28 && code <= 0x2c) || (code >= 0x3a && code <= 0x65)) {
                inject_key(code, KEY_STATE_RELEASED);
                continue;
            }

            uint8_t ch = hid_code_to_char(code, prev_mod, capslock);
            if (ch == 0) continue;

            inject_key((char)ch, KEY_STATE_RELEASED);
        }
    }

    memcpy(prev_report, report, HID_REPORT_SIZE);
    has_prev_report = true;
}

static void packet_handler(uint8_t packet_type, uint16_t channel,
                           uint8_t *packet, uint16_t size)
{
    bd_addr_t event_addr;

    switch (packet_type) {
    case HCI_EVENT_PACKET:
        switch (hci_event_packet_get_type(packet)) {
        case BTSTACK_EVENT_STATE:
            {
                uint8_t s = btstack_event_state_get_state(packet);
                printf("BTSTACK_EVENT_STATE %d\n", s);
                if (s == HCI_STATE_WORKING) {
                    printf("BT stack ready, scanning...\n");
                    gap_set_scan_parameters(1, 0x100, 0x50);
                    gap_start_scan();
                    scanning = true;
                }
            }
            break;

        case BTSTACK_EVENT_POWERON_FAILED:
            printf("BTSTACK_EVENT_POWERON_FAILED\n");
            break;

        case GAP_EVENT_ADVERTISING_REPORT:
            {
                bool is_hid = ad_data_contains_uuid16(
                    gap_event_advertising_report_get_data_length(packet),
                    gap_event_advertising_report_get_data(packet),
                    ORG_BLUETOOTH_SERVICE_HUMAN_INTERFACE_DEVICE);
                if (!is_hid) break;

                gap_event_advertising_report_get_address(packet, event_addr);
                printf("Found HID keyboard %s!\n", bd_addr_to_str(event_addr));
                gap_stop_scan();
                scanning = false;
                bd_addr_copy(target_addr, event_addr);
                has_target = true;
                uint8_t addr_type = gap_event_advertising_report_get_address_type(packet);
                uint8_t err = gap_connect(target_addr, addr_type);
                if (err != ERROR_CODE_SUCCESS) {
                    printf("gap_connect failed: 0x%02x, rescanning\n", err);
                    gap_start_scan();
                    scanning = true;
                }
            }
            break;

        case HCI_EVENT_LE_META:
            if (hci_event_le_meta_get_subevent_code(packet) ==
                HCI_SUBEVENT_LE_CONNECTION_COMPLETE) {
                uint8_t status = hci_subevent_le_connection_complete_get_status(packet);
                if (status == ERROR_CODE_SUCCESS) {
                    le_connection_handle = hci_subevent_le_connection_complete_get_connection_handle(packet);
                    hci_subevent_le_connection_complete_get_peer_address(packet, event_addr);
                    printf("LE connected to %s, handle 0x%04x (waiting for pairing)\n",
                        bd_addr_to_str(event_addr), le_connection_handle);
                } else {
                    printf("LE connection failed: status 0x%02x\n", status);
                    scanning = true;
                    gap_start_scan();
                }
            }
            break;

        case HCI_EVENT_DISCONNECTION_COMPLETE:
            printf("Disconnected\n");
            connected = false;
            has_prev_report = false;
            has_target = false;
            scanning = true;
            gap_start_scan();
            break;

        case GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED:
            {
                uint8_t hid_status = gattservice_subevent_hid_service_connected_get_status(packet);
                if (hid_status == ERROR_CODE_SUCCESS) {
                    printf("HID service connected!\n");

                    // Try to get HID report descriptor - add debug info
                    printf("Attempting to get HID descriptor, hids_cid=%u\n", hids_cid);

                    const uint8_t *hid_descriptor = hids_client_descriptor_storage_get_descriptor_data(hids_cid, 0);
                    printf("Descriptor pointer: %p\n", (void*)hid_descriptor);

                    if (hid_descriptor) {
                        hid_descriptor_len = hids_client_descriptor_storage_get_descriptor_len(hids_cid, 0);
                        printf("HID descriptor available (%u bytes)\n", hid_descriptor_len);

                        if (hid_descriptor_len > 0) {
                            memcpy(hid_descriptor_storage, hid_descriptor,
                                   hid_descriptor_len > HID_DESCRIPTOR_STORAGE_SIZE ? HID_DESCRIPTOR_STORAGE_SIZE : hid_descriptor_len);
                            hid_descriptor_available = true;

                            // Show first few bytes of descriptor
                            printf("Descriptor first bytes: ");
                            for (int i = 0; i < (hid_descriptor_len < 16 ? hid_descriptor_len : 16); i++) {
                                printf("%02x ", hid_descriptor[i]);
                            }
                            printf("\n");
                        }
                    } else {
                        printf("No HID descriptor available - using standard keyboard mappings\n");
                        hid_descriptor_available = false;
                    }

                    connected = true;
                } else {
                    printf("HID service connect failed: 0x%02x\n", hid_status);
                    gap_disconnect(le_connection_handle);
                }
            }
            break;

        case GATTSERVICE_SUBEVENT_HID_SERVICE_DISCONNECTED:
            printf("HID service disconnected\n");
            connected = false;
            has_prev_report = false;
            break;

        default:
            {
                uint8_t evt = hci_event_packet_get_type(packet);
                if (evt != HCI_EVENT_COMMAND_COMPLETE
                    && evt != HCI_EVENT_TRANSPORT_PACKET_SENT
                    && evt != HCI_EVENT_NUMBER_OF_COMPLETED_PACKETS) {
                    if (evt == HCI_EVENT_GATTSERVICE_META) {
                        uint8_t subevent = hci_event_gattservice_meta_get_subevent_code(packet);
                        if (subevent == GATTSERVICE_SUBEVENT_HID_SERVICE_CONNECTED) {
                            uint8_t status = gattservice_subevent_hid_service_connected_get_status(packet);
                            if (status == ERROR_CODE_SUCCESS) {
                                printf("HID service connected!\n");
                                connected = true;
                            } else {
                                printf("HID service connect failed: 0x%02x\n", status);
                                gap_disconnect(le_connection_handle);
                            }
                        } else if (subevent == GATTSERVICE_SUBEVENT_HID_SERVICE_DISCONNECTED) {
                            printf("HID service disconnected\n");
                            connected = false;
                            has_prev_report = false;
                        } else if (subevent == GATTSERVICE_SUBEVENT_HID_REPORT) {
                            const uint8_t *report = gattservice_subevent_hid_report_get_report(packet);
                            uint16_t report_len = gattservice_subevent_hid_report_get_report_len(packet);
                            uint8_t report_id = gattservice_subevent_hid_report_get_report_id(packet);
                            if (connected) {
                                process_hid_report(report, report_len, report_id);
                            }
                        } else {
                            printf("GATTSVC subevt 0x%02x\n", subevent);
                        }
                    } else {
                        printf("HCI_EVENT 0x%02x\n", evt);
                    }
                }
            }
            break;
        }
        break;

    case HCI_EVENT_GATTSERVICE_META:
        {
            uint8_t subevent = hci_event_gattservice_meta_get_subevent_code(packet);
            if (subevent == GATTSERVICE_SUBEVENT_HID_REPORT) {
                const uint8_t *report = gattservice_subevent_hid_report_get_report(packet);
                uint16_t report_len = gattservice_subevent_hid_report_get_report_len(packet);
                uint8_t report_id = gattservice_subevent_hid_report_get_report_id(packet);
                if (connected) {
                    process_hid_report(report, report_len, report_id);
                }
            }
        }
        break;

    default:
        printf("PACKET_TYPE 0x%02x\n", packet_type);
        break;
    }
}

static void sm_packet_handler(uint8_t packet_type, uint16_t channel,
                              uint8_t *packet, uint16_t size)
{
    if (packet_type != HCI_EVENT_PACKET) return;

    switch (hci_event_packet_get_type(packet)) {
    case SM_EVENT_JUST_WORKS_REQUEST:
        printf("SM: just works request, confirming\n");
        sm_just_works_confirm(sm_event_just_works_request_get_handle(packet));
        break;
    case SM_EVENT_NUMERIC_COMPARISON_REQUEST:
        printf("SM: numeric comparison %u, confirming\n",
            sm_event_numeric_comparison_request_get_passkey(packet));
        sm_numeric_comparison_confirm(sm_event_passkey_display_number_get_handle(packet));
        break;
    case SM_EVENT_PASSKEY_DISPLAY_NUMBER:
        printf("SM: passkey %06u, enter on keyboard\n",
            sm_event_passkey_display_number_get_passkey(packet));
        break;
    case SM_EVENT_PAIRING_COMPLETE:
        if (sm_event_pairing_complete_get_status(packet) == ERROR_CODE_SUCCESS) {
            printf("SM: pairing complete, connecting to HID service\n");
            uint8_t err = hids_client_connect(le_connection_handle, packet_handler,
                HID_PROTOCOL_MODE_REPORT, &hids_cid);
            if (err != ERROR_CODE_SUCCESS) {
                printf("hids_client_connect failed: 0x%02x\n", err);
            }
        } else {
            printf("SM: pairing failed, status %u reason %u\n",
                sm_event_pairing_complete_get_status(packet),
                sm_event_pairing_complete_get_reason(packet));
        }
        break;
    default:
        break;
    }
}

int btstack_main(int argc, const char *argv[])
{
    async_context_t *context = cyw43_arch_async_context();
    if (!btstack_cyw43_init(context)) {
        printf("btstack_cyw43_init FAILED\n");
        return -1;
    }
    printf("btstack_cyw43_init OK\n");

    l2cap_init();
    sm_init();

    sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    sm_set_authentication_requirements(SM_AUTHREQ_BONDING | SM_AUTHREQ_SECURE_CONNECTION);

    gatt_client_init();
    hids_client_init(hid_descriptor_storage, HID_DESCRIPTOR_STORAGE_SIZE);

    hci_event_callback_registration.callback = &packet_handler;
    hci_add_event_handler(&hci_event_callback_registration);

    sm_event_callback_registration.callback = &sm_packet_handler;
    sm_add_event_handler(&sm_event_callback_registration);

    int err = hci_power_control(HCI_POWER_ON);
    printf("hci_power_control returned %d\n", err);

    return 0;
}

void bt_keyboard_init(void)
{
}

bool bt_keyboard_is_connected(void)
{
    return connected;
}
