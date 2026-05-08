#include "bt_keyboard.h"
#include "reg.h"
#include "fifo.h"
#include "interrupt.h"

#include <btstack.h>
#include <pico/cyw43_arch.h>
#include <pico/btstack_cyw43.h>
#include <ble/gatt-service/hids_client.h>
#include <ble/sm.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

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

#define HID_MOD_LCTRL  0x01
#define HID_MOD_LSHIFT 0x02
#define HID_MOD_LALT   0x04
#define HID_MOD_LGUI   0x08
#define HID_MOD_RCTRL  0x10
#define HID_MOD_RSHIFT 0x20
#define HID_MOD_RALT   0x40
#define HID_MOD_RGUI   0x80

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
static uint16_t hids_cid = 0;
static hci_con_handle_t le_connection_handle = HCI_CON_HANDLE_INVALID;

static uint8_t prev_report[HID_REPORT_SIZE];
static bool has_prev_report = false;
static bool connected = false;
static bd_addr_t target_addr;
static bool has_target = false;
static bool scanning = false;
static bool capslock = false;

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

static void inject_key(char key, enum key_state state)
{
    struct fifo_item item = { .key = key, .state = state };

    if (!fifo_enqueue(item)) {
        if (reg_is_bit_set(REG_ID_CFG, CFG_OVERFLOW_ON)) {
            fifo_flush();
            fifo_enqueue_force(item);
            if (reg_is_bit_set(REG_ID_CFG, CFG_OVERFLOW_INT)) {
                reg_set_bit(REG_ID_INT, INT_OVERFLOW);
            }
        }
    }
    interrupt_pulse();
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

    printf("report: rid=%u mod=0x%02x keys=", report_id, modifiers);
    for (int i = 2; i < HID_REPORT_SIZE && i < len; i++) {
        if (report[i]) printf("0x%02x ", report[i]);
    }
    printf("\n");

    if (!find_in_prev(HID_KEY_CAPSLOCK) && find_in_report(report, HID_KEY_CAPSLOCK)) {
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
        uint8_t code = report[i];
        if (code == 0 || code == HID_KEY_CAPSLOCK) continue;
        if (has_prev_report && find_in_prev(code)) continue;

        uint8_t ch = hid_code_to_char(code, modifiers, capslock);
        if (ch == 0) continue;

        inject_key((char)ch, KEY_STATE_PRESSED);
    }

    if (has_prev_report) {
        for (int i = 2; i < HID_REPORT_SIZE; i++) {
            uint8_t code = prev_report[i];
            if (code == 0 || code == HID_KEY_CAPSLOCK) continue;
            if (find_in_report(report, code)) continue;

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
