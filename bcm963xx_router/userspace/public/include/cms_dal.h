/***********************************************************************
 *
 *  Copyright (c) 2006-2007  Broadcom Corporation
 *  All Rights Reserved
 *
# 
# 
# This program is the proprietary software of Broadcom Corporation and/or its 
# licensors, and may only be used, duplicated, modified or distributed pursuant 
# to the terms and conditions of a separate, written license agreement executed 
# between you and Broadcom (an "Authorized License").  Except as set forth in 
# an Authorized License, Broadcom grants no license (express or implied), right 
# to use, or waiver of any kind with respect to the Software, and Broadcom 
# expressly reserves all rights in and to the Software and all intellectual 
# property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU HAVE 
# NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY NOTIFY 
# BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE. 
# 
# Except as expressly set forth in the Authorized License, 
# 
# 1. This program, including its structure, sequence and organization, 
#    constitutes the valuable trade secrets of Broadcom, and you shall use 
#    all reasonable efforts to protect the confidentiality thereof, and to 
#    use this information only in connection with your use of Broadcom 
#    integrated circuit products. 
# 
# 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS" 
#    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR 
#    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH 
#    RESPECT TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND 
#    ALL IMPLIED WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, 
#    FITNESS FOR A PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR 
#    COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE 
#    TO DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR 
#    PERFORMANCE OF THE SOFTWARE. 
# 
# 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR 
#    ITS LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, 
#    INDIRECT, OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY 
#    WAY RELATING TO YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN 
#    IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; 
#    OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE 
#    SOFTWARE ITSELF OR U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS 
#    SHALL APPLY NOTWITHSTANDING ANY FAILURE OF ESSENTIAL PURPOSE OF ANY 
#    LIMITED REMEDY. 
#
 *
 ************************************************************************/

#ifndef __CMS_DAL_H__
#define __CMS_DAL_H__


/*!\file cms_dal.h
 * \brief Header file for the Data Aggregation Layer API.
 * This API is used by httpd, cli, and snmp and presents an API similar to the
 * old dbAPI.  A lot of code in http, cli, and snmp rely on functions that are
 * more in the old dbAPI style than the new object layer API.
 * 
 * This API is implemented in the cms_dal.so library.
 */

#include "cms_core.h"
#include "cms_msg.h"  /* for NetworkAccessMode enum */

/** Board data buf len (legacy) ? 
 */
#define WEB_MD_BUF_SIZE_MAX   264

/** Length of TR69C_URL */
#define TR69C_URL_LEN         258

/** PortMapping Interface list size */
#define PMAP_INTF_LIST_SIZE 724
#define dummyVLANID  -178

/** Legacy structure used by httpd and cli to hold a bunch of variables.
 */
typedef struct {
#ifdef AEI_VDSL_CUSTOMER_SASKTEL
   char inputPassword_tech[BUFLEN_64];
#endif
   char adminUserName[BUFLEN_16];
#ifdef AEI_VDSL_CUSTOMER_SASKTEL
   char adminPassword[BUFLEN_64];
#else
   char adminPassword[BUFLEN_24];
#endif
//xlxia(20110518):return back to be the same as Telnet by Sasktel_V1000H_PRDv2 8_050911_SH.doc(R.SVG.S4) 
#ifdef AEI_VDSL_CUSTOMER_SASKTEL
   char techPassword[BUFLEN_24];
#endif
#if defined(AEI_VDSL_CUSTOMER_NCS)
   char Password[BUFLEN_24];
   UINT32 upgradePercent;
   char ethIpAddress_tmp[BUFLEN_16];
   char ethSubnetMask_tmp[BUFLEN_16];
   char dhcpEthStart_tmp[BUFLEN_16];
   char dhcpEthEnd_tmp[BUFLEN_16];	
#endif
#if defined(AEI_VDSL_CUSTOMER_QWEST) || defined (AEI_VDSL_CUSTOMER_TELUS)
   UBOOL8 fwStealthMode;
   UBOOL8 showPassword;
   UINT32 sesTimeRec;
   UINT32 pppClearRecv;
   UINT32 pppClearSend;
   UINT32 upTimeRec;
   UINT32 dsl1ClearTime;
   UINT32 dsl2ClearTime;
   UINT32 wanClearClick;
#endif
   char sptUserName[BUFLEN_16];
#ifdef AEI_VDSL_CUSTOMER_SASKTEL
   char sptPassword[BUFLEN_64];
#else
   char sptPassword[BUFLEN_24];
#endif
   char usrUserName[BUFLEN_16];
#ifdef AEI_VDSL_CUSTOMER_SASKTEL
   char usrPassword[BUFLEN_64];
#else
   char usrPassword[BUFLEN_24];
#endif
   char curUserName[BUFLEN_16];
   char inUserName[BUFLEN_16];
#ifdef AEI_VDSL_CUSTOMER_SASKTEL
   char inPassword[BUFLEN_64];
#else
   char inPassword[BUFLEN_24];
#endif
   char inOrgPassword[BUFLEN_24];
   SINT32 recvSessionKey;  /**< Session key received from browser, used by httpd only */
   UBOOL8 ipAddrAccessControlEnabled; /**< For the IP address access control list mechanism */
   char brName[BUFLEN_64];
#if defined(AEI_VDSL_CUSTOMER_NCS)
   char brNameEx[3][BUFLEN_64];
   char RipVer[BUFLEN_16];
#endif
   char ethIpAddress[BUFLEN_16];
   char ethSubnetMask[BUFLEN_16];
   UBOOL8 enblLanFirewall;          /**< Is LAN side firewall enabled */
#if defined(AEI_VDSL_CUSTOMER_NCS)
   char ethIpAddressEx[3][BUFLEN_16];
   char ethSubnetMaskEx[3][BUFLEN_16];
   UBOOL8 enblLanFirewallEx[3];     /**< Is LAN side firewall enabled */
#endif

   char lan2IpAddress[BUFLEN_16];
   char lan2SubnetMask[BUFLEN_16];
   SINT32 enblLan2;
   char wanL2IfName[BUFLEN_16];     /**< The layer 2 interface name of a service connection */
   char wanIpAddress[BUFLEN_16];
   char wanSubnetMask[BUFLEN_16];
   char defaultGatewayList[CMS_MAX_DEFAULT_GATEWAY * CMS_IFNAME_LENGTH];  /**< max default gateways allowed in the system */
   char wanIntfGateway[BUFLEN_16];  /**< For static IPoE WAN gateway ip (could served as system default gateway if it is seleected) */
   char wanIfName[BUFLEN_32];
   char dnsIfcsList[CMS_MAX_DNSIFNAME * CMS_IFNAME_LENGTH];      /**< max wan interfaces allowed in dns interface list */
   char dnsPrimary[CMS_IPADDR_LENGTH];
   char dnsSecondary[CMS_IPADDR_LENGTH];
   char dnsHostName[BUFLEN_32];   /**< dnsHostName (was IFC_SMALL_LEN) */
   char dnsDomainName[BUFLEN_32]; /**< dnsDomainName */
   char dhcpEthStart[BUFLEN_16];
   char dhcpEthEnd[BUFLEN_16];
   char dhcpSubnetMask[BUFLEN_16]; /**< added by lgd for TR98, IFC_TINY_LEN */
   SINT32  dhcpLeasedTime;     /**< this is in hours, MDM stores it in seconds */
#if defined(AEI_VDSL_CUSTOMER_QWEST) ||  defined(AEI_VDSL_CUSTOMER_NCS)
   UBOOL8 dhcpAutoReserve;                      /* Automatically set DHCP reservations on DHCP IP allocation */
   char lanDnsType[BUFLEN_16]; /* default or custom dns servers */
#endif
#if defined(AEI_VDSL_CUSTOMER_NCS)
   char dhcpEthStartEx[3][BUFLEN_16];
   char dhcpEthEndEx[3][BUFLEN_16];
   char dhcpSubnetMaskEx[3][BUFLEN_16]; /**< added by lgd for TR98, IFC_TINY_LEN */
   SINT32  dhcpLeasedTimeEx[3];     /**< this is in hours(ifdef ACTION_TEC it's minutes), MDM stores it in seconds */
#endif

#if defined(SSID234_URL_REDIRECT)
   SINT32 urlredirect[3];
   char urldefault[3][BUFLEN_256];
   char urllock[3][BUFLEN_256];
#endif
   char dhcpRelayServer[BUFLEN_16];
   char pppUserName[BUFLEN_256];
   char pppPassword[BUFLEN_256];
   char pppServerName[BUFLEN_40];
   char serviceName[BUFLEN_40];
   SINT32  serviceId;
   SINT32  enblIgmp;   /**< Enable multicast on a WAN connection, implies starting igmp proxy */
   SINT32  enblService;
   SINT32  ntwkPrtcl;
   SINT32  encapMode;
   SINT32  enblDhcpClnt;
   SINT32  enblDhcpSrv;
#if defined(AEI_VDSL_CUSTOMER_NCS)
   SINT32  enblDhcpSrvEx[3];
#endif

   SINT32  enblAutoScan;
   SINT32  enblNat;
   SINT32  enblFullcone;  /**< enable fullcone nat */
   SINT32  enblFirewall;
   SINT32  enblOnDemand;
   SINT32  pppTimeOut;
   SINT32  pppIpExtension;
   SINT32  pppAuthMethod;
   SINT32  pppShowAuthErrorRetry;
   SINT32  pppAuthErrorRetry;   
   SINT32  enblPppDebug;
   SINT32  pppToBridge;  /**< ppp to bridge */
#if defined(AEI_VDSL_CUSTOMER_NCS)
   SINT32  ipToBridge;  /**< ip to bridge */
   SINT32  multiWanVlan;  /**< ip to bridge */
#endif


   SINT32  logStatus;   /**< Enabled or disabled */
   SINT32  logDisplay;  /**< What level to display when asked to dump log */
   SINT32  logLevel;    /**< What level to log at */
   SINT32  logMode;     /**< 1=local, 2=remote, 3=local and remote */
   char logIpAddress[BUFLEN_16];  /* Remote log server IP address */
   SINT32  logPort;     /**< Remote log server port */
#if defined(AEI_VDSL_CUSTOMER_NCS)
   SINT32  logLocalSize; /**< the value*10 is the size of the local log */
   SINT32  logRemoteSize; /**< the value*10 is the size of the remote log*/
#endif

   UINT32  adslFlag;  /**< various bits in the WanDslInterfaceConfig object. */
#ifdef SUPPORT_RIP
   SINT32  ripMode;
   SINT32  ripVersion;
   SINT32  ripOperation;
#endif

#if defined(AEI_VDSL_CUSTOMER_NCS)
   int timezoneindex;
   int isie;
#ifdef AEI_VDSL_CUSTOMER_TELUS
   char currentPage[10000];
#else
   char currentPage[BUFLEN_1024];
#endif
   char fromPage[BUFLEN_256];
   char noThankyou;
   UBOOL8 vipmode;
#if defined(AEI_VDSL_MULTILEVEL)
   SINT32 userID;
#endif
   char inputUserName[BUFLEN_16];
#ifdef AEI_VDSL_CUSTOMER_SASKTEL
   char inputPassword[BUFLEN_64];
#else
   char inputPassword[BUFLEN_24];
#endif
#endif


#ifdef DMP_X_BROADCOM_COM_ADSLWAN_1
   char dslPhyDrvVersion[BUFLEN_64];  /**< dsl phy and driver version */
#endif

   SINT32  portId;   /**< port Id, dual latency stuff */
   SINT32  ptmPriorityNorm; /**< PTM mode priority,  default priorityNorm is TRUE */
   SINT32  ptmPriorityHigh; /**< PTM mode priority,  default priorityHigh is FALSE */
   SINT32  atmVpi;
   SINT32  atmVci;
#if defined(AEI_VDSL_CUSTOMER_NCS)
   char  autoSearchResult[BUFLEN_16]; /**< ACTION_TEC's extend, store the result of the lev2 auto detection*/
   UBOOL8 hpnaEnable;
#endif

   SINT32  connMode;  /**< connection mode for Default=0, VLAN=1 and MSC=3 */
   SINT32  enVlanMux;  /**< enable VLAN mux */
   SINT32  vlanMuxPr;  /**< VLAN 8021p priority */   
   SINT32  vlanMuxId;  /**< VLAN mux ID */
   SINT32  atmPeakCellRate;      /**< really should be UINT32 to match data-model */
   SINT32  atmSustainedCellRate; /**< really should be UINT32 to match data-model */
   SINT32  atmMaxBurstSize;      /**< really should be UINT32 to match data-model */
   char atmServiceCategory[BUFLEN_16];
   char linkType[BUFLEN_16];  /**< Link type, EoA, IPOA, PPPOA */
   char schedulerAlgorithm[BUFLEN_8];  /**< SP, WRR, WFQ */
   UINT32  queueWeight;
   UINT32  grpPrecedence;
   SINT32  snmpStatus;
   char snmpRoCommunity[BUFLEN_16];
   char snmpRwCommunity[BUFLEN_16];
   char snmpSysName[BUFLEN_16];
   char snmpSysLocation[BUFLEN_16];
   char snmpSysContact[BUFLEN_16];
   char snmpTrapIp[BUFLEN_16];

   SINT32  macPolicy;
   SINT32  enblQos;
   SINT32  enblDiffServ;
 
   SINT32  qosClsKey;                      
   char qosClsName[BUFLEN_16];   
   SINT32  qosClsOrder;                      
   SINT32  qosClsEnable;                   
   SINT32  qosProtocol;                    
   SINT32  qosWanVlan8021p;
   SINT32  ipoptionlist;      
   char optionval[BUFLEN_16];  
   SINT32  qosVlan8021p;                   
   char qosSrcAddr[BUFLEN_16];   
   char qosSrcMask[BUFLEN_16];   
   char qosSrcPort[BUFLEN_16];   
   char qosDstAddr[BUFLEN_16];   
   char qosDstMask[BUFLEN_16];   
   char qosDstPort[BUFLEN_16];   

   SINT32  qosDscpMark;                    
   SINT32  qosDscpCheck;                   
   char qosSrcMacAddr[BUFLEN_32];
   char qosDstMacAddr[BUFLEN_32];
   char qosSrcMacMask[BUFLEN_32];
   char qosDstMacMask[BUFLEN_32];
   SINT32  qosClsQueueKey;                 
   SINT32  qosProtocolExclude;             
   SINT32  qosSrcPortExclude;              
   SINT32  qosSrcIPExclude;                
   SINT32  qosDstIPExclude;                
   SINT32  qosDstPortExclude;              
   SINT32  qosDscpChkExclude;              
   SINT32  qosEtherPrioExclude;            
   char qosLanIfcName[BUFLEN_16];    
   
#ifdef SUPPORT_QUICKSETUP
   SINT32  quicksetupErrCode;
#endif

#ifdef SUPPORT_SAMBA
   char storageuserName[BUFLEN_64];
   char storagePassword[BUFLEN_64];
   char storagevolumeName[BUFLEN_64];
#endif

#ifdef SUPPORT_UPNP
   SINT32  enblUpnp;
#if defined(AEI_VDSL_CUSTOMER_QWEST) || defined(AEI_VDSL_CUSTOMER_NCS )
   SINT32  enblUpnpnat;
   UBOOL8 enblSipAlg;
#endif
#endif

#if defined(AEI_VDSL_PORTBRIDGING) 
   UBOOL8 enblPortBridging;
#endif

   SINT32  enblIgmpSnp;  /**< igmp snooping (should be per lan interface), configured into bridge */
   SINT32  enblIgmpMode; /**< 0 means standard, 1 means blocking */
#ifdef ETH_CFG
   SINT32  ethSpeed;
   SINT32  ethType;
   SINT32  ethMtu;
#endif
#if SUPPORT_PORT_MAP
   char groupName[BUFLEN_16];  /**< name of port group, also user friendly name of bridge */
   SINT32  editNtwkPrtcl;
   SINT32  editPortId ;
   SINT32  editAtmVpi;
   SINT32  editAtmVci;
#endif

   /*
    * Ethernet switch configuration is related to port mapping, but is not only for port mapping,
    * so it is outside of the SUPPORT_PORT_MAP define.
    */
   UBOOL8  virtualPortsEnabled;   /**< Is virtual ports enabled on the ethernet switch */
   UINT32  numberOfVirtualPorts;  /**< A value of 2 or more indicates ethernet switch supports virtual ports */
   char    ethSwitchIfName[BUFLEN_32];  /**< Base name of the ethernet switch */

   /* define this even if SUPPORT_ETHWAN is not defined to make code cleaner */
   SINT32 enableEthWan;

#ifdef SUPPORT_ADVANCED_DMZ
   SINT32  enableAdvancedDmz;  /**< Added by Keven */
   char    nonDmzIpAddress[BUFLEN_16];
   char    nonDmzIpMask[BUFLEN_16];
#endif
   SINT32  useStaticIpAddress;
   char pppLocalIpAddress[BUFLEN_16];
#ifdef SUPPORT_VLAN
   SINT32 enblVlan;
   SINT32 vlanId;
#endif
#ifdef SUPPORT_IPSEC
   SINT32 ipsTableIndex;
   SINT32 enable;
   char ipsConnName[BUFLEN_40];
   char ipsTunMode[BUFLEN_16];
   char ipsRemoteGWAddr[BUFLEN_40];
   char ipsLocalIPMode[BUFLEN_16];
   char ipsLocalIP[BUFLEN_40];
   char ipsLocalMask[BUFLEN_40]; 
   char ipsRemoteIPMode[BUFLEN_16]; 
   char ipsRemoteIP[BUFLEN_40];
   char ipsRemoteMask[BUFLEN_40];
   char ipsKeyExM[BUFLEN_16];
   char ipsAuthM[BUFLEN_16]; 
   char ipsPSK[BUFLEN_16];
   char ipsCertificateName[BUFLEN_40];
   SINT32 ipsKeyTime;
   char ipsPerfectFSEn[BUFLEN_16];
   char ipsManualEncryptionAlgo[BUFLEN_16];
   char ipsManualEncryptionKey[BUFLEN_256];
   char ipsManualAuthAlgo[BUFLEN_16];
   char ipsManualAuthKey[BUFLEN_256];
   char ipsSPI[BUFLEN_40];
   char ipsPh1Mode[BUFLEN_16];
   char ipsPh1EncryptionAlgo[BUFLEN_16];
   char ipsPh1IntegrityAlgo[BUFLEN_16];
   char ipsPh1DHGroup[BUFLEN_16];
    SINT32 ipsPh1KeyTime;
   char ipsPh2EncryptionAlgo[BUFLEN_16];
   char ipsPh2IntegrityAlgo[BUFLEN_16];
   char ipsPh2DHGroup[BUFLEN_16];
   SINT32 ipsPh2KeyTime;
#endif
#if defined(AEI_VDSL_CUSTOMER_NCS) || defined (AEI_VDSL_CUSTOMER_TELUS) 
   SINT32  diagAtmVpi;           /** Dsl vpi for testing **/
   SINT32  diagAtmVci;           /** Dsl vci for testing **/
   char hostName[BUFLEN_64];
   int  pingNum;
   int  packetSize;
   int  pingSign;
   int  pingSign1;
   int  dnsType;
   char pingTestResult[BUFLEN_1024];
#endif

#ifdef SUPPORT_CERT
   char certCategory[BUFLEN_16];
   char certName[BUFLEN_16];
#endif
#ifdef PORT_MIRRORING
   SINT32  mirrorPort ;
   SINT32  mirrorStatus ;
#endif
#ifdef SUPPORT_TR69C
   UBOOL8  tr69cInformEnable;   /**< InformEnabled */
   SINT32  tr69cInformInterval; /**< Inform interval, in seconds? */
#if defined(AEI_VDSL_CUSTOMER_NCS)
   char tr69cPeriodicInformTime[TR69C_URL_LEN];
#endif
   UBOOL8  tr69cNoneConnReqAuth; /**< NoneConnReqAuth */
   UBOOL8  tr69cDebugEnable;   /**< DebugEnabled */
   char tr69cAcsURL[TR69C_URL_LEN];  /**< URL of ACS server */
   char tr69cAcsUser[TR69C_URL_LEN]; /**< username for ACS server */
   char tr69cAcsPwd[TR69C_URL_LEN];  /**< password for ACS server */
   char tr69cConnReqURL[TR69C_URL_LEN]; /**< Inbound connection request URL */
   char tr69cConnReqUser[TR69C_URL_LEN]; /**< Inbound connection request username */
   char tr69cConnReqPwd[TR69C_URL_LEN];  /**< Inbound connection request password */
   char tr69cBoundIfName[BUFLEN_32];     /**< Interface which tr69c uses to talk to ACS */
#if defined(AEI_VDSL_CUSTOMER_QWEST)
        UBOOL8 tr69cBootstrapped;
        char tr69cLastPeriodicInform[TR69C_URL_LEN];
        UINT32 tr69cAuthenticationFailures;
        char tr69cLastTR069Upgrade[TR69C_URL_LEN];
#endif
#endif
#ifdef DMP_X_ITU_ORG_GPON_1 /**< aka SUPPORT_OMCI */
   UINT32 omciTcontNum;
   UINT32 omciTcontMeId;
   UINT32 omciEthNum;
   UINT32 omciEthMeId1;
   UINT32 omciEthMeId2;
   UINT32 omciEthMeId3;
   UINT32 omciEthMeId4;
   UINT32 omciMocaNum;
   UINT32 omciMocaMeId1;
   UINT32 omciDsPrioQueueNum;
   UBOOL8 omciDbgOmciEnable;
   UBOOL8 omciDbgModelEnable;
   UBOOL8 omciDbgVlanEnable;
   UBOOL8 omciDbgCmfEnable;
   UBOOL8 omciDbgFlowEnable;
   UBOOL8 omciDbgFileEnable;
   UINT32 omciPathMode;
#endif
   char swVers[WEB_MD_BUF_SIZE_MAX];  /**< Software Version */
   char boardID[BUFLEN_16];   /**< Board Id */
   char cfeVers[BUFLEN_32];   /**< cfe version string */
   char cmsBuildTimestamp[BUFLEN_32];   /**< CMS build timestamp */
   char pcIpAddr[CMS_IPADDR_LENGTH];
   UBOOL8 enblv6;
#ifdef DMP_X_BROADCOM_COM_IPV6_1 /* aka SUPPORT_IPV6 */
   UBOOL8 unnumberedModel;
   UBOOL8 enblDhcp6sStateful;
   char ipv6IntfIDStart[CMS_IPADDR_LENGTH];
   char ipv6IntfIDEnd[CMS_IPADDR_LENGTH];
   SINT32 dhcp6LeasedTime; /**< this is in hours, MDM stores it in seconds */
   char dns6Type[BUFLEN_16];
   char dns6Ifc[BUFLEN_32];
   char dns6Pri[CMS_IPADDR_LENGTH];
   char dns6Sec[CMS_IPADDR_LENGTH];
   char wanAddr6Type[BUFLEN_16];
   char wanAddr6[CMS_IPADDR_LENGTH];
   char wanGtwy6[CMS_IPADDR_LENGTH];
   char dfltGw6Ifc[BUFLEN_32];
   char lanIntfAddr6[CMS_IPADDR_LENGTH];
   SINT32  enblMld;   /**< Enable multicast on a WAN connection, implies starting mld proxy */
   SINT32  enblMldSnp;  /**< MLD snooping (should be per lan interface), configured into bridge */
   SINT32  enblMldMode; /**< 0 means standard, 1 means blocking */
#endif
#ifdef SUPPORT_SNTP
   UBOOL8 NTPEnable;/**< Enable NTP server */
   char NTPServer1[BUFLEN_64];	/**< NTPServer1 */
   char NTPServer2[BUFLEN_64];	/**< NTPServer2 */
   char NTPServer3[BUFLEN_64];	/**< NTPServer3 */
   char NTPServer4[BUFLEN_64];	/**< NTPServer4 */
   char NTPServer5[BUFLEN_64];	/**< NTPServer5 */
   char localTimeZone[BUFLEN_8];	/**< LocalTimeZone */

   char localTimeZoneName[BUFLEN_64];	/**< LocalTimeZoneName */
   UBOOL8 daylightSavingsUsed;	/**< DaylightSavingsUsed */
   char  daylightSavingsStart[BUFLEN_64];	/**< DaylightSavingsStart */
   char  daylightSavingsEnd[BUFLEN_64];	/**< DaylightSavingsEnd */
#endif
   char vdslSoftwareVersion[BUFLEN_64];  /**< VDSL software version */
#ifdef SUPPORT_P8021AG
   char test8021ag[BUFLEN_16];
   char macAddr8021ag[BUFLEN_18];
   int mdLevel8021ag;
   int vlanId8021ag;
#endif
   char dhcpcOp60VID[BUFLEN_256];
   char dhcpcOp61DUID[BUFLEN_256];
   char dhcpcOp61IAID[BUFLEN_16];
   UBOOL8 dhcpcOp125Enabled;
#if defined (DMP_X_BROADCOM_COM_PWRMNGT_1) /* aka SUPPORT_PWRMNGT */
   UINT32 pmCPUSpeed;         /* CPU speed 1,2, 4 and 8 are valid */
   UBOOL8 pmCPUr4kWaitEn;       /* r4k wait state sleep to enable */
   UBOOL8 pmDRAMSelfRefreshEn;  /* DRAM Sekf-Refresh mode */
#endif /* aka SUPPORT_PWRMNGT */
#ifdef DMP_X_BROADCOM_COM_STANDBY_1
   UBOOL8 pmStandbyEnable;                 /* To enable Standby feature */
   char pmStandbyStatusString[BUFLEN_64];  /* To report the current status of the standby functionality */
   UINT32 pmStandbyHour;                   /* Time to go in Standby */
   UINT32 pmStandbyMinutes;                /* Time to go in Standby */
   UINT32 pmWakeupHour;                    /* Time to wake up */
   UINT32 pmWakeupMinutes;                 /* Time to wake up */
#endif 
#ifdef SUPPORT_DSL_BONDING
   int dslBonding;
#ifdef AEI_VDSL_CUSTOMER_NCS
   int dslDatapump;
#endif
#if defined(AEI_VDSL_BONDING_NONBONDING_AUTOSWITCH) || defined(AEI_ADSL_BONDING_NONBONDING_AUTOSWITCH)
   int dslBondingAutoDetect;
#endif
#if defined(AEI_VDSL_TOGGLE_DATAPUMP)
   int dslDatapumpAutoDetect;
#endif
#endif
   UINT32 bondingLineNum;  /**< DSL line number, used even in non-bonding builds */

   // WAN Ethernet Status
   char   wanEthStatus_LinkStatus[BUFLEN_32];
   char   wanEthStatus_ISPStatus[BUFLEN_32];
   char   wanEthStatus_MACAddress[BUFLEN_32];
   char   wanEthStatus_IPAddress[BUFLEN_32];
   char   wanEthStatus_SubnetMask[BUFLEN_32];
   char   wanEthStatus_DefaultGateway[BUFLEN_32];
   char   wanEthStatus_DNSServer[BUFLEN_32];
   char   wanEthStatus_Protocol[BUFLEN_32];
   char   wanEthStatus_AddressingType[BUFLEN_32];
#ifdef AEI_VDSL_CUSTOMER_TELUS 
   UINT32 leaseTimeRemaining;
#endif
   SINT32 wanEthStatus_ReceivedPackets;
   SINT32 wanEthStatus_SendPackets;
   SINT32 wanEthStatus_TimeSpan;
#ifdef SUPPORT_DDNSD
#ifdef AEI_VDSL_CUSTOMER_NCS
   UBOOL8 ddnsEnable;
   char   ddnsProvider[BUFLEN_16];
   char   ddnsUsername[BUFLEN_32];
   char   ddnsPassword[BUFLEN_32];
   char   ddnsHostname[BUFLEN_64];
#endif
#endif
#ifdef AEI_VDSL_CUSTOMER_NCS
   char   speedTestUrl[BUFLEN_256];
   UINT32 speedTestAction;
   UINT32 speedTestStatus;

   UINT32 diagTestAction;
   UINT32 diagTestStatus;
#endif
#if defined(AEI_VDSL_CUSTOMER_QWEST) || defined (AEI_VDSL_CUSTOMER_TELUS)
   unsigned long adslRetr;
   unsigned long adslLprs;
   unsigned long adslLoss;
   unsigned long adslRetrLom;
   unsigned long adslInitErr;
   unsigned long adslUAS;
   unsigned char chType;
#endif
#ifdef AEI_VDSL_CUSTOMER_NCS
   UBOOL8 defaultWAN;
   int PPPAutoConnect;
   char old_wanL2IfName[BUFLEN_24];

   char hostnameAlias[BUFLEN_256];
   char hostnameMac[BUFLEN_32];
   int iconId;
#endif
#ifdef DMP_X_BROADCOM_COM_IGMP_1
   UINT32 igmpVer;	/**< IgmpVer */
   UINT32 igmpQI;	/**< IgmpQI */
   UINT32 igmpQRI;	/**< IgmpQRI */
   UINT32 igmpLMQI;	/**< IgmpLMQI */
   UINT32 igmpRV;	/**< IgmpRV */
   UINT32 igmpMaxGroups;	/**< IgmpMaxGroups */
   UINT32 igmpMaxSources;	/**< IgmpMaxSources */
   UINT32 igmpMaxMembers;	/**< IgmpMaxMembers */
   UBOOL8 igmpFastLeaveEnable;	/**< IgmpFastLeaveEnable */
   UBOOL8 igmpLan2LanMulticastEnable;	/**< IgmpLan2LanMulticastEnable */
#endif /* SUPPORT_IGMP */
#ifdef DMP_X_BROADCOM_COM_MLD_1
   UINT32 mldVer;	/**< MldVer */
   UINT32 mldQI;	/**< MldQI */
   UINT32 mldQRI;	/**< MldQRI */
   UINT32 mldLMQI;	/**< MldLMQI */
   UINT32 mldRV;	/**< MldRV */
   UINT32 mldMaxGroups;	/**< MldMaxGroups */
   UINT32 mldMaxSources;	/**< MldMaxSources */
   UINT32 mldMaxMembers;	/**< MldMaxMembers */
   UBOOL8 mldFastLeaveEnable;	/**< MldFastLeaveEnable */
   UBOOL8 mldLan2LanMulticastEnable;	/**< MldLan2LanMulticastEnable */
#endif /* SUPPORT_MLD */
#if defined(AEI_VDSL_CUSTOMER_NCS) && defined(REVERSION_HPNA)
   char lanDnsType[BUFLEN_16];
#endif
#if defined (CUSTOMER_TELUS_TR098)
   UBOOL8 passwordRequired;
   UBOOL8 passwordUserSelectable;
#endif
#if defined (REVERSION_R1000H)
   UBOOL8 specialVLANBackdoor;
#endif
#if defined(CUSTOMER_TDS_V1000W)//ken add mtu option for TDS V100W project,2010/05/19
   UINT32 udmtu; // user defined mtu
#endif
#ifdef AEI_VDSL_CUSTOMER_QWEST //ken, for dscp prioriziation
  UBOOL8 defaultDSCPPRIO;
#endif

   SINT32 cfgL2tpAc;                      /**< If set to 1, in configuring PPPoL2tpAC mode */
#ifdef DMP_X_BROADCOM_COM_L2TPAC_1
   char lnsIpAddress[CMS_IPADDR_LENGTH];  /**< L2TP server ip address */
   char tunnelName[BUFLEN_64];            /**< tunnel name */
#endif /* DMP_X_BROADCOM_COM_L2TPAC_1 */
   SINT32 cfgPptpAc;                      /**< If set to 1, in configuring PPTP mode */
#ifdef DMP_X_BROADCOM_COM_PPTPAC_1
   char pnsIpAddress[CMS_IPADDR_LENGTH];  /**< PPTP server ip address */
   char pptpTunnelName[BUFLEN_64];            /**< tunnel name */
#endif /* DMP_X_BROADCOM_COM_PPTPAC_1 */

#ifdef SUPPORT_MOCA
   char mocaVersion[BUFLEN_128];  /**< Version string for moca */
   char mocaIfName[BUFLEN_16];    /**< Interface name for moca */
   SINT32 enblMocaPrivacy;        /**< MoCA Privacy enable setting */
   char mocaPassword[BUFLEN_18];  /**< MoCA Privacy password */
   SINT32 enblMocaAutoScan;       /**< MoCA Auto network search setting */
   SINT32 mocaFrequency;          /**< MoCA Last operating frequency */
#endif

#ifdef DMP_X_BROADCOM_COM_PSTNENDPOINT_1
   char voiceServiceVersion[BUFLEN_64];  /**< Version string for our VOIP service */
#endif
#if defined(AEI_VDSL_DNS_CACHE) 
    UBOOL8 dns_cache_enabled;
   char lanDns1[CMS_IPADDR_LENGTH];
   char lanDns2[CMS_IPADDR_LENGTH];
   char lanDnsType1[BUFLEN_16];
   char lanDnsType2[BUFLEN_16]; 
   char fwLevel[BUFLEN_16];
   char fwInVal[BUFLEN_32];
   char fwOutVal[BUFLEN_32];
   char fwip[CMS_IPADDR_LENGTH];
   char fwset[BUFLEN_256];
   UINT8 pcname[MAC_ADDR_LEN];
   char Currentmode[BUFLEN_8];
   SINT32  mainConnMode;  /**< main connection mode for Default=0, VLAN=1 and MSC=3 */
#endif

#ifdef AEI_VDSL_HPNA
   char   hpnaStatus[BUFLEN_32];
   char   hpnaMode[BUFLEN_32];
   char   hpnaChipID[BUFLEN_64];
   char   hpnaHWVersion[BUFLEN_64];
   //-->hpna Link
   UBOOL8 hpnaLinkEnable_1;
   char   hpnaLinkStatus_1[BUFLEN_32];
   char   hpnaLinkMACAddress_1[BUFLEN_32];
   char   hpnaLinkInterfaceName_1[BUFLEN_32];
   int    hpnaLinkVlanID_1;
   int    hpnaLinkQos_1;
   char   hpnaLinkServiceType_1[BUFLEN_32];
   //
   UBOOL8 hpnaLinkEnable_2;
   char   hpnaLinkStatus_2[BUFLEN_32];
   char   hpnaLinkMACAddress_2[BUFLEN_32];
   char   hpnaLinkInterfaceName_2[BUFLEN_32];
   int    hpnaLinkVlanID_2;
   int    hpnaLinkQos_2;
   char   hpnaLinkServiceType_2[BUFLEN_32];
   //
   UBOOL8 hpnaLinkEnable_3;
   char   hpnaLinkStatus_3[BUFLEN_32];
   char   hpnaLinkMACAddress_3[BUFLEN_32];
   char   hpnaLinkInterfaceName_3[BUFLEN_32];
   int    hpnaLinkVlanID_3;
   int    hpnaLinkQos_3;
   char   hpnaLinkServiceType_3[BUFLEN_32];
   //
   UBOOL8 hpnaLinkEnable_4;
   char   hpnaLinkStatus_4[BUFLEN_32];
   char   hpnaLinkMACAddress_4[BUFLEN_32];
   char   hpnaLinkInterfaceName_4[BUFLEN_32];
   int    hpnaLinkVlanID_4;
   int    hpnaLinkQos_4;
   char   hpnaLinkServiceType_4[BUFLEN_32];

   //-->HPNA DIAG
   int    hpnaDiagSign;
   char   hpnaDiagResult[BUFLEN_1024];
   char   hpnaDiagCommand[BUFLEN_256];
#if defined(AEI_VDSL_CUSTOMER_Q2000H)
    UBOOL8 hpnaClear;
#endif
#endif


/*remote GUI, TELNET*/
#ifdef AEI_VDSL_CUSTOMER_NCS
   UBOOL8 adminPwState; 
   int serCtlHttp;
   int serCtlTelnetSsh;   
   char remTelUser[BUFLEN_32];
   char remTelPass[BUFLEN_32];
   int remTelTimeout;
   int remTelPassChanged;
   int remGuiTimeout;
   int remGuiPort;
   int remGuiPassChanged;
   char remoteGuiUserName[BUFLEN_16];
   char remoteGuiPassword[BUFLEN_24];
#ifdef AEI_VDSL_CUSTOMER_SASKTEL
   char remoteGuiPasswordShow[BUFLEN_24];
#endif
   UINT32 udmtu; // user defined mtu   
#endif
#if defined(AEI_VDSL_CUSTOMER_QWEST) || defined (AEI_VDSL_CUSTOMER_TELUS) 
   int serCtlTelnet;   
#endif

#if defined(AEI_VDSL_SMARTLED)
    UBOOL8 smartledEnable;  /*smart led enable disabled*/
#endif
#ifdef AEI_VDSL_UPGRADE_DUALIMG_HISTORY_SPAD
	UINT32 activeimage;
#endif
//add by vincent 2010-12-23
#ifdef AEI_VDSL_TR098_TELUS
	UBOOL8 passwordUserSelectable;
	UBOOL8 passwordRequired;
#endif

#ifdef AEI_CONTROL_LAYER
	int ipv6_enable;
	
	int ipv6_bbtype;
	char ipv6_wan_ifname[BUFLEN_16];
	char ipv6_wan_ipv4_type[BUFLEN_16];
	
   	char ipv6_wan_type[BUFLEN_16];
   
   	char ipv6_wan_staticaddr[BUFLEN_48];
   	char ipv6_wan_staticprefix[BUFLEN_48];
   	char ipv6_wan_staticgw[BUFLEN_48];
   	char ipv6_wan_staticdns[BUFLEN_256];
   	
   	char ipv6_wan_dhcpdnstype[BUFLEN_16];
   	
   	char ipv6_wan_pppaddrtype[BUFLEN_16];
   	char ipv6_wan_pppaddr[BUFLEN_48];
   	char ipv6_wan_pppprefix[BUFLEN_48];
   	char ipv6_wan_pppgw[BUFLEN_256];
   	
   	char ipv6_wan_6rdtype[BUFLEN_24];
   	char ipv6_wan_6rdaddr[BUFLEN_48];
   	int ipv6_wan_6rdaddrmask;
   	char ipv6_wan_6rdprefix[BUFLEN_48];
   	int ipv6_wan_6rdprefixmask;
   	int ipv6_wan_6rdmtu;
	   	
   	int ipv6_lan_type;
   	char ipv6_lan_ulaaddr[BUFLEN_48];
   	char ipv6_lan_ulaprefix[BUFLEN_48];
   	int ipv6_lan_subnet;
   	int ipv6_lan_statelessadvlifetime;
   	char ipv6_lan_statefulmin[BUFLEN_16];
   	char ipv6_lan_statefulmax[BUFLEN_16];
   	int ipv6_lan_statefuladdrlifetime;
   	
   	int ipv6_fwenable;
   	char ipv6_fwip[BUFLEN_16];
   	char ipv6_fwlevel[BUFLEN_16];
   	char ipv6_fwinvalue[BUFLEN_24];
   	char ipv6_fwoutvalue[BUFLEN_24];
   	
   	int ipv6_qosenable;
   	char ipv6_qosinterface[BUFLEN_16];
   	char ipv6_qosclassname[BUFLEN_32];
   	int ipv6_qosdscp;
	int ipv6_qospriority;
   	char ipv6_qossrcip[BUFLEN_48];
   	char ipv6_qossrcmask[BUFLEN_32];
   	int ipv6_qossrcport1;
   	int ipv6_qossrcport2;
   	char ipv6_qosdestip[BUFLEN_48];
   	char ipv6_qosdestmask[BUFLEN_32];
   	int ipv6_qosdestport1;
   	int ipv6_qosdestport2;
   	int ipv6_qosrmidx;

	char ipv6_hostName[BUFLEN_256];
	int  ipv6_pingNum;
    int  ipv6_packetSize;   
    int  ipv6_pingSign;    
    char ipv6_pingTestResult[BUFLEN_1024];
	char ipv6_interfaceName[BUFLEN_32];
	int  ipv6_tracerouteSign;
	char ipv6_tracerouteResult[BUFLEN_1024];
#endif    
#if defined(AEI_VDSL_CUSTOMER_TDS)
   char recvGlobalSessionKey[BUFLEN_16];  /**< Session key received from browser, used by httpd only */
#endif
#ifdef REVERSION_DUAL_LANG
   SINT32 langState;
   SINT32 indexChangeLang;
#endif
#ifdef SUPPORT_URLFILTER
   UBOOL8 enblUrlCfg;
#endif

} WEB_NTWK_VAR, *PWEB_NTWK_VAR;

#if defined(AEI_VDSL_CUSTOMER_NCS)
#define VLANMUX_DISABLE 1
#else
#define VLANMUX_DISABLE -1
#endif

#define VLANMUX_MATCH_ANY -2

/* TODO: no limit for pppoe and mer ? */
#define NUM_OF_ALLOWED_SVC_PPPOE 4
#define NUM_OF_ALLOWED_SVC_MER   1
#define NUM_OF_ALLOWED_SVC_BR    1

typedef struct {
   SINT32  vpi;
   SINT32  vci;
   SINT32  port;
   SINT32  conId;
} PORT_VPI_VCI_TUPLE, *PPORT_VPI_VCI_TUPLE;


typedef struct {
   char layer3Name[CMS_IFNAME_LENGTH];
   char protocolStr[BUFLEN_16];
} DIAG_ID, *PDIAG_ID;

/** Generic name list structure
 */

typedef struct _NameList
{
   char              *name;
   struct _NameList  *next;
} NameList;


#include "dal_wan.h"
#include "dal_dsl.h"
#include "dal_lan.h"


/** Fill in all fields in the WEB_NTWK_VAR.
 *
 * This function was formerly called BcmWeb_getAllInfo.
 *
 * @param (OUT) pointer to WEB_NTWK_VAR
 */
void cmsDal_getAllInfo(WEB_NTWK_VAR *webVar);


/** Get current NAT information.
 *
 * @param varValue   (IN) In not NULL, will be filled with "1" or "0"
 */
CmsRet cmsDal_getEnblNatForWeb(char *varValue);


/** Get current Full Cone NAT information.
 *
 * @param varValue   (IN) In not NULL, will be filled with "1" or "0"
 */
CmsRet cmsDal_getEnblFullconeForWeb(char *varValue);


/** This function returns the interface tr98 full path name from the interface linux name. 
 *
 * @param intfname   (IN) the interface linux name.
 * @param layer2     (IN) boolean to indicate whether intfname is a layer 2 or layer 3 interface name.
 * @param mdmPath    (OUT)the interface tr98 full path name. caller shall free the memory after used.
 * @return CmsRet         enum.
 */
CmsRet cmsDal_intfnameToFullPath(const char *intfname, UBOOL8 layer2, char **mdmPath);

/** This function returns the interface linux name from the interface tr98 full path name.
 *
 * @param mdmPath    (IN) the interface tr98 full path name.
 * @param intfname   (OUT)the interface linux name. caller shall have allocated memory for it.
 * @return CmsRet         enum.
 */
CmsRet cmsDal_fullPathToIntfname(const char *mdmPath, char *intfname);


/** Get the current syslog config info from MDM and fill in appropriate
 *  fields in webVar.
 *
 * @param (OUT) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_getCurrentSyslogCfg(WEB_NTWK_VAR *webVar);


/** Call appropriate cmsObj_set function to set the syslog config info.
 *
 * @param (IN) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_setSyslogCfg(const WEB_NTWK_VAR *webVar);


/** Get the current login info from MDM and fill in appropriate
 *  fields in webVar.
 *
 * @param (OUT) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_getCurrentLoginCfg(WEB_NTWK_VAR *webVar);

#if defined(AEI_VDSL_CUSTOMER_NCS)
CmsRet cmsDal_getCurrentSshdCfg(WEB_NTWK_VAR *webVar);
CmsRet cmsDal_getCurrentHttpdCfg(WEB_NTWK_VAR *webVar);
CmsRet cmsDal_getCurrentTelnetdCfg(WEB_NTWK_VAR *webVar);
CmsRet cmsDal_setHttpdCfg(const WEB_NTWK_VAR *webVar);
CmsRet cmsDal_setTelnetdCfg(const WEB_NTWK_VAR *webVar);
CmsRet cmsDal_getCurrentTelnetdCfg_timeoutValue(int *timeout, UINT32 lockTimeoutMs);
CmsRet cmsDal_getCurrentHttpdCfg_timeoutValue(int *timeout, UINT32 lockTimeoutMs);
CmsRet dalDsl_deleteAtmInterfaceByName(char *l2infName); 
CmsRet dalDsl_deletePtmInterfaceByName(char *l2infName);
UBOOL8 dalWan_getEthintfByIfName(char *ifName, InstanceIdStack *iidStack);
#endif

#ifdef AEI_VDSL_TR098_TELUS
UBOOL8 cmsDal_checkSshAccess(UINT32 lockTimeoutMs, UINT32 accessMode);
#endif

/** Call appropriate cmsObj_set function to set the loginCfg related info.
 *
 * @param (IN) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_setLoginCfg(const WEB_NTWK_VAR *webVar);


/** Call appropriate cmsObj_set function to set the tr69c related info.
 *
 * @param (IN) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_setTr69cCfg(const WEB_NTWK_VAR *webVar);


/** Get the default gateway interface list to lay3BridgingObject
 * 
 * @param gwList (IN) The linux user friendly wan interface name list.
 * 
 * @return CmsRet enum.
 */
CmsRet dalRt_getDefaultGatewayList(char *gwIfNames);



/** Get the active default gateway interface (the one is used in the system)
 * 
 * @param varValue (OUT) The linux user friendly wan interface name.
 * 
 * @return CmsRet enum.
 */
CmsRet dalRt_getActiveDefaultGateway(char *varValue);


/** Set the default gateway interface list to lay3BridgingObject
 * 
 * @param gwList (IN) The linux user friendly wan interface name list.
 * 
 * @return CmsRet enum.
 */
CmsRet dalRt_setDefaultGatewayList(const char *gwList);

/** add a static route entry.
 *
 * @param addr      (IN) destination IP address
 * @param mask     (IN) destination subnet mask
 * @param gateway (IN) default gateway IP address of this static route
 * @param wanif     (IN) default interface of this static route
 * @param metric  (IN) hop number to the destination.  Pass in an empty
 *                     string to let the kernel set the metric, otherwise,
 *                     pass in an integer, in the form of a string, to
 *                     specify the metric.
 *
 * @return CmsRet enum.
 */
CmsRet dalStaticRoute_addEntry(const char* addr, const char *mask, const char *gateway, const char *wanif, const char *metric);


/** delete a static route entry.
 *
 * @param addr      (IN) destination IP address
 * @param mask     (IN) destination subnet mask
 *
 * @return CmsRet enum.
 **/
CmsRet dalStaticRoute_deleteEntry(const char* addr, const char *mask);
CmsRet dalStaticRoute_deleteEntryByGW(const char* addr, const char *mask, const char *gw);


/** add a virtual server entry.
 *
 * @param dstWanIf (IN) virtual server of which WAN interface
 * @param srvName (IN) virtual server service name
 * @param srvAddr  (IN) virtual server IP address
 * @param protocol  (IN) virtual server service protocol
 * @param EPS        (IN) virtual server external port start
 * @param EPE        (IN) virtual server external port end
 * @param IPS        (IN) virtual server internal port start
 * @param IPE        (IN) virtual server internal port end
 *
 * @return CmsRet enum.
 */
#if defined(AEI_VDSL_CUSTOMER_NCS)
CmsRet dalVirtualServer_addEntry(const char *dstWanIf, const char *srvName, const char *srvAddr, const char *protocol, const UINT16 EPS, const UINT16 EPE, const UINT16 IPS, const UINT16 IPE, const UBOOL8 isAddbyGUI);
#else
CmsRet dalVirtualServer_addEntry(const char *dstWanIf, const char *srvName, const char *srvAddr, const char *protocol, const UINT16 EPS, const UINT16 EPE, const UINT16 IPS, const UINT16 IPE);
#endif


#if defined(AEI_VDSL_CUSTOMER_NCS)
CmsRet dalVirtualServer_deletePortForwardingEntry(const char * srvAddr, const char * protocol, const UINT16 EPS, const UINT16 EPE, const UINT16 IPS, const UINT16 IPE, const char *srvWanAddr);
#endif

/** deleter a virtual server entry.
 *
 * @param srvAddr  (IN) virtual server IP address
 * @param protocol  (IN) virtual server service protocol
 * @param EPS        (IN) virtual server external port start
 * @param EPE        (IN) virtual server external port end
 * @param IPS        (IN) virtual server internal port start
 * @param IPE        (IN) virtual server internal port end
 *
 * @return CmsRet enum.
 */
CmsRet dalVirtualServer_deleteEntry(const char * srvAddr, const char * protocol, const UINT16 EPS, const UINT16 EPE, const UINT16 IPS, const UINT16 IPE);


/** add a Dmz host entry.
 *
 * @param addr      (IN) DMZ host IP address
 *
 * @return CmsRet enum.
 */
CmsRet dalDmzHost_addEntry(const char *srvAddr);

#ifdef AEI_VDSL_CUSTOMER_SASKTEL
/** add a Dmz host entry.
 *
 * @param addr      (IN) DMZ host IP address
 *
 * @param mac      (IN) DMZ host mac address
 *
 * @return CmsRet enum.
 */
CmsRet AEI_dalDmzHost_addEntry(const char *ipaddr,const char *mac,const char *dmzPersist,const WEB_NTWK_VAR *webVar);

#endif

#if defined(AEI_VDSL_CUSTOMER_TELUS)
/** add a Dmz host entry.
 *
 * @param addr      (IN) DMZ host IP address
 *
 * @return CmsRet enum.
 */
CmsRet dalDmzHost_addEntryEx(const char *srvAddr,const char *dmzPersist);
#endif

/** get Dmz host information.
 *
 * @param address      (IN) IP address
 *
 * @return CmsRet enum.
 */
CmsRet dalGetDmzHost(char *address);


/** add a port triggering entry.
 *
 * @param dstWanIf (IN) port triggering of which WAN interface
 * @param appName (IN) port triggering application name
 * @param tProto  (IN) trigger protocol
 * @param oProto  (IN) open protocol
 * @param tPS        (IN) trigger port start
 * @param tPE        (IN) trigger port end
 * @param oPS        (IN) open port start
 * @param oPE        (IN) open port end
 *
 * @return CmsRet enum.
 */
CmsRet dalPortTriggering_addEntry(const char *dstWanIf, const char *appName,
   const char *tProto, const char *oProto, const UINT16 tPS,
   const UINT16 tPE, const UINT16 oPS, const UINT16 oPE);


/** add a port triggering entry.
 *
 * @param dstWanIf (IN) port triggering of which WAN interface
 * @param tProto  (IN) trigger protocol
 * @param tPS        (IN) trigger port start
 * @param tPE        (IN) trigger port end
 *
 * @return CmsRet enum.
 */
CmsRet dalPortTriggering_deleteEntry(const char *dstWanIf, const char *tProto,
   const UINT16 tPS, const UINT16 tPE);


/** add a IP filter in entry.
 *
 * @param name          (IN) IP filter name
 * @param ipver         (IN) IP filter ip version
 * @param protocol      (IN) IP filter protocol
 * @param srcAddr       (IN) IP filter source IP address
 * @param srcMask      (IN) IP filter source netmask
 * @param srcPort        (IN) IP filter source port number
 * @param dstAddr       (IN) IP filter destination IP address
 * @param dstMask      (IN) IP filter destination netmask
 * @param dstPort        (IN) IP filter destination port number
 * @param ifName        (IN) wan interface for IP filter in rule
 *
 * @return CmsRet enum.
 */
CmsRet dalSec_addIpFilterIn(const char *name, const char *ipver, const char *protocol,
                            const char *srcAddr, const char *srcMask, const char *srcPort, 
                            const char *dstAddr, const char *dstMask, const char *dstPort, const char *ifName);


/** delete a IP filter in entry.
 *
 * @param name      (IN) IP filter name
 *
 * @return CmsRet enum.
 **/
CmsRet dalSec_deleteIpFilterIn(const char* name);


/** add a IP filter out entry.
 *
 * @param name          (IN) IP filter name
 * @param ipver         (IN) IP filter ip version
 * @param protocol      (IN) IP filter protocol
 * @param srcAddr       (IN) IP filter source IP address
 * @param srcMask      (IN) IP filter source netmask
 * @param srcPort        (IN) IP filter source port number
 * @param dstAddr       (IN) IP filter destination IP address
 * @param dstMask      (IN) IP filter destination netmask
 * @param dstPort        (IN) IP filter destination port number
 * @param ifName        (IN) wan interface for IP filter out rule
 *
 * @return CmsRet enum.
 */
CmsRet dalSec_addIpFilterOut(const char *name, const char *ipver, const char *protocol,
                             const char *srcAddr, const char *srcMask, const char *srcPort, 
                             const char *dstAddr, const char *dstMask, const char *dstPort, const char *ifName);


/** delete a IP filter out entry.
 *
 * @param name      (IN) IP filter name
 *
 * @return CmsRet enum.
 **/
CmsRet dalSec_deleteIpFilterOut(const char* name);

/** Add an IPSec tunnel entry.
 *
 * @param name      (IN) pWebVar
 *
 * @return CmsRet enum.
 **/
CmsRet dalIPSec_addTunnel(const PWEB_NTWK_VAR pWebVar);

/** delete an IPSec tunnel entry.
 *
 * @param name      (IN) IPSec tunnel name
 *
 * @return CmsRet enum.
 **/
CmsRet dalIPSec_deleteTunnel(const char* name);

/** add a Mac filter entry.
 *
 * @param protocol     (IN) Mac filter protocol
 * @param srcMac       (IN) Mac filter source Mac address
 * @param dstMac       (IN) Mac filter destination Mac address
 * @param direction     (IN) Mac filter direction
 * @param ifName       (IN) wan interface for Mac filter rule
 *
 * @return CmsRet enum.
 */
CmsRet dalSec_addMacFilter(const char *protocol, const char *srcMac, const char *dstMac, const char *direction, const char *ifName);


/** delete a Mac filter entry.
 *
 * @param protocol     (IN) Mac filter protocol
 * @param srcMac       (IN) Mac filter source Mac address
 * @param dstMac       (IN) Mac filter destination Mac address
 * @param direction     (IN) Mac filter direction
 * @param ifName       (IN) wan interface for Mac filter rule
 *
 * @return CmsRet enum.
 **/
CmsRet dalSec_deleteMacFilter(const char *protocol, const char *srcMac, const char *dstMac, const char *direction, const char *ifName);


/** change a Mac filter policy.
 *
 * @param ifName      (IN)
 *
 * @return CmsRet enum.
 **/
CmsRet dalSec_ChangeMacFilterPolicy(const char *ifName);


/** configure Dns proxy.
 *
 * @param enable          (IN) 
 * @param hostname      (IN) 
 * @param domainname (IN) 
 *
 * @return CmsRet enum.
 */
CmsRet dalDnsProxyCfg(const char *enable, const char *hostname, const char *domainname);


/** get Dns proxy information.
 *
 * @param info      (IN) 
 *
 * @return CmsRet enum.
 */
CmsRet dalGetDnsProxy(char *info);


/** add a access time restriction entry.
 *
 * @param username (IN) service name
 * @param mac         (IN) block mac address
 * @param days        (IN) block days
 * @param starttime  (IN) block start time
 * @param endtime   (IN) block end time
 *
 * @return CmsRet enum.
 */
CmsRet dalAccessTimeRestriction_addEntry(const char *username, const char *mac, const unsigned char days, const unsigned short int starttime, const unsigned short int endtime);


/** deleter a access time restriction entry.
 *
 * @param username  (IN) delete service name
 *
 * @return CmsRet enum.
 */
CmsRet dalAccessTimeRestriction_deleteEntry(const char *username);


/** add a url filter list entry.
 *
 * @param url_address  (IN) add list url address
 * @param url_port       (IN) add list port number
 *
 * @return CmsRet enum.
 */
#if defined AEI_VDSL_CUSTOMER_QWEST
CmsRet dalUrlFilter_addEntry(const char* url_address, const UINT32 url_port, const char* lanIP, const char* lanPcName);
#else
CmsRet dalUrlFilter_addEntry(const char* url_address, const UINT32 url_port);
#endif


/** deleter a url filter list entry.
 *
 * @param url_address  (IN) delete url address
 *
 * @return CmsRet enum.
 */
#if defined AEI_VDSL_CUSTOMER_QWEST
CmsRet dalUrlFilter_removeEntry(const char* url_address, const char* lanIP);
#else
CmsRet dalUrlFilter_removeEntry(const char* url_address);
#endif


/** set the type of the url filter list.
 *
 * @param type   (IN) type of url list
 *
 * @return CmsRet enum.
 */
CmsRet dalUrlFilter_setType(const char *type);


/** obtain the type of the url filter list.
 *
 * @param type   (IN) 
 *
 * @return CmsRet enum.
 */
CmsRet dalUrlFilter_getType(const char *type);


/** Get the network access mode of an application.
 *
 *  This function also checks with the AppCfg. object to see if network
 *  access from LAN/WAN is allowed for this app.
 *
 * This interface requires the caller to obtain the cms lock
 *
 * @param eid  (IN) CMS entity id that is trying to access the modem.
 * @param addr (IN) IPv4 address of the entity that is trying to access the modem.
 *
 * @return enum of access modes.
 */
NetworkAccessMode cmsDal_getNetworkAccessMode(CmsEntityId eid, const char *addr);

/** Authenticate given username and password.
 *
 * The algorithm works as follows:
 * If network access mode is LAN_SIDE or CONSOLE, validate against admin or user
 * If network access is from WAN_SIDE, validate against support.
 *
 * This interface requires the caller to obtain the cms lock
 *
 * @param accessMode    (IN) the network access mode.
 * @param username      (IN) username
 * @param passwd        (IN) password
 * @param lockTimeoutMs (IN) Amount of time in milliseconds to wait for the MDM lock.
 *
 * @return TRUE if user is authenticated.
 */
UBOOL8 cmsDal_authenticate(NetworkAccessMode accessMode,
                         const char *username,
                         const char *passwd);

/** Set current primary and secondary DNS server names for ppp ip extension only.
 *
 * @param wanIp         (IN)  ppp wan ip
 * @param dnsServerList (IN)  dns server list
 *
 * @return CmsRet enum.
 *
 */
CmsRet cmsDal_setDhcpRelayConfig(const char *wanIp,  const char *dnsServerList);


/** Find the Layer 2 bridging entry object with the given bridge key.
 * 
 * @param bridgeKey (IN) bridge key.
 * @param iidStack (OUT) If found, this iidStack will be filled out with
 *                       the instance info for the found obj.
 * @param bridgeObj (OUT) If found, this pointer will be set to the
 *                        L2BridgingEntryObject.  Caller is responsible for
 *                        freeing this object.
 * 
 * @return CmsRet enum.
 */
CmsRet dalPMap_getBridgeByKey(UINT32 bridgeKey,
                              InstanceIdStack *iidStack,
                              L2BridgingEntryObject **bridgeObj);
                  
                               
/** Get the bridge key of the specified bridge.
 * 
 * @param bridgeName (IN) name of the bridge
 * @param bridgeKey (OUT) If the bridge is found, the bridge key of the bridge
 *                        is set in this variable.
 * @return CmsRet enum.
 */
CmsRet dalPMap_getBridgeKey(const char *bridgeName, UINT32 *bridgeKey);


/** Add a new layer 2 bridge entry and enable it.
 * 
 * @param bridgeName (IN) The name of the bridge, which is also the LAN ports group name.
 * 
 * @return CmsRet enum.
 */
CmsRet dalPMap_addBridge(const char *bridgeName);


/** Remove the Layer2BridgingEntry object in the MDM.
 * 
 * @param bridgeName (IN) The name of the bridge.
 * 
 */
void dalPMap_deleteBridge(const char *bridgeName);


/** Find the Layer 2 bridging filter interface object with the given name.
 * 
 * @param ifName (IN) name of the filter interface.
 * @param iidStack (OUT) If found, this iidStack will be filled out with
 *                       the instance info for the found obj.
 * @param filterIntfObj (OUT) If found, this pointer will be set to the
 *                L2BridgingFilterObject.  The caller is responsible for
 *                freeing this object.
 * 
 * @return CmsRet enum.
 */
CmsRet dalPMap_getFilterIntf(const char *ifName, InstanceIdStack *iidStack, L2BridgingFilterObject **filterObj);


/** Find the Layer 2 bridging filter object associated with the specified
 *  bridge and uses a DHCP Vendor Id filter.
 *
 * We assume there is only 1 such filter associated with each bridge.
 * 
 * @param ifName (IN) name of the filter interface.
 * @param iidStack (OUT) If found, this iidStack will be filled out with
 *                       the instance info for the found obj.
 * 
 * @param filterIntfObj (OUT) If found, this pointer will be set to the
 *                L2BridgingFilterObject.  The caller is responsible for
 *                freeing this object.
 * 
 * @return CmsRet enum.
 */
CmsRet dalPMap_getFilterDhcpVendorIdByBridgeName(const char *ifName, InstanceIdStack *iidStack, L2BridgingFilterObject **filterObj);



/** Create a new Layer 2 bridging filter object that is associated with the
 *  specified bridge and use the specified aggregate DHCP Vendor id string.
 *
 * @param bridgeName (IN) Name of the bridge which this filter will belong to.
 * @param aggregateString (IN) Aggregate DHCP Vendor Id string as returned by
 *                             cmsUtl_getAggregateStringFromDhcpVendorIds().
 *
 * @return CmsRet enum.
 */
CmsRet dalPMap_addFilterDhcpVendorId(const char *bridgeName, const char *aggregateString);


/** Delete the Layer 2 bridging filter object that is associated with the
 *  specified bridge and uses DHCP Vendor id string.
 *
 * We assume there is only 1 filter per bridge that uses the DHCP Vendor Id string,
 * so by supplying only the bridge name, we should be able to find the filter.
 *
 * @param bridgeName (IN) Name of the bridge which the filter belongs to.
 *
 */
void dalPMap_deleteFilterDhcpVendorId(const char *bridgeName);


/** Associate an Linux interface with a bridge by seting the Layer2BridgingFilter->filterBridgeReference field.
 *
 * @param ifName (IN) The Linux interface name of the interface to associate
 *                    with the specified bridge.
 * 
 * @param grpName (IN) The user fiendly bridge name, aka interface group name.
 *                     In the Layer2BridgingEntry->bridgeName.  As a special
 *                     case, if grpName is NULL, then this interface is
 *                     disassociated from all bridges.
 * 
 * 
 * @return CmsRet enum.
 */
CmsRet dalPMap_assocFilterIntfToBridge(const char *ifName, const char *grpName);


/** Disassociate all filter interfaces from the specified bridge and associate them
 * with the Default bridge.  
 * 
 * @param bridgeName (IN) The bridge name (cannot be the default bridge).
 * 
 * @return CmsRet enum.
 */
CmsRet dalPMap_disassocAllFilterIntfFromBridge(const char *bridgeName);


/** Find the Layer 2 bridging available interface object with the specified
 *  InterfaceReference parameter.
 * 
 * @param availInterfaceReference (IN) Value of the InterfaceReference
 *                       parameter in the AvailableInterface object. 
 * @param iidStack (OUT) If found, this iidStack will be filled out with
 *                       the instance info for the found obj.
 * @param filterIntfObj (OUT) If found, this pointer will be set to the
 *                L2BridgingIntfObject.  The caller is responsible for freeing
 *                the object.
 * 
 * @return CmsRet enum.
 */
CmsRet dalPMap_getAvailableInterfaceByRef(const char *availInterfaceReference, InstanceIdStack *iidStack, L2BridgingIntfObject **availIntfObj);


/** Find the Layer 2 bridging available interface object with the specified
 *  AvailableInterfaceKey.
 * 
 * @param availInterfaceKey (IN) Value of the AvailableInterfaceKey
 *                       parameter in the AvailableInterface object. 
 * @param iidStack (OUT) If found, this iidStack will be filled out with
 *                       the instance info for the found obj.
 * @param availIntfObj (OUT) If found, this pointer will be set to the
 *                L2BridgingIntfObject.  The caller is responsible for freeing
 *                the object.
 * 
 * @return CmsRet enum.
 */
CmsRet dalPMap_getAvailableInterfaceByKey(UINT32 availableInterfaceKey, InstanceIdStack *iidStack, L2BridgingIntfObject **availIntfObj);


/** Given a WAN IfName, get the full param name in the format required
 * by the InterfaceReference parameter name of the AvailableInterface object.
 * 
 * @param ifName (IN) name of the available interface.
 * @param availInterfaceReference (OUT) This must be a buffer of 256 bytes long.
 *                    On success, this buffer will be filled with the full param name.
 * 
 * @return CmsRet enum.
 */
CmsRet dalPMap_wanIfNameToAvailableInterfaceReference(const char *ifName, char *availInterfaceReference);


/** Given a LAN IfName, get the full param name in the format required
 * by the InterfaceReference parameter name of the AvailableInterface object.
 * 
 * @param ifName (IN) name of the available interface.
 * @param availInterfaceReference (OUT) This must be a buffer of 256 bytes long.
 *                    On success, this buffer will be filled with the full param name.
 * 
 * @return CmsRet enum.
 */
CmsRet dalPMap_lanIfNameToAvailableInterfaceReference(const char *ifName, char *availInterfaceReference);


/** Convert the InterfaceReference parameter in the available interface object
 *  to the Linux interface name.
 * 
 * @param availableInterfaceReference (IN) The interface reference parameter value 
 *               in the available interface object.
 * @param ifName (OUT) This buffer must be at least 32 bytes long.  On successful
 *               return, this will contain the Linux ifName associated with
 *               the availableInterfaceReference.
 * 
 * @return CmsRet enum
 */
CmsRet dalPMap_availableInterfaceReferenceToIfName(const char *availableInterfaceReference, char *ifName);


/** Enable or disable virtual ports on the ethernet switch.
 *
 * @param cfgStatus (IN) TRUE or FALSE for enable or disable.
 *
 */
void dalEsw_enableVirtualPorts(UBOOL8 cfgStatus);


/** Read eterhnet switch info and put it into glbWebVar
 *
 * @param (OUT) pointer to the webvar struct to fill out.
 */
CmsRet dalEsw_getEthernetSwitchInfo(WEB_NTWK_VAR *webVar);




/** Set the upnp enable flag if it has been changed.
 * @param enableUpnp    (IN)  The new upnp enable value
 * @param *outNeedSave  (OUT) If a new value is set, need to set to TRUE for saving to flash.
 *
 * @return CmsRet enum.
 */
//add by vincent 2010-12-10
#if defined(AEI_VDSL_CUSTOMER_QWEST) || defined(AEI_VDSL_CUSTOMER_TELUS)
CmsRet dalUpnp_config(UBOOL8 enableUpnp, UBOOL8 enableUpnpnat, UBOOL8 *outNeedSave);
#else
CmsRet dalUpnp_config(UBOOL8 enableUpnp, UBOOL8 *outNeedSave);
#endif
/** add a dynamic dns entry.
 *
 * @param fullyQualifiedDomainName (IN) fully qualified domain name
 * @param userName       (IN) user name
 * @param password       (IN) user password
 * @param interface      (IN) interface
 * @param providerName   (IN) provider name
 *
 * @return CmsRet enum.
 */

CmsRet dalDDns_addEntry(const char *fullyQualifiedDomainName, const char *userName, const char *password, const char *interface, unsigned short int providerName);



/** delete a dynamic dns entry.
 *
 * @param fullyQualifiedDomainName (IN) fully qualified domain name
 *
 * @return CmsRet enum.
 */
CmsRet dalDDns_deleteEntry(const char *fullyQualifiedDomainName);


/** Get the system active dns ip addresses. 
 * 
 * @param dns1       (OUT) dns1 (primary) ip address
 * @param dns2       (OUT) dns2 (secondary) ip address
 *
 * @return void.
 */
void dalNtwk_getActiveDnsIp(char *dns1, char *dns2);


/** Get the system NetworkCofigObj for system dns info.  Only one parameter will be
 * filled since the system is either use WAN interface list or static dns ip. ie.
 * one parameter will be NULL.
 * 
 * @param dnsIfNameList    (IN) wan Interface list (seperated by ',')for dns
 * @param dnsServers       (IN) static dns ip (seperated by ',').
 *
 * @return CmsRet enum.
 */
CmsRet dalNtwk_getSystemDns(char *dnsIfNameList, char *dnsServers);


/** Set the system NetworkCofigObj for system dns info.  Only one parameter will be
 * used since the system is either use WAN interface list or static dns ip. ie.
 * one parameter has to be NULL.
 * 
 * @param dnsIfNameList    (IN) wan Interface list (seperated by ',')for dns
 * @param dnsServers       (IN) static dns ip (seperated by ',').
 *
 * @return CmsRet enum.
 */
CmsRet dalNtwk_setSystemDns(const char *dnsIfNameList, const char *dnsServers);


#ifdef DMP_X_BROADCOM_COM_IPV6_1 /* aka SUPPORT_IPV6 */

/** add an IPv6 static route entry.
 *
 * @param addr    (IN) destination IPv6 address with subnet prefix length in CIDR notation
 * @param gateway (IN) gateway IPv6 address of this static route
 * @param wanif   (IN) default interface of this static route
 * @param metric  (IN) hop number to the destination
 *
 * @return CmsRet enum.
 */
CmsRet dalStaticRoute6_addEntry(const char* addr, const char *gateway, const char *wanif, const char *metric);


/** delete an IPv6 static route entry.
 *
 * @param addr      (IN) destination IPv6 address with subnet prefix length in CIDR notation
 *
 * @return CmsRet enum.
 **/
CmsRet dalStaticRoute6_deleteEntry(const char* addr);


/** Get the IPv6 address of a LAN interface.
 * 
 * @param ifname     (IN)  The LAN interface name.
 * @param addr       (OUT) The IPv6 address.
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_getLanAddr6(const char *ifname, char *addr);


/** Get the IPv6LANHostConfigManagement object for ipv6 dns info
 * 
 * @param dns6Type   (OUT) The system ipv6 dns server type, either static or dhcp.
 * @param dns6Ifc    (OUT) The broadcom interface name
 * @param dns6Pri    (OUT) The primary dns server, either statically specified or dhcp assigned.
 * @param dns6Sec    (OUT) The secondary dns server, either statically specified or dhcp assigned.
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_getDns6Info(char *dns6Type, char *dns6Ifc, char *dns6Pri, char *dns6Sec);


/** Set the IPv6LANHostConfigManagement object for ipv6 dns info
 * 
 * @param dns6Type   (IN) The system ipv6 dns server type, either static or dhcp.
 * @param dns6Ifc    (IN) The broadcom interface name 
 * @param dns6Pri    (IN) The static primary dns server when dns6Type is static.
 * @param dns6Sec    (IN) The static secondary dns server when dns6Type is static.
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_setDns6Info(char *dns6Type, char *dns6Ifc, char *dns6Pri, char *dns6Sec);


/** Set the IPv6LANHostConfigManagement object for ipv6 site prefix info
 * 
 * @param sitePrefixType (OUT) The site prefix type, either static or delegated.
 * @param pdWanIfc (OUT) Full path name of the wan interface selected for acquiring site prefix.
 * @param sitePrefix (OUT) The site prefix, either statically specified or dynamically delegated.
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_getSitePrefixInfo(char *sitePrefixType, char *pdWanIfc, char *sitePrefix);


/** Set the IPv6LANHostConfigManagement object for ipv6 site prefix info
 * 
 * @param sitePrefixType (IN) The site prefix type, either static or delegated.
 * @param pdWanIfc (IN) Full path name of the wan interface selected for acquiring site prefix.
 * @param sitePrefix (IN) The site prefix, when sitePrefixType is static.
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_setSitePrefixInfo(char *sitePrefixType, char *pdWanIfc, char *sitePrefix);


/** Get the IPv6L3Forwarding object for system ipv6 default gateway interface.
 * 
 * @param gwIfc   (OUT) The gateway interface name.
 * @param gw      (OUT) The gateway address.
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_getSysDfltGw6(char *gwIfc, char *gw);


/** Set the IPv6L3Forwarding object for system ipv6 default gateway interface.
 * 
 * @param gwIfc      (IN) The gateway interface name.
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_setSysDfltGw6Ifc(char *gw6Ifc);

#endif   /* DMP_X_BROADCOM_COM_IPV6_1 aka SUPPORT_IPV6 */

/** Set the ripd after WEB configuration change
 *
 * @param pIfcName    (IN) WAN interface name
 * @param pRipVer     (IN) rip version (0= Off, 1 = V1, 2=V2, 3=V1V2)
 * @param pOperation  (IN) operation mode (0= ACTIVE, 1 = PASSIVE)
 * @param pEnabled    (IN) 0 = Off, 1 = ON
 *
 */
CmsRet dalRip_setRipEntry(char *pIfcName, char *pRipVer, char *pOperation, char *pEnabled);


#ifdef SUPPORT_IPP
/** Get the IppCfgObj for print server info.
 * 
 * @param ippEnabled   (OUT) print server enable boolean.
 * @param ippMake      (OUT) the printer make.
 * @param ippName      (OUT) the printer name.
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_getPrintServerInfo(char *ippEnabled, char *ippMake, char *ippName);
#endif

/** Get the DmsCfgObj for digital media server info.
 * 
 * @param dmsEnabled   (OUT) digital media server enable boolean.
 * @param dmsMediaPath (OUT) location of media files.
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_getDigitalMediaServerInfo(char *dmsEnabled, char *dmsMediaPath, char *dmsBrName);

/** Get available interface for WAN service.
 * 
 * @param ifList    (OUT) the list of available interface.
 * @param skipUsed  (IN) If FALSE, get all the layer 2 interface even if it 
 *                       and in DEFAULT_CONNECTON_MODE.

 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_getAvailableIfForWanService(NameList **ifList, UBOOL8 skipUsed);


/** Get available L2 Eth interface 
 * 
 * @param ifList  (OUT) the list of available interface.
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_getAvailableL2EthIntf(NameList **ifList);


/** if device has ETH WAN  
 * 
 * @param none
 *
 * @return UBOOL8 indicating whether ETH wan be set.
 */
UBOOL8 dalEth_isEthWanMode();


/** if device has DSL wan service  
 * 
 * @param none
 *
 * @return UBOOL8 indicating whether DSL wan service be set.
 */
UBOOL8 dalEth_hasDslWanService();


/* Get WanEthIntfObject by ETH wan interface name
 * 
 * @param ifname     (IN)  The LAN interface name.
 * @param iidStack (OUT) iidStack of the ethIntfCfg object found.
 * @param ethIntfCfg (OUT) if not null, this will contain a pointer to the found
 *                         ethIntfCfg object.  Caller is responsible for calling
 *                         cmsObj_free() on this object.
 *
 * @return UBOOL8 indicating whether the desired ethIntfCfg object was found.
 */
UBOOL8 dalEth_getEthIntfByIfName(char *ifName, InstanceIdStack *iidStack, WanEthIntfObject **ethIntfCfg);


/** Add the WanEthIntfObject object
 *
 * @param (IN) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
CmsRet dalEth_addEthInterface(const WEB_NTWK_VAR *webVar);


/** Delete the WanEthIntfObject object
 *
 * @param (IN) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
CmsRet dalEth_deleteEthInterface(const WEB_NTWK_VAR *webVar);



/** Free memory allocated for name list.
 * 
 * @param nl  (IN) the name list.
 */
void cmsDal_freeNameList(NameList *nl);


/** Add the WanMocaIntfObject object
 *
 * @param (IN) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
CmsRet dalMoca_addMocaInterface(const WEB_NTWK_VAR *webVar);


/** Delete the WanMocaIntfObject object
 *
 * @param (IN) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
CmsRet dalMoca_deleteMocaInterface(const WEB_NTWK_VAR *webVar);


/** Start the MoCA Core if it's currently stopped
 *
 * @param ifName (IN) pointer MoCA interface name string
 * @param newObj (IN) pointer to MoCA object with new init parameters
 * @param initMask (IN) bitmask of fields to set from newObj 
 *
 * @return CmsRet enum.
 */
CmsRet dalMoca_start ( 
   char * ifName, 
   LanMocaIntfObject *newObj, 
   UINT64 initMask );


/** Stop the MoCA Core if it's currently started
 *
 * @param ifName (IN) pointer MoCA interface name string
 *
 * @return CmsRet enum.
 */
CmsRet dalMoca_stop ( char * ifName );


/** Restart the MoCA Core
 *
 * @param ifName (IN) pointer MoCA interface name string
 * @param newObj (IN) pointer to MoCA object with new init parameters
 * @param reinitMask (IN) bitmask of fields to set from newObj 
 *
 * @return CmsRet enum.
 */
CmsRet dalMoca_restart ( char * ifName, LanMocaIntfObject * newObj, UINT64 reinitMask );

/** Configure MoCA operating parameters
 *
 * @param ifName (IN) pointer MoCA interface name string
 * @param newObj (IN) pointer to MoCA object with new parameters
 *
 * @return CmsRet enum.
 */
CmsRet dalMoca_setConfig ( char * ifName, LanMocaIntfObject * newObj );


/** Configure MoCA tracing level.
 *
 * @param ifName (IN) pointer MoCA interface name string
 * @param newObj (IN) pointer to MoCA object with new parameters
 *
 * @return CmsRet enum.
 */
CmsRet dalMoca_setTrace ( char * ifName, LanMocaIntfObject * newObj );

/** Enable MoCA interface
 *
 * @param ifName (IN) pointer MoCA interface name string
 *
 * @return CmsRet enum.
 */
CmsRet dalMoca_enable ( char * ifName );


/** Disable MoCA interface
 *
 * @param ifName (IN) pointer MoCA interface name string
 * @param newObj (IN) pointer to MoCA object with new init parameters
 * @param reinitMask (IN) bitmask of fields to set from newObj 
 *
 * @return CmsRet enum.
 */
CmsRet dalMoca_disable ( char * ifName );

/** Get moca configuration object
 *
 * @param ifName (IN) pointer MoCA interface name string
 * @param lanMocaObj (OUT) pointer to MoCA object with updated parameters
 *
 * @return CmsRet enum.
 */
CmsRet dalMoca_getConfig(const char * ifName, LanMocaIntfObject ** lanMocaObj );

/** Get moca privacy setting
 *
 * @param ifName (IN) pointer MoCA interface name string
 * @param varValue (OUT) pointer to MoCA privacy setting string
 *
 * @return None.
 */
void dalMoca_getPrivacy(const char *ifName, char *varValue);

/** Get moca password setting
 *
 * @param ifName (IN) pointer MoCA interface name string
 * @param varValue (OUT) pointer to MoCA password string
 *
 * @return None.
 */
void dalMoca_getPassword(const char *ifName, char *varValue);

/** Get moca last operating frequency setting
 *
 * @param ifName (IN) pointer MoCA interface name string
 * @param varValue (OUT) pointer to MoCA LOF string
 *
 * @return None.
 */
void dalMoca_getFrequency(const char *ifName, char *varValue);

/** Get moca auto network search enable setting
 *
 * @param ifName (IN) pointer MoCA interface name string
 * @param varValue (OUT) pointer to MoCA auto network search enable string
 *
 * @return None.
 */
void dalMoca_getAutoScan(const char *ifName, char *varValue);

/** Set moca configuration parameters from the httpd application
 *
 * @param webData (IN) pointer to global web data structure
 *
 * @return CmsRet enum.
 */
CmsRet dalMoca_setWebParams(WEB_NTWK_VAR * webData );

/** Get current moca configuration parameters for the httpd application
 *
 * @param webData (IN/OUT) pointer to global web data structure
 *
 * @return CmsRet enum.
 */
CmsRet dalMoca_getCurrentCfg(WEB_NTWK_VAR * webData );

/** Get available L2 Moca interfaces
 * 
 * @param ifList  (OUT) the list of available interface.
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_getAvailableL2MocaIntf(NameList **ifList);


/* Get WanMocaIntfObject by interface name
 * 
 * @param ifname     (IN)  The WAN moca interface name.
 * @param iidStack  (OUT) iidStack of the WAN mocaIntf object found.
 * @param mocaIntfCfg (OUT) if not null, this will contain a pointer to the found
 *                         mocaIntfCfg object.  Caller is responsible for calling
 *                         cmsObj_free() on this object.
 *
 * @return UBOOL8 indicating whether the desired mocaIntfCfg object was found.
 */
UBOOL8 dalMoca_getWanMocaIntfByIfName(const char *ifName, InstanceIdStack *iidStack, WanMocaIntfObject **ethIntfCfg);



#ifdef SUPPORT_CERT

/*
 * A lot of the defs here really do not belong this high level 
 * common header file.  They should get moved closer to where they
 * are used.  Often, these defs are not even shared.
 */

#define CERT_LOCAL_MAX_ENTRY      4
#define CERT_CA_MAX_ENTRY         4

#define CERT_BUFF_MAX_LEN         300
#define CERT_NAME_LEN             64
#define CERT_TYPE_SIGNING_REQ     "request"
#define CERT_TYPE_SIGNED          "signed"
#define CERT_TYPE_CA              "ca"

#define CERT_KEY_MAX_LEN          2048

#define CERT_TYPE_LEN             8

#define CERT_KEY_SIZE             1024

#define CERT_LOCAL 1
#define CERT_CA 2


#define CERT_SUBJECT_CN       0
#define CERT_SUBJECT_O        1
#define CERT_SUBJECT_ST       2
#define CERT_SUBJECT_C        3

typedef struct {
   char certName[CERT_NAME_LEN];
   char commonName[CERT_NAME_LEN];
   char country[CERT_BUFF_MAX_LEN];
   char state[CERT_NAME_LEN];
   char organization[CERT_NAME_LEN];
} CERT_ADD_INFO, *PCERT_ADD_INFO;

/** Get the number of existed certificates that match the given certificate type
 *
 * @param type       (IN) certificate type

 * @return the number of existed certificates that match the given certificate type
 */
UINT32 dalCert_getNumberOfExistedCert(SINT32 type);

/** Find the existed certificate that matches the given name and type
 *
 * @param name     (IN) certificate name
 * @param type       (IN) certificate type

 * @return TRUE if certificate is found, otherwise return FALSE
  */
UBOOL8 dalCert_findCert(char *name, SINT32 type);

/* Copy the certificate object with the input data entry
 *
 * @param obj               (OUT) the certCfg object that needs to be filled
 * @param certCfg         (IN) the input certificate
 *
 * @return CmsRet enum.
 */
CmsRet dalCert_copyCert(CertificateCfgObject *obj, CertificateCfgObject *certCfg);

/** add a certificate entry.
 *
 * @param certCfg         (IN) the input certificate
 *
 * @return CmsRet enum.
 */
CmsRet dalCert_addCert(CertificateCfgObject *certCfg);

/** retrieve a certificate object that matches the given name and type
 *
 * @param name           (IN) the certificate name
 * @param type             (IN) the certificate type
 * @param certCfg         (OUT) the certificate object
 *
 * @return CmsRet enum.
 */
CmsRet dalCert_getCert(char *name, SINT32 type, CertificateCfgObject *certCfg);

/** store a certificate object that matches the given name and type
 *
 * @param name           (IN) the certificate name
 * @param type             (IN) the certificate type
 * @param certCfg         (IN) the certificate entry
 *
 * @return CmsRet enum.
 */
CmsRet dalCert_setCert(char *name, SINT32 type, CertificateCfgObject *certCfg);

/** remove a certificate object that matches the given name and type
 *
 * @param cert         (IN) the certificate name
 * @param cert         (IN) the certificate type
 *
 * @return CmsRet enum.
 */
CmsRet dalCert_delCert(char *name, SINT32 type);

/** reset all reference count to 0 for signed or signed request certificates
 * but not for CA certificates
 *
 * @return CmsRet enum.
 */
CmsRet dalCert_resetRefCount(void);

/** Set symbolic links for CA certificates (needed by ssl, racoon etc.)
 *
 * @param cert         (IN) the input certificate
 *
 */
void rutCert_setCACertLinks(void);

/** create Openssl configuration file.
 *
 * @param pAddInfo       (IN) the certificate information
 *
 */
void rutCert_createCertConf(PCERT_ADD_INFO pAddInfo);

/** create certificate request
 *
 * @param pAddInfo       (IN) the certificate information
 * @param certCfg         (IN) the certificate object
 *
 * @return CmsRet enum.
 */
CmsRet rutCert_createCertReq(PCERT_ADD_INFO pAddInfo, CertificateCfgObject *certCfg);

/** create signed certificate or CA certificate
 *
 * @param certCfg         (IN) the certificate object
 *
 * @return CmsRet enum.
 */
CmsRet rutCert_createCertFile(const CertificateCfgObject *certCfg);

/** remove signed certificate or CA certificate
 *
 * @param certCfg         (IN) the certificate object
 *
 * @return CmsRet enum.
 */
CmsRet rutCert_removeCertFile(const CertificateCfgObject *certCfg);

/** compare two files
 *
 * @param fn1         (IN) the name of the first file
 * @param fn2         (IN) the name of the second file
 *
 * @return TRUE if the content of the first file is the same with the second file, otherwise return FALSE.
 */
UBOOL8 rutCert_compareFile(char *fn1, char *fn2);

/** Verify issued certificate against request
 *
 * @param certCfg         (IN) the certificate object
 *
 * @return CmsRet enum.
 */
CmsRet rutCert_verifyCertReq(CertificateCfgObject *certCfg); 

/** Get subject of certificate
 *
 * @param certCfg         (IN) the certificate object
 *
 * @return CmsRet enum.
 */
CmsRet rutCert_retrieveSubject(CertificateCfgObject *certCfg);

/** Process and verify imported certificate and private key
 *
 * @param certCfg         (IN) the certificate object
 *
 * @return CmsRet enum.
 */
CmsRet rutCert_processImportedCert(CertificateCfgObject *certCfg);



#endif   /* SUPPORT_CERT */


#define PMIRROR_DISABLED                           0
#define PMIRROR_ENABLED                            1
#define PMIRROR_DIR_IN                             0
#define PMIRROR_DIR_OUT                            1

/** Add default port mirroring configurations
 *
 * @param certCfg         (IN) the certificate object
 *
 * @return CmsRet enum.
 */
CmsRet dalPMirror_addDefautPortMirrors(void);

/** Configure port mirroring 
 *
 * @param lst         (IN) the string contains port mirroring information for configuration
 *
 * @return CmsRet enum.
 */
CmsRet dalPMirror_configPortMirrors(char *lst);


/** Get port mirroring for engdebug.cmd web page.
 *
 * @param lst         (IN) the string contains port mirroring information for configuration
 *
 * @return CmsRet enum.
 */
void dalPMirror_getPMirrorList(char *lst);


#ifdef SUPPORT_SNTP
/** Call appropriate cmsObj_set function to set the sntp config info.
 *
 * @param (IN) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_setNtpCfg(const WEB_NTWK_VAR *webVar);
/** Get the current sntp config info from MDM and fill in appropriate
 *  fields in webVar.
 *
 * @param (OUT) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
void cmsDal_getNtpCfg(WEB_NTWK_VAR *webVar);
#endif // #ifdef SUPPORT_SNTP


/** Fill in webVar for the configured non DMZ LAN info
 * @param nonDmzIpAddress  (OUT) Non DMZ ip address.
 * @param nonDmzIpMask     (OUT) Non DMZ ip address mask.
 */
void dalNtwk_fillInNonDMZIpAndMask(char *nonDmzIpAddress, char *nonDmzIpMask);

/** call rut_isAdvancedDmzEnabled to return the configuration flag 
 * for if advanced DMZ is enabled in the system 
 *
 */
UBOOL8 dalWan_isAdvancedDmzEnabled(void);

/** call rutWan_isPPPIpExtension to return if the system is configured as 
 * ppp ip extension
 *
 */
UBOOL8 dalWan_isPPPIpExtension(void);



/** call rutPMap_isWanUsedForIntfGroup to find out 
 * if this WAN interface is used in the interface group
 */
UBOOL8 dalPMap_isWanUsedForIntfGroup(const char *ifName);

/** Start/Stop Ping diagnostics
 *
 * @param pingParms         (IN) all ping parameters (i.e. repetition, host address, ...)
 *
 * @return CmsRet enum.
 */
CmsRet dalDiag_startStopPing(IPPingDiagObject *pingParms);

/** Get Ping diagnostics result -- get the most recent ping test result
 *
 * @param pingParms         (IN) structure to hold ping results.   Ping's state is a string,
 *                          caller needs to free the state field when done reading the result.
 *
 * @return CmsRet enum.
 */
CmsRet dalDiag_getPingResult(IPPingDiagObject *pingResult);

/** Start and wait for OAM Loopback test result
 *
 * @param type         (IN) type of loopback (BCM_DIAG_OAM_SEGMENT,BCM_DIAG_OAM_END2END,
 *                          BCM_DIAG_OAM_F4_SEGMENT,BCM_DIAG_OAM_F4_END2END)
 * @param addr         (IN) PORT/VPI/VCI to send loopback on
 * @param repetition   (IN) number of loopback cell to send.
 * @param timeout      (IN) timeout value in second waiting for loopback response
 *                          
 *
 * @return CmsRet enum (OUT) Success/Pass if CMS_SUCCESS; otherwise test fails.
 */
CmsRet dalDiag_doOamLoopback(int type, PPORT_VPI_VCI_TUPLE addr, UINT32 repetition, UINT32 timeout);


CmsRet dalDiag_startStopPing(IPPingDiagObject *pingParms);

/** Delete the dslLinkConfig object
 *
 * @param (IN) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
 CmsRet dalDsl_deleteAtmInterface(const WEB_NTWK_VAR *webVar);

/** Add the dslLinkConfig object
 *
 * @param (IN) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
CmsRet dalDsl_addAtmInterface(const WEB_NTWK_VAR *webVar);

/** get the default dslLinkConfig info to webVar
 *
 * @param (OUT) pointer to WEB_NTWK_VAR
 *
 */
void getDefaultWanDslLinkCfg(WEB_NTWK_VAR *webVar);

/** find the dslLinkCfg object with the given destAddr.
 *
 * This function assumes there is only one WANDevice.
 *
 * @param portId   (IN) ATM interface port id
 * @param destAddr (IN) vpi/vci of the DslLinkCfg to find.
 * @param iidStack (OUT) iidStack of the DslLinkCfg object found.
 * @param dslLinkCfg (OUT) if not null, this will contain a pointer to the found
 *                         dslLinkCfg object.  Caller is responsible for calling
 *                         cmsObj_free() on this object.
 *
 * @return UBOOL8 indicating whether the desired DslLinkCfg object was found.
 */
UBOOL8 getDslLinkCfg(UINT32 portId, const char *destAddr, InstanceIdStack *iidStack, WanDslLinkCfgObject **dslLinkCfg);


/** get WanDev for ATM/PTM iidStack.  Uses rut function.
 *
 * This function assumes both ATM and PTM WANDevice is initialized by mdm_init
 *
 * @param isAtm   (IN) TRUE is for DSL ATM WanDev, FALSE is PTM WanDev
 * @param iidStack (OUT) iidStack of the WanDev object for ATM/PTM found.
 *
 * @return UBOOL8 indicating whether the desired WanDev for ATM or PTM object was found.
 */
UBOOL8 dalDsl_getDslWanDevIidStack(UBOOL8 isAtm, InstanceIdStack *wanDevIid);

/** Add the WanPtmLinkCfgObject object
 *
 * @param (IN) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
CmsRet dalDsl_addPtmInterface(const WEB_NTWK_VAR *webVar);

/** find the ptmLinkCfg object with the given portId, priority and return if ptmWanDev
 * exist or not and if existed, the ptmWanDev IidStack and ptmLinkCfg IidStack and object.
 *
 * This function assumes there is only one WANDevice.
 *
 * @param portId        (IN) PTM interface port id
 * @param priorityNorm  (IN) normal priority 
 * @param priorityHigh  (IN) High priority 
 * @param ptmIid        (OUT) iidStack of the ptm Wan Dev object found.
 *  @parm  pmCfgObj     (OUT) ptmLinkCfg object
 *
 * @return UBOOL8 indicating whether the desired DslLinkCfg object was found.
 */

UBOOL8 dalDsl_getPtmLinkCfg(UINT32 portId, 
                            UINT32 priorityNorm,
                            UINT32 priorityHigh,
                            InstanceIdStack *ptmIid,
                            WanPtmLinkCfgObject **ptmCfgObj);


/** Delete the WanPtmLinkCfgObject object
 *
 * @param (IN) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
 CmsRet dalDsl_deletePtmInterface(const WEB_NTWK_VAR *webVar);



/** Get fresh from MDM the default gateway list and dns info 
 *
 * @param (IN) pointer to WEB_NTWK_VAR
 *
 */ 
void cmsDal_getDefaultGatewayAndDnsCfg(WEB_NTWK_VAR *webVar);


#ifdef AEI_VDSL_CUSTOMER_NCS
/** Get fresh from MDM the LAN Host DNS info 
 *
 * @param (IN/OUT) pointer to WEB_NTWK_VAR
 *
 */ 
void cmsDal_getLanDnsCfg(WEB_NTWK_VAR *webVar);
CmsRet dalDsl_addAtmInterfaceByObj(WanDslLinkCfgObject *dslLinkCfg);
CmsRet dalDsl_addPtmInterfaceByObj(WanPtmLinkCfgObject *ptmLinkCfg);

CmsRet cmsDal_createQosQueuesForWanInf(char *wanIntf);

#endif /* AEI_VDSL_CUSTOMER_NCS */

#ifdef SUPPORT_TR69C
/** Load MDM management server info into webVar.
 *
 * @param webVar (OUT) webvar variable to be filled in.
 */
void cmsDal_getTr69cCfg(WEB_NTWK_VAR *webVar);
#endif

#ifdef DMP_X_ITU_ORG_GPON_1
/** Call appropriate cmsObj_set function to set the OMCI system related info.
 *
 * @param (IN) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_setOmciSystem(const WEB_NTWK_VAR *webVar);

/** Load MDM OMCI system object info into webVar.
 *
 * @param webVar (OUT) webvar variable to be filled in.
 */
void cmsDal_getOmciSystem(WEB_NTWK_VAR *webVar);
#endif  /* DMP_X_ITU_ORG_GPON_1 */

#ifdef SUPPORT_P8021AG
CmsRet dalP8021AG_setMDLevel(int level);
CmsRet dalP8021AG_getMDLevel(int *level);
#endif


/** get the next available Connection Id against this layer 2 wan interface name
 * @param wanL2IfName (IN) layer 2 ifName.
 * @param outConId (OUT) connection Id
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_getAvailableConIdForMSC(const char *wanL2IfName, SINT32 *outConId);

/** get the number of used xtm transmit queues.
 * @param wanType      (IN) wan protocol type
 * @param usedQueues   (OUT) the number of used xtm transmit queues.
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_getNumUsedQueues(WanLinkType wanType, UINT32 *usedQueues);


/** get the number of unused xtm transmit queues.
 * @param wanType      (IN) wan protocol type
 * @param unusedQueues (OUT) the number of unused xtm transmit queues.
 *
 * @return CmsRet enum.
 */
CmsRet cmsDal_getNumUnusedQueues(WanLinkType wanType, UINT32 *unusedQueues);


/** Return true if any WAN interface is valid
 * 
 * @param (IN) WAN interface name
 *
 * @return BOOL
 */
UBOOL8 dalWan_isValidWanInterface(const char *ifName);

#ifdef DMP_X_BROADCOM_COM_PWRMNGT_1
/** configure power mangement conf. params
 * @param pWebVar (IN) WEB_NTWK_VAR type
 *
 * @return CmsRet enum.
 */
CmsRet dalPowerManagement(const PWEB_NTWK_VAR pWebVar);

/** get power mangement conf. params
 * @param pWebVar (IN/OUT) WEB_NTWK_VAR type
 *
 * @return CmsRet enum.
 */
CmsRet dalGetPowerManagement(WEB_NTWK_VAR *pWebVar);
#endif /* aka SUPPORT_PWRMNGT */

#ifdef DMP_X_BROADCOM_COM_STANDBY_1
/** configure standby conf. params
 * @param pWebVar (IN) WEB_NTWK_VAR type
 *
 * @return CmsRet enum.
 */
CmsRet dalStandby(const PWEB_NTWK_VAR pWebVar);
CmsRet dalStandbyDemo(void);

/** get standby conf. params
 * @param pWebVar (IN/OUT) WEB_NTWK_VAR type
 *
 * @return CmsRet enum.
 */
CmsRet dalGetStandby(WEB_NTWK_VAR *pWebVar);
#endif


/** Add the L2tpAcIntfConfigObject object
 *
 * @param (IN) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
CmsRet dalL2tpAc_addL2tpAcInterface(const WEB_NTWK_VAR *webVar);

/** Delete the L2tpAcIntfConfigObject and associated WanPppConnection object
 *
 * @param (IN) pointer to WEB_NTWK_VAR
 *
 * @return CmsRet enum.
 */
CmsRet dalL2tpAc_deleteL2tpAcInterface(WEB_NTWK_VAR *webVar);

#ifdef SUPPORT_SAMBA
/**  Adds a user account to etc/passwd and smbpasswd
 *
 * @param webVar (IN) With username,password and homedir pathset for account to be created.
 * @param erroStr (OUT) message with failurei cause   .
 * @return CmsRet
 */
CmsRet dalStorage_addUserAccount(const WEB_NTWK_VAR *webVar, char**errorStr);

/** Deletes a user account from the system
 *
 * @param userName (IN) name with which user account has to be created.
 * @return CmsRet
 */
CmsRet dalStorage_deleteUserAccount(const char *userName);
#endif /* SUPPORT_SAMBA */

#if defined(AEI_VDSL_CUSTOMER_NCS)
//add by vincent 2010-12-09
#if defined(AEI_VDSL_CUSTOMER_QWEST) || defined(AEI_VDSL_CUSTOMER_TELUS)
CmsRet dalSec_setFirewallLevel(const char *ifName, const char *fwip, const char *fwset, const UBOOL8 fwStealthMode, const char *fwLevel, const char* fwInVal, const char* fwOutVal);
CmsRet dalSec_getFirewallLevel(const char *ifName, UBOOL8* fwStealthMode, char *fwLevel, char *fwInVal, char *fwOutVal);
#else
CmsRet dalSec_setFirewallLevel(const char *ifName, const char *fwip, const char *fwset, const char *fwLevel, const char* fwInVal, const char* fwOutVal);
CmsRet dalSec_getFirewallLevel(const char *ifName, char *fwLevel, char* fwInVal, char* fwOutVal);
#endif
CmsRet dalSec_getFirewallUnum(const char *ifName, char *fwip, char *fwset);
CmsRet dalSec_setFirewallUnum(const char *ifName, const char *fwip, const char *fwset);
CmsRet dalSec_addBlockingService(const char *name, const char *interfaces, const char *protocol, const char *srcAddr, const char *srcMask, const UINT16 srcPortStart, const UINT16 srcPortEnd, 
     const char *dstAddr, const char *dstMask, const UINT16 dstPortStart, const UINT16 dstPortEnd, const char *blockingService, const UBOOL8 isAccept);
CmsRet dalSec_deleteBlockingService(const char *name, const char *interfaces, const char *protocol, const UBOOL8 isAccept);

CmsRet dalVirtualServer_addPortForwardingEntry(const char *dstWanIf, const char *srvName, const char *srvWanAddr, const char *srvAddr, const char *protocol, 
	     const UINT16 EPS, const UINT16 EPE, const UINT16 IPS, const UINT16 IPE, const UBOOL8 isAddbyGUI);

CmsRet dalSec_getFilterOut(char *blockingService);
CmsRet dalSec_deletePortMappingApplicationRule(int app_id, int rule_id);
CmsRet dalSec_deletePortMappingApplication(const char* name);
/*for mac filtering*/
CmsRet dalMacFilter_addEntry(const char *macAddr);
CmsRet dalMacFilter_removeEntry(const char *macAddr);
CmsRet dalMacFilter_getEntry(char *macFilterList);

/* Get the wan interface name served as the default system gateway
 * 
 * @param gwIfName (OUT) The linux user friendly wan interface name.
 * 
 * @return CmsRet enum.
 */
 CmsRet dalWan_getDfltGatewayIfName(char *gwIfName);
	     
		  
/* Set the wan interface name served as the default system gateway
 * 
 * @param gwIfName (OUT) The linux user friendly wan interface name.
 * 
 * @return CmsRet enum.
 */
  CmsRet dalWan_setDfltGatewayIfName(char *gwIfName);

/** Get the system NetworkCofigObj for system dns info. 
 * 
 * @param dnsIfName       (OUT) wan Interface served as dns info for the system
 * @param dnsServers       (OUT) static dns1 or dns2 if not NULL
 *
 * @return CmsRet enum.
 */
void  dalWan_getDlftDnsInfo(char *dnsIfName, char *dnsServers);


/** Set the system NetworkCofigObj for system dns info. 
 *  ONly one parameter can be set at the time (either use dnsIfname or 
 *  dsnServer - static dns).  So one should be NULL and other one should
 *  be something.
 * 
 * @param dnsIfName       (IN) wan Interface served as dns info for the system if not NULL
 * @param dnsServers       (IN) static dns1 or dns2 if not NULL
 *
 * @return CmsRet enum.
 */
CmsRet  dalWan_setDlftDnsInfo(char *dnsIfName, char *dnsServers);

#if defined(AEI_VDSL_CUSTOMER_NCS)
#ifndef AEI_VDSL_CUSTOMER_QWEST
void dalWan_setPppDNSServers(char *dnsIfName, char *dnsServers);
#endif
#endif

#ifdef AEI_VDSL_CUSTOMER_SASKTEL
void cmsDal_getRemoteGuiCfg(WEB_NTWK_VAR *webVar);
int cmsDal_remoteGuiLogin(UBOOL8 is_ssl2, const char *username, const char *password);
#endif

#if defined(SUPPORT_HTTPD_SSL)
UBOOL8 cmsDal_remoteHttpAccessCheck(UINT32 lockTimeoutMs);
#endif

UBOOL8 cmsDal_remoteTelnetAccessCheck(UINT32 lockTimeoutMs, UINT32 accessMode);

#endif
#if defined(AEI_VDSL_CUSTOMER_QWEST)
void cmsDal_getDeviceInfo(WEB_NTWK_VAR *webVar);
#endif

#endif /* __CMS_DAL_H__ */
