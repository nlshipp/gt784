/*
 * Enrollee state machine
 *
 * Copyright (C) 2009, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: enr_reg_sm.c,v 1.1 2010/08/05 21:59:02 ywu Exp $
 */

#ifdef WIN32
#include <windows.h>
#endif

#ifndef UNDER_CE

#ifdef __ECOS
#include <sys/types.h>
#include <sys/socket.h>
#endif

#if defined(__linux__) || defined(__ECOS)
#include <net/if.h>
#endif

#endif /* UNDER_CE */

#include <bn.h>
#include <wps_dh.h>
#include <wps_sha256.h>

#include <wpsheaders.h>
#include <wpscommon.h>
#include <wpserror.h>
#include <portability.h>
#include <tutrace.h>
#include <sminfo.h>
#include <reg_protomsg.h>
#include <reg_proto.h>
#include <statemachine.h>
#include <wps_staeapsm.h>
#include <info.h>
#include <wpsapi.h>

#define M2D_SLEEP_TIME 25
void * enr_sm_handle_message_timeout(IN void *p_data, uint32 message);

EnrSM *
enr_sm_new(void *g_mc, CRegProtocol *pc_regProt)
{
	EnrSM *e = (EnrSM *)malloc(sizeof(EnrSM));

	if (!e)
		return NULL;

	memset(e, 0, sizeof(EnrSM));

	TUTRACE((TUTRACE_INFO, "Enrollee constructor\n"));
	e->g_mc = g_mc;
	e->m_sm = state_machine_new(g_mc, pc_regProt, MODE_ENROLLEE);
	if (!e->m_sm) {
		free(e);
		return NULL;
	}

	WpsSyncCreate(&e->mp_m2dLock);
	/* e->m_stophandlemsg_enr = 0; */
	WpsSyncCreate(&e->mp_ptimerLock_enr);
	return e;
}

void
enr_sm_delete(EnrSM *e)
{
	if (e) {
		state_machine_delete(e->m_sm);
		WpsSyncDestroy(e->mp_m2dLock);
		WpsSyncDestroy(e->mp_ptimerLock_enr);

		TUTRACE((TUTRACE_INFO, "Free EnrSM %p\n", e));
		free(e);
	}
}

uint32
enr_sm_initializeSM(EnrSM *e, IN DevInfo *p_registrarInfo, void * p_StaEncrSettings,
	void * p_ApEncrSettings, char *p_devicePasswd, uint32 passwdLength)
{
	uint32 err;

	TUTRACE((TUTRACE_INFO, "Enrollee Initialize SM\n"));

	err = state_machine_init_sm(e->m_sm);
	if (WPS_SUCCESS != err) {
		return err;
	}

	e->m_sm->mps_regData->p_enrolleeInfo = e->m_sm->mps_localInfo;
	e->m_sm->mps_regData->p_registrarInfo = p_registrarInfo;
	state_machine_set_passwd(e->m_sm, p_devicePasswd, passwdLength);
	state_machine_set_encrypt(e->m_sm, p_StaEncrSettings, p_ApEncrSettings);

	return WPS_SUCCESS;
}

void
enr_sm_restartSM(EnrSM *e)
{
	BufferObj *passwd = buffobj_new();
	void *apEncrSettings, *staEncrSettings;
	RegData *   mps_regData;
	uint32 err;

	TUTRACE((TUTRACE_INFO, "SM: Restarting the State Machine\n"));

	/* first extract the all info we need to save */
	mps_regData = e->m_sm->mps_regData;
	buffobj_Append(passwd, mps_regData->password->m_dataLength,
		mps_regData->password->pBase);
	staEncrSettings = mps_regData->staEncrSettings;
	apEncrSettings = mps_regData->apEncrSettings;

	if (mps_regData->p_registrarInfo) {
		free(mps_regData->p_registrarInfo);
		mps_regData->p_registrarInfo = NULL;
	}

	/* next, reinitialize the base SM class */
	err = state_machine_init_sm(e->m_sm);
	if (WPS_SUCCESS != err) {
		buffobj_del(passwd);
		return;
	}

	/* Finally, store the data back into the regData struct */
	e->m_sm->mps_regData->p_enrolleeInfo = e->m_sm->mps_localInfo;

	state_machine_set_passwd(e->m_sm, (char *)passwd->pBase, passwd->m_dataLength);
	buffobj_del(passwd);
	state_machine_set_encrypt(e->m_sm, staEncrSettings, apEncrSettings);

	e->m_sm->m_initialized = true;
}

uint32
enr_sm_step(EnrSM *e, uint32 msgLen, uint8 *p_msg, uint8 *outbuffer, uint32 *out_len)
{
	BufferObj *inMsg, *outMsg;
	uint32 retVal = WPS_CONT;

	TUTRACE((TUTRACE_INFO, "ENRSM: Entering Step, , length = %d\n", msgLen));

	if (false == e->m_sm->m_initialized) {
		TUTRACE((TUTRACE_ERR, "ENRSM: Not yet initialized.\n"));
		return WPS_ERR_NOT_INITIALIZED;
	}

	if (START == e->m_sm->mps_regData->e_smState) {
		TUTRACE((TUTRACE_INFO, "ENRSM: Step 1\n"));
		/* No special processing here */
		inMsg = buffobj_new();
		outMsg = buffobj_setbuf(outbuffer, *out_len);
		buffobj_dserial(inMsg, p_msg, msgLen);

		retVal = enr_sm_handleMessage(e, inMsg, outMsg);
		*out_len = buffobj_Length(outMsg);

		TUTRACE((TUTRACE_INFO, "ENRSM: Step 2\n"));

		buffobj_del(inMsg);
		buffobj_del(outMsg);
	}
	else {
		/* do the regular processing */
		if (!p_msg || !msgLen) {
			/* Preferential treatment for UPnP */
			if (e->m_sm->mps_regData->e_lastMsgSent == M1) {
				/*
				 * If we have already sent M1 and we get here, assume that it is
				 * another request for M1 rather than an error.
				 * Send the bufferred M1 message
				 */
				TUTRACE((TUTRACE_INFO, "ENRSM: Got another request for M1. "
					"Resending the earlier M1\n"));

				/* copy buffered M1 to msg_to_send buffer */
				if (*out_len < e->m_sm->mps_regData->outMsg->m_dataLength) {
					e->m_sm->mps_regData->e_smState = FAILURE;
					TUTRACE((TUTRACE_ERR, "output message buffer to small\n"));
					return WPS_MESSAGE_PROCESSING_ERROR;
				}
				memcpy(outbuffer, (char *)e->m_sm->mps_regData->outMsg->pBase,
					e->m_sm->mps_regData->outMsg->m_dataLength);
				*out_len = buffobj_Length(e->m_sm->mps_regData->outMsg);
				return WPS_SEND_MSG_CONT;
			}
			return WPS_CONT;
		}

		inMsg = buffobj_new();
		outMsg = buffobj_setbuf(outbuffer, *out_len);
		buffobj_dserial(inMsg, p_msg, msgLen);

		retVal = enr_sm_handleMessage(e, inMsg, outMsg);
		*out_len = buffobj_Length(outMsg);

		buffobj_del(inMsg);
		buffobj_del(outMsg);
	}

	/* now check the state so we can act accordingly */
	switch (e->m_sm->mps_regData->e_smState) {
	case START:
	case CONTINUE:
		/* do nothing. */
		break;

	case SUCCESS:
		e->m_sm->m_initialized = false;
		if (retVal == WPS_SEND_MSG_CONT)
			return WPS_SEND_MSG_SUCCESS;
		else
			return WPS_SUCCESS;
		/* break;	unreachable */

	case FAILURE:
		if (retVal == WPS_SEND_MSG_CONT)
			return WPS_SEND_MSG_ERROR;
		else
			return WPS_MESSAGE_PROCESSING_ERROR;
		/* break;	unreachable */
	default:
		break;
	}

	return retVal;
}


uint32
enr_sm_handleMessage(EnrSM *e, BufferObj *msg, BufferObj *outmsg)
{
	uint32 err, retVal = WPS_CONT;
	void *encrSettings = NULL;
	uint32 msgType = 0;
	uint16 configError = WPS_ERROR_MSG_TIMEOUT;
	RegData *regInfo = e->m_sm->mps_regData;

	err = reg_proto_get_msg_type(&msgType, msg);
	if (WPS_SUCCESS != err) {
		TUTRACE((TUTRACE_ERR, "ENRSM: Expected M1, received %d\n", msgType));
		/* For DTM testing */
		/* goto out; */
	}

	/* If we get a valid message, extract the message type received.  */
	if (MNONE != e->m_sm->mps_regData->e_lastMsgSent) {
		/*
		 * If this is a late-arriving M2D, ACK it.  Otherwise, ignore it.
		 * Should probably also NACK an M2 at this point.
		 */
		if ((SM_RECVD_M2 == e->m_m2dStatus) &&
			(msgType <= WPS_ID_MESSAGE_M2D)) {
			if (WPS_ID_MESSAGE_M2D == msgType) {
				retVal = state_machine_send_ack(e->m_sm, outmsg);
				TUTRACE((TUTRACE_INFO, "ENRSM: Possible late M2D received.  "
					"Sending ACK.\n"));
				goto out;
			}
		}
	}

	switch (e->m_sm->mps_regData->e_lastMsgSent) {
	case MNONE:
		if (WPS_ID_MESSAGE_M2 == msgType || WPS_ID_MESSAGE_M4 == msgType ||
			WPS_ID_MESSAGE_M6 == msgType || WPS_ID_MESSAGE_M8 == msgType ||
			WPS_ID_MESSAGE_NACK == msgType) {
			TUTRACE((TUTRACE_ERR, "ENRSM: msgType error type=%d, "
				"stay silent \n", msgType));
			goto out;
		}

		err = reg_proto_create_m1(e->m_sm->mps_regData, outmsg);
		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Expected M1, received %d\n", msgType));
			goto out;
		}
		e->m_sm->mps_regData->e_lastMsgSent = M1;

		/* Set the m2dstatus. */
		e->m_m2dStatus = SM_AWAIT_M2;

		/* set the message state to CONTINUE */
		e->m_sm->mps_regData->e_smState = CONTINUE;

		/* msg_to_send is ready */
		retVal = WPS_SEND_MSG_CONT;
		break;

	case M1:
		e->m_ptimerStatus_enr = 0;
		/* for DTM 1.1 test */
		if (e->m_m2dStatus == SM_RECVD_M2D && WPS_ID_MESSAGE_NACK == msgType) {
			err = reg_proto_process_nack(e->m_sm->mps_regData, msg,
				&configError);
			if (configError != WPS_ERROR_NO_ERROR) {
				TUTRACE((TUTRACE_ERR, "ENRSM: Recvd NACK with config err: "
					"%x\n", configError));
			}
			/* for DTM 1.3 push button test */
			retVal = WPS_MESSAGE_PROCESSING_ERROR;

			enr_sm_restartSM(e);
			goto out;
		}
/* brcm added for message timeout */
		if (SM_RECVD_M2 != e->m_m2dStatus) {
			TUTRACE((TUTRACE_INFO, "ENRSM: Starting wps ProtocolTimerThread\n"));
		}
/* brcm added for message timeou --	*/

		/* Check whether this is M2D */
		if (WPS_ID_MESSAGE_M2D == msgType) {
			err = reg_proto_process_m2d(e->m_sm->mps_regData, msg);
			if (WPS_SUCCESS != err) {
				TUTRACE((TUTRACE_ERR, "ENRSM: Expected M2D, err %x\n", msgType));
				goto out;
			}

			/* Send NACK if enrollee is AP */
			if (regInfo->p_enrolleeInfo->primDeviceCategory == 6) {
				/* Send an NACK to the registrar */
				/* for DTM1.1 test */
				retVal = state_machine_send_nack(e->m_sm,
					WPS_ERROR_NO_ERROR, outmsg);
				if (WPS_SEND_MSG_CONT != retVal) {
					TUTRACE((TUTRACE_ERR, "ENRSM: SendNack err %x\n", msgType));
					goto out;
				}
			}
			else {
				/* Send an ACK to the registrar */
				retVal = state_machine_send_ack(e->m_sm, outmsg);
				if (WPS_SEND_MSG_CONT != retVal) {
					TUTRACE((TUTRACE_ERR, "ENRSM: SendAck err %x\n", msgType));
					goto out;
				}
			}

			/*
			 * Now, schedule a thread to sleep for some time to allow other
			 * registrars to send M2 or M2D messages.
			 */
			WpsLock(e->mp_m2dLock);
			if (SM_AWAIT_M2 == e->m_m2dStatus) {
				/*
				 * if the M2D status is 'await', set the timer. For all
				 * other cases, don't do anything, because we've either
				 * already received an M2 or M2D, and possibly, the SM reset
				 * process has already been initiated
				 */
				TUTRACE((TUTRACE_INFO, "ENRSM: Starting M2DThread\n"));

				e->m_m2dStatus = SM_RECVD_M2D;

				WpsSleep(1);

				/* set the message state to CONTINUE */
				e->m_sm->mps_regData->e_smState = CONTINUE;
				WpsUnlock(e->mp_m2dLock);
				goto out;
			}
			else {
				TUTRACE((TUTRACE_INFO, "ENRSM: Did not start M2DThread. "
					"status = %d\n", e->m_m2dStatus));
			}
			WpsUnlock(e->mp_m2dLock);

			goto out; /* done processing for M2D, return */
		}
		/* message wasn't M2D, do processing for M2 */
		else {
			WpsLock(e->mp_m2dLock);
			if (SM_M2D_RESET == e->m_m2dStatus) {
				WpsUnlock(e->mp_m2dLock);
				/* a SM reset has been initiated. Don't process any M2s */
				goto out;
			}
			else {
				e->m_m2dStatus = SM_RECVD_M2;
			}
			WpsUnlock(e->mp_m2dLock);

			err = reg_proto_process_m2(e->m_sm->mps_regData, msg, NULL);

			if (WPS_SUCCESS != err) {
				if (err == RPROT_ERR_AUTH_ENC_FLAG) {
					TUTRACE((TUTRACE_ERR, "ENRSM: Check auth or encry flag "
						"error: SendNack! %x\n", err));
					retVal = state_machine_send_nack(e->m_sm,
						WPS_ERROR_NW_AUTH_FAIL, outmsg);
					/* send msg_to_send message immedicate */
					enr_sm_restartSM(e);
					goto out;
				}
				else if (err == RPROT_ERR_MULTIPLE_M2) {
					TUTRACE((TUTRACE_ERR, "ENRSM: receive multiple M2 "
						"error: SendNack! %x\n", err));
					retVal = state_machine_send_nack(e->m_sm,
						WPS_ERROR_DEVICE_BUSY, outmsg);
					goto out;
				}
				else { /* for DTM1.1 test */
					TUTRACE((TUTRACE_ERR, "ENRSM: HMAC "
						"error: SendNack! %x\n", err));
					retVal = state_machine_send_nack(e->m_sm,
						WPS_ERROR_DECRYPT_CRC_FAIL, outmsg);
					/* send msg_to_send message immedicate */
					enr_sm_restartSM(e);
					goto out;
				}
			}
			e->m_sm->mps_regData->e_lastMsgRecd = M2;

			err = reg_proto_create_m3(e->m_sm->mps_regData, outmsg);

			if (WPS_SUCCESS != err) {
				TUTRACE((TUTRACE_ERR, "ENRSM: Build M3 failed %x\n", err));
				retVal = state_machine_send_nack(e->m_sm,
					WPS_MESSAGE_PROCESSING_ERROR, outmsg);
				goto out;
			}
			e->m_sm->mps_regData->e_lastMsgSent = M3;

			/* set the message state to CONTINUE */
			e->m_sm->mps_regData->e_smState = CONTINUE;

			/* msg_to_send is ready */
			retVal = WPS_SEND_MSG_CONT;
		}
		break;

	case M3:
		err = reg_proto_process_m4(e->m_sm->mps_regData, msg);

		if (WPS_SUCCESS != err) {
			/* for DTM testing */
			if (WPS_ID_MESSAGE_M2 == msgType) {
				TUTRACE((TUTRACE_ERR, "ENRSM: Receive multiple M2 "
					"error! %x\n", err));
				goto out;
			}

			TUTRACE((TUTRACE_ERR, "ENRSM: Process M4 failed %x\n", err));
			/* Send a NACK to the registrar */
			if (err == RPROT_ERR_CRYPTO) {
				retVal = state_machine_send_nack(e->m_sm,
					WPS_ERROR_DEV_PWD_AUTH_FAIL, outmsg);
				retVal = WPS_ERR_CONFIGAP_PINFAIL;
			} else {
				retVal = state_machine_send_nack(e->m_sm,
					WPS_MESSAGE_PROCESSING_ERROR, outmsg);
			}
			enr_sm_restartSM(e);
			goto out;
		}
		e->m_sm->mps_regData->e_lastMsgRecd = M4;

		err = reg_proto_create_m5(e->m_sm->mps_regData, outmsg);

		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Build M5 failed %x\n", err));
			retVal = state_machine_send_nack(e->m_sm,
				WPS_MESSAGE_PROCESSING_ERROR, outmsg);
			goto out;
		}
		e->m_sm->mps_regData->e_lastMsgSent = M5;

		/* set the message state to CONTINUE */
		e->m_sm->mps_regData->e_smState = CONTINUE;

		/* msg_to_send is ready */
		retVal = WPS_SEND_MSG_CONT;
		break;

	case M5:
		err = reg_proto_process_m6(e->m_sm->mps_regData, msg);

		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Process M6 failed %x\n", err));
			/* Send a NACK to the registrar */
			if (err == RPROT_ERR_CRYPTO) {
				retVal = state_machine_send_nack(e->m_sm,
					WPS_ERROR_DEV_PWD_AUTH_FAIL, outmsg);
			} else {
				retVal = state_machine_send_nack(e->m_sm,
					WPS_MESSAGE_PROCESSING_ERROR, outmsg);
			}
			enr_sm_restartSM(e);
			goto out;
		}
		e->m_sm->mps_regData->e_lastMsgRecd = M6;

		/* Build message 7 with the appropriate encrypted settings */
		if (e->m_sm->mps_regData->p_enrolleeInfo->b_ap) {
			err = reg_proto_create_m7(e->m_sm->mps_regData, outmsg,
				e->m_sm->mps_regData->apEncrSettings);
		}
		else {
			err = reg_proto_create_m7(e->m_sm->mps_regData, outmsg,
				e->m_sm->mps_regData->staEncrSettings);
		}

		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Build M7 failed %x\n", err));
			retVal = state_machine_send_nack(e->m_sm,
				WPS_MESSAGE_PROCESSING_ERROR, outmsg);
			goto out;
		}

		e->m_sm->mps_regData->e_lastMsgSent = M7;

		/* set the message state to CONTINUE */
		e->m_sm->mps_regData->e_smState = CONTINUE;

		/* msg_to_send is ready */
		retVal = WPS_SEND_MSG_CONT;
		break;

	case M7:
		/* for DTM 1.1 test, if enrollee is AP */
		if (regInfo->p_enrolleeInfo->primDeviceCategory == 6 &&
			WPS_ID_MESSAGE_NACK == msgType) {
			err = reg_proto_process_nack(e->m_sm->mps_regData,
				msg, &configError);
			if (configError != WPS_ERROR_NO_ERROR) {
				TUTRACE((TUTRACE_ERR, "ENRSM: Recvd NACK with config err: "
					"%d\n", configError));
			}
			e->m_ptimerStatus_enr = 1;
			/* set the message state to success */
			e->m_sm->mps_regData->e_smState = SUCCESS;
			retVal = WPS_SUCCESS;
			goto out;
		}

		err = reg_proto_process_m8(e->m_sm->mps_regData, msg, &encrSettings);

		if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Process M8 failed %x\n", err));

			/* send fail in all case */
			retVal = state_machine_send_nack(e->m_sm, configError, outmsg);
			enr_sm_restartSM(e);
			goto out;
		}

		e->m_sm->mps_regData->e_lastMsgRecd = M8;
		e->mp_peerEncrSettings = encrSettings;

		/* Send a Done message */
		retVal = state_machine_send_done(e->m_sm, outmsg);
		e->m_sm->mps_regData->e_lastMsgSent = DONE;
		e->m_ptimerStatus_enr = 1;
		/* Decide if we need to wait for an ACK
			Wait only if we're an AP AND we're running EAP
		*/
		if ((!e->m_sm->mps_regData->p_enrolleeInfo->b_ap) ||
			(e->m_sm->m_transportType != TRANSPORT_TYPE_EAP)) {
			/* set the message state to success */
			e->m_sm->mps_regData->e_smState = SUCCESS;
			e->m_ptimerStatus_enr = 1;
		}
		else {
			/* Wait for ACK. set the message state to continue */
			e->m_sm->mps_regData->e_smState = CONTINUE;
		}
		break;

	case DONE:
		err = reg_proto_process_ack(e->m_sm->mps_regData, msg);

		if (RPROT_ERR_NONCE_MISMATCH == err) {
			 /* ignore nonce mismatches */
			e->m_sm->mps_regData->e_smState = CONTINUE;
		}
		else if (WPS_SUCCESS != err) {
			TUTRACE((TUTRACE_ERR, "ENRSM: Expected M1, received %d\n", msgType));
			goto out;
		}

		/* set the message state to success */
		e->m_sm->mps_regData->e_smState = SUCCESS;
		e->m_ptimerStatus_enr = 1;
		break;

	default:
		TUTRACE((TUTRACE_ERR, "ENRSM: Expected M1, received %d\n", msgType));
		goto out;
	}

out:

	return retVal;
}

int
reg_proto_verCheck(TlvObj_uint8 *version, TlvObj_uint8 *msgType, uint8 msgId, BufferObj *msg)
{
	uint8 reg_proto_version = WPS_VERSION;
	int err = 0;

	err |= tlv_dserialize(version, WPS_ID_VERSION, msg, 0, 0);
	err |= tlv_dserialize(msgType, WPS_ID_MSG_TYPE, msg, 0, 0);

	if (err)
		return RPROT_ERR_REQD_TLV_MISSING; /* for DTM 1.1 */

	/* If the major version number matches, assume we can parse it successfully. */
	if ((reg_proto_version & 0xF0) != (version->m_data & 0xF0)) {
		TUTRACE((TUTRACE_ERR, "message version %d  differs from protocol version %d\n",
			version->m_data, reg_proto_version));
		return RPROT_ERR_INCOMPATIBLE;
	}
	if (msgId != msgType->m_data) {
		TUTRACE((TUTRACE_ERR, "message type %d  differs from protocol version %d\n",
			msgType->m_data, msgId));
		return RPROT_ERR_WRONG_MSGTYPE;
	}

	return WPS_SUCCESS;
}

uint32
reg_proto_create_m1(RegData *regInfo, BufferObj *msg)
{
	uint8 message;
	uint8 reg_proto_version = WPS_VERSION;
	int len = 0;
	CTlvPrimDeviceType primDev;

	/* First generate/gather all the required data. */
	message = WPS_ID_MESSAGE_M1;

	/* Enrollee nonce */
	/*
	 * Hacking, do not generate new random enrollee nonce
	 * in case of we have prebuild enrollee nonce.
	 */
	if (regInfo->e_lastMsgSent == MNONE) {
		RAND_bytes(regInfo->enrolleeNonce, SIZE_128_BITS);
		TUTRACE((TUTRACE_ERR, "RPROTO: create enrolleeNonce\n"));
	}
	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM1 \n"));

	/* It should not generate new key pair if we have prebuild enrollee nonce */
	if (!regInfo->DHSecret) {
		BufferObj *pubKey = buffobj_new();
		reg_proto_generate_dhkeypair(&regInfo->DHSecret, pubKey);
		buffobj_del(pubKey);

		TUTRACE((TUTRACE_ERR, "RPROTO: create DHSecret\n"));
	}

	/* Extract the DH public key */
	len = BN_bn2bin(regInfo->DHSecret->pub_key, regInfo->pke);
	if (0 == len) {
		return RPROT_ERR_CRYPTO;
	}

	/* Now start composing the message */
	tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
	tlv_serialize(WPS_ID_UUID_E, msg, regInfo->p_enrolleeInfo->uuid, SIZE_UUID);
	tlv_serialize(WPS_ID_MAC_ADDR, msg, regInfo->p_enrolleeInfo->macAddr, SIZE_MAC_ADDR);
	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_PUBLIC_KEY, msg, regInfo->pke, SIZE_PUB_KEY);
	tlv_serialize(WPS_ID_AUTH_TYPE_FLAGS, msg, &regInfo->p_enrolleeInfo->authTypeFlags,
		WPS_ID_AUTH_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_ENCR_TYPE_FLAGS, msg, &regInfo->p_enrolleeInfo->encrTypeFlags,
		WPS_ID_ENCR_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_CONN_TYPE_FLAGS, msg, &regInfo->p_enrolleeInfo->connTypeFlags,
		WPS_ID_CONN_TYPE_FLAGS_S);
	tlv_serialize(WPS_ID_CONFIG_METHODS, msg, &regInfo->p_enrolleeInfo->configMethods,
		WPS_ID_CONFIG_METHODS_S);
	tlv_serialize(WPS_ID_SC_STATE, msg, &regInfo->p_enrolleeInfo->scState, WPS_ID_SC_STATE_S);
	tlv_serialize(WPS_ID_MANUFACTURER, msg, regInfo->p_enrolleeInfo->manufacturer,
		strlen(regInfo->p_enrolleeInfo->manufacturer));
	tlv_serialize(WPS_ID_MODEL_NAME, msg, regInfo->p_enrolleeInfo->modelName,
		strlen(regInfo->p_enrolleeInfo->modelName));
	tlv_serialize(WPS_ID_MODEL_NUMBER, msg, regInfo->p_enrolleeInfo->modelNumber,
		strlen(regInfo->p_enrolleeInfo->modelNumber));
	tlv_serialize(WPS_ID_SERIAL_NUM, msg, regInfo->p_enrolleeInfo->serialNumber,
		strlen(regInfo->p_enrolleeInfo->serialNumber));

	primDev.categoryId = regInfo->p_enrolleeInfo->primDeviceCategory;
	primDev.oui = regInfo->p_enrolleeInfo->primDeviceOui;
	primDev.subCategoryId = regInfo->p_enrolleeInfo->primDeviceSubCategory;
	tlv_primDeviceTypeWrite(&primDev, msg);
	tlv_serialize(WPS_ID_DEVICE_NAME, msg, regInfo->p_enrolleeInfo->deviceName,
		strlen(regInfo->p_enrolleeInfo->deviceName));

	/* set real RF band */
	tlv_serialize(WPS_ID_RF_BAND, msg, &regInfo->p_enrolleeInfo->rfBand, WPS_ID_RF_BAND_S);
	tlv_serialize(WPS_ID_ASSOC_STATE, msg, &regInfo->p_enrolleeInfo->assocState,
		WPS_ID_ASSOC_STATE_S);

	tlv_serialize(WPS_ID_DEVICE_PWD_ID, msg, &regInfo->p_enrolleeInfo->devPwdId,
		WPS_ID_DEVICE_PWD_ID_S);
	tlv_serialize(WPS_ID_CONFIG_ERROR, msg, &regInfo->p_enrolleeInfo->configError,
		WPS_ID_CONFIG_ERROR_S);
	tlv_serialize(WPS_ID_OS_VERSION, msg, &regInfo->p_enrolleeInfo->osVersion,
		WPS_ID_OS_VERSION_S);

	/* skip optional attributes */

	/* copy message to outMsg buffer */
	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM1 copying to output %d bytes\n",
		buffobj_Length(msg)));

	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, buffobj_Length(msg), buffobj_GetBuf(msg));

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM1 built: %d bytes\n",
		buffobj_Length(msg)));

	return WPS_SUCCESS;
}

uint32
reg_proto_process_m2(RegData *regInfo, BufferObj *msg, void **encrSettings)
{
	WpsM2 m;
	uint8 *Pos;
	uint32 err, tlv_err = 0;
	uint8 DHKey[SIZE_256_BITS];
	BufferObj *buf1 = buffobj_new();
	BufferObj *buf1_dser = buffobj_new();
	BufferObj *buf2_dser = buffobj_new();
	uint8 secret[SIZE_PUB_KEY];
	int secretLen;
	BufferObj *kdkBuf;
	BufferObj *kdkData;
	BufferObj *pString;
	BufferObj *keys;
	BufferObj *hmacData;
	uint8 kdk[SIZE_256_BITS];
	uint8 null_uuid[SIZE_16_BYTES] = {0};

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM2: %d byte message\n",
		buffobj_Length(msg)));

	/* First, deserialize (parse) the message. */

	/* First and foremost, check the version and message number */
	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_M2, msg)) != WPS_SUCCESS)
		goto error;

	reg_msg_init(&m, WPS_ID_MESSAGE_M2);

	tlv_err |= tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE,
		msg, SIZE_128_BITS, 0);
	tlv_err |= tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE,
		msg, SIZE_128_BITS, 0);
	tlv_err |= tlv_dserialize(&m.uuid, WPS_ID_UUID_R, msg, SIZE_UUID, 0);
	tlv_err |= tlv_dserialize(&m.publicKey, WPS_ID_PUBLIC_KEY, msg, SIZE_PUB_KEY, 0);
	tlv_err |= tlv_dserialize(&m.authTypeFlags, WPS_ID_AUTH_TYPE_FLAGS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.encrTypeFlags, WPS_ID_ENCR_TYPE_FLAGS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.connTypeFlags, WPS_ID_CONN_TYPE_FLAGS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.configMethods, WPS_ID_CONFIG_METHODS, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.manufacturer, WPS_ID_MANUFACTURER,
		msg, SIZE_64_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.modelName, WPS_ID_MODEL_NAME, msg, SIZE_32_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.modelNumber, WPS_ID_MODEL_NUMBER,
		msg, SIZE_32_BYTES, 0);
	tlv_err |= tlv_dserialize(&m.serialNumber, WPS_ID_SERIAL_NUM, msg, SIZE_32_BYTES, 0);

	tlv_err |= tlv_primDeviceTypeParse(&m.primDeviceType, msg);
	tlv_err |= tlv_dserialize(&m.deviceName, WPS_ID_DEVICE_NAME,
		msg, SIZE_32_BYTES, 1);
	tlv_err |= tlv_dserialize(&m.rfBand, WPS_ID_RF_BAND, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.assocState, WPS_ID_ASSOC_STATE, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.configError, WPS_ID_CONFIG_ERROR, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.devPwdId, WPS_ID_DEVICE_PWD_ID, msg, 0, 0);
	tlv_err |= tlv_dserialize(&m.osVersion, WPS_ID_OS_VERSION, msg, 0, 0);

	/* for VISTA TEST */
	memcpy(regInfo->reg2Nonce, m.registrarNonce.m_data, m.registrarNonce.tlvbase.m_len);

	TUTRACE((TUTRACE_INFO, "RPROTO: m2.authTypeFlags.m_data()=%x, authTypeFlags=%x, "
		"m2.encrTypeFlags.m_data()=%x, encrTypeFlags=%x  \n",
		m.authTypeFlags.m_data, regInfo->p_enrolleeInfo->authTypeFlags,
		m.encrTypeFlags.m_data, regInfo->p_enrolleeInfo->encrTypeFlags));

	if (((m.authTypeFlags.m_data & regInfo->p_enrolleeInfo->authTypeFlags) == 0) ||
		((m.encrTypeFlags.m_data & regInfo->p_enrolleeInfo->encrTypeFlags) == 0)) {
		err = RPROT_ERR_AUTH_ENC_FLAG;
		goto error;
	}

	/* AP have received M2,it must ignor other M2 */
	if (regInfo->p_registrarInfo &&
	    memcmp(regInfo->p_registrarInfo->uuid, null_uuid, SIZE_16_BYTES) != 0 &&
	    memcmp(regInfo->p_registrarInfo->uuid, m.uuid.m_data, m.uuid.tlvbase.m_len) != 0) {
		TUTRACE((TUTRACE_ERR, "\n***********  multiple M2 !!!! *********\n"));
		err = RPROT_ERR_MULTIPLE_M2;
		goto error;
	}

	/* for VISTA TEST -- */
	if (tlv_err) {
		err =  RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/*
	 * skip the vendor extensions and any other optional TLVs until
	 * we get to the authenticator
	 */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		/* optional encrypted settings */
		if (WPS_ID_ENCR_SETTINGS == buffobj_NextType(msg) &&
			m.encrSettings.encrDataLength == 0) {
			/* only process the first encrypted settings attribute encountered */
			tlv_err |= tlv_encrSettingsParse(&m.encrSettings, msg);
			Pos = buffobj_Pos(msg);
		}
		else {
			/* advance past the TLV */
			Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
				WpsNtohs((buffobj_Pos(msg)+sizeof(uint16))));
		}

		/*
		 * If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL) {
			err = RPROT_ERR_REQD_TLV_MISSING;
			goto error;
		}
	}

	tlv_err |= tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR,
		msg, SIZE_64_BITS, 0);
	if (tlv_err) {
		err =  RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* Now start processing the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		err = RPROT_ERR_NONCE_MISMATCH;
		goto error;
	}

	/*
	  * to verify the hmac, we need to process the nonces, generate
	  * the DH secret, the KDK and finally the auth key
	  */
	memcpy(regInfo->registrarNonce, m.registrarNonce.m_data, m.registrarNonce.tlvbase.m_len);

	/*
	 * read the registrar's public key
	 * First store the raw public key (to be used for e/rhash computation)
	 */
	memcpy(regInfo->pkr, m.publicKey.m_data, SIZE_PUB_KEY);

	/* Next, allocate memory for the pub key */
	regInfo->DH_PubKey_Peer = BN_new();
	if (!regInfo->DH_PubKey_Peer) {
		err = WPS_ERR_OUTOFMEMORY;
		goto error;
	}

	/* Finally, import the raw key into the bignum datastructure */
	if (BN_bin2bn(regInfo->pkr, SIZE_PUB_KEY,
		regInfo->DH_PubKey_Peer) == NULL) {
		err =  RPROT_ERR_CRYPTO;
		goto error;
	}

	/* KDK generation */
	/* 1. generate the DH shared secret */
	secretLen = DH_compute_key_bn(secret, regInfo->DH_PubKey_Peer,
		regInfo->DHSecret);
	if (secretLen == -1) {
		err = RPROT_ERR_CRYPTO;
		goto error;
	}

	/* 2. compute the DHKey based on the DH secret */
	if (SHA256(secret, secretLen, DHKey) == NULL) {
		TUTRACE((TUTRACE_ERR, "RPROTO: SHA256 calculation failed\n"));
		err = RPROT_ERR_CRYPTO;
		goto error;
	}

	/* 3. Append the enrollee nonce(N1), enrollee mac and registrar nonce(N2) */
	kdkData = buf1;
	buffobj_Reset(kdkData);
	buffobj_Append(kdkData, SIZE_128_BITS, regInfo->enrolleeNonce);
	buffobj_Append(kdkData, SIZE_MAC_ADDR, regInfo->p_enrolleeInfo->macAddr);
	buffobj_Append(kdkData, SIZE_128_BITS, regInfo->registrarNonce);

	/* 4. now generate the KDK */
	hmac_sha256(DHKey, SIZE_256_BITS,
		buffobj_GetBuf(kdkData), buffobj_Length(kdkData), kdk, NULL);

	/* KDK generation */
	/* Derivation of AuthKey, KeyWrapKey and EMSK */
	/* 1. declare and initialize the appropriate buffer objects */
	kdkBuf = buf1_dser;
	buffobj_dserial(kdkBuf, kdk, SIZE_256_BITS);
	pString = buf2_dser;
	buffobj_dserial(pString, (uint8 *)PERSONALIZATION_STRING,
		strlen(PERSONALIZATION_STRING));
	keys = buf1;
	buffobj_Reset(keys);

	/* 2. call the key derivation function */
	reg_proto_derivekey(kdkBuf, pString, KDF_KEY_BITS, keys);

	/* 3. split the key into the component keys and store them */
	buffobj_RewindLength(keys, buffobj_Length(keys));
	buffobj_Append(regInfo->authKey, SIZE_256_BITS, buffobj_Pos(keys));
	buffobj_Advance(keys, SIZE_256_BITS);

	buffobj_Append(regInfo->keyWrapKey, SIZE_128_BITS, buffobj_Pos(keys));
	buffobj_Advance(keys, SIZE_128_BITS);

	buffobj_Append(regInfo->emsk, SIZE_256_BITS, buffobj_Pos(keys));
	/* Derivation of AuthKey, KeyWrapKey and EMSK */

	/* HMAC validation */
	hmacData = buf1;
	buffobj_Reset(hmacData);
	/* append the last message sent */
	buffobj_Append(hmacData, buffobj_Length(regInfo->outMsg), buffobj_GetBuf(regInfo->outMsg));

	/* append the current message. Don't append the last TLV (auth) */
	buffobj_Append(hmacData,
	        msg->m_dataLength -(sizeof(WpsTlvHdr)+ m.authenticator.tlvbase.m_len),
	        msg->pBase);

	if (!reg_proto_validate_mac(hmacData, m.authenticator.m_data, regInfo->authKey)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: HMAC validation failed\n"));
		err = RPROT_ERR_CRYPTO;
		goto error;
	}
	/* HMAC validation */

	/* Now we can proceed with copying out and processing the other data */
	if (!regInfo->p_registrarInfo) {
		regInfo->p_registrarInfo = (DevInfo *)alloc_init(sizeof(DevInfo));
		if (!regInfo->p_registrarInfo) {
			err = WPS_ERR_OUTOFMEMORY;
			goto error;
		}
	}
	memcpy(regInfo->p_registrarInfo->uuid, m.uuid.m_data, m.uuid.tlvbase.m_len);

	regInfo->p_registrarInfo->authTypeFlags = m.authTypeFlags.m_data;
	regInfo->p_registrarInfo->encrTypeFlags = m.encrTypeFlags.m_data;
	regInfo->p_registrarInfo->connTypeFlags = m.connTypeFlags.m_data;
	regInfo->p_registrarInfo->configMethods = m.configMethods.m_data;
	strncpy(regInfo->p_registrarInfo->manufacturer, m.manufacturer.m_data, SIZE_64_BYTES);
	strncpy(regInfo->p_registrarInfo->modelName, m.modelName.m_data, SIZE_32_BYTES);
	strncpy(regInfo->p_registrarInfo->serialNumber, m.serialNumber.m_data, SIZE_32_BYTES);
	regInfo->p_registrarInfo->primDeviceCategory = m.primDeviceType.categoryId;
	regInfo->p_registrarInfo->primDeviceOui = m.primDeviceType.oui;
	regInfo->p_registrarInfo->primDeviceSubCategory = m.primDeviceType.subCategoryId;
	strncpy(regInfo->p_registrarInfo->deviceName, m.deviceName.m_data, SIZE_32_BYTES);
	regInfo->p_registrarInfo->rfBand = m.rfBand.m_data;
	regInfo->p_registrarInfo->assocState = m.assocState.m_data;
	regInfo->p_registrarInfo->configError = m.configError.m_data;
	regInfo->p_registrarInfo->devPwdId = m.devPwdId.m_data;
	regInfo->p_registrarInfo->osVersion = m.osVersion.m_data;

	/* extract encrypted settings */
	if (m.encrSettings.encrDataLength) {
		BufferObj *cipherText = buf1_dser;
		BufferObj *plainText = 0;
		BufferObj *iv = 0;
		buffobj_dserial(cipherText, m.encrSettings.ip_encryptedData,
			m.encrSettings.encrDataLength);
		iv = buf2_dser;
		buffobj_dserial(iv, m.encrSettings.iv, SIZE_128_BITS);
		plainText = buf1;
		buffobj_Reset(plainText);
		reg_proto_decrypt_data(cipherText, iv, regInfo->keyWrapKey,
			regInfo->authKey, plainText);

		/* Usually, only out of band method gets enrSettings at M2 stage */
		if (encrSettings) {
			if (regInfo->p_enrolleeInfo->b_ap) {
				CTlvEsM8Ap *esAP = reg_msg_m8ap_new();
				reg_msg_m8ap_parse(esAP, plainText, regInfo->authKey, true);
				*encrSettings = (void *)esAP;
			}
			else {
				CTlvEsM8Sta *esSta = reg_msg_m8sta_new();
				reg_msg_m8sta_parse(esSta, plainText, regInfo->authKey, true);
				*encrSettings = (void *)esSta;
			}
		}
	}
	/* extract encrypted settings */

	/*
	 * now set the registrar's b_ap flag. If the local enrollee is an ap,
	 * the registrar shouldn't be one
	 */
	if (regInfo->p_enrolleeInfo->b_ap)
		regInfo->p_registrarInfo->b_ap = true;

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);
	err = WPS_SUCCESS;

error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM2 : got error %x\n", err));
	buffobj_del(buf1);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

uint32
reg_proto_process_m2d(RegData *regInfo, BufferObj *msg)
{
	WpsM2 m;
	uint32 err;

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM2D: %d byte message\n",
		msg->m_dataLength));

	/* First, deserialize (parse) the message. */

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_M2D, msg)) != WPS_SUCCESS)
		goto error;

	reg_msg_init(&m, WPS_ID_MESSAGE_M2D);

	tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE, msg, SIZE_128_BITS, 0);

	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE, msg, SIZE_128_BITS, 0);
	tlv_dserialize(&m.uuid, WPS_ID_UUID_R, msg, SIZE_UUID, 0);
	/* No Public Key in M2D */
	tlv_dserialize(&m.authTypeFlags, WPS_ID_AUTH_TYPE_FLAGS, msg, 0, 0);
	tlv_dserialize(&m.encrTypeFlags, WPS_ID_ENCR_TYPE_FLAGS, msg, 0, 0);
	tlv_dserialize(&m.connTypeFlags, WPS_ID_CONN_TYPE_FLAGS, msg, 0, 0);
	tlv_dserialize(&m.configMethods, WPS_ID_CONFIG_METHODS, msg, 0, 0);
	tlv_dserialize(&m.manufacturer, WPS_ID_MANUFACTURER, msg, SIZE_64_BYTES, 0);
	tlv_dserialize(&m.modelName, WPS_ID_MODEL_NAME, msg, SIZE_32_BYTES, 0);
	tlv_dserialize(&m.modelNumber, WPS_ID_MODEL_NUMBER, msg, SIZE_32_BYTES, 0);
	tlv_dserialize(&m.serialNumber, WPS_ID_SERIAL_NUM, msg, SIZE_32_BYTES, 0);

	tlv_primDeviceTypeParse(&m.primDeviceType, msg);

	tlv_dserialize(&m.deviceName, WPS_ID_DEVICE_NAME, msg, SIZE_32_BYTES, 1);
	tlv_dserialize(&m.rfBand, WPS_ID_RF_BAND, msg, 0, 0);
	tlv_dserialize(&m.assocState, WPS_ID_ASSOC_STATE, msg, 0, 0);
	tlv_dserialize(&m.configError, WPS_ID_CONFIG_ERROR, msg, 0, 0);
	tlv_dserialize(&m.osVersion, WPS_ID_OS_VERSION, msg, 0, 0);

	/* ignore any other TLVs in the message */

	/* Now start processing the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		err = RPROT_ERR_NONCE_MISMATCH;
		goto error;
	}

	/* for VISTA client issue */
	/*
	 * memcpy(regInfo->registrarNonce, m.registrarNonce.m_data,
	 * m.registrarNonce.tlvbase.m_len);
	 */
	memcpy(regInfo->reg2Nonce, m.registrarNonce.m_data, m.registrarNonce.tlvbase.m_len);

	if (!regInfo->p_registrarInfo) {
		regInfo->p_registrarInfo = (DevInfo *)alloc_init(sizeof(DevInfo));
		if (!regInfo->p_registrarInfo) {
			err = WPS_ERR_OUTOFMEMORY;
			goto error;
		}
	}

	/* Now we can proceed with copying out and processing the other data */
	/*
	memcpy(regInfo->p_registrarInfo->uuid, m.uuid.m_data, m.uuid.tlvbase.m_len);
	*/

	/* No public key in M2D */
	if (0 == (m.authTypeFlags.m_data && 0x3F)) {
		err =  RPROT_ERR_INCOMPATIBLE;
		goto error;
	}
	regInfo->p_registrarInfo->authTypeFlags = m.authTypeFlags.m_data;

	if (0 == (m.encrTypeFlags.m_data && 0x0F)) {
		err = RPROT_ERR_INCOMPATIBLE;
		goto error;
	}
	regInfo->p_registrarInfo->encrTypeFlags = m.encrTypeFlags.m_data;
	regInfo->p_registrarInfo->connTypeFlags = m.connTypeFlags.m_data;
	regInfo->p_registrarInfo->configMethods = m.configMethods.m_data;
	strncpy(regInfo->p_registrarInfo->manufacturer, m.manufacturer.m_data, SIZE_32_BYTES);
	strncpy(regInfo->p_registrarInfo->modelName, m.modelName.m_data, SIZE_32_BYTES);
	strncpy(regInfo->p_registrarInfo->serialNumber, m.serialNumber.m_data, SIZE_32_BYTES);
	regInfo->p_registrarInfo->primDeviceCategory = m.primDeviceType.categoryId;
	regInfo->p_registrarInfo->primDeviceOui = m.primDeviceType.oui;
	regInfo->p_registrarInfo->primDeviceSubCategory = m.primDeviceType.subCategoryId;
	strncpy(regInfo->p_registrarInfo->deviceName, m.deviceName.m_data, SIZE_32_BYTES);
	regInfo->p_registrarInfo->rfBand = m.rfBand.m_data;
	regInfo->p_registrarInfo->assocState = m.assocState.m_data;
	regInfo->p_registrarInfo->configError = m.configError.m_data;
	/* No Dev Pwd Id in this message: regInfo->p_registrarInfo->devPwdId = m.devPwdId.m_data; */

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);
	err = WPS_SUCCESS;

error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM2D : got error %x", err));

	return err;
}

uint32
reg_proto_create_m3(RegData *regInfo, BufferObj *msg)
{
	uint8 message;
	uint8 hashBuf[SIZE_256_BITS];
	uint32 err;
	BufferObj *hmacData;
	uint8 hmac[SIZE_256_BITS];
	BufferObj *ehashBuf;
	uint8 *pwdPtr = 0;
	int pwdLen;
	/* First, generate or gather the required data */
	uint8 reg_proto_version = WPS_VERSION;

	BufferObj *buf1 = buffobj_new();
	message = WPS_ID_MESSAGE_M3;

	/* PSK1 and PSK2 generation */
	pwdPtr = regInfo->password->pBase;
	pwdLen = regInfo->password->m_dataLength;

	/*
	 * Hash 1st half of passwd. If it is an odd length, the extra byte
	 * goes along with the first half
	 */
	hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		pwdPtr, (pwdLen/2)+(pwdLen%2), hashBuf, NULL);

	/* copy first 128 bits into psk1; */
	memcpy(regInfo->psk1, hashBuf, SIZE_128_BITS);

	/* Hash 2nd half of passwd */
	hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		pwdPtr+(pwdLen/2)+(pwdLen%2), pwdLen/2, hashBuf, NULL);

	/* copy first 128 bits into psk2; */
	memcpy(regInfo->psk2, hashBuf, SIZE_128_BITS);
	/* PSK1 and PSK2 generation */

	/* EHash1 and EHash2 generation */
	RAND_bytes(regInfo->es1, SIZE_128_BITS);
	RAND_bytes(regInfo->es2, SIZE_128_BITS);

	ehashBuf = buf1;
	buffobj_Reset(ehashBuf);
	buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->es1);
	buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->psk1);
	buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pke);
	buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pkr);
	hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		ehashBuf->pBase, ehashBuf->m_dataLength, hashBuf, NULL);

	memcpy(regInfo->eHash1, hashBuf, SIZE_256_BITS);

	buffobj_Reset(ehashBuf);
	buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->es2);
	buffobj_Append(ehashBuf, SIZE_128_BITS, regInfo->psk2);
	buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pke);
	buffobj_Append(ehashBuf, SIZE_PUB_KEY, regInfo->pkr);
	hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		ehashBuf->pBase, ehashBuf->m_dataLength, hashBuf, NULL);

	memcpy(regInfo->eHash2, hashBuf, SIZE_256_BITS);
	/* EHash1 and EHash2 generation */

	/* Now assemble the message */
	tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
	tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, regInfo->registrarNonce, SIZE_128_BITS);

	tlv_serialize(WPS_ID_E_HASH1, msg, regInfo->eHash1, SIZE_256_BITS);
	tlv_serialize(WPS_ID_E_HASH2, msg, regInfo->eHash2, SIZE_256_BITS);
	/* No vendor extension */

	/* Calculate the hmac */
	hmacData = buf1;
	buffobj_Reset(hmacData);
	buffobj_Append(hmacData, regInfo->inMsg->m_dataLength, regInfo->inMsg->pBase);
	buffobj_Append(hmacData, msg->m_dataLength, msg->pBase);

	hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
		hmacData->pBase, hmacData->m_dataLength, hmac, NULL);
	tlv_serialize(WPS_ID_AUTHENTICATOR, msg, hmac, SIZE_64_BITS);

	/* Store the outgoing message  */
	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, msg->m_dataLength, msg->pBase);

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM3 built: %d bytes\n",
		msg->m_dataLength));
	err = WPS_SUCCESS;

	buffobj_del(buf1);
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "BuildMessageM3 : got error %x", err));

	return err;
}

uint32
reg_proto_process_m4(RegData *regInfo, BufferObj *msg)
{
	uint8 *Pos;
	WpsM4 m;
	uint32 err, tlv_err = 0;

	BufferObj *buf1 = buffobj_new();
	BufferObj *buf1_dser = buffobj_new();
	BufferObj *buf2_dser = buffobj_new();
	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM4: %d byte message\n",
		msg->m_dataLength));
	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_M4, msg)) != WPS_SUCCESS)
		goto error;

	reg_msg_init(&m, WPS_ID_MESSAGE_M4);

	tlv_err |= tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE,
		msg, SIZE_128_BITS, 0);

	tlv_err |= tlv_dserialize(&m.rHash1, WPS_ID_R_HASH1, msg, SIZE_256_BITS, 0);
	tlv_err |= tlv_dserialize(&m.rHash2, WPS_ID_R_HASH2, msg, SIZE_256_BITS, 0);

	tlv_err |= tlv_encrSettingsParse(&m.encrSettings, msg);
	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* skip all optional attributes until we get to the authenticator */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		TUTRACE((TUTRACE_ERR, "reg_proto_process_m4: optional attribute found! "
			"type = %04x, len=%d \n", buffobj_NextType(msg),
			WpsNtohs((uint8 *)(msg->pCurrent+sizeof(uint16)))));
		/* advance past the TLV */
		Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
			WpsNtohs((msg->pCurrent+sizeof(uint16))));

		/*
		 * If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL) {
			err =  RPROT_ERR_REQD_TLV_MISSING;
			goto error;
		}
	}

	tlv_err |= tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR,
		msg, SIZE_64_BITS, 0);
	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* Now start validating the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		err =  RPROT_ERR_NONCE_MISMATCH;
		goto error;
	}

	/* HMAC validation */
	{
		BufferObj *hmacData = buf1;
		buffobj_Reset(hmacData);
		/* append the last message sent */
		buffobj_Append(hmacData, regInfo->outMsg->m_dataLength, regInfo->outMsg->pBase);
		/* append the current message. Don't append the last TLV (auth) */
		buffobj_Append(hmacData,
			msg->m_dataLength-(sizeof(WpsTlvHdr)+m.authenticator.tlvbase.m_len),
			msg->pBase);

		if (!reg_proto_validate_mac(hmacData, m.authenticator.m_data, regInfo->authKey)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: HMAC validation failed"));
			err =  RPROT_ERR_CRYPTO;
			goto error;
		}
	}
	/* HMAC validation */

	/* Now copy the relevant data */
	memcpy(regInfo->rHash1, m.rHash1.m_data, SIZE_256_BITS);
	memcpy(regInfo->rHash2, m.rHash2.m_data, SIZE_256_BITS);

	/* extract encrypted settings */
	{
		BufferObj *cipherText = buf1_dser;
		BufferObj *iv = 0;
		BufferObj *plainText = 0;
		TlvEsNonce rNonce;

		buffobj_dserial(cipherText, m.encrSettings.ip_encryptedData,
		                  m.encrSettings.encrDataLength);
		iv = buf2_dser;
		buffobj_dserial(iv, m.encrSettings.iv, SIZE_128_BITS);
		plainText = buf1;
		buffobj_Reset(plainText);
		reg_proto_decrypt_data(cipherText, iv, regInfo->keyWrapKey,
			regInfo->authKey, plainText);
		reg_msg_nonce_parse(&rNonce, WPS_ID_R_SNONCE1, plainText, regInfo->authKey);

		/* extract encrypted settings */

		/* RHash1 validation */
		/* 1. Save RS1 */
		memcpy(regInfo->rs1, rNonce.nonce.m_data, rNonce.nonce.tlvbase.m_len);
	}

	/* 2. prepare the buffer */
	{
		uint8 hashBuf[SIZE_256_BITS];
		BufferObj *rhashBuf = buf1;
		buffobj_Reset(rhashBuf);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->rs1);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->psk1);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pke);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pkr);

		/* 3. generate the mac */
		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			rhashBuf->pBase, rhashBuf->m_dataLength, hashBuf, NULL);

		/* 4. compare the mac to rhash1 */
		if (memcmp(regInfo->rHash1, hashBuf, SIZE_256_BITS)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: RS1 hash doesn't match RHash1\n"));
			err =  RPROT_ERR_CRYPTO;
			goto error;
		}
	}
	/* 5. Instead of steps 3 & 4, we could have called ValidateMac */
	/* RHash1 validation */

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);
	err = WPS_SUCCESS;
	TUTRACE((TUTRACE_INFO, "RPROTO: ProcessMessageM4 done: %d bytes\n",
		msg->m_dataLength));
error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM4: got error %x\n", err));

	buffobj_del(buf1);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

uint32
reg_proto_create_m5(RegData *regInfo, BufferObj *msg)
{
	uint8 message;
	uint32 err;
	uint8 reg_proto_version = WPS_VERSION;

	BufferObj *buf1 = buffobj_new();
	BufferObj *buf2 = buffobj_new();
	BufferObj *buf3 = buffobj_new();
	/* First, generate or gather the required data */
	message = WPS_ID_MESSAGE_M5;

	/* encrypted settings. */
	{
		BufferObj *encData = buf1;
		BufferObj *cipherText = buf2;
		BufferObj *iv = buf3;
		TlvEsNonce esNonce;
		buffobj_Reset(encData);
		buffobj_Reset(cipherText);
		buffobj_Reset(iv);
		tlv_set(&esNonce.nonce, WPS_ID_E_SNONCE1, regInfo->es1, SIZE_128_BITS);
		reg_msg_nonce_write(&esNonce, encData, regInfo->authKey);

		reg_proto_encrypt_data(encData, regInfo->keyWrapKey,
			regInfo->authKey, cipherText, iv);

		/* Now assemble the message */
		tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
		tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
		tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, regInfo->registrarNonce, SIZE_128_BITS);
		{
			CTlvEncrSettings encrSettings;
			encrSettings.iv = iv->pBase;
			encrSettings.ip_encryptedData = cipherText->pBase;
			encrSettings.encrDataLength = cipherText->m_dataLength;
			tlv_encrSettingsWrite(&encrSettings, msg);
		}
	}
	/* No vendor extension */

	/* Calculate the hmac */
	{
		BufferObj *hmacData = buf1;
		uint8 hmac[SIZE_256_BITS];
		buffobj_Reset(hmacData);
		buffobj_Append(hmacData, regInfo->inMsg->m_dataLength, regInfo->inMsg->pBase);
		buffobj_Append(hmacData, msg->m_dataLength, msg->pBase);

		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			hmacData->pBase, hmacData->m_dataLength, hmac, NULL);

		tlv_serialize(WPS_ID_AUTHENTICATOR, msg, hmac, SIZE_64_BITS);
	}

	/* Store the outgoing message */
	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, msg->m_dataLength, msg->pBase);


	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM5 built: %d bytes\n",
		msg->m_dataLength));
	err = WPS_SUCCESS;

	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "BuildMessageM5 : got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf2);
	buffobj_del(buf3);

	return err;
}

uint32
reg_proto_process_m6(RegData *regInfo, BufferObj *msg)
{
	uint8 *Pos;
	WpsM6 m;
	uint32 err, tlv_err = 0;

	BufferObj *buf1 = buffobj_new();
	BufferObj *buf1_dser = buffobj_new();
	BufferObj *buf2_dser = buffobj_new();
	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM6: %d byte message\n",
		msg->m_dataLength));

	/*
	 * First, deserialize (parse) the message.
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_M6, msg)) != WPS_SUCCESS)
		goto error;

	reg_msg_init(&m, WPS_ID_MESSAGE_M6);
	tlv_err |= tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE,
		msg, SIZE_128_BITS, 0);

	tlv_err |= tlv_encrSettingsParse(&m.encrSettings, msg);
	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* skip all optional attributes until we get to the authenticator */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		TUTRACE((TUTRACE_ERR, "reg_proto_process_m6: optional attribute found! "
			"type = %04x, len=%d \n", buffobj_NextType(msg),
			WpsNtohs((uint8 *)(msg->pCurrent+sizeof(uint16)))));

		/* advance past the TLV */
		Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
			WpsNtohs((msg->pCurrent+sizeof(uint16))));

		/*
		 * If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL) {
			err =  RPROT_ERR_REQD_TLV_MISSING;
			goto error;
		}
	}

	tlv_err |= tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR,
		msg, SIZE_64_BITS, 0);
	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* Now start validating the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		err = RPROT_ERR_NONCE_MISMATCH;
		goto error;
	}

	/* HMAC validation */
	{
		BufferObj *hmacData = buf1;
		buffobj_Reset(hmacData);
		/* append the last message sent */
		buffobj_Append(hmacData, regInfo->outMsg->m_dataLength, regInfo->outMsg->pBase);
		/* append the current message. Don't append the last TLV (auth) */
		buffobj_Append(hmacData,
			msg->m_dataLength-(sizeof(WpsTlvHdr)+m.authenticator.tlvbase.m_len),
			msg->pBase);

		if (!reg_proto_validate_mac(hmacData, m.authenticator.m_data, regInfo->authKey)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: HMAC validation failed"));
			err = RPROT_ERR_CRYPTO;
			goto error;
		}
	}
	/* HMAC validation */

	/* extract encrypted settings */
	{
		BufferObj *cipherText = buf1_dser;
		BufferObj *iv = 0;
		BufferObj *plainText = 0;
		TlvEsNonce rNonce;

		buffobj_dserial(cipherText, m.encrSettings.ip_encryptedData,
			m.encrSettings.encrDataLength);
		iv = buf2_dser;
		buffobj_dserial(iv, m.encrSettings.iv, SIZE_128_BITS);
		plainText = buf1;
		buffobj_Reset(plainText);
		reg_proto_decrypt_data(cipherText, iv, regInfo->keyWrapKey,
			regInfo->authKey, plainText);
		reg_msg_nonce_parse(&rNonce, WPS_ID_R_SNONCE2, plainText, regInfo->authKey);

		/* extract encrypted settings */

		/* RHash2 validation */
		/* 1. Save RS2 */
		memcpy(regInfo->rs2, rNonce.nonce.m_data, rNonce.nonce.tlvbase.m_len);
	}

	/* 2. prepare the buffer */
	{
		uint8 hashBuf[SIZE_256_BITS];

		BufferObj *rhashBuf = buf1;
		buffobj_Reset(rhashBuf);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->rs2);
		buffobj_Append(rhashBuf, SIZE_128_BITS, regInfo->psk2);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pke);
		buffobj_Append(rhashBuf, SIZE_PUB_KEY, regInfo->pkr);

		/* 3. generate the mac */
		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			rhashBuf->pBase, rhashBuf->m_dataLength, hashBuf, NULL);

		/* 4. compare the mac to rhash2 */
		if (memcmp(regInfo->rHash2, hashBuf, SIZE_256_BITS)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: RS2 hash doesn't match RHash2\n"));
			err = RPROT_ERR_CRYPTO;
			goto error;
		}

		/* 5. Instead of steps 3 & 4, we could have called ValidateMac */
		/* RHash2 validation */
	}

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);

	err = WPS_SUCCESS;
	TUTRACE((TUTRACE_INFO, "RPROTO: ProcessMessageM6 done: %d bytes\n",
		msg->m_dataLength));
error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM6: got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

uint32
reg_proto_create_m7(RegData *regInfo, BufferObj *msg, void *encrSettings)
{
	uint8 message;
	uint32 err;
	uint8 reg_proto_version = WPS_VERSION;

	BufferObj *buf1 = buffobj_new();
	BufferObj *buf2 = buffobj_new();
	BufferObj *buf3 = buffobj_new();
	/* First, generate or gather the required data */
	message = WPS_ID_MESSAGE_M7;

	/* encrypted settings. */
	{
		CTlvEsM7Ap *apEs = 0;
		BufferObj *esBuf = buf1;
		BufferObj *cipherText = buf2;
		BufferObj *iv = buf3;
		buffobj_Reset(esBuf);
		buffobj_Reset(cipherText);
		buffobj_Reset(iv);

		if (regInfo->p_enrolleeInfo->b_ap) {
			if (!encrSettings) {
				TUTRACE((TUTRACE_ERR, "RPROTO: AP Encr settings are NULL\n"));
				err =  WPS_ERR_INVALID_PARAMETERS;
				goto error;
			}

			apEs = (CTlvEsM7Ap *)encrSettings;
			/* Set ES Nonce2 */
			tlv_set(&apEs->nonce, WPS_ID_E_SNONCE2, regInfo->es2, SIZE_128_BITS);
			reg_msg_m7ap_write(apEs, esBuf, regInfo->authKey);
		}
		else {
			TlvEsM7Enr *staEs;
			if (!encrSettings) {
				TUTRACE((TUTRACE_INFO, "RPROTO: NULL STA Encrypted settings."
					" Allocating memory...\n"));
				staEs = reg_msg_m7enr_new();
				regInfo->staEncrSettings = (void *)staEs;
			}
			else {
				staEs = (TlvEsM7Enr *)encrSettings;
			}
			/* Set ES Nonce2 */
			tlv_set(&staEs->nonce, WPS_ID_E_SNONCE2, regInfo->es2, SIZE_128_BITS);
			reg_msg_m7enr_write(staEs, esBuf, regInfo->authKey);
		}

		/* Now encrypt the serialize Encrypted settings buffer */

		reg_proto_encrypt_data(esBuf, regInfo->keyWrapKey,
			regInfo->authKey, cipherText, iv);

		/* Now assemble the message */
		tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
		tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
		tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, regInfo->registrarNonce, SIZE_128_BITS);
		{
			CTlvEncrSettings encrSettings;
			encrSettings.iv = iv->pBase;
			encrSettings.ip_encryptedData = cipherText->pBase;
			encrSettings.encrDataLength = cipherText->m_dataLength;
			tlv_encrSettingsWrite(&encrSettings, msg);
		}

		if (regInfo->x509csr->m_dataLength) {
			tlv_serialize(WPS_ID_X509_CERT_REQ, msg, regInfo->x509csr->pBase,
				regInfo->x509csr->m_dataLength);
		}
	}

	/* No vendor extension */

	/* Calculate the hmac */
	{
		uint8 hmac[SIZE_256_BITS];

		BufferObj *hmacData = buf1;
		buffobj_Reset(hmacData);
		buffobj_Append(hmacData, regInfo->inMsg->m_dataLength, regInfo->inMsg->pBase);
		buffobj_Append(hmacData, msg->m_dataLength, msg->pBase);

		hmac_sha256(regInfo->authKey->pBase, SIZE_256_BITS,
			hmacData->pBase, hmacData->m_dataLength, hmac, NULL);

		tlv_serialize(WPS_ID_AUTHENTICATOR, msg, hmac, SIZE_64_BITS);
	}

	/* Store the outgoing message  */
	buffobj_Reset(regInfo->outMsg);
	buffobj_Append(regInfo->outMsg, msg->m_dataLength, msg->pBase);
	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageM7 built: %d bytes\n",
		msg->m_dataLength));
	err = WPS_SUCCESS;
error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "BuildMessageM7 : got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf2);
	buffobj_del(buf3);

	return err;
}

uint32
reg_proto_process_m8(RegData *regInfo, BufferObj *msg, void **encrSettings)
{
	uint8 *Pos;
	WpsM8 m;
	uint32 err, tlv_err = 0;

	BufferObj *buf1 = buffobj_new();
	BufferObj *buf1_dser = buffobj_new();
	BufferObj *buf2_dser = buffobj_new();

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageM8: %d byte message\n",
		msg->m_dataLength));

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_M8, msg)) != WPS_SUCCESS)
		goto error;

	reg_msg_init(&m, WPS_ID_MESSAGE_M8);
	tlv_err |= tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE,
		msg, SIZE_128_BITS, 0);

	tlv_err |= tlv_encrSettingsParse(&m.encrSettings, msg);

	if (WPS_ID_X509_CERT == buffobj_NextType(msg))
		tlv_err |= tlv_dserialize(&m.x509Cert, WPS_ID_X509_CERT, msg, 0, 0);

	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* skip all optional attributes until we get to the authenticator */
	while (WPS_ID_AUTHENTICATOR != buffobj_NextType(msg)) {
		TUTRACE((TUTRACE_ERR, "ProcessMessageM8: optional attribute found! type = %04x, "
			"len=%d \n", buffobj_NextType(msg),
			WpsNtohs((uint8 *)(msg->pCurrent+sizeof(uint16)))));

		/* advance past the TLV */
		Pos = buffobj_Advance(msg, sizeof(WpsTlvHdr) +
			WpsNtohs((msg->pCurrent+sizeof(uint16))));

		/*
		 * If Advance returned NULL, it means there's no more data in the
		 * buffer. This is an error.
		 */
		if (Pos == NULL) {
			err =  RPROT_ERR_REQD_TLV_MISSING;
			goto error;
		}
	}

	tlv_err |= tlv_dserialize(&m.authenticator, WPS_ID_AUTHENTICATOR, msg, SIZE_64_BITS, 0);
	if (tlv_err) {
		err = RPROT_ERR_REQD_TLV_MISSING;
		goto error;
	}

	/* Now start validating the message */

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		err = RPROT_ERR_NONCE_MISMATCH;
		goto error;
	}

	/* HMAC validation */
	{
		BufferObj *hmacData = buf1;
		buffobj_Reset(hmacData);
		/* append the last message sent */
		buffobj_Append(hmacData,  regInfo->outMsg->m_dataLength, regInfo->outMsg->pBase);
		/* append the current message. Don't append the last TLV (auth) */
		buffobj_Append(hmacData,
		        msg->m_dataLength-(sizeof(WpsTlvHdr)+m.authenticator.tlvbase.m_len),
		        msg->pBase);

		if (!reg_proto_validate_mac(hmacData, m.authenticator.m_data, regInfo->authKey)) {
			TUTRACE((TUTRACE_ERR, "RPROTO: HMAC validation failed\n"));
			err = RPROT_ERR_CRYPTO;
			goto error;
		}
	}
	/* HMAC validation */

	/* extract encrypted settings */
	{
		BufferObj *cipherText = buf1_dser;
		BufferObj *iv = 0;
		BufferObj *plainText = 0;
		buffobj_dserial(cipherText, m.encrSettings.ip_encryptedData,
			m.encrSettings.encrDataLength);
		iv = buf2_dser;
		buffobj_dserial(iv, m.encrSettings.iv, SIZE_128_BITS);
		plainText = buf1;
		buffobj_Reset(plainText);
		reg_proto_decrypt_data(cipherText, iv, regInfo->keyWrapKey,
			regInfo->authKey, plainText);

		if (regInfo->p_enrolleeInfo->b_ap) {
			CTlvEsM8Ap *esAP = reg_msg_m8ap_new();
			reg_msg_m8ap_parse(esAP, plainText, regInfo->authKey, true);
			*encrSettings = (void *)esAP;
		}
		else {
			CTlvEsM8Sta *esSta = reg_msg_m8sta_new();
			reg_msg_m8sta_parse(esSta, plainText, regInfo->authKey, true);
			*encrSettings = (void *)esSta;
		}
	}
	/* extract encrypted settings */

	/* Store the received buffer */
	buffobj_Reset(regInfo->inMsg);
	buffobj_Append(regInfo->inMsg, msg->m_dataLength, msg->pBase);
	err = WPS_SUCCESS;
	TUTRACE((TUTRACE_INFO, "RPROTO: ProcessMessageM8 done: %d bytes\n",
		msg->m_dataLength));
error:
	if (err != WPS_SUCCESS)
		TUTRACE((TUTRACE_ERR, "ProcessMessageM8: got error %x", err));
	buffobj_del(buf1);
	buffobj_del(buf1_dser);
	buffobj_del(buf2_dser);

	return err;
}

uint32
reg_proto_create_ack(RegData *regInfo, BufferObj *msg)
{
	uint8 message;
	uint8 reg_proto_version = WPS_VERSION;

	TUTRACE((TUTRACE_INFO, "RPROTO: In BuildMessageAck\n"));

	message = WPS_ID_MESSAGE_ACK;

	tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, regInfo->registrarNonce, SIZE_128_BITS);

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageAck built: %d bytes\n",
		msg->m_dataLength));

	return WPS_SUCCESS;
}

uint32
reg_proto_process_ack(RegData *regInfo, BufferObj *msg)
{
	WpsACK m;
	int err;

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageAck: %d byte message\n",
		msg->m_dataLength));

	/* First, deserialize (parse) the message. */

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_ACK, msg)) != WPS_SUCCESS)
	    return err;

	reg_msg_init(&m, WPS_ID_MESSAGE_ACK);
	tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE, msg, SIZE_128_BITS, 0);
	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE, msg, SIZE_128_BITS, 0);

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		return RPROT_ERR_NONCE_MISMATCH;
	}

	/* confirm the registrar nonce */
	if (memcmp(regInfo->registrarNonce, m.registrarNonce.m_data,
		m.registrarNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect registrar nonce\n"));
		return RPROT_ERR_NONCE_MISMATCH;
	}

	/* ignore any other TLVs */

	return WPS_SUCCESS;
}

uint32
reg_proto_create_nack(RegData *regInfo, BufferObj *msg, uint16 configError)
{
	uint8 message;
	uint8 reg_proto_version = WPS_VERSION;

	TUTRACE((TUTRACE_INFO, "RPROTO: In BuildMessageNack\n"));
	message = WPS_ID_MESSAGE_NACK;
	tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, /* regInfo->registrarNonce, */
		regInfo->reg2Nonce, SIZE_128_BITS);

	tlv_serialize(WPS_ID_CONFIG_ERROR, msg, &configError, WPS_ID_CONFIG_ERROR_S);

	return WPS_SUCCESS;
}

uint32
reg_proto_process_nack(RegData *regInfo, BufferObj *msg, uint16 *configError)
{
	WpsNACK m;
	int err;

	/* First, deserialize (parse) the message. */

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */

	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageNack: %d byte message\n",
		msg->m_dataLength));
	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_NACK, msg)) != WPS_SUCCESS)
		return err;

	reg_msg_init(&m, WPS_ID_MESSAGE_NACK);
	tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE, msg, SIZE_128_BITS, 0);
	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE, msg, SIZE_128_BITS, 0);
	tlv_dserialize(&m.configError, WPS_ID_CONFIG_ERROR, msg, 0, 0);

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		return  RPROT_ERR_NONCE_MISMATCH;
	}

	/* confirm the registrar nonce */
	if (memcmp(regInfo->registrarNonce, m.registrarNonce.m_data,
		m.registrarNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect registrar nonce\n"));
		return  RPROT_ERR_NONCE_MISMATCH;
	}

	/* ignore any other TLVs */

	*configError = m.configError.m_data;

	return WPS_SUCCESS;
}

uint32
reg_proto_create_done(RegData *regInfo, BufferObj *msg)
{
	uint8 message;
	uint8 reg_proto_version = WPS_VERSION;

	TUTRACE((TUTRACE_INFO, "RPROTO: In BuildMessageDone\n"));

	message = WPS_ID_MESSAGE_DONE;
	tlv_serialize(WPS_ID_VERSION, msg, &reg_proto_version, WPS_ID_VERSION_S);
	tlv_serialize(WPS_ID_MSG_TYPE, msg, &message, WPS_ID_MSG_TYPE_S);
	tlv_serialize(WPS_ID_ENROLLEE_NONCE, msg, regInfo->enrolleeNonce, SIZE_128_BITS);
	tlv_serialize(WPS_ID_REGISTRAR_NONCE, msg, regInfo->registrarNonce, SIZE_128_BITS);

	TUTRACE((TUTRACE_INFO, "RPROTO: BuildMessageDone built: %d bytes\n",
		msg->m_dataLength));

	return WPS_SUCCESS;
}

uint32
reg_proto_process_done(RegData *regInfo, BufferObj *msg)
{
	WpsDone m;
	int err;

	/*
	 * First and foremost, check the version and message number.
	 * Don't deserialize incompatible messages!
	 */
	TUTRACE((TUTRACE_INFO, "RPROTO: In ProcessMessageDone: %d byte message\n",
		msg->m_dataLength));

	if ((err = reg_proto_verCheck(&m.version, &m.msgType,
		WPS_ID_MESSAGE_DONE, msg)) != WPS_SUCCESS)
		return err;

	reg_msg_init(&m, WPS_ID_MESSAGE_DONE);
	tlv_dserialize(&m.enrolleeNonce, WPS_ID_ENROLLEE_NONCE, msg, SIZE_128_BITS, 0);
	tlv_dserialize(&m.registrarNonce, WPS_ID_REGISTRAR_NONCE, msg, SIZE_128_BITS, 0);

	/* confirm the enrollee nonce */
	if (memcmp(regInfo->enrolleeNonce, m.enrolleeNonce.m_data,
		m.enrolleeNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect enrollee nonce\n"));
		return RPROT_ERR_NONCE_MISMATCH;
	}

	/* confirm the registrar nonce */
	if (memcmp(regInfo->registrarNonce, m.registrarNonce.m_data,
		m.registrarNonce.tlvbase.m_len)) {
		TUTRACE((TUTRACE_ERR, "RPROTO: Incorrect registrar nonce\n"));
		return RPROT_ERR_NONCE_MISMATCH;
	}

	return WPS_SUCCESS;
}
