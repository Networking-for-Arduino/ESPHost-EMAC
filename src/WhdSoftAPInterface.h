
// a fake WhdSoftAPInterface for the Mbed Core WiFi library to compile

#ifndef WHD_SOFTAP_INTERFACE_H
#define WHD_SOFTAP_INTERFACE_H

#include "netsocket/EMACInterface.h"
#include "netsocket/OnboardNetworkStack.h"

#include "ESPHostEMAC.h"

#define WLC_E_ASSOC_IND  8
#define WLC_E_DISASSOC_IND 12
#define WLC_E_LINK 16
#define WLC_E_IF 54

#define WHD_IOCTL_LOG_ADD_EVENT(w, x, y, z)

enum whd_event_num_t {

};

typedef struct whd_ap_int_info
{
//    whd_bool_t ap_is_up;
//    whd_bool_t is_waiting_event;
  osSemaphoreId_t whd_wifi_sleep_flag;
} whd_ap_int_info_t;

struct whd_driver
{
    whd_ap_int_info_t ap_info;
};
typedef struct whd_driver *whd_driver_t;

struct whd_interface
{
    whd_driver_t whd_driver;
};
typedef struct whd_interface *whd_interface_t;

struct whd_event_header_t
{
    uint32_t event_type;            /* Message (see below) */
};

typedef void *(*whd_event_handler_t)(whd_interface_t ifp, const whd_event_header_t *event_header,
                                     const uint8_t *event_data, void *handler_user_data);

class WhdSoftAPInterface : public EMACInterface {
public:

    WhdSoftAPInterface(ESPHostEMAC &emac = ESPHostEMAC::get_instance(),
                       OnboardNetworkStack &stack = OnboardNetworkStack::get_default_instance());

    static WhdSoftAPInterface *get_default_instance();

    int start(const char *ssid, const char *pass, nsapi_security_t security, uint8_t channel,
              bool start_dhcp_server = true, const void *ie_info = NULL, bool ap_sta_concur = false);

    int stop(void) {}

    int get_associated_client_list(void *client_list_buffer, uint16_t buffer_length);

    int register_event_handler(whd_event_handler_t softap_event_handler);

    int unregister_event_handler(void) {}

    nsapi_error_t set_blocking(bool blocking)
    {
        if (blocking) {
            _blocking = blocking;
            return NSAPI_ERROR_OK;
        } else {
            return NSAPI_ERROR_UNSUPPORTED;
        }
    }
};

#endif
