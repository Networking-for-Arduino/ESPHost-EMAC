#ifndef ESPHOST_EMAC_INTERFACE_H
#define ESPHOST_EMAC_INTERFACE_H

#include <inttypes.h>

#include "mbed.h"
#include "mbed_debug.h"
#include "netsocket/WiFiInterface.h"
#include "netsocket/EMACInterface.h"
#include "netsocket/OnboardNetworkStack.h"
#include "ESPHostEMAC.h"
#include "CCtrlWrapper.h"

#define      MAX_AP_COUNT    10

/** ESPHostEMACInterface class
 *  Implementation of the WiFiInterface for the ESPHost
 */
class ESPHostEMACInterface : public WiFiInterface, public EMACInterface {
public:
  ESPHostEMACInterface(bool debug = false, EMAC &emac = ESPHostEMAC::get_instance(), OnboardNetworkStack &stack = OnboardNetworkStack::get_default_instance());

  /** Start the interface
   *
   *  Attempts to connect to a WiFi network. Requires ssid and passphrase to be set.
   *  If passphrase is invalid, NSAPI_ERROR_AUTH_ERROR is returned.
   *
   *  @return         0 on success, negative error code on failure
   */
  nsapi_error_t connect();

  /** Start the interface
   *
   *  Attempts to connect to a WiFi network.
   *
   *  @param ssid      Name of the network to connect to
   *  @param pass      Security passphrase to connect to the network
   *  @param security  Type of encryption for connection (Default: NSAPI_SECURITY_NONE)
   *  @param channel   This parameter is not supported, setting it to anything else than 0 will result in NSAPI_ERROR_UNSUPPORTED
   *  @return          0 on success, or error code on failure
   */
  nsapi_error_t connect(const char *ssid, const char *pass, nsapi_security_t security = NSAPI_SECURITY_NONE, uint8_t channel = 0);

  /** Stop the interface
   *  @return             0 on success, negative on failure
   */
  nsapi_error_t disconnect();

  /** Set the WiFi network credentials
   *
   *  @param ssid      Name of the network to connect to
   *  @param pass      Security passphrase to connect to the network
   *  @param security  Type of encryption for connection
   *                   (defaults to NSAPI_SECURITY_NONE)
   *  @return          0 on success, or error code on failure
   */
  nsapi_error_t set_credentials(const char *ssid, const char *pass, nsapi_security_t security = NSAPI_SECURITY_NONE);

  /** Set the WiFi network channel - NOT SUPPORTED
   *
   * This function is not supported and will return NSAPI_ERROR_UNSUPPORTED
   *
   *  @param channel   Channel on which the connection is to be made, or 0 for any (Default: 0)
   *  @return          Not supported, returns NSAPI_ERROR_UNSUPPORTED
   */
  nsapi_error_t set_channel(uint8_t channel) {
    if (channel != 0) {
      return NSAPI_ERROR_UNSUPPORTED;
    }

    return 0;
  }

  /** Gets the current radio signal strength for active connection
   *
   * @return          Connection strength in dBm (negative value)
   */
  int8_t get_rssi();

  /** Scan for available networks
   *
   * This function will block.
   *
   * @param  ap       Pointer to allocated array to store discovered AP
   * @param  count    Size of allocated @a res array, or 0 to only count available AP
   * @param  timeout  Timeout in milliseconds; 0 for no timeout (Default: 0)
   * @return          Number of entries in @a, or if @a count was 0 number of available networks, negative on error
   *                  see @a nsapi_error
   */
  int scan(WiFiAccessPoint *res, unsigned count);

private:
  static bool wifiHwInitialized;
  WifiApCfg_t ap;
  volatile bool isConnected;
  rtos::Mutex mutex;
  uint8_t debug_level;

  static int initEventCb(CCtrlMsgWrapper *resp);

  bool initHW();

  nsapi_security_t sec2nsapisec(int sec) {
    nsapi_security_t sec_out;

    switch (sec) {
      case WIFI_AUTH_OPEN:
        sec_out = NSAPI_SECURITY_NONE;
        break;
      case WIFI_AUTH_WEP:
        sec_out = NSAPI_SECURITY_WEP;
        break;
      case WIFI_AUTH_WPA_PSK:
        sec_out = NSAPI_SECURITY_WPA;
        break;
      case WIFI_AUTH_WPA_WPA2_PSK:
        sec_out = NSAPI_SECURITY_WPA_WPA2;
        break;
      case WIFI_AUTH_WPA2_PSK:
        sec_out = NSAPI_SECURITY_WPA2;
        break;
      case WIFI_AUTH_WPA2_WPA3_PSK:
        sec_out = NSAPI_SECURITY_WPA3_WPA2;
        break;
      case WIFI_AUTH_WPA2_ENTERPRISE:
        sec_out = NSAPI_SECURITY_WPA2_ENT;
        break;
      default:
        sec_out = NSAPI_SECURITY_WPA_WPA2;
        break;

    }
    return sec_out;
  }

  int nsapi_sec2esp_sec(nsapi_security_t sec) {
    int esp_sec;

    switch (sec) {
      case NSAPI_SECURITY_NONE:
        esp_sec = WIFI_AUTH_OPEN;
        break;
      case NSAPI_SECURITY_WEP:
        esp_sec = WIFI_AUTH_WEP;
        break;
      case NSAPI_SECURITY_WPA:
        esp_sec = WIFI_AUTH_WPA_PSK;
        break;
      case NSAPI_SECURITY_WPA_WPA2:
        esp_sec = WIFI_AUTH_WPA_WPA2_PSK;
        break;
      case NSAPI_SECURITY_WPA2:
        esp_sec = WIFI_AUTH_WPA2_PSK;
        break;
      case NSAPI_SECURITY_WPA2_ENT:
        esp_sec = WIFI_AUTH_WPA2_ENTERPRISE;
        break;
      case NSAPI_SECURITY_WPA3_WPA2:
        esp_sec = WIFI_AUTH_WPA2_WPA3_PSK;
        break;
      case NSAPI_SECURITY_WPA3:
        esp_sec = WIFI_AUTH_WPA3_PSK;
        break;
      default:
        esp_sec = WIFI_AUTH_WPA_WPA2_PSK;
        break;
    }
    return esp_sec;
  }
};

#endif
