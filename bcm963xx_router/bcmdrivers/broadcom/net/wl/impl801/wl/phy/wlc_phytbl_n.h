/*
 * Declarations for Broadcom PHY core tables,
 * Networking Adapter Device Driver.
 *
 * THIS IS A GENERATED FILE - DO NOT EDIT (ARE WE SURE??)
 * Generated on Wed Aug 30 17:06:38 PDT 2006
 *
 * Copyright(c) 2005 Broadcom Corp.
 * All Rights Reserved.
 *
 * $Id: wlc_phytbl_n.h,v 1.1 2010/08/05 21:59:01 ywu Exp $
 */
/* FILE-CSTYLED */


/* The position of the ant_swctrl_tbl in the volatile array of tables */
#define ANT_SWCTRL_TBL_REV3_IDX (0)

typedef phytbl_info_t mimophytbl_info_t;


extern CONST mimophytbl_info_t mimophytbl_info_rev0[], mimophytbl_info_rev0_volatile[];
extern CONST uint32 mimophytbl_info_sz_rev0, mimophytbl_info_sz_rev0_volatile;

extern CONST mimophytbl_info_t mimophytbl_info_rev3[], mimophytbl_info_rev3_volatile[],
        mimophytbl_info_rev3_volatile1[],  mimophytbl_info_rev3_volatile2[],
        mimophytbl_info_rev3_volatile3[];
extern CONST uint32 mimophytbl_info_sz_rev3, mimophytbl_info_sz_rev3_volatile,
        mimophytbl_info_sz_rev3_volatile1, mimophytbl_info_sz_rev3_volatile2,
        mimophytbl_info_sz_rev3_volatile3;

extern CONST uint32 noise_var_tbl_rev3[];

extern CONST mimophytbl_info_t mimophytbl_info_rev7[];
extern CONST uint32 mimophytbl_info_sz_rev7;
extern CONST uint32 noise_var_tbl_rev7[];
/* LCNXN */
extern CONST mimophytbl_info_t mimophytbl_info_rev16[];
extern CONST uint32 mimophytbl_info_sz_rev16;
