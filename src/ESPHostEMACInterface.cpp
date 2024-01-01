#include <ESPHostEMACInterface.h>
#include "CEspControl.h"
#include "CCtrlWrapper.h"

#define DEBUG_SILENT  0
#define DEBUG_WARNING 1
#define DEBUG_INFO    2
#define DEBUG_LOG     3
#define DEFAULT_DEBUG DEBUG_WARNING

#define ESPHOST_INIT_TIMEOUT_MS             10000

static ESPHostEMACInterface* espHostObject;
bool ESPHostEMACInterface::wifiHwInitialized = false;

#include <stdarg.h>

static void debug(int condition, const char *format, ...) {
  if (condition) {
    char buff[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buff, sizeof(buff), format, args);
    va_end(args);
    Serial.println(buff);
  }
}

ESPHostEMACInterface::ESPHostEMACInterface(bool debug, EMAC &emac, OnboardNetworkStack &stack) :
    EMACInterface(emac, stack), isConnected(false) {

  espHostObject = this;
  ap.ssid[0] = 0;

  if (debug) {
    debug_level = DEBUG_LOG;
  } else {
    debug_level = DEFAULT_DEBUG;
  }
}

int ESPHostEMACInterface::set_credentials(const char *ssid, const char *pass, nsapi_security_t security) {
  if ((ssid == NULL) || (strlen(ssid) == 0) || (strlen(ssid) > 32)) {
    debug(debug_level >= DEBUG_WARNING, "ESPHostEMACInterface : bad credential\n");
    return NSAPI_ERROR_PARAMETER;
  }

  if (security != NSAPI_SECURITY_NONE) {
    if ((pass == NULL) || (strcmp(pass, "") == 0) || (strlen(pass) > 63)) {
      debug(debug_level >= DEBUG_WARNING, "ESPHostEMACInterface : bad security\n");
      return NSAPI_ERROR_PARAMETER;
    }
  }

  mutex.lock();
  memset(ap.ssid, 0, sizeof(ap.ssid));
  memcpy(ap.ssid, ssid, sizeof(ap.ssid));

  memset(ap.pwd, 0, sizeof(ap.pwd));
  if (security != NSAPI_SECURITY_NONE) {
    memcpy(ap.pwd, pass, sizeof(ap.pwd));
  }
  ap.encryption_mode = nsapi_sec2esp_sec(security);

  mutex.unlock();
  debug(debug_level >= DEBUG_LOG, "ESPHostEMACInterface : set credential OK %s %s \n", ap.ssid, ap.pwd);
  return NSAPI_ERROR_OK;
}

int ESPHostEMACInterface::initEventCb(CCtrlMsgWrapper *resp) {
  (void) resp;
  wifiHwInitialized = true;
  return ESP_CONTROL_OK;
}

nsapi_error_t ESPHostEMACInterface::connect(const char *ssid, const char *pass, nsapi_security_t security, uint8_t channel) {
  nsapi_error_t ret;

  if (channel != 0) {
    debug(debug_level >= DEBUG_WARNING, "ESPHostEMACInterface : connect bad channel value, only 0 is supported\n");
    ret = NSAPI_ERROR_UNSUPPORTED;
  } else {

    mutex.lock();
    nsapi_error_t credentials_status = set_credentials(ssid, pass, security);
    mutex.unlock();

    if (credentials_status) {
      debug(debug_level >= DEBUG_WARNING, "ESPHostEMACInterface : connect unable to set credential\n");
      ret = credentials_status;
    } else {
      ret = connect();
    }
  }
  return ret;
}

bool ESPHostEMACInterface::initHW() {
  if (wifiHwInitialized)
    return true;

  //  CEspControl::getInstance().listenForStationDisconnectEvent(CLwipIf::disconnectEventcb);
  CEspControl::getInstance().listenForInitEvent(initEventCb);
  if (CEspControl::getInstance().initSpiDriver() != 0)
    return false;

  int time_num = 0;
  while (!wifiHwInitialized && time_num < ESPHOST_INIT_TIMEOUT_MS) {
    CEspControl::getInstance().communicateWithEsp();
    delay(100);
    time_num++;
  }
  return wifiHwInitialized;

}

nsapi_error_t ESPHostEMACInterface::connect() {
  nsapi_error_t ret;
  mutex.lock();

  if (!initHW())
    return false;

  if (ap.ssid[0] == '\0') {
    debug(debug_level >= DEBUG_WARNING, "ESPHostEMACInterface : connect , ssid is missing\n");
    ret = NSAPI_ERROR_NO_SSID;
  } else if (isConnected) {
    debug(debug_level >= DEBUG_WARNING, "ESPHostEMACInterface : connect is already connected\n");
    ret = NSAPI_ERROR_IS_CONNECTED;
  } else {
    debug(debug_level >= DEBUG_INFO, "ESPHostEMACInterface : connecting WIFI\n");
    if (CEspControl::getInstance().connectAccessPoint(ap) != ESP_CONTROL_OK) {
      debug(debug_level >= DEBUG_WARNING, "ESPHostEMACInterface : Connect failed; wrong parameter ?\n");
      ret = NSAPI_ERROR_PARAMETER;
    } else {
      CEspControl::getInstance().getAccessPointConfig(ap);
      debug(debug_level >= DEBUG_INFO, "ESPHostEMACInterface : connecting EMAC\n");
      ret = EMACInterface::connect();
      /* EMAC is waiting for UP conection , UP means we join an hotspot and  IP services running */
      if (ret == NSAPI_ERROR_OK || ret == NSAPI_ERROR_IS_CONNECTED) {
        debug(debug_level >= DEBUG_LOG, "ESPHostEMACInterface : Connected to emac! (using ssid %s , passw %s  )\n", ap.ssid, ap.pwd);
        isConnected = true;
        ret = NSAPI_ERROR_OK;
      } else {
        debug(debug_level >= DEBUG_WARNING, "ESPHostEMACInterface : EMAC Fail to connect NSAPI_ERROR %d\n", ret);
        CEspControl::getInstance().disconnectAccessPoint();
        EMACInterface::disconnect();
        ret = NSAPI_ERROR_CONNECTION_TIMEOUT;
      }
    }
  }
  mutex.unlock();

  return ret;
}

nsapi_error_t ESPHostEMACInterface::disconnect() {
  nsapi_error_t ret;
  mutex.lock();

  if (isConnected == false) {
    ret = NSAPI_ERROR_NO_CONNECTION;
  } else {
    debug(debug_level >= DEBUG_INFO, "ESPHostEMACInterface : disconnecting EspHost WIFI and EMAC\n");
    int rv = CEspControl::getInstance().disconnectAccessPoint();
    if (rv != ESP_CONTROL_OK) {
      debug(debug_level >= DEBUG_WARNING, "ESPHost disconnect command failed\n");
      ret = NSAPI_ERROR_DEVICE_ERROR;
    } else {
      ret = NSAPI_ERROR_OK;
    }
    isConnected = false;
    EMACInterface::disconnect();
  }
  mutex.unlock();
  return ret;
}

int8_t ESPHostEMACInterface::get_rssi() {
  mutex.lock();
  int8_t ret = 0;
  if (isConnected) {
    WifiApCfg_t ap;
    CEspControl::getInstance().getAccessPointConfig(ap);
    ret = ap.rssi;
  }
  mutex.unlock();
  debug(debug_level >= DEBUG_INFO, "ESPHostEMACInterface : Get RSSI return %d\n", ret);
  return ret;
}

int ESPHostEMACInterface::scan(WiFiAccessPoint *res, unsigned int count) {
  mutex.lock();
  if (count == 0)
    return MAX_AP_COUNT;
  if (count > MAX_AP_COUNT) {
    count = MAX_AP_COUNT;
  }

  if (!initHW())
    return false;

  std::vector<AccessPoint_t> accessPoints;
  int rv = CEspControl::getInstance().getAccessPointScanList(accessPoints);
  if (rv != ESP_CONTROL_OK)
    return NSAPI_ERROR_DEVICE_ERROR;
  count = accessPoints.size();
  debug(debug_level >= DEBUG_INFO, "ESPHostEMACInterface : Scan find %d HotSpot\n", count);

  for (uint32_t i = 0; i < count; i++) {
    nsapi_wifi_ap_t ap;
    debug(debug_level >= DEBUG_LOG, "ESPHostEMACInterface : %" PRIu32 "  SSID %s rssi %" PRIu32 "\n", i, accessPoints[i].ssid, accessPoints[i].rssi);
    debug(debug_level >= DEBUG_LOG, "ESPHostEMACInterface : BSSID %hhx:%hhx:%hhx:%hhx:%hhx:%hhx\n", accessPoints[i].bssid[0], accessPoints[i].bssid[1], accessPoints[i].bssid[2],
        accessPoints[i].bssid[3], accessPoints[i].bssid[4], accessPoints[i].bssid[5]);

    memcpy(ap.ssid, accessPoints[i].ssid, 33);
    CNetUtilities::macStr2macArray(ap.bssid, (char*) accessPoints[i].bssid);
    ap.security = sec2nsapisec(accessPoints[i].encryption_mode);
    ap.rssi = accessPoints[i].rssi;
    ap.channel = accessPoints[i].channel;
    res[i] = WiFiAccessPoint(ap);
  }
  mutex.unlock();
  return count;
}

#if MBED_CONF_ESPHOST_PROVIDE_DEFAULT
WiFiInterface *WiFiInterface::get_default_instance()
{
    static ESPHostEMACInterface emw;
    return &emw;
}
#endif /* MBED_CONF_EMW3080B_PROVIDE_DEFAULT */

#if defined(MBED_CONF_NSAPI_PRESENT)
WiFiInterface* WiFiInterface::get_target_default_instance() {
#if (DEFAULT_DEBUG == DEBUG_LOG)
    printf("get_target_default_instance\n");
#endif /* MBED_CONF_NSAPI_PRESENT */
  static ESPHostEMACInterface wifi;
  return &wifi;
}
#endif /* MBED_CONF_NSAPI_PRESENT */
