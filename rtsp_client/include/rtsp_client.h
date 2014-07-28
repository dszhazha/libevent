#ifndef   RTSP_CLIENT_H_
#define  RTSP_CLIENT_H_
typedef struct
{
	unsigned long csrc_count : 4;
	unsigned long extension : 1;
	unsigned long padding : 1;
	unsigned long version : 2;

	unsigned long payload_type : 7;
	unsigned long marker : 1;
	
	unsigned long seq_num : 16;

	unsigned long timestamp;

	unsigned long ssrc;
}RTP_HEADER;

#define TRUE 	1
#define FALSE	0

enum
{
   DEBUG=0,
   ERR=1,
};

typedef struct rtsp_param
{
	int 	iSocketfd;
	int 	iCseq;
	char 	cRtspUrl[128];
	char 	cTrack[16];
	char	ContentBase[128];
	char 	cSessionId[32];
	
}ty_rtsp_param;

typedef struct st_cloud_talk
{
	char cFileID[64];
	char cFileURL[64];
	int iFileLen;
}ty_cloud_talk;


/*printf("[File: %-15s, Func: %-20s, Line: %05d] :", __FILE__, \
								__FUNCTION__, __LINE__ ); \*/

#define DEBUG_PRT(level,err, fmt...) { \
		if(level >= DEBUG){\
			printf(fmt); \
			printf("\n"); \
			if(err == TRUE){ perror("Perror: ");} \
		}\
}

#endif

