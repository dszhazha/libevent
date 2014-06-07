#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#include "md5.h"
#include "avilib.h"
#include "rtsp_client.h"

#define ECSINO_PRODUCT_NAME         "ECSINO_IPC"    //产品标识
#define IPC_VER                     "ECSINOV1.0"    //产品版本号
#define RTSP_VER		"RTSP/1.0"
#define DEFAULT_HOST 		"59.55.33.138"
#define DEFAULT_RTSP_PORT	554
//rtsp://59.55.33.138:554/899200088_0_1406079220135.wav


int init_rtsp_client(char *cServerAddr,int iServerPort)
{
	int iRet = 0;
	int sockfd;
	struct sockaddr_in dest;

	sockfd= socket(AF_INET,SOCK_STREAM,0);
	if(sockfd==-1)
	{
		printf("socket error!\r\n");
		return -1;
	}
	
	bzero(&dest,sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(iServerPort);
	inet_pton(AF_INET,cServerAddr,&dest.sin_addr);
	
	iRet = connect(sockfd,(struct sockaddr*)&dest,sizeof(dest));
	if(iRet != 0)
	{
		printf("connetc error!\r\n");
		return -1;	
	}

	return sockfd;
}

int init_rtsp_param(ty_rtsp_param *pstRtspParam,char *cRtspUrl,ty_cloud_talk *pstCloudTalk)
{
	int port;
	char ip[32] = {0};
	char *cTmpPrt1,*cTmpPrt2;
	
	if(cRtspUrl == NULL)
	{
		DEBUG_PRT(ERR,FALSE,"input data error");
		return -1;
	}

	cTmpPrt1 = strstr(cRtspUrl,"#CLOUDTALK#");
	if(NULL == cTmpPrt1)
	{
		DEBUG_PRT(ERR,FALSE,"input data error");
		return -1;
	}
	cTmpPrt1 += strlen("#CLOUDTALK#");
	
	cTmpPrt2 = strstr(cTmpPrt1,"#");
	if(NULL == cTmpPrt2)
	{
		DEBUG_PRT(ERR,FALSE,"input data error");
		return -1;
	}
	strncpy(pstCloudTalk->cFileID,cTmpPrt1,cTmpPrt2-cTmpPrt1); //fileid

	cTmpPrt1 = strstr(cRtspUrl,"rtsp://");
	if(NULL == cTmpPrt1)
	{
		DEBUG_PRT(ERR,FALSE,"input data error");
		return -1;
	}

	cTmpPrt2 = strstr(cTmpPrt1,"#");
	if(NULL == cTmpPrt2)
	{
		DEBUG_PRT(ERR,FALSE,"input data error");
		return -1;
	}
	strncpy(pstRtspParam->cRtspUrl,cTmpPrt1,cTmpPrt2-cTmpPrt1);
	printf("cRtspUrl=%s\n",pstRtspParam->cRtspUrl);	

	pstCloudTalk->iFileLen = atoi(cTmpPrt2+1);
	printf("cFileID=%s,filelen=%d\n",pstCloudTalk->cFileID,pstCloudTalk->iFileLen);
	
	cTmpPrt1 += strlen("rtsp://");
	cTmpPrt2 = strstr(cTmpPrt1,":");
	if(NULL == cTmpPrt2)
	{
		DEBUG_PRT(ERR,FALSE,"input data error");
		return -1;
	}
	strncpy(ip,cTmpPrt1,cTmpPrt2-cTmpPrt1);
	port = atoi(cTmpPrt2+1);
	DEBUG_PRT(DEBUG,FALSE,"ip=%s,port=%d",ip,port);
	
	pstRtspParam->iCseq = 1;
	pstRtspParam->iSocketfd = -1;
	pstRtspParam->iSocketfd = init_rtsp_client(ip,port);
	if(pstRtspParam->iSocketfd < 0)
	{
		DEBUG_PRT(ERR,TRUE,"init_rtsp_client error");
		return -1;
	}

	return 0;
}



int init_rtsp_descibe(ty_rtsp_param *pstRtspParam)
{
	int iRet;
	int iLen;
	char *cTmpPtr,*cTmpPtr2;
	char cSendBuf[512] = {0};
	char cRecvBuf[1600] = {0};

	iLen = sprintf(cSendBuf,	
		"DESCRIBE %s RTSP/1.0\r\n"
		"CSeq: %d\r\n"
		"Accept: application/sdp\r\n"
		"User-Agent: %s Client\r\n\r\n", 
		pstRtspParam->cRtspUrl,
		pstRtspParam->iCseq,
		ECSINO_PRODUCT_NAME);

	iRet = send(pstRtspParam->iSocketfd, cSendBuf, iLen, 0);
	if(iRet != iLen)
	{
		DEBUG_PRT(ERR,TRUE,"DESCRIBE send error");
		return -1;
	}

	DEBUG_PRT(DEBUG,FALSE,"================c->s len=%d byte================\n%s",iRet,cSendBuf);

	iLen = recv(pstRtspParam->iSocketfd, cRecvBuf, sizeof(cRecvBuf)-1, 0);
	if(iLen < 0)
	{
		DEBUG_PRT(ERR,TRUE,"DESCRIBE recv error");
		return -1;
	}
	DEBUG_PRT(DEBUG,FALSE,"================s->c len=%d byte================\n%s",iLen,cRecvBuf);

	if(NULL == strstr(cRecvBuf, "200 OK"))
	{
		DEBUG_PRT(ERR,FALSE,"DESCRIBE fail");
		return -1;
	}

	cTmpPtr=strstr(cRecvBuf, "audio 0");
	if(NULL != cTmpPtr)
	{
		cTmpPtr=strstr(cTmpPtr,"a=control:");
		if(NULL != cTmpPtr )
		{
			cTmpPtr2=strstr(cTmpPtr,"\r\n");
			if(NULL != cTmpPtr2)
			{
				strncpy(pstRtspParam->cTrack,cTmpPtr+strlen("a=control:"),cTmpPtr2-cTmpPtr-strlen("a=control:"));
				DEBUG_PRT(DEBUG,FALSE,"trackbuf=%s",pstRtspParam->cTrack);
			}
		}
	}

	
	cTmpPtr = strstr(cRecvBuf,"Content-Base:");
	if(NULL != cTmpPtr)
	{
		cTmpPtr2=strstr(cTmpPtr,"\r\n");
		if(NULL != cTmpPtr2)
		{
			strncpy(pstRtspParam->ContentBase,cTmpPtr+strlen("Content-Base:"),cTmpPtr2-cTmpPtr-strlen("Content-Base:")-1);
			DEBUG_PRT(DEBUG,FALSE,"ContentBase=%s",pstRtspParam->ContentBase);
		}
	}
	
	pstRtspParam->iCseq++;
	
	return 0;
}

int init_rtsp_setup(ty_rtsp_param *pstRtspParam)
{
	int iRet;
	int iLen;
	char *cTmpPtr;
	char cSendBuf[512] = {0};
	char cRecvBuf[512] = {0};


	sprintf(cSendBuf, "SETUP %s/%s %s\r\nCSeq: %d\r\n",pstRtspParam->ContentBase,
		pstRtspParam->cTrack,RTSP_VER, pstRtspParam->iCseq);
	strcpy(cSendBuf+strlen(cSendBuf), "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n");
	strcpy(cSendBuf+strlen(cSendBuf), "User-Agent: "ECSINO_PRODUCT_NAME" Client\r\n\r\n");
	iLen = strlen(cSendBuf);

	iRet = send(pstRtspParam->iSocketfd, cSendBuf, iLen, 0);
	if(iRet < 0)
	{
		DEBUG_PRT(DEBUG,TRUE,"SETUP send error");
		return -1;
	}

	DEBUG_PRT(DEBUG,FALSE,"================c->s len=%d byte================\n%s",iRet,cSendBuf);

	iLen = recv(pstRtspParam->iSocketfd, cRecvBuf, sizeof(cRecvBuf)-1, 0);
	if(iLen < 0)
	{
		DEBUG_PRT(ERR,TRUE,"SETUP recv error");
		return -1;
	}
	DEBUG_PRT(DEBUG,FALSE,"================s->c len=%d byte================\n%s",iLen,cRecvBuf);
	if(NULL == strstr(cRecvBuf, "200 OK"))
	{
		DEBUG_PRT(ERR,FALSE,"SETUP fail");
		return -1;
	}

	cTmpPtr = strstr(cRecvBuf, "Session:");
	if(NULL != cTmpPtr)
	{
		char *pSrc = cTmpPtr + 9;
		char *pDst = pstRtspParam->cSessionId;
		while((*pSrc >= '0' && *pSrc <= '9') ||  \
			(*pSrc >= 'a' &&  *pSrc <= 'f') || \
			(*pSrc >= 'A' && *pSrc <= 'F'))  //sometimes session wsa hex 
		{
			*pDst++ = *pSrc++;
		}
	}

	pstRtspParam->iCseq++;
	
	return 0;
}

int init_rtsp_play(ty_rtsp_param *pstRtspParam)
{
	int iRet;
	int iLen;
	char  	Magic;
	char  	Channel;
	short 	Length;
	char cSendBuf[512] = {0};
	char cRecvBuf[1600] = {0};
	
	sprintf(cSendBuf, "PLAY %s %s\r\n"
		"CSeq: %d\r\nSession: %s\r\n",
		pstRtspParam->ContentBase, 
		RTSP_VER, 
		pstRtspParam->iCseq, 
		pstRtspParam->cSessionId);

	strcpy(cSendBuf + strlen(cSendBuf), 
		"Rang: npt=0.000-\r\n"
		"User-Agent: "ECSINO_PRODUCT_NAME" Client\r\n\r\n");
	
	iRet = send(pstRtspParam->iSocketfd, cSendBuf, (int)strlen(cSendBuf), 0);
	if(iRet < 0)
	{
		DEBUG_PRT(ERR,TRUE,"PLAY send error");
		return -1;
	}
	DEBUG_PRT(DEBUG,FALSE,"================c->s len=%d byte================\n%s",iRet,cSendBuf);

	iRet = recv(pstRtspParam->iSocketfd, &Magic, 1, 0);
	if(iRet <= 0) 
	{
		DEBUG_PRT(ERR,FALSE,"init_rtsp_play recv error");
		return -1;
	}
	DEBUG_PRT(DEBUG,FALSE,"Magic=%c",Magic);
	if(Magic != '$')
	{
		memcpy(cRecvBuf, &Magic, 1);
		iLen = recv(pstRtspParam->iSocketfd, cRecvBuf+1, sizeof(cRecvBuf)-1, 0);
		if(iLen < 0)
		{
			DEBUG_PRT(ERR,TRUE,"PLAY recv Magic error");
			return -1;
		}
		DEBUG_PRT(DEBUG,FALSE,"================s->c len=%d byte================\n%s",iLen,cRecvBuf);
	}
	else
	{
		iRet = recv(pstRtspParam->iSocketfd, &Channel, 1, 0);
		if(iRet <= 0)
		{
			DEBUG_PRT(ERR,FALSE,"PLAY recv Channel error");
			return -1;
		}
		DEBUG_PRT(DEBUG,FALSE,"Channel=%d",Channel);	

		iRet = recv(pstRtspParam->iSocketfd,(char *)&Length, 2,0);
		if(iRet <= 0)
		{
			DEBUG_PRT(ERR,FALSE,"PLAY recv Length error");
			return -1;
		}

		if(Channel == 0x01) //rtcp jump it
		{
			Length = ntohs(Length);
			if(Length <= 0)
			{
				DEBUG_PRT(ERR,FALSE,"Length not correct");
				return -1;
			}
			DEBUG_PRT(DEBUG,FALSE,"Length=%d",Length);
			
			iLen = recv(pstRtspParam->iSocketfd, cRecvBuf, Length, 0);
			if(iLen < 0)
			{
				DEBUG_PRT(ERR,TRUE,"PLAY recv data error");
				return -1;
			}
		}
	}

	memset(cRecvBuf,0,sizeof(cRecvBuf));
	iLen = recv(pstRtspParam->iSocketfd, cRecvBuf, sizeof(cRecvBuf)-1, 0);
	if(iLen < 0)
	{
		DEBUG_PRT(ERR,TRUE,"PLAY recv Magic error");
		return -1;
	}
	
	DEBUG_PRT(DEBUG,FALSE,"================s->c len=%d byte================\n%s",iLen,cRecvBuf);
	if(NULL == strstr(cRecvBuf, "RTSP/1.0 200 OK"))
	{
		DEBUG_PRT(ERR,FALSE,"PLAY fail");
		return -1;
	}
	printf("================================rtsp finish===================================\n");
	
	return 0;
}


int init_rtsp_connect(char *cRtspUrl)
{
	int iRet;
	ty_rtsp_param stRtspParam;
	ty_cloud_talk stCloudTalk;

	memset(&stRtspParam,'\0',sizeof(ty_rtsp_param));
	
	iRet = init_rtsp_param(&stRtspParam,cRtspUrl,&stCloudTalk);
	if(iRet != 0)
	{
		DEBUG_PRT(ERR,FALSE,"init_rtsp_param error");
		return -1;
	}

	iRet = init_rtsp_descibe(&stRtspParam);
	if(iRet != 0)
	{
		DEBUG_PRT(ERR,FALSE,"init_rtsp_param error");
		return -1;
	}

	iRet = init_rtsp_setup(&stRtspParam);
	if(iRet != 0)
	{
		DEBUG_PRT(ERR,FALSE,"init_rtsp_param error");
		return -1;
	}

	iRet = init_rtsp_play(&stRtspParam);
	if(iRet != 0)
	{
		DEBUG_PRT(ERR,FALSE,"init_rtsp_param error");
		return -1;
	}
	
	return stRtspParam.iSocketfd;
}

int recv_full_frame(int iSocketFd,char* buffer, int len)
{
 	int cur=0, ret;
	do{
		ret = recv(iSocketFd, buffer+cur, len-cur, 0);
		if(ret <= 0) 
			return -1;
		cur += ret;
	}while(cur < len);

	return len;
}

int  recv_media_data(int iSocketFd,char* packet, int* len)
{
	int 	index=0, ret=0;
	char  	Magic;
	char  	Channel;
	short 	Length;

	ret = recv(iSocketFd, &Magic, 1, 0);
	if(ret <= 0) 
	{
		*len=0;
		DEBUG_PRT(ERR,FALSE,"recv_media_data recv  error");
		return -1;
	}
	printf("Magic=%c\r\n",Magic);
	if(Magic != '$')
	{
		memcpy(packet, &Magic, 1);
		Length = recv(iSocketFd, packet+1, 1024, 0);
		if(Length <= 0)
		{
			DEBUG_PRT(ERR,FALSE,"recv_media_data recv 2 error");
			return -1;
		}
		*len = 1+Length;
		return 0xFF;  //abandon this frame
		
	}
	
	ret = recv(iSocketFd, &Channel, 1, 0);
	if(ret <= 0)
	{
		DEBUG_PRT(ERR,FALSE,"recv_media_data recv 3 error");
		return -1;
	}
		
	printf("RecvMediaTCP  Channel=%d\r\n",Channel);
	if(Channel<0x00 || Channel>0x02)
	{
		DEBUG_PRT(ERR,FALSE,"Channel not correct");
		return -1;
	}
		
	ret = recv(iSocketFd,(char *)&Length, 2,0);
	if(ret <= 0)
	{
		DEBUG_PRT(ERR,FALSE,"recv_full_frame error");
		return -1;
	}
		
	Length = ntohs(Length);
	if(Length <= 0)
	{
		DEBUG_PRT(ERR,FALSE,"Length not correct");
		return -1;
	}
  	printf("Length =%d \r\n",Length);
	
	ret = recv_full_frame(iSocketFd,packet, Length);
	if(ret <= 0)
	{
		DEBUG_PRT(ERR,FALSE,"recv_full_frame error");
		return -1;
	}
		
	*len = Length;
	return Channel;
}

int rtsp_cloud_talk(char *cRtspUrl)
{
	int i;
	int iRet,iLen,chancel;
	int bDataEndFlag = 0;
	int iCount = 0;
	char cBuf[2048] = {0};
	int iSockFd = -1;
	RTP_HEADER stRtpHead;

	memset(&stRtpHead,0,sizeof(RTP_HEADER));
	iSockFd = init_rtsp_connect(cRtspUrl);
	if(iSockFd < 0)
	{
		DEBUG_PRT(ERR,FALSE,"init_rtsp_connect error");
		return -1;
	}

	while(1)
	{
		memset(cBuf,0,sizeof(cBuf));
		chancel = recv_media_data(iSockFd, cBuf,&iLen);
		if(chancel == 0xff)
		{
			DEBUG_PRT(DEBUG,FALSE,"abandon this frame");
			continue;
		}
		else if(chancel == 0x00) //rtp
		{
			//play audio
			bDataEndFlag = 0;
			printf("len=%d\n",iLen);
			memcpy(&stRtpHead,cBuf,sizeof(RTP_HEADER));
			printf("version=%d\n",stRtpHead.version);
			printf("padding=%d\n",stRtpHead.padding);
			printf("extension=%d\n",stRtpHead.extension);
			printf("csrc_count=%d\n",stRtpHead.csrc_count);
			printf("payload_type=%d\n",stRtpHead.payload_type);
			printf("marker=%d\n",stRtpHead.marker);
			printf("seq_num=%d\n",(unsigned)ntohs(stRtpHead.seq_num));
			printf("timestamp=%d\n",ntohl(stRtpHead.timestamp));
			printf("ssrc=%d\n",ntohl(stRtpHead.ssrc));
			for(i = 0;i < 16;i++)
			{
				printf("%02x ",*(unsigned char *)(cBuf+sizeof(RTP_HEADER)+i));
			}
			printf("\n");
			//fd ff fd 7f 7e fd fe 7a 7d 78 fe f5 fc fd fc 7e
		}
		else if(chancel == 0x01)//rtcp
		{
			bDataEndFlag++;
			if(bDataEndFlag >= 3)
			{
				DEBUG_PRT(DEBUG,FALSE,"recv data end");
				return 0;
			}
		}
		else
		{
			DEBUG_PRT(DEBUG,FALSE,"unknow error");
			return -1;
		}
	}
}

#if 0
int main()
{
	int iRet = -1;
	char url[] = "#CLOUDTALK#763#rtsp://59.55.33.138:554/899200088_0_1406194837894.wav#30508#";
	iRet = rtsp_cloud_talk(url);
	if(iRet < 0)
	{
		DEBUG_PRT(ERR,FALSE,"rtsp_cloud_talk error");
		return -1;
	}
	
	return 0;
}
#endif
