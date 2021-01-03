//*************************************************************************************
// Broadcom Corp. Confidential
// Copyright 2000, 2001, 2002 Broadcom Corp. All Rights Reserved.
//
// THIS SOFTWARE MAY ONLY BE USED SUBJECT TO AN EXECUTED
// SOFTWARE LICENSE AGREEMENT BETWEEN THE USER AND BROADCOM.
// YOU HAVE NO RIGHT TO USE OR EXPLOIT THIS MATERIAL EXCEPT
// SUBJECT TO THE TERMS OF SUCH AN AGREEMENT.
//
//**************************************************************************************
// File Name  : addvtoken.c
//
// Description: Add validation token - 20 bytes, to the firmware image file to 
//              be uploaded by CFE 'w' command. For now, just 4 byte crc in 
//              network byte order
//
// Created    : 08/18/2002  seanl
//**************************************************************************************

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netinet/in.h>

typedef unsigned char byte;
typedef unsigned int UINT32;
#define BUF_SIZE        100 * 1024

#define  BCMTAG_EXE_USE
#include "bcmTag.h"

byte buffer[BUF_SIZE];
byte vToken[TOKEN_LEN];

/***************************************************************************
// Function Name: getCrc32
// Description  : caculate the CRC 32 of the given data.
// Parameters   : pdata - array of data.
//                size - number of input data bytes.
//                crc - either CRC32_INIT_VALUE or previous return value.
// Returns      : crc.
****************************************************************************/
UINT32 getCrc32(byte *pdata, UINT32 size, UINT32 crc) 
{
    while (size-- > 0)
        crc = (crc >> 8) ^ Crc32_table[(crc ^ *pdata++) & 0xff];

    return crc;
}


int main(int argc, char **argv)
{
    FILE *stream_in, *stream_out;
    int total = 0, count, numWritten = 0;
    UINT32 imageCrc;
    PWFI_TAG pWt = (PWFI_TAG) vToken;

    memset(vToken, 0, TOKEN_LEN);

    while( argc > 1 && argv[1][0] == '-' && argv[1][1] == '-' )
    {
        if( !strcmp(&argv[1][2], "chip") )
        {
            pWt->wfiChipId = htonl(strtoul(argv[2], NULL, 16));
            pWt->wfiVersion = htonl(WFI_VERSION);
        }
        else if( !strcmp(&argv[1][2], "flashtype") )
        {
            if( !strcmp(argv[2], "NOR") )
                pWt->wfiFlashType = htonl(WFI_NOR_FLASH);
            else if( !strcmp(argv[2], "NAND16") )
                pWt->wfiFlashType = htonl(WFI_NAND16_FLASH);
            else if( !strcmp(argv[2], "NAND128") )
                pWt->wfiFlashType = htonl(WFI_NAND128_FLASH);
            else
            {
                printf("invalid flash type '%s'\n", argv[2]);
                exit(1);
            }
            pWt->wfiVersion = htonl(WFI_VERSION);
        }
        else
        {
            printf("invalid option '%s'\n", argv[1]);
            exit(1);
        }

        argc -= 2;
        argv += 2;
    }

    if( pWt->wfiVersion == htonl(WFI_VERSION) &&
        (pWt->wfiChipId == 0 || pWt->wfiFlashType == 0) )
    {
        printf("options --chip and --flashtype must both be specified\n");
        exit(1);
    }

    if (argc != 3)
    {
        printf("Usage:\n");
        printf("addcrc input-file tag-output-file\n");
        exit(1);
    }
    if( (stream_in  = fopen(argv[1], "rb")) == NULL)
    {
	     printf("failed on open input file: %s\n", argv[1]);
		 exit(1);
	}
    if( (stream_out = fopen(argv[2], "wb+")) == NULL)
    {
	     printf("failed on open output file: %s\n", argv[2]);
	     exit(1);
    }

    total = count = 0;
    imageCrc = CRC32_INIT_VALUE;

    while( !feof( stream_in ) )
    {
        count = fread( buffer, sizeof( char ), BUF_SIZE, stream_in );
        if( ferror( stream_in ) )
        {
            perror( "Read error" );
            exit(1);
        }

        imageCrc = getCrc32(buffer, count, imageCrc);
        numWritten = fwrite(buffer, sizeof(char), count, stream_out);
        if (ferror(stream_out)) 
        {
            perror("addcrc: Write image file error");
            exit(1);
        }
        total += count;
    }
    
    // *** assume it is always in network byte order (big endia)
    imageCrc = htonl(imageCrc);
    memcpy(&pWt->wfiCrc, (byte*)&imageCrc, CRC_LEN);

    // write the crc to the end of the output file
    numWritten = fwrite(vToken, sizeof(char), TOKEN_LEN, stream_out);
    if (ferror(stream_out)) 
    {
        perror("addcrc: Write image file error");
        exit(1);
    }

    fclose(stream_in);
    fclose(stream_out);

    printf( "addvtoken: Output file size = %d with image crc = 0x%x\n", total+TOKEN_LEN, imageCrc);

	exit(0);
}
