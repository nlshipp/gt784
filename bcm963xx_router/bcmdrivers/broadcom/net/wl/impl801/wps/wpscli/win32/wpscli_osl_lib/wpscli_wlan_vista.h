#ifndef __WPSCLI_WLAN_VISTA_H__
#define __WPSCLI_WLAN_VISTA_H__

extern brcm_wpscli_status wpscli_wlan_open_vista(const char *interface_guid);
extern brcm_wpscli_status wpscli_wlan_close_vista();
extern brcm_wpscli_status wpscli_wlan_connect_vista(const char *ssid, uint wsec);
extern brcm_wpscli_status wpscli_wlan_disconnect_vista();
extern brcm_wpscli_status wpscli_wlan_scan_vista(wl_scan_results_t *ap_list, uint32 buf_size);

#endif
