#include <stdio.h>
#include <string.h>

#define MAX_WEB_LEN	40
#define MAX_FOLDER_LEN	56
#define MAX_LIST_NUM	100
#define BUFSIZE 2048

#define IPQ_TIMEOUT 10000000

#if defined(AEI_VDSL_CUSTOMER_NCS)
#define URL_COUNT 100
#define ENTRY_SIZE 256
#define LOG_TIMEOUT 10
#endif

typedef struct _URL{
	char website[MAX_WEB_LEN];
	char folder[MAX_FOLDER_LEN];
#if defined(AEI_VDSL_CUSTOMER_QWEST)
    char lanIP[16];
#endif
	struct _URL *next;
}URL, *PURL;



