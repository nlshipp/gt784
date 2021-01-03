#ifndef __MDM_OBJECTID_H__
#define __MDM_OBJECTID_H__


/*
 * This file is automatically generated from the data-model spreadsheet.
 * Do not modify this file directly - You will lose all your changes the
 * next time this file is generated!
 */


/*!\file mdm_objectid.h 
 * \brief Automatically generated header file mdm_objectid.h
 */


/*! \brief InternetGatewayDevice. */
#define MDMOID_IGD  1

/*! \brief InternetGatewayDevice.DeviceInfo. */
#define MDMOID_IGD_DEVICE_INFO  2

/*! \brief InternetGatewayDevice.DeviceInfo.VendorConfigFile.{i}. */
#define MDMOID_VENDOR_CONFIG_FILE  3

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_SyslogCfg. */
#define MDMOID_SYSLOG_CFG  4

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_LoginCfg. */
#define MDMOID_LOGIN_CFG  5

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_AppCfg. */
#define MDMOID_APP_CFG  6

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_AppCfg.HttpdCfg. */
#define MDMOID_HTTPD_CFG  7

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_AppCfg.Tr69cCfg. */
#define MDMOID_TR69C_CFG  8

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_AppCfg.Tr64cCfg. */
#define MDMOID_TR64C_CFG  9

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_AppCfg.SshdCfg. */
#define MDMOID_SSHD_CFG  10

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_AppCfg.TelnetdCfg. */
#define MDMOID_TELNETD_CFG  11

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_AppCfg.ConsoledCfg. */
#define MDMOID_CONSOLED_CFG  12

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_AppCfg.SmdCfg. */
#define MDMOID_SMD_CFG  13

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_AppCfg.SskCfg. */
#define MDMOID_SSK_CFG  14

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_AppCfg.SnmpdCfg. */
#define MDMOID_SNMPD_CFG  15

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_AppCfg.FtpdCfg. */
#define MDMOID_FTPD_CFG  16

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_AppCfg.TftpdCfg. */
#define MDMOID_TFTPD_CFG  17

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_AppCfg.PppdCfg. */
#define MDMOID_PPPD_CFG  18

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_AppCfg.IcmpCfg. */
#define MDMOID_ICMP_CFG  19

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_AppCfg.UpnpCfg. */
#define MDMOID_UPNP_CFG  20

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_AppCfg.DnsProxyCfg. */
#define MDMOID_DNS_PROXY_CFG  21

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_EthernetSwitch. */
#define MDMOID_ETHERNET_SWITCH  22

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_PwrMngtCfg. */
#define MDMOID_PWR_MNGT  23

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_StandbyCfg. */
#define MDMOID_STANDBY_CFG  24

/*! \brief InternetGatewayDevice.DeviceConfig. */
#define MDMOID_DEVICE_CONFIG  25

/*! \brief InternetGatewayDevice.ManagementServer. */
#define MDMOID_MANAGEMENT_SERVER  26

/*! \brief InternetGatewayDevice.ManagementServer.ManageableDevice.{i}. */
#define MDMOID_MANAGEABLE_DEVICE  27

/*! \brief InternetGatewayDevice.Time. */
#define MDMOID_TIME_SERVER_CFG  28

/*! \brief InternetGatewayDevice.Layer2Bridging. */
#define MDMOID_L2_BRIDGING  29

/*! \brief InternetGatewayDevice.Layer2Bridging.Bridge.{i}. */
#define MDMOID_L2_BRIDGING_ENTRY  30

/*! \brief InternetGatewayDevice.Layer2Bridging.Filter.{i}. */
#define MDMOID_L2_BRIDGING_FILTER  31

/*! \brief InternetGatewayDevice.Layer2Bridging.AvailableInterface.{i}. */
#define MDMOID_L2_BRIDGING_INTF  32

/*! \brief InternetGatewayDevice.QueueManagement. */
#define MDMOID_Q_MGMT  33

/*! \brief InternetGatewayDevice.QueueManagement.Classification.{i}. */
#define MDMOID_Q_MGMT_CLASSIFICATION  34

/*! \brief InternetGatewayDevice.QueueManagement.Queue.{i}. */
#define MDMOID_Q_MGMT_QUEUE  35

/*! \brief InternetGatewayDevice.IPPingDiagnostics. */
#define MDMOID_IP_PING_DIAG  36

/*! \brief InternetGatewayDevice.LANDevice.{i}. */
#define MDMOID_LAN_DEV  37

/*! \brief InternetGatewayDevice.LANDevice.{i}.X_BROADCOM_COM_IgmpSnoopingConfig. */
#define MDMOID_IGMP_SNOOPING_CFG  38

/*! \brief InternetGatewayDevice.LANDevice.{i}.LANHostConfigManagement. */
#define MDMOID_LAN_HOST_CFG  39

/*! \brief InternetGatewayDevice.LANDevice.{i}.LANHostConfigManagement.IPInterface.{i}. */
#define MDMOID_LAN_IP_INTF  40

/*! \brief InternetGatewayDevice.LANDevice.{i}.LANHostConfigManagement.IPInterface.{i}.X_BROADCOM_COM_FirewallException.{i}. */
#define MDMOID_LAN_IP_INTF_FIREWALL_EXCEPTION  41

/*! \brief InternetGatewayDevice.LANDevice.{i}.LANHostConfigManagement.IPInterface.{i}.X_BROADCOM_COM_IpFilterCfg.{i}. */
#define MDMOID_IP_FILTER_CFG  42

/*! \brief InternetGatewayDevice.LANDevice.{i}.LANHostConfigManagement.DHCPConditionalServingPool.{i}. */
#define MDMOID_DHCP_CONDITIONAL_SERVING  43

/*! \brief InternetGatewayDevice.LANDevice.{i}.X_BROADCOM_COM_IPv6LANHostConfigManagement. */
#define MDMOID_I_PV6_LAN_HOST_CFG  44

/*! \brief InternetGatewayDevice.LANDevice.{i}.X_BROADCOM_COM_IPv6LANHostConfigManagement.X_BROADCOM_COM_MldSnoopingConfig. */
#define MDMOID_MLD_SNOOPING_CFG  45

/*! \brief InternetGatewayDevice.LANDevice.{i}.LANEthernetInterfaceConfig.{i}. */
#define MDMOID_LAN_ETH_INTF  46

/*! \brief InternetGatewayDevice.LANDevice.{i}.LANEthernetInterfaceConfig.{i}.Stats. */
#define MDMOID_LAN_ETH_INTF_STATS  47

/*! \brief InternetGatewayDevice.LANDevice.{i}.LANUSBInterfaceConfig.{i}. */
#define MDMOID_LAN_USB_INTF  48

/*! \brief InternetGatewayDevice.LANDevice.{i}.LANUSBInterfaceConfig.{i}.Stats. */
#define MDMOID_LAN_USB_INTF_STATS  49

/*! \brief InternetGatewayDevice.LANDevice.{i}.WLANConfiguration.{i}. */
#define MDMOID_LAN_WLAN  50

/*! \brief InternetGatewayDevice.LANDevice.{i}.WLANConfiguration.{i}.AssociatedDevice.{i}. */
#define MDMOID_LAN_WLAN_ASSOCIATED_DEVICE_ENTRY  51

/*! \brief InternetGatewayDevice.LANDevice.{i}.WLANConfiguration.{i}.WEPKey.{i}. */
#define MDMOID_LAN_WLAN_WEP_KEY  52

/*! \brief InternetGatewayDevice.LANDevice.{i}.WLANConfiguration.{i}.PreSharedKey.{i}. */
#define MDMOID_LAN_WLAN_PRE_SHARED_KEY  53

/*! \brief InternetGatewayDevice.LANDevice.{i}.WLANConfiguration.{i}.X_BROADCOM_COM_WlanAdapter. */
#define MDMOID_WLAN_ADAPTER  54

/*! \brief InternetGatewayDevice.LANDevice.{i}.WLANConfiguration.{i}.X_BROADCOM_COM_WlanAdapter.WlBaseCfg. */
#define MDMOID_WL_BASE_CFG  55

/*! \brief InternetGatewayDevice.LANDevice.{i}.WLANConfiguration.{i}.X_BROADCOM_COM_WlanAdapter.WlStaticWdsCfg.{i}. */
#define MDMOID_WL_STATIC_WDS_CFG  56

/*! \brief InternetGatewayDevice.LANDevice.{i}.WLANConfiguration.{i}.X_BROADCOM_COM_WlanAdapter.WlWdsCfg.{i}. */
#define MDMOID_WL_WDS_CFG  57

/*! \brief InternetGatewayDevice.LANDevice.{i}.WLANConfiguration.{i}.X_BROADCOM_COM_WlanAdapter.WlScanWdsCfg.{i}. */
#define MDMOID_WL_SCAN_WDS_CFG  58

/*! \brief InternetGatewayDevice.LANDevice.{i}.WLANConfiguration.{i}.X_BROADCOM_COM_WlanAdapter.WlMimoCfg. */
#define MDMOID_WL_MIMO_CFG  59

/*! \brief InternetGatewayDevice.LANDevice.{i}.WLANConfiguration.{i}.X_BROADCOM_COM_WlanAdapter.WlSesCfg. */
#define MDMOID_WL_SES_CFG  60

/*! \brief InternetGatewayDevice.LANDevice.{i}.WLANConfiguration.{i}.X_BROADCOM_COM_WlanAdapter.WlVirtIntfCfg.{i}. */
#define MDMOID_WL_VIRT_INTF_CFG  61

/*! \brief InternetGatewayDevice.LANDevice.{i}.WLANConfiguration.{i}.X_BROADCOM_COM_WlanAdapter.VirtIntf.{i}.WlMacFltCfg.{i}. */
#define MDMOID_WL_MAC_FLT  62

/*! \brief InternetGatewayDevice.LANDevice.{i}.WLANConfiguration.{i}.X_BROADCOM_COM_WlanAdapter.VirtIntf.{i}.WlKey64Cfg.{i}. */
#define MDMOID_WL_KEY64_CFG  63

/*! \brief InternetGatewayDevice.LANDevice.{i}.WLANConfiguration.{i}.X_BROADCOM_COM_WlanAdapter.VirtIntf.{i}.WlKey128Cfg.{i}. */
#define MDMOID_WL_KEY128_CFG  64

/*! \brief InternetGatewayDevice.LANDevice.{i}.WLANConfiguration.{i}.X_BROADCOM_COM_WlanAdapter.VirtIntf.{i}.WlWpsCfg. */
#define MDMOID_WL_WPS_CFG  65

/*! \brief InternetGatewayDevice.LANDevice.{i}.X_BROADCOM_COM_LANMocaInterfaceConfig.{i}. */
#define MDMOID_LAN_MOCA_INTF  66

/*! \brief InternetGatewayDevice.LANDevice.{i}.X_BROADCOM_COM_LANMocaInterfaceConfig.{i}.Status. */
#define MDMOID_LAN_MOCA_INTF_STATUS  67

/*! \brief InternetGatewayDevice.LANDevice.{i}.X_BROADCOM_COM_LANMocaInterfaceConfig.{i}.Stats. */
#define MDMOID_LAN_MOCA_INTF_STATS  68

/*! \brief InternetGatewayDevice.LANDevice.{i}.Hosts. */
#define MDMOID_LAN_HOSTS  69

/*! \brief InternetGatewayDevice.LANDevice.{i}.Hosts.Host.{i}. */
#define MDMOID_LAN_HOST_ENTRY  70

/*! \brief InternetGatewayDevice.WANDevice.{i}. */
#define MDMOID_WAN_DEV  71

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANCommonInterfaceConfig. */
#define MDMOID_WAN_COMMON_INTF_CFG  72

/*! \brief InternetGatewayDevice.WANDevice.{i}.X_BROADCOM_COM_XTM_Interface_Stats.{i}. */
#define MDMOID_XTM_INTERFACE_STATS  73

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANDSLInterfaceConfig. */
#define MDMOID_WAN_DSL_INTF_CFG  74

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANDSLInterfaceConfig.X_BROADCOM_COM_BertTest. */
#define MDMOID_WAN_BERT_TEST  75

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANDSLInterfaceConfig.Stats. */
#define MDMOID_WAN_DSL_INTF_STATS  76

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANDSLInterfaceConfig.Stats.Total. */
#define MDMOID_WAN_DSL_INTF_STATS_TOTAL  77

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANDSLInterfaceConfig.Stats.Showtime. */
#define MDMOID_WAN_DSL_INTF_STATS_SHOWTIME  78

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANDSLInterfaceConfig.Stats.CurrentDay. */
#define MDMOID_WAN_DSL_INTF_STATS_CURRENT_DAY  79

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANDSLInterfaceConfig.Stats.QuarterHour. */
#define MDMOID_WAN_DSL_INTF_STATS_QUARTER_HOUR  80

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANDSLInterfaceConfig.TestParams. */
#define MDMOID_WAN_DSL_TEST_PARAMS  81

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANEthernetInterfaceConfig. */
#define MDMOID_WAN_ETH_INTF  82

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANEthernetInterfaceConfig.Stats. */
#define MDMOID_WAN_ETH_INTF_STATS  83

/*! \brief InternetGatewayDevice.WANDevice.{i}.X_BROADCOM_COM_WANMocaInterfaceConfig. */
#define MDMOID_WAN_MOCA_INTF  84

/*! \brief InternetGatewayDevice.WANDevice.{i}.X_BROADCOM_COM_WANMocaInterfaceConfig.Status. */
#define MDMOID_WAN_MOCA_INTF_STATUS  85

/*! \brief InternetGatewayDevice.WANDevice.{i}.X_BROADCOM_COM_WANMocaInterfaceConfig.Stats. */
#define MDMOID_WAN_MOCA_INTF_STATS  86

/*! \brief InternetGatewayDevice.WANDevice.{i}.X_BROADCOM_COM_L2tpAcIntfConfig. */
#define MDMOID_L2TP_AC_INTF_CONFIG  87

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANDSLDiagnostics. */
#define MDMOID_WAN_DSL_DIAG  88

/*! \brief InternetGatewayDevice.WANDevice.{i}.X_BROADCOM_COM_WANDSLDiagnostics. */
#define MDMOID_WAN_DSL_PROPRIETARY_DIAG  89

/*! \brief InternetGatewayDevice.WANDevice.{i}.X_BROADCOM_COM_WANDSLDiagnostics.PeakLineNoise. */
#define MDMOID_WAN_DSL_DIAG_PLN  90

/*! \brief InternetGatewayDevice.WANDevice.{i}.X_BROADCOM_COM_WANDSLDiagnostics.NonLinearity. */
#define MDMOID_WAN_DSL_DIAG_NON_LINEARITY  91

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}. */
#define MDMOID_WAN_CONN_DEVICE  92

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.X_BROADCOM_COM_WANATMF4EndToEndLoopbackDiagnostics. */
#define MDMOID_WAN_ATM_F4_END_TO_END_LOOPBACK_DIAG  93

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.X_BROADCOM_COM_WANATMF4LoopbackDiagnostics. */
#define MDMOID_WAN_ATM_F4_LOOPBACK_DIAG  94

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.X_BROADCOM_COM_WANATMF5EndToEndLoopbackDiagnostics. */
#define MDMOID_WAN_ATM_F5_END_TO_END_LOOPBACK_DIAG  95

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.WANDSLLinkConfig. */
#define MDMOID_WAN_DSL_LINK_CFG  96

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.WANDSLLinkConfig.X_BROADCOM_COM_ATM_PARMS. */
#define MDMOID_WAN_DSL_ATM_PARAMS  97

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.WANATMF5LoopbackDiagnostics. */
#define MDMOID_WAN_ATM5_LOOPBACK_DIAG  98

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.WANPTMLinkConfig. */
#define MDMOID_WAN_PTM_LINK_CFG  99

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.WANEthernetLinkConfig. */
#define MDMOID_WAN_ETH_LINK_CFG  100

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.X_BROADCOM_COM_L2tpAcLinkConfig. */
#define MDMOID_L2TP_AC_LINK_CONFIG  101

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.WANIPConnection.{i}. */
#define MDMOID_WAN_IP_CONN  102

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.WANIPConnection.{i}.PortMapping.{i}. */
#define MDMOID_WAN_IP_CONN_PORTMAPPING  103

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.WANIPConnection.{i}.X_BROADCOM_COM_PortTriggering.{i}. */
#define MDMOID_WAN_IP_CONN_PORT_TRIGGERING  104

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.WANIPConnection.{i}.X_BROADCOM_COM_FirewallException.{i}. */
#define MDMOID_WAN_IP_CONN_FIREWALL_EXCEPTION  105

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.WANIPConnection.{i}.X_BROADCOM_COM_MacFilterObj. */
#define MDMOID_MAC_FILTER  106

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.WANIPConnection.{i}.X_BROADCOM_COM_MacFilterObj.X_BROADCOM_COM_MacFilterCfg.{i}. */
#define MDMOID_MAC_FILTER_CFG  107

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.WANIPConnection.{i}.Stats. */
#define MDMOID_WAN_IP_CONN_STATS  108

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.WANPPPConnection.{i}. */
#define MDMOID_WAN_PPP_CONN  109

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.WANPPPConnection.{i}.PortMapping.{i}. */
#define MDMOID_WAN_PPP_CONN_PORTMAPPING  110

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.WANPPPConnection.{i}.X_BROADCOM_COM_PortTriggering.{i}. */
#define MDMOID_WAN_PPP_CONN_PORT_TRIGGERING  111

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.WANPPPConnection.{i}.X_BROADCOM_COM_FirewallException.{i}. */
#define MDMOID_WAN_PPP_CONN_FIREWALL_EXCEPTION  112

/*! \brief InternetGatewayDevice.WANDevice.{i}.WANConnectionDevice.{i}.WANPPPConnection.{i}.Stats. */
#define MDMOID_WAN_PPP_CONN_STATS  113

/*! \brief InternetGatewayDevice.Layer3Forwarding. */
#define MDMOID_L3_FORWARDING  114

/*! \brief InternetGatewayDevice.Layer3Forwarding.Forwarding.{i}. */
#define MDMOID_L3_FORWARDING_ENTRY  115

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_IPv6Layer3Forwarding. */
#define MDMOID_I_PV6_L3_FORWARDING  116

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_IPv6Layer3Forwarding.IPv6Forwarding.{i}. */
#define MDMOID_I_PV6_L3_FORWARDING_ENTRY  117

/*! \brief InternetGatewayDevice.Services. */
#define MDMOID_SERVICES  118

/*! \brief InternetGatewayDevice.Services.StorageService.{i}. */
#define MDMOID_STORAGE_SERVICE  119

/*! \brief InternetGatewayDevice.Services.StorageService.{i}.UserAccount.{i}. */
#define MDMOID_USER_ACCOUNT  120

/*! \brief InternetGatewayDevice.Services.StorageService.{i}.Capabilites. */
#define MDMOID_CAPABILITES  121

/*! \brief InternetGatewayDevice.Services.StorageService.{i}.NetInfo. */
#define MDMOID_NETWORK_INFO  122

/*! \brief InternetGatewayDevice.Services.StorageService.{i}.NetworkServer. */
#define MDMOID_NETWORK_SERVER  123

/*! \brief InternetGatewayDevice.Services.StorageService.{i}.PhysicalMedium.{i}. */
#define MDMOID_PHYSICAL_MEDIUM  124

/*! \brief InternetGatewayDevice.Services.StorageService.{i}.LogicalVolume.{i}. */
#define MDMOID_LOGICAL_VOLUME  125

/*! \brief InternetGatewayDevice.Services.StorageService.{i}.LogicalVolume.{i}.Folder.{i}. */
#define MDMOID_FOLDER  126

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}. */
#define MDMOID_VOICE  127

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.Capabilities. */
#define MDMOID_VOICE_CAP  128

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.Capabilities.SIP. */
#define MDMOID_VOICE_CAP_SIP  129

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.Capabilities.MGCP. */
#define MDMOID_VOICE_CAP_MGCP  130

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.Capabilities.Codecs.{i}. */
#define MDMOID_VOICE_CAP_CODECS  131

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.VoiceProfile.{i}. */
#define MDMOID_VOICE_PROF  132

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.VoiceProfile.{i}.ServiceProviderInfo. */
#define MDMOID_VOICE_PROF_PROVIDER  133

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.VoiceProfile.{i}.SIP. */
#define MDMOID_VOICE_PROF_SIP  134

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.VoiceProfile.{i}.SIP.EventSubscribe.{i}. */
#define MDMOID_VOICE_PROF_SIP_SUBSCRIBE  135

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.VoiceProfile.{i}.MGCP. */
#define MDMOID_VOICE_PROF_MGCP  136

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.VoiceProfile.{i}.RTP. */
#define MDMOID_VOICE_PROF_RTP  137

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.VoiceProfile.{i}.RTP.SRTP. */
#define MDMOID_VOICE_PROF_RTP_SRTP  138

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.VoiceProfile.{i}.FaxT38. */
#define MDMOID_VOICE_PROF_FAX_T38  139

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.VoiceProfile.{i}.Line.{i}. */
#define MDMOID_VOICE_LINE  140

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.VoiceProfile.{i}.Line.{i}.SIP. */
#define MDMOID_VOICE_LINE_SIP  141

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.VoiceProfile.{i}.Line.{i}.CallingFeatures. */
#define MDMOID_VOICE_LINE_CALLING_FEATURES  142

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.VoiceProfile.{i}.Line.{i}.VoiceProcessing. */
#define MDMOID_VOICE_LINE_PROCESSING  143

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.VoiceProfile.{i}.Line.{i}.Codec. */
#define MDMOID_VOICE_LINE_CODEC  144

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.VoiceProfile.{i}.Line.{i}.Codec.List.{i}. */
#define MDMOID_VOICE_LINE_CODEC_LIST  145

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.VoiceProfile.{i}.Line.{i}.Stats. */
#define MDMOID_VOICE_LINE_STATS  146

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.PhyInterface.{i}. */
#define MDMOID_VOICE_PHY_INTF  147

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.PhyInterface.{i}.Tests. */
#define MDMOID_VOICE_PHY_INTF_TESTS  148

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.X_BROADCOM_COM_PSTN.{i}. */
#define MDMOID_VOICE_PSTN  149

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.X_BROADCOM_COM_Ntr. */
#define MDMOID_VOICE_NTR  150

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.X_BROADCOM_COM_Ntr.History.{i}. */
#define MDMOID_VOICE_NTR_HISTORY  151

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.DECTInterface. */
#define MDMOID_DECT_INTF  152

/*! \brief InternetGatewayDevice.Services.VoiceService.{i}.DECTInterface.Handset.{i}. */
#define MDMOID_DECT_HANDSET  153

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_SnmpCfg. */
#define MDMOID_SNMP_CFG  154

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_SecDmzHostCfg. */
#define MDMOID_SEC_DMZ_HOST_CFG  155

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_AccessTimeRestriction.{i}. */
#define MDMOID_ACCESS_TIME_RESTRICTION  156

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_UrlFilterCfg. */
#define MDMOID_URL_FILTER_CFG  157

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_UrlFilterCfg.X_BROADCOM_COM_UrlFilterListCfgObj.{i}. */
#define MDMOID_URL_FILTER_LIST  158

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_DDnsCfg.{i}. */
#define MDMOID_D_DNS_CFG  159

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_IPPCfg. */
#define MDMOID_IPP_CFG  160

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_DLNA. */
#define MDMOID_DLNA  161

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_DLNA.DmsCfg. */
#define MDMOID_DMS_CFG  162

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_DebugPortMirroringCfg.{i}. */
#define MDMOID_WAN_DEBUG_PORT_MIRRORING_CFG  163

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_IPSecCfg.{i}. */
#define MDMOID_IP_SEC_CFG  164

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_CertificateCfg.{i}. */
#define MDMOID_CERTIFICATE_CFG  165

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_WapiCertificate. */
#define MDMOID_WAPI_CERTIFICATE  166

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_WapiCertificate.WapiAsCertificate. */
#define MDMOID_WAPI_AS_CERTIFICATE  167

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_WapiCertificate.WapiIssuedCertificate. */
#define MDMOID_WAPI_ISSUED_CERTIFICATE  168

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_NetworkConfig. */
#define MDMOID_NETWORK_CONFIG  169

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_InterfaceControl. */
#define MDMOID_INTERFACE_CONTROL  170

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_P8021agCfg. */
#define MDMOID_P8021AG_CFG  171

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_IGMPCfg. */
#define MDMOID_IGMP_CFG  172

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_MLDCfg. */
#define MDMOID_MLD_CFG  173

/*! \brief InternetGatewayDevice.X_ITU_T_ORG. */
#define MDMOID_ITU_T_ORG  174

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4. */
#define MDMOID_G_984_4  175

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.EquipmentManagement. */
#define MDMOID_EQUIPMENT_MANAGEMENT  176

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.EquipmentManagement.OntG. */
#define MDMOID_ONT_G  177

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.EquipmentManagement.Ont2G. */
#define MDMOID_ONT2_G  178

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.EquipmentManagement.OntData. */
#define MDMOID_ONT_DATA  179

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.EquipmentManagement.SoftwareImage.{i}. */
#define MDMOID_SOFTWARE_IMAGE  180

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.EquipmentManagement.CardHolder.{i}. */
#define MDMOID_CARD_HOLDER  181

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.EquipmentManagement.CircuitPack.{i}. */
#define MDMOID_CIRCUIT_PACK  182

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.EquipmentManagement.PowerShedding.{i}. */
#define MDMOID_POWER_SHEDDING  183

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.EquipmentManagement.PortMappingPackageG.{i}. */
#define MDMOID_PORT_MAPPING_PACKAGE_G  184

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.AniManagement. */
#define MDMOID_ANI_MANAGEMENT  185

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.AniManagement.AniG.{i}. */
#define MDMOID_ANI_G  186

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.AniManagement.TCont.{i}. */
#define MDMOID_T_CONT  187

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.AniManagement.GemPortNetworkCtp.{i}. */
#define MDMOID_GEM_PORT_NETWORK_CTP  188

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.AniManagement.X_BROADCOM_COM_GemPortNetworkCtp.{i}. */
#define MDMOID_BC_GEM_PORT_NETWORK_CTP  189

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.AniManagement.GemInterworkingTp.{i}. */
#define MDMOID_GEM_INTERWORKING_TP  190

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.AniManagement.MulticastGemInterworkingTp.{i}. */
#define MDMOID_MULTICAST_GEM_INTERWORKING_TP  191

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.AniManagement.MulticastGemInterworkingTp.{i}.MulticastAddressTable.{i}. */
#define MDMOID_GEM_INTERWORKING_TP_MULTICAST_ADDRESS_TABLE  192

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.AniManagement.GemPortPmHistoryData.{i}. */
#define MDMOID_GEM_PORT_PM_HISTORY_DATA  193

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.AniManagement.GalEthernetProfile.{i}. */
#define MDMOID_GAL_ETHERNET_PROFILE  194

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.AniManagement.GalEthernetPmHistoryData.{i}. */
#define MDMOID_GAL_ETHERNET_PM_HISTORY_DATA  195

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.AniManagement.FecPmHistoryData.{i}. */
#define MDMOID_FEC_PM_HISTORY_DATA  196

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices. */
#define MDMOID_LAYER2_DATA_SERVICES  197

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.MacBridgeServiceProfile.{i}. */
#define MDMOID_MAC_BRIDGE_SERVICE_PROFILE  198

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.X_BROADCOM_COM_MacBridgeServiceProfile.{i}. */
#define MDMOID_BC_MAC_BRIDGE_SERVICE_PROFILE  199

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.MacBridgeConfigData.{i}. */
#define MDMOID_MAC_BRIDGE_CONFIG_DATA  200

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.MacBridgePmHistoryData.{i}. */
#define MDMOID_MAC_BRIDGE_PM_HISTORY_DATA  201

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.MacBridgePortConfigData.{i}. */
#define MDMOID_MAC_BRIDGE_PORT_CONFIG_DATA  202

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.MacBridgePortDesignationData.{i}. */
#define MDMOID_MAC_BRIDGE_PORT_DESIGNATION_DATA  203

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.MacBridgePortFilterTableData.{i}. */
#define MDMOID_MAC_BRIDGE_PORT_FILTER_TABLE_DATA  204

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.MacBridgePortFilterTableData.{i}.MacFilterTable.{i}. */
#define MDMOID_MAC_FILTER_TABLE  205

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.MacBridgePortFilterPreAssignTable.{i}. */
#define MDMOID_MAC_BRIDGE_PORT_FILTER_PRE_ASSIGN_TABLE  206

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.MacBridgePortBridgeTableData.{i}. */
#define MDMOID_MAC_BRIDGE_PORT_BRIDGE_TABLE_DATA  207

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.MacBridgePortBridgeTableData.{i}.BridgeTable.{i}. */
#define MDMOID_BRIDGE_TABLE  208

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.MacBridgePortPmHistoryData.{i}. */
#define MDMOID_MAC_BRIDGE_PORT_PM_HISTORY_DATA  209

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.MapperServiceProfile.{i}. */
#define MDMOID_MAPPER_SERVICE_PROFILE  210

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.X_BROADCOM_COM_MapperServiceProfile.{i}. */
#define MDMOID_BC_MAPPER_SERVICE_PROFILE  211

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.VlanTaggingFilterData.{i}. */
#define MDMOID_VLAN_TAGGING_FILTER_DATA  212

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.VlanTaggingOperationConfigurationData.{i}. */
#define MDMOID_VLAN_TAGGING_OPERATION_CONFIGURATION_DATA  213

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.ExtendedVlanTaggingOperationConfigurationData.{i}. */
#define MDMOID_EXTENDED_VLAN_TAGGING_OPERATION_CONFIGURATION_DATA  214

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer2DataServices.ExtendedVlanTaggingOperationConfigurationData.{i}.ReceivedFrameVlanTaggingOperationTable.{i}. */
#define MDMOID_RECEIVED_FRAME_VLAN_TAGGING_OPERATION_TABLE  215

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer3DataServices. */
#define MDMOID_LAYER3_DATA_SERVICES  216

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer3DataServices.IpHostConfigData.{i}. */
#define MDMOID_IP_HOST_CONFIG_DATA  217

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer3DataServices.X_BROADCOM_COM_IpHostConfigData.{i}. */
#define MDMOID_BC_IP_HOST_CONFIG_DATA  218

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer3DataServices.IpHostPmHistoryData.{i}. */
#define MDMOID_IP_HOST_PM_HISTORY_DATA  219

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.Layer3DataServices.TcpUdpConfigData.{i}. */
#define MDMOID_TCP_UDP_CONFIG_DATA  220

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.EthernetServices. */
#define MDMOID_ETHERNET_SERVICES  221

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.EthernetServices.PptpEthernetUni.{i}. */
#define MDMOID_PPTP_ETHERNET_UNI  222

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.EthernetServices.EthernetPmHistoryData.{i}. */
#define MDMOID_ETHERNET_PM_HISTORY_DATA  223

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.EthernetServices.EthernetPmHistoryData2.{i}. */
#define MDMOID_ETHERNET_PM_HISTORY_DATA2  224

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.EthernetServices.EthernetPmHistoryData3.{i}. */
#define MDMOID_ETHERNET_PM_HISTORY_DATA3  225

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices. */
#define MDMOID_VOICE_SERVICES  226

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.PptpPotsUni.{i}. */
#define MDMOID_PPTP_POTS_UNI  227

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.SipUserData.{i}. */
#define MDMOID_SIP_USER_DATA  228

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.SipAgentConfigData.{i}. */
#define MDMOID_SIP_AGENT_CONFIG_DATA  229

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.VoIpVoiceCtp.{i}. */
#define MDMOID_VO_IP_VOICE_CTP  230

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.VoIpMediaProfile.{i}. */
#define MDMOID_VO_IP_MEDIA_PROFILE  231

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.VoiceServiceProfile.{i}. */
#define MDMOID_VOICE_SERVICE  232

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.RtpProfileData.{i}. */
#define MDMOID_RTP_PROFILE_DATA  233

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.VoIpAppServiceProfile.{i}. */
#define MDMOID_VO_IP_APP_SERVICE_PROFILE  234

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.VoiceFeatureAccessCodes.{i}. */
#define MDMOID_VOICE_FEATURE_ACCESS_CODES  235

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.NetworkDialPlanTable.{i}. */
#define MDMOID_NETWORK_DIAL_PLAN_TABLE  236

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.NetworkDialPlanTable.{i}.DialPlanTable.{i}. */
#define MDMOID_DIAL_PLAN_TABLE  237

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.VoIpLineStatus.{i}. */
#define MDMOID_VO_IP_LINE_STATUS  238

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.CallControlPmHistoryData.{i}. */
#define MDMOID_CALL_CONTROL_PM_HISTORY_DATA  239

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.RtpPmHistoryData.{i}. */
#define MDMOID_RTP_PM_HISTORY_DATA  240

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.SipAgentPmHistoryData.{i}. */
#define MDMOID_SIP_AGENT_PM_HISTORY_DATA  241

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.SipCallInitPmHistoryData.{i}. */
#define MDMOID_SIP_CALL_INIT_PM_HISTORY_DATA  242

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.MgcConfigData.{i}. */
#define MDMOID_MGC_CONFIG_DATA  243

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.MgcPmHistoryData.{i}. */
#define MDMOID_MGC_PM_HISTORY_DATA  244

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.VoiceServices.VoIpConfigData.{i}. */
#define MDMOID_VO_IP_CONFIG_DATA  245

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.MocaServices. */
#define MDMOID_MOCA_SERVICES  246

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.MocaServices.PptpMocaUni.{i}. */
#define MDMOID_PPTP_MOCA_UNI  247

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.MocaServices.X_BROADCOM_COM_PptpMocaUni.{i}. */
#define MDMOID_BRCM_PPTP_MOCA_UNI  248

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.MocaServices.X_BROADCOM_COM_MocaStatus.{i}. */
#define MDMOID_MOCA_STATUS  249

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.MocaServices.X_BROADCOM_COM_MocaStats.{i}. */
#define MDMOID_MOCA_STATS  250

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.MocaServices.MocaEthernetPmHistoryData.{i}. */
#define MDMOID_MOCA_ETHERNET_PM_HISTORY_DATA  251

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.MocaServices.MocaInterfacePmHistoryData.{i}. */
#define MDMOID_MOCA_INTERFACE_PM_HISTORY_DATA  252

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.MocaServices.MocaInterfacePmHistoryData.{i}.NodeTable.{i}. */
#define MDMOID_MOCA_INTERFACE_PM_HISTORY_DATA_NODE_TABLE  253

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.TrafficManagement. */
#define MDMOID_TRAFFIC_MANAGEMENT  254

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.TrafficManagement.PriorityQueueG.{i}. */
#define MDMOID_PRIORITY_QUEUE_G  255

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.TrafficManagement.TrafficSchedulerG.{i}. */
#define MDMOID_TRAFFIC_SCHEDULER_G  256

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.TrafficManagement.GemTrafficDescriptor.{i}. */
#define MDMOID_GEM_TRAFFIC_DESCRIPTOR  257

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.General. */
#define MDMOID_GENERAL  258

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.General.UniG.{i}. */
#define MDMOID_UNI_G  259

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.General.OltG. */
#define MDMOID_OLT_G  260

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.General.NetworkAddress.{i}. */
#define MDMOID_NETWORK_ADDRESS  261

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.General.AuthenticationSecurityMethod.{i}. */
#define MDMOID_AUTHENTICATION_SECURITY_METHOD  262

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.General.LargeString.{i}. */
#define MDMOID_LARGE_STRING  263

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.General.ThresholdData1.{i}. */
#define MDMOID_THRESHOLD_DATA1  264

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.General.ThresholdData2.{i}. */
#define MDMOID_THRESHOLD_DATA2  265

/*! \brief InternetGatewayDevice.X_ITU_T_ORG.G_984_4.General.Omci. */
#define MDMOID_OMCI  266

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_OmciSystem. */
#define MDMOID_OMCI_SYSTEM  267

/*! \brief InternetGatewayDevice.X_BROADCOM_COM_GponOmciStats. */
#define MDMOID_GPON_OMCI_STATS  268

/*! \brief maximum OID value */
#define MDM_MAX_OID 268



#endif /* __MDM_OBJECTID_H__ */
