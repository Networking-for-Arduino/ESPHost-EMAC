#include "ESPHostEMAC.h"
#include "ESPHostEMAC_config.h"
#include "CEspControl.h"

ESPHostEMAC::ESPHostEMAC() :
    memoryManager(NULL) {

}

/** Return the ESPHost EMAC
 * Returns the default on-board EMAC - this will be target-specific, and
 * may not be available on all targets.
 */
ESPHostEMAC& ESPHostEMAC::get_instance(void) {
  static ESPHostEMAC emac;
  return emac;
}

/**
 * Return maximum transmission unit
 *
 * @return     MTU in bytes
 */
uint32_t ESPHostEMAC::get_mtu_size(void) const {
  return ESPHOST_WIFI_MTU_SIZE;
}

/**
 * Gets memory buffer alignment preference
 *
 * Gets preferred memory buffer alignment of the Emac device. IP stack may
 * or may not align link out memory buffer chains using the alignment.
 *
 * @return         Memory alignment requirement in bytes
 */
uint32_t ESPHostEMAC::get_align_preference(void) const {
  return ESPHOST_BUFF_ALIGNMENT;
}

/**
 * Return interface name
 *
 * @param name Pointer to where the name should be written
 * @param size Maximum number of character to copy
 */
void ESPHostEMAC::get_ifname(char *name, uint8_t size) const {
  memcpy(name, ESPHOST_WIFI_IF_NAME, (size < sizeof(ESPHOST_WIFI_IF_NAME)) ? size : sizeof(ESPHOST_WIFI_IF_NAME));
}

/**
 * Returns size of the underlying interface HW address size.
 *
 * @return     HW address size in bytes
 */
uint8_t ESPHostEMAC::get_hwaddr_size(void) const {
  return ESPHOST_HWADDR_SIZE;
}

/**
 * Return interface-supplied HW address
 *
 * Copies HW address to provided memory, @param addr has to be of correct
 * size see @a get_hwaddr_size
 *
 * HW address need not be provided if this interface does not have its own
 * HW address configuration; stack will choose address from central system
 * configuration if the function returns false and does not write to addr.
 *
 * @param addr HW address for underlying interface
 * @return     true if HW address is available
 */
bool ESPHostEMAC::get_hwaddr(uint8_t *addr) const {
  WifiMac_t MAC;
  MAC.mode = WIFI_MODE_STA;
  if (CEspControl::getInstance().getWifiMacAddress(MAC) != ESP_CONTROL_OK)
    return false;
  CNetUtilities::macStr2macArray(addr, MAC.mac);
  return true;
}

/**
 * Set HW address for interface
 *
 * Provided address has to be of correct size, see @a get_hwaddr_size
 *
 * Called to set the MAC address to actually use - if @a get_hwaddr is
 * provided the stack would normally use that, but it could be overridden,
 * eg for test purposes.
 *
 * @param addr Address to be set
 */
void ESPHostEMAC::set_hwaddr(const uint8_t *addr) {
//  WifiMac_t MAC;
//  MAC.mode = WIFI_MODE_STA;
//  CNetUtilities::macArray2macStr(MAC.mac, addr);
//  CEspControl::getInstance().setWifiMacAddress(MAC);
}

/**
 * Initializes the HW
 *
 * @return True on success, False in case of an error.
 */
bool ESPHostEMAC::power_up(void) {

  /* Trigger thread to deal with any RX packets that arrived
   * before receiver_thread was started */
  receiveTaskHandle = mbed::mbed_event_queue()->call_every(ESPHOST_RECEIVE_TASK_PERIOD_MS, mbed::callback(this, &ESPHostEMAC::receiveTask));

  if (emac_link_state_cb) {
    emac_link_state_cb(true);
  }
  return true;
}

/**
 * Deinitializes the HW
 *
 */
void ESPHostEMAC::power_down(void) {

}

/**
 * Sends the packet over the link
 *
 * That can not be called from an interrupt context.
 *
 * @param buf  Packet to be send
 * @return     True if the packet was send successfully, False otherwise
 */
bool ESPHostEMAC::link_out(emac_mem_buf_t *buf) {

  emac_mem_buf_t* chain = buf;

  if (buf == NULL)
    return false;

  // If buffer is chained or not aligned then make a contiguous aligned copy of it
  if (memoryManager->get_next(buf) || reinterpret_cast<uint32_t>(memoryManager->get_ptr(buf)) % ESPHOST_BUFF_ALIGNMENT) {
    emac_mem_buf_t* copy_buf;
    copy_buf = memoryManager->alloc_heap(memoryManager->get_total_len(buf), ESPHOST_BUFF_ALIGNMENT);
    if (NULL == copy_buf) {
      memoryManager->free(buf);
      return false;
    }

    // Copy to new buffer and free original
    memoryManager->copy(copy_buf, buf);
    memoryManager->free(buf);
    buf = copy_buf;
  }

  uint16_t len = memoryManager->get_len(buf);
  uint8_t* data = (uint8_t*) (memoryManager->get_ptr(buf));
  uint8_t ifn = 0;
  int error = CEspControl::getInstance().sendBuffer(ESP_STA_IF, ifn, data, len);
  memoryManager->free(buf);

  if (error != ESP_CONTROL_OK)
    return false;

  return true;
}

void ESPHostEMAC::receiveTask() {

  CEspControl::getInstance().communicateWithEsp();

  emac_mem_buf_t* payload = lowLevelInput();
  if (payload != NULL) {
    if (emac_link_input_cb) {
      emac_link_input_cb(payload);
    }
  }
}

emac_mem_buf_t* ESPHostEMAC::lowLevelInput() {

  uint16_t size = CEspControl::getInstance().peekStationRxMsgSize();
  if (size == 0)
    return nullptr;

  emac_mem_buf_t* buf = memoryManager->alloc_heap(size, ESPHOST_BUFF_ALIGNMENT);
  if (buf == nullptr)
    return nullptr;
  uint8_t* data = (uint8_t*) (memoryManager->get_ptr(buf));
  uint8_t if_num = 0;
  CEspControl::getInstance().getStationRx(if_num, data, size);
  return buf;
}

/**
 * Sets a callback that needs to be called for packets received for that
 * interface
 *
 * @param input_cb Function to be register as a callback
 */
void ESPHostEMAC::set_link_input_cb(emac_link_input_cb_t input_cb) {
  emac_link_input_cb = input_cb;
}

/**
 * Sets a callback that needs to be called on link status changes for given
 * interface
 *
 * @param state_cb Function to be register as a callback
 */
void ESPHostEMAC::set_link_state_cb(emac_link_state_change_cb_t state_cb) {
  emac_link_state_cb = state_cb;
}

/** Add device to a multicast group
 *
 * @param address  A multicast group hardware address
 */
void ESPHostEMAC::add_multicast_group(const uint8_t *address) {

}

/** Remove device from a multicast group
 *
 * @param address  A multicast group hardware address
 */
void ESPHostEMAC::remove_multicast_group(const uint8_t *address) {

}

/** Request reception of all multicast packets
 *
 * @param all True to receive all multicasts
 *            False to receive only multicasts addressed to specified groups
 */
void ESPHostEMAC::set_all_multicast(bool all) {

}

/** Sets memory manager that is used to handle memory buffers
 *
 * @param mem_mngr Pointer to memory manager
 */
void ESPHostEMAC::set_memory_manager(EMACMemoryManager &mem_mngr) {
  memoryManager = &mem_mngr;
}

