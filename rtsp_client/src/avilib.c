/*
 *  Some utilities for writing and reading AVI files.
 *  These are not intended to serve for a full blown
 *  AVI handling software (this would be much too complex)
 *  The only intention is to write out MJPEG encoded
 *  AVIs with sound and to be able to read them back again.
 *  These utilities should work with other types of codecs too, however.
 *
 *  Copyright (C) 1999 Rainer Johanni <Rainer@Johanni.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#include "avilib.h"

#define MODLE_NAME		"Avilib "
long AVI_errno = 0;
#define ERR_EXIT(x) \
{ \
   AVI_close(AVI); \
   AVI_errno = x; \
   return 0; \
}

int AVI_close_fd(avi_t *AVI);

/*******************************************************************
 *                                                                 *
 *    Utilities for writing an AVI File                            *
 *                                                                 *
 *******************************************************************/

static ssize_t avi_write (int fd, char *buf, size_t len)
{
   ssize_t n = 0;
   ssize_t r = 0;

   	while (r < len) 
	{
 		n = write(fd, buf + r, len - r);
  		if(n <= 0)
  		{
			return n;
  		}
  		r += n;
   	}
   return r;
}


/* HEADERBYTES: The number of bytes to reserve for the header */

#define HEADERBYTES 2048
#define PAD_EVEN(x) ( ((x)+1) & ~1 )

/*use the same buf of FtpAVBuffer*/
extern unsigned char *pFtpAVBuffer;


/* Copy n into dst as a 4 byte, little endian number.
   Should also work on big endian machines */

static void long2str(unsigned char *dst, int n)
{
   dst[0] = (n    )&0xff;
   dst[1] = (n>> 8)&0xff;
   dst[2] = (n>>16)&0xff;
   dst[3] = (n>>24)&0xff;
}


/* Calculate audio sample size from number of bits and number of channels.
   This may have to be adjusted for eg. 12 bits and stereo */

static int avi_sampsize(avi_t *AVI)
{
   int s;
   s = ((AVI->a_bits+7)/8)*AVI->a_chans;
   if(s==0) s=1; /* avoid possible zero divisions */
   return s;
}


/* Add a chunk (=tag and data) to the AVI file,
   returns -1 on write error, 0 on success */

static int avi_add_chunk_fd(avi_t *AVI, unsigned char *tag, unsigned char *data, int length)
{
   unsigned char c[8]={0};
   char p=0;

   /* Copy tag and length int c, so that we need only 1 write system call
      for these two values */

   memcpy(c,tag,4);
   long2str(c+4,length);

   /* Output tag, length and data, restore previous position
      if the write fails */

   if( avi_write(AVI->fdes,(char *)c,8) != 8 ||
       avi_write(AVI->fdes,(char *)data,length) != length ||
       avi_write(AVI->fdes,&p,length&1) != (length&1)) // if len is uneven, write a pad byte
   {
      lseek(AVI->fdes,AVI->pos,SEEK_SET);
      return -1;
   }

   /* Update file position */

   AVI->pos += 8 + PAD_EVEN(length);

   return 0;
}



/* Add a chunk (=tag and data) to the AVI file,
   returns -1 on write error, 0 on success */

static int avi_add_chunk(avi_t *AVI, unsigned char *tag, unsigned char *data, int length)
{
   unsigned char c[8]={0};
   char p=0;

   /* Copy tag and length int c, so that we need only 1 write system call
      for these two values */

   memcpy(c,tag,4);
   long2str(c+4,length);

   /* Output tag, length and data, restore previous position
      if the write fails */

  memcpy(AVI->buf + AVI->pos, c, 8);
  memcpy(AVI->buf + AVI->pos + 8, data, length);
  if(length&1)	// if len is uneven, write a pad byte
	memcpy(AVI->buf + AVI->pos + 8 + length,&p,length&1);
   /* Update file position */

   AVI->pos += 8 + PAD_EVEN(length);

   return 0;
}


static int avi_add_index_entry(avi_t *AVI, unsigned char *tag, long flags, long pos, long len)
{
   memcpy(AVI->idx[AVI->n_idx],tag,4);
   long2str(AVI->idx[AVI->n_idx]+ 4,flags);
   long2str(AVI->idx[AVI->n_idx]+ 8,pos);
   long2str(AVI->idx[AVI->n_idx]+12,len);


   AVI->n_idx++;

   return 0;
}


/*
   AVI_open_output_file: Open an AVI File and write a bunch
                         of zero bytes as space for the header.

   returns a pointer to avi_t on success, a zero pointer on error
*/

int AVI_Init_fd(avi_t *AVI, int width, int height, int fps, const char *compressor, int duration, const char *file_name)
{
	void *ptr = NULL;

	/* apply for index space */
    ptr = malloc((duration*80+1024)*16);
    if(NULL == ptr) 
	{
       return -1;
    }
	AVI->idx = (unsigned char((*)[16]) ) ptr;

    AVI->pos = HEADERBYTES;
	AVI->buf_len = 1024*1024*512;		

	/* write avi head */
	AVI_set_video(AVI, width, height, fps, compressor);
	//if(AUDIO_ENC_FMT_AAC == CFG_GetAudioEncType())
		//AVI_set_audio(AVI, 1, 16000, 16, 1);
	//else
		AVI_set_audio(AVI,1,8000,16, 1);

	AVI->fdes = open(file_name, O_RDWR | O_NONBLOCK | O_CREAT | O_TRUNC, 00644);
	if (AVI->fdes < 0) 
	{
		printf("create file %s err!\n",file_name);
		AVI_close_fd(AVI);
		return(-1);
	}
	/* 偏移AVI文件读写头至HEADERBYTES */
    if (lseek(AVI->fdes,AVI->pos,SEEK_SET) < 0)
     {
		printf("create file %s err!\n",file_name);
		AVI_close_fd(AVI);
		return -1;
     }

	AVI->buf = NULL;
	return(1);
}


/*
   AVI_open_output_file: Open an AVI File and write a bunch
                         of zero bytes as space for the header.

   returns a pointer to avi_t on success, a zero pointer on error
*/

int AVI_Init(avi_t *AVI, int file_size, int idx_size)
{
	/* apply for index space */
    AVI->idxPtr = malloc((idx_size+1024)*16);
    if(NULL == AVI->idxPtr) {
		printf("malloc error \n");
       return GS_FAIL;
    }
	AVI->idx = (unsigned char((*)[16]))AVI->idxPtr;

	/* apply for buf space */
	AVI->buf = (unsigned char*)malloc(file_size+(idx_size+1024)*24);
	if(NULL == AVI->buf){
		printf("malloc error \n");
		return GS_FAIL;
	}
    memset(AVI->buf,0,HEADERBYTES);
    AVI->pos  = HEADERBYTES;
	AVI->buf_len = file_size+(idx_size+1024)*24;
	AVI->fdes = -1;

	return GS_SUCCESS;
}


void AVI_set_video(avi_t *AVI, int width, int height, int fps, const char *compressor)
{
	AVI->width  = width; 
    AVI->height = height;
	if(((height%288)==0) && width >=352)
	{
		/* 大于 PAL CIF */
		AVI->dwMicroSecPerFrame = 40000;
        AVI->dwSuggestedBufferSize =144008;
	}
	else 
	{
		/* 小于 PAL CIF */
		AVI->dwMicroSecPerFrame = 33366;
        AVI->dwSuggestedBufferSize =120008;
	}
	AVI->fps    = fps;
	memcpy(AVI->compressor,compressor,4);
	AVI->compressor[4] = 0;
}


void AVI_set_audio(avi_t *AVI, int channels, int rate, int bits, int format)
{
   /* may only be called if file is open for writing */

   AVI->a_chans = channels;
   AVI->a_rate  = rate;
   AVI->a_bits  = bits;
   AVI->a_fmt   = format;
}

#define OUT4CC(s) \
   if(nhb<=HEADERBYTES-4) memcpy(AVI_header+nhb,s,4); nhb += 4

#define OUTLONG(n) \
   if(nhb<=HEADERBYTES-4) long2str(AVI_header+nhb,n); nhb += 4

#define OUTSHRT(n) \
   if(nhb<=HEADERBYTES-2) { \
      AVI_header[nhb  ] = (n   )&0xff; \
      AVI_header[nhb+1] = (n>>8)&0xff; \
   } \
   nhb += 2

/*
  Write the header of an AVI file and close it.
  returns 0 on success, -1 on write error.
*/

 int AVI_output_file_fd(avi_t *AVI)
{

   int ret, njunk, sampsize, hasIndex,  idxerror;
   int movi_len, hdrl_start, strl_start;
   unsigned char AVI_header[HEADERBYTES];
   long nhb;

   /* Calculate accurate fps */
    if((AVI->et) > (AVI->bt))
   AVI->fps = AVI->video_frames/(AVI->et - AVI->bt);
   //printf("... ...%d/(%d-%d) = %d\n",AVI->video_frames,AVI->et,AVI->bt,AVI->fps);

   /* Calculate length of movi list */

   movi_len = AVI->pos - HEADERBYTES + 4;

   /* Try to ouput the index entries. This may fail e.g. if no space
      is left on device. We will report this as an error, but we still
      try to write the header correctly (so that the file still may be
      readable in the most cases */

  idxerror = 0;
  ret = avi_add_chunk_fd(AVI,(unsigned char *)"idx1",
  					(unsigned char *)AVI->idx,(int)AVI->n_idx*16);

   hasIndex = (ret==0);
   if(ret)
   {
     printf("error when writing index chunk\n");
   }

   /* Prepare the file header */

   nhb = 0;

   /* The RIFF header */

   OUT4CC ("RIFF");
   OUTLONG(AVI->pos - 8);    /* # of bytes to follow */
   OUT4CC ("AVI ");

   /* Start the header list */

   OUT4CC ("LIST");
   OUTLONG(0);        /* Length of list in bytes, don't know yet */
   hdrl_start = nhb;  /* Store start position */
   OUT4CC ("hdrl");

   /* The main AVI header */

   /* The Flags in AVI File header */

   OUT4CC ("avih");
   OUTLONG(56);                 /* # of bytes to follow */
   OUTLONG(AVI->dwMicroSecPerFrame);       /* Microseconds per frame */
   OUTLONG(3600000);           /* MaxBytesPerSec, I hope this will never be used */
   OUTLONG(512);                  /* PaddingGranularity (whatever that might be) */
                                /* Other sources call it 'reserved' */

   OUTLONG(2064);               /* Flags */
   OUTLONG(AVI->video_frames);  /* TotalFrames */
   OUTLONG(0);                  /* InitialFrames */
   if (AVI->audio_bytes)
      { OUTLONG(2); }           /* Streams */
   else
      { OUTLONG(1); }           /* Streams */
   OUTLONG(AVI->dwSuggestedBufferSize);                  /* SuggestedBufferSize */
   OUTLONG(AVI->width);         /* Width */
   OUTLONG(AVI->height);        /* Height */
                                /* MS calls the following 'reserved': */
   OUTLONG(0);                  /* TimeScale:  Unit used to measure time */
   OUTLONG(0);                  /* DataRate:   Data rate of playback     */
   OUTLONG(AVI->bt);                  /* StartTime:  Starting time of AVI data */
   OUTLONG(0);                  /* DataLength: Size of AVI data chunk    */


   /* Start the video stream list ---------------------------------- */

   OUT4CC ("LIST");
   OUTLONG(0);        /* Length of list in bytes, don't know yet */
   strl_start = nhb;  /* Store start position */
   OUT4CC ("strl");

   /* The video stream header */

   OUT4CC ("strh");
   OUTLONG(64);                 /* # of bytes to follow */
   OUT4CC ("vids");             /* Type */
   OUT4CC (AVI->compressor);    /* Handler */
   OUTLONG(0);                  /* Flags */
   OUTLONG(0);                  /* Reserved, MS says: wPriority, wLanguage */
   OUTLONG(0);                  /* InitialFrames */
   OUTLONG(1);                  /* Scale */
   OUTLONG(AVI->fps);                  /* Rate: Rate/Scale == samples/second */
   OUTLONG(0);                  /* Start */
   OUTLONG(AVI->video_frames);  /* Length */
   OUTLONG(-1);                 /* SuggestedBufferSize */
   OUTLONG(0);                  /* Quality */
   OUTLONG(0);                  /* SampleSize */
   OUTLONG(0);                  /* Frame */
   OUTLONG(0);                  /* Frame */
   OUTLONG(0);                  /* Frame */
   OUTLONG(0);                  /* Frame */

   /* The video stream format */

   OUT4CC ("strf");
   OUTLONG(40);                 /* # of bytes to follow */
   OUTLONG(40);                 /* Size */
   OUTLONG(AVI->width);         /* Width */
   OUTLONG(AVI->height);        /* Height */
   OUTSHRT(1); OUTSHRT(24);     /* Planes, Count */
   OUT4CC (AVI->compressor);    /* Compression */
   //OUTLONG(AVI->width*AVI->height);  /* SizeImage (in bytes?) */
   OUTLONG(144000);             /* SizeImage (in bytes?) */
   OUTLONG(0);                  /* XPelsPerMeter */
   OUTLONG(0);                  /* YPelsPerMeter */
   OUTLONG(0);                  /* ClrUsed: Number of colors used */
   OUTLONG(0);                  /* ClrImportant: Number of colors important */

   /* Finish stream list, i.e. put number of bytes in the list to proper pos */

   long2str(AVI_header+strl_start-4,nhb-strl_start);

   if (AVI->a_chans && AVI->audio_bytes)
   {

   sampsize = avi_sampsize(AVI);

   /* Start the audio stream list ---------------------------------- */

   OUT4CC ("LIST");
   OUTLONG(0);        /* Length of list in bytes, don't know yet */
   strl_start = nhb;  /* Store start position */
   OUT4CC ("strl");

   /* The audio stream header */

   OUT4CC ("strh");
   OUTLONG(64);            /* # of bytes to follow */
   OUT4CC ("auds");
   OUT4CC ("\0\0\0\0");
   OUTLONG(0);             /* Flags */
   OUTLONG(0);             /* Reserved, MS says: wPriority, wLanguage */
   OUTLONG(0);             /* InitialFrames */
   OUTLONG(sampsize);      /* Scale */
   OUTLONG(sampsize*AVI->a_rate); /* Rate: Rate/Scale == samples/second */
   OUTLONG(0);             /* Start */
   OUTLONG(AVI->audio_bytes/sampsize);   /* Length */
   OUTLONG(0);             /* SuggestedBufferSize */
   OUTLONG(-1);            /* Quality */
   OUTLONG(sampsize);      /* SampleSize */
   OUTLONG(0);             /* Frame */
   OUTLONG(0);             /* Frame */
   OUTLONG(0);             /* Frame */
   OUTLONG(0);             /* Frame */

   /* The audio stream format */

   OUT4CC ("strf");
   OUTLONG(16);                   /* # of bytes to follow */
   OUTSHRT(AVI->a_fmt);           /* Format */
   OUTSHRT(AVI->a_chans);         /* Number of channels */
   OUTLONG(AVI->a_rate);          /* SamplesPerSec */
   OUTLONG(sampsize*AVI->a_rate); /* AvgBytesPerSec */
   OUTSHRT(sampsize);             /* BlockAlign */
   OUTSHRT(AVI->a_bits);          /* BitsPerSample */

   /* Finish stream list, i.e. put number of bytes in the list to proper pos */

   long2str(AVI_header+strl_start-4,nhb-strl_start);

   }

   /* Finish header list */

   long2str(AVI_header+hdrl_start-4,nhb-hdrl_start);

   /* Calculate the needed amount of junk bytes, output junk */

   njunk = HEADERBYTES - nhb - 8 - 12;

   /* Safety first: if njunk <= 0, somebody has played with
      HEADERBYTES without knowing what (s)he did.
      This is a fatal error */

   if(njunk<=0)
   {
      printf("AVI_close_output_file: # of header bytes too small\n");
      return(1);
   }

   OUT4CC ("JUNK");
   OUTLONG(njunk);
   memset(AVI_header+nhb,0,njunk);
   strcpy((char *)(AVI_header+nhb),(const char *)"GrandStream");
   nhb += njunk;

   /* Start the movi list */

   OUT4CC ("LIST");
   OUTLONG(movi_len); /* Length of list in bytes */
   OUT4CC ("movi");

   /* Output the header, truncate the file to the number of bytes
      actually written, report an error if someting goes wrong */

   if ( lseek(AVI->fdes,0,SEEK_SET)<0 ||
        avi_write(AVI->fdes,(char *)AVI_header,HEADERBYTES)!=HEADERBYTES ||
		lseek(AVI->fdes,AVI->pos,SEEK_SET)<0)
     {
       return -1;
     }

   if(idxerror) 
   {
   	printf("AVI_close_output_file: #  idxerror>0\n");
   	return -1;
   }
   return 0;
}


/*
  Write the header of an AVI file and close it.
  returns 0 on success, -1 on write error.
*/

 int AVI_output_file(avi_t *AVI)
{

   int ret, njunk, sampsize, hasIndex,  idxerror;
   int movi_len, hdrl_start, strl_start;
   unsigned char AVI_header[HEADERBYTES];
   long nhb;

   /* Calculate length of movi list */

   movi_len = AVI->pos - HEADERBYTES + 4;

   /* Try to ouput the index entries. This may fail e.g. if no space
      is left on device. We will report this as an error, but we still
      try to write the header correctly (so that the file still may be
      readable in the most cases */

  idxerror = 0;
  ret = avi_add_chunk(AVI,(unsigned char *)"idx1",(void*)AVI->idx,AVI->n_idx*16);

   hasIndex = (ret==0);
   if(ret)
   {
      printf("error when writing index chunk\n");
   }


   /* Prepare the file header */

   nhb = 0;

   /* The RIFF header */

   OUT4CC ("RIFF");
   OUTLONG(AVI->pos - 8);    /* # of bytes to follow */
   OUT4CC ("AVI ");

   /* Start the header list */

   OUT4CC ("LIST");
   OUTLONG(0);        /* Length of list in bytes, don't know yet */
   hdrl_start = nhb;  /* Store start position */
   OUT4CC ("hdrl");

   /* The main AVI header */

   /* The Flags in AVI File header */

   OUT4CC ("avih");
   OUTLONG(56);                 /* # of bytes to follow */
   OUTLONG(AVI->dwMicroSecPerFrame);       /* Microseconds per frame */
   OUTLONG(3600000);           /* MaxBytesPerSec, I hope this will never be used */
   OUTLONG(512);                  /* PaddingGranularity (whatever that might be) */
                                /* Other sources call it 'reserved' */

   OUTLONG(2064);               /* Flags */
   OUTLONG(AVI->video_frames);  /* TotalFrames */
   OUTLONG(0);                  /* InitialFrames */
   if (AVI->audio_bytes)
      { OUTLONG(2); }           /* Streams */
   else
      { OUTLONG(1); }           /* Streams */
   OUTLONG(AVI->dwSuggestedBufferSize);                  /* SuggestedBufferSize */
   OUTLONG(AVI->width);         /* Width */
   OUTLONG(AVI->height);        /* Height */
                                /* MS calls the following 'reserved': */
   OUTLONG(0);                  /* TimeScale:  Unit used to measure time */
   OUTLONG(0);                  /* DataRate:   Data rate of playback     */
   OUTLONG(0);                  /* StartTime:  Starting time of AVI data */
   OUTLONG(0);                  /* DataLength: Size of AVI data chunk    */


   /* Start the video stream list ---------------------------------- */

   OUT4CC ("LIST");
   OUTLONG(0);        /* Length of list in bytes, don't know yet */
   strl_start = nhb;  /* Store start position */
   OUT4CC ("strl");

   /* The video stream header */

   OUT4CC ("strh");
   OUTLONG(64);                 /* # of bytes to follow */
   OUT4CC ("vids");             /* Type */
   OUT4CC (AVI->compressor);    /* Handler */
   OUTLONG(0);                  /* Flags */
   OUTLONG(0);                  /* Reserved, MS says: wPriority, wLanguage */
   OUTLONG(0);                  /* InitialFrames */
   OUTLONG(1);                  /* Scale */
   OUTLONG(AVI->fps);                  /* Rate: Rate/Scale == samples/second */
   OUTLONG(0);                  /* Start */
   OUTLONG(AVI->video_frames);  /* Length */
   OUTLONG(-1);                 /* SuggestedBufferSize */
   OUTLONG(0);                  /* Quality */
   OUTLONG(0);                  /* SampleSize */
   OUTLONG(0);                  /* Frame */
   OUTLONG(0);                  /* Frame */
   OUTLONG(0);                  /* Frame */
   OUTLONG(0);                  /* Frame */

   /* The video stream format */

   OUT4CC ("strf");
   OUTLONG(40);                 /* # of bytes to follow */
   OUTLONG(40);                 /* Size */
   OUTLONG(AVI->width);         /* Width */
   OUTLONG(AVI->height);        /* Height */
   OUTSHRT(1); OUTSHRT(24);     /* Planes, Count */
   OUT4CC (AVI->compressor);    /* Compression */
   //OUTLONG(AVI->width*AVI->height);  /* SizeImage (in bytes?) */
   OUTLONG(144000);             /* SizeImage (in bytes?) */
   OUTLONG(0);                  /* XPelsPerMeter */
   OUTLONG(0);                  /* YPelsPerMeter */
   OUTLONG(0);                  /* ClrUsed: Number of colors used */
   OUTLONG(0);                  /* ClrImportant: Number of colors important */

   /* Finish stream list, i.e. put number of bytes in the list to proper pos */

   long2str(AVI_header+strl_start-4,nhb-strl_start);

   if (AVI->a_chans && AVI->audio_bytes)
   {

   sampsize = avi_sampsize(AVI);

   /* Start the audio stream list ---------------------------------- */

   OUT4CC ("LIST");
   OUTLONG(0);        /* Length of list in bytes, don't know yet */
   strl_start = nhb;  /* Store start position */
   OUT4CC ("strl");

   /* The audio stream header */

   OUT4CC ("strh");
   OUTLONG(64);            /* # of bytes to follow */
   OUT4CC ("auds");
   OUT4CC ("\0\0\0\0");
   OUTLONG(0);             /* Flags */
   OUTLONG(0);             /* Reserved, MS says: wPriority, wLanguage */
   OUTLONG(0);             /* InitialFrames */
   OUTLONG(sampsize);      /* Scale */
   OUTLONG(sampsize*AVI->a_rate); /* Rate: Rate/Scale == samples/second */
   OUTLONG(0);             /* Start */
   OUTLONG(AVI->audio_bytes/sampsize);   /* Length */
   OUTLONG(0);             /* SuggestedBufferSize */
   OUTLONG(-1);            /* Quality */
   OUTLONG(sampsize);      /* SampleSize */
   OUTLONG(0);             /* Frame */
   OUTLONG(0);             /* Frame */
   OUTLONG(0);             /* Frame */
   OUTLONG(0);             /* Frame */

   /* The audio stream format */

   OUT4CC ("strf");
   OUTLONG(16);                   /* # of bytes to follow */
   OUTSHRT(AVI->a_fmt);           /* Format */
   OUTSHRT(AVI->a_chans);         /* Number of channels */
   OUTLONG(AVI->a_rate);          /* SamplesPerSec */
   OUTLONG(sampsize*AVI->a_rate); /* AvgBytesPerSec */
   OUTSHRT(sampsize);             /* BlockAlign */
   OUTSHRT(AVI->a_bits);          /* BitsPerSample */

   /* Finish stream list, i.e. put number of bytes in the list to proper pos */

   long2str(AVI_header+strl_start-4,nhb-strl_start);

   }

   /* Finish header list */

   long2str(AVI_header+hdrl_start-4,nhb-hdrl_start);

   /* Calculate the needed amount of junk bytes, output junk */

   njunk = HEADERBYTES - nhb - 8 - 12;

   /* Safety first: if njunk <= 0, somebody has played with
      HEADERBYTES without knowing what (s)he did.
      This is a fatal error */

   if(njunk<=0)
   {
      printf("AVI_close_output_file: # of header bytes too small\n");
      return(1);
   }

   OUT4CC ("JUNK");
   OUTLONG(njunk);
   memset(AVI_header+nhb,0,njunk);
   strcpy((char *)AVI_header+nhb,"GrandStream");
   nhb += njunk;

   /* Start the movi list */

   OUT4CC ("LIST");
   OUTLONG(movi_len); /* Length of list in bytes */
   OUT4CC ("movi");

   /* Output the header, truncate the file to the number of bytes
      actually written, report an error if someting goes wrong */

  /*written by liao 2007-6-27*/
  memcpy(AVI->buf, AVI_header, HEADERBYTES);

   if(idxerror) return -1;

   return 0;
}


/*
   AVI_write_data:
   Add video or audio data to the file;

   Return values:
    0    No error;
   -1    Error, AVI_errno is set appropriatly;

*/

static int avi_write_data(avi_t *AVI, unsigned char *data, long length, int audio)
{
   int n;

   /* Check for maximum file length */

   if ( (AVI->pos + 8 + length + 8 + (AVI->n_idx+1)*16) > AVI->buf_len )
   {
      printf("avifile had beyond buf_len %d\n",AVI->buf_len);
      return -1;
   }

   /* Add index entry */

   if(audio)
      n = avi_add_index_entry(AVI,(u8 *)"01wb",0x00,AVI->pos,length);
   else
      n = avi_add_index_entry(AVI,(u8 *)"00dc",0x10,AVI->pos,length);

   if(n)
   {
	   return -1;
   }

   /* Output tag and data */

   if((1 == audio)&&(NULL != AVI->buf))
	{
      n = avi_add_chunk(AVI,(u8 *)"01wb",data,length);
	}
   else if((1 == audio)&&(AVI->fdes > 1))
	{
      n = avi_add_chunk_fd(AVI,(u8 *)"01wb",data,length);
	}
   else if((0 == audio)&&(NULL != AVI->buf))
	{
	   n = avi_add_chunk(AVI,(u8 *)"00dc",data,length);
	}
   else if((0 == audio)&&(AVI->fdes > 1))
	{
	   n = avi_add_chunk_fd(AVI,(u8 *)"00dc",data,length);
	}

   if(n)
	 {
		return -1;
	 }
	return 1;
}


int AVI_write_frame(avi_t *AVI, unsigned char *data, int bytes, unsigned int rt)
{
	long pos;

	pos = AVI->pos;
	if(avi_write_data(AVI,data,bytes,0) != 1) 
	{
	   return -1;
	}

	AVI->last_pos = pos;
	AVI->last_len = bytes;
	AVI->video_frames++;

	/*计算帧率*/
	if(0 == AVI->bt)
		 AVI->bt = rt;
	AVI->et = rt;

   return 0;
}


int AVI_write_audio(avi_t *AVI, unsigned char *data, int bytes, unsigned int rt)
{
   if( avi_write_data(AVI,data,bytes,1) != 1) 
   {
	   return -1;
   }
   AVI->audio_bytes += bytes;
   AVI->et = rt;

   return 0;
}


int AVI_close(avi_t *AVI)
{
   /* 释放索引空间 */
   //dbg(Dbg, DbgNoPerror, "free idx \n");
   
   if(NULL != AVI->idxPtr){
		free(AVI->idxPtr);
		AVI->idxPtr = NULL;
	}
    
	//dbg(Dbg, DbgNoPerror, "free buf\n");
   /* 释放码流空间 */
   if(NULL != AVI->buf){
	   free(AVI->buf);
	   AVI->buf = NULL; 
	}

   return(0);
}

int AVI_close_fd(avi_t *AVI)
{
	if(AVI->fdes >0)
	{
		 /* 更新AVI头 */
		 AVI_output_file_fd(AVI);
		 /* 最后写索引数据 */
		 //avi_add_chunk_fd(AVI,"idx1",(void*)AVI->idx,AVI->n_idx*16);
		 /* 关闭AVI文件 */
		 close(AVI->fdes);
		 AVI->fdes=-1;
	}

	/* 释放索引空间 */
	if(NULL != AVI->idx)
	{
		free(AVI->idx);
		AVI->idx= NULL;
	}

	return(0);
}

int AVI_output_file_fd_1(avi_t *AVI)
{
   int ret, njunk, sampsize, hasIndex,  idxerror;
   int movi_len, hdrl_start, strl_start;
   unsigned char AVI_header[HEADERBYTES];
   long nhb;

   /* Calculate accurate fps */
   
   if((AVI->et) > (AVI->bt))
 	 AVI->fps = AVI->video_frames/(AVI->et - AVI->bt);

   /* Calculate length of movi list */
   movi_len = AVI->pos - HEADERBYTES + 4;
   if(movi_len <= 4)
   {
      	printf("No data\n");
	return (-1);
   }

   /* Try to ouput the index entries. This may fail e.g. if no space
      is left on device. We will report this as an error, but we still
      try to write the header correctly (so that the file still may be
      readable in the most cases */

  idxerror = 0;

  ret = avi_add_chunk_fd(AVI,(unsigned char *)"idx1",(unsigned char *)AVI->idx,AVI->n_idx*16);
   hasIndex = (ret==0);
   if(ret)
   {
		printf("error when writing index chunk\n");
		//return (-1);
   }

   /* Prepare the file header */

   nhb = 0;

   /* The RIFF header */

   OUT4CC ("RIFF");
   OUTLONG(AVI->pos - 8);    /* # of bytes to follow */
   OUT4CC ("AVI ");

   /* Start the header list */

   OUT4CC ("LIST");
   OUTLONG(0);        /* Length of list in bytes, don't know yet */
   hdrl_start = nhb;  /* Store start position */
   OUT4CC ("hdrl");

   /* The main AVI header */

   /* The Flags in AVI File header */

   OUT4CC ("avih");
   OUTLONG(56);                 /* # of bytes to follow */
   OUTLONG(AVI->dwMicroSecPerFrame);       /* Microseconds per frame */
   OUTLONG(3600000);           /* MaxBytesPerSec, I hope this will never be used */
   OUTLONG(512);                  /* PaddingGranularity (whatever that might be) */
                                /* Other sources call it 'reserved' */

   OUTLONG(2064);               /* Flags */
   OUTLONG(AVI->video_frames);  /* TotalFrames */
   OUTLONG(0);                  /* InitialFrames */
   if (AVI->audio_bytes)
      { OUTLONG(2); }           /* Streams */
   else
      { OUTLONG(1); }           /* Streams */
   OUTLONG(AVI->dwSuggestedBufferSize);                  /* SuggestedBufferSize */
   OUTLONG(AVI->width);         /* Width */
   OUTLONG(AVI->height);        /* Height */
                                /* MS calls the following 'reserved': */
   OUTLONG(0);                  /* TimeScale:  Unit used to measure time */
   OUTLONG(0);                  /* DataRate:   Data rate of playback     */
  // OUTLONG(0);      		/* StartTime:  Starting time of AVI data */
   OUTLONG(AVI->bt);       /* StartTime:  Starting time of AVI data */
   OUTLONG(0);                  /* DataLength: Size of AVI data chunk    */
   /* Start the video stream list ---------------------------------- */

   OUT4CC ("LIST");
   OUTLONG(0);        /* Length of list in bytes, don't know yet */
   strl_start = nhb;  /* Store start position */
   OUT4CC ("strl");

   /* The video stream header */

   OUT4CC ("strh");
   OUTLONG(64);                 /* # of bytes to follow */
   OUT4CC ("vids");             /* Type */
   OUT4CC (AVI->compressor);    /* Handler */
   OUTLONG(0);                  /* Flags */
   OUTLONG(0);                  /* Reserved, MS says: wPriority, wLanguage */
   OUTLONG(0);                  /* InitialFrames */
   OUTLONG(1);                  /* Scale */
   OUTLONG(AVI->fps);                  /* Rate: Rate/Scale == samples/second */
   OUTLONG(0);                  /* Start */
   OUTLONG(AVI->video_frames);  /* Length */
   OUTLONG(-1);                 /* SuggestedBufferSize */
   OUTLONG(0);                  /* Quality */
   OUTLONG(0);                  /* SampleSize */
   OUTLONG(0);                  /* Frame */
   OUTLONG(0);                  /* Frame */
   OUTLONG(0);                  /* Frame */
   OUTLONG(0);                  /* Frame */
   /* The video stream format */

   OUT4CC ("strf");
   OUTLONG(40);                 /* # of bytes to follow */
   OUTLONG(40);                 /* Size */
   OUTLONG(AVI->width);         /* Width */
   OUTLONG(AVI->height);        /* Height */
   OUTSHRT(1); OUTSHRT(24);     /* Planes, Count */
   OUT4CC (AVI->compressor);    /* Compression */
   //OUTLONG(AVI->width*AVI->height);  /* SizeImage (in bytes?) */
   OUTLONG(144000);             /* SizeImage (in bytes?) */
   OUTLONG(0);                  /* XPelsPerMeter */
   OUTLONG(0);                  /* YPelsPerMeter */
   OUTLONG(0);                  /* ClrUsed: Number of colors used */
   OUTLONG(0);                  /* ClrImportant: Number of colors important */

   /* Finish stream list, i.e. put number of bytes in the list to proper pos */

   long2str(AVI_header+strl_start-4,nhb-strl_start);

   if (AVI->a_chans && AVI->audio_bytes)
   {

   sampsize = avi_sampsize(AVI);

   /* Start the audio stream list ---------------------------------- */

   OUT4CC ("LIST");
   OUTLONG(0);        /* Length of list in bytes, don't know yet */
   strl_start = nhb;  /* Store start position */
   OUT4CC ("strl");

   /* The audio stream header */

   OUT4CC ("strh");
   OUTLONG(64);            /* # of bytes to follow */
   OUT4CC ("auds");
   OUT4CC ("\0\0\0\0");
   OUTLONG(0);             /* Flags */
   OUTLONG(0);             /* Reserved, MS says: wPriority, wLanguage */
   OUTLONG(0);             /* InitialFrames */
   OUTLONG(sampsize);      /* Scale */
   OUTLONG(sampsize*AVI->a_rate); /* Rate: Rate/Scale == samples/second */
   OUTLONG(0);             /* Start */
   OUTLONG(AVI->audio_bytes/sampsize);   /* Length */
   OUTLONG(0);             /* SuggestedBufferSize */
   OUTLONG(-1);            /* Quality */
   OUTLONG(sampsize);      /* SampleSize */
   OUTLONG(0);             /* Frame */
   OUTLONG(0);             /* Frame */
   OUTLONG(0);             /* Frame */
   OUTLONG(0);             /* Frame */
   /* The audio stream format */

   OUT4CC ("strf");
   OUTLONG(16);                   /* # of bytes to follow */
   OUTSHRT(AVI->a_fmt);           /* Format */
   OUTSHRT(AVI->a_chans);         /* Number of channels */
   OUTLONG(AVI->a_rate);          /* SamplesPerSec */
   OUTLONG(sampsize*AVI->a_rate); /* AvgBytesPerSec */
   OUTSHRT(sampsize);             /* BlockAlign */
   OUTSHRT(AVI->a_bits);          /* BitsPerSample */
   /* Finish stream list, i.e. put number of bytes in the list to proper pos */

   long2str(AVI_header+strl_start-4,nhb-strl_start);

   }

   /* Finish header list */

   long2str(AVI_header+hdrl_start-4,nhb-hdrl_start);

   /* Calculate the needed amount of junk bytes, output junk */

   njunk = HEADERBYTES - nhb - 8 - 12;

   /* Safety first: if njunk <= 0, somebody has played with
      HEADERBYTES without knowing what (s)he did.
      This is a fatal error */

   if(njunk<=0)
   {
      printf("AVI_close_output_file: # of header bytes too small\n");
      return(1);
   }

   OUT4CC ("JUNK");
   OUTLONG(njunk);
   memset(AVI_header+nhb,0,njunk);
   strcpy(AVI_header+nhb,"GrandStream");
   nhb += njunk;

   /* Start the movi list */

   OUT4CC ("LIST");
   OUTLONG(movi_len); /* Length of list in bytes */
   OUT4CC ("movi");

   /* Output the header, truncate the file to the number of bytes
      actually written, report an error if someting goes wrong */

   if ( lseek(AVI->fdes,0,SEEK_SET)<0 ||
        avi_write(AVI->fdes,(char *)AVI_header,HEADERBYTES)!=HEADERBYTES ||
		lseek(AVI->fdes,AVI->pos,SEEK_SET)<0)
     {
      		printf("error when writing index chunk\n");
		return (-1);
     }

   if(idxerror) return (-1);
   
   return 0;
}

int AVI_close_fd_1(avi_t *AVI)
{
	int ret = 0;
	if(AVI->fdes >0)
	{
		 /* 更新AVI头 */
		 ret = AVI_output_file_fd_1(AVI);
		 /* 最后写索引数据 */
		 //avi_add_chunk_fd(AVI,"idx1",(void*)AVI->idx,AVI->n_idx*16);
		 /* 关闭AVI文件 */
		 close(AVI->fdes);
		 AVI->fdes=-1;
	}
	else if(AVI->fdes <0)
		ret = -1;
	/* 释放索引空间 */
	if(NULL != AVI->idx)
	{
		free(AVI->idx);
		AVI->idx = NULL;
	}

	return ret;
}

static ssize_t avi_read(int fd, char *buf, size_t len)
{
   ssize_t n = 0;
   ssize_t r = 0;

   while (r < len) {
      n = read (fd, buf + r, len - r);
      if (n == 0)
	  break;
      if (n < 0) {
	  if (errno == EINTR)
	      continue;
	  else
	      break;
      } 

      r += n;
   }

   return r;
}

static u32 str2ulong(unsigned char *str)
{
   return ( str[0] | (str[1]<<8) | (str[2]<<16) | (str[3]<<24) );
}

static u32 str2ushort(unsigned char *str)
{
   return ( str[0] | (str[1]<<8) );
}

int avi_get_av_info_by_file_head(avi_t *AVI)
{
	int gs_flag = 0;
	long i, rate, scale;
	off_t n;
	unsigned char *hdrl_data;
	long header_offset=0, hdrl_len=0;
	int lasttag = 0;
	int vids_strh_seen = 0;
	int vids_strf_seen = 0;
	int auds_strh_seen = 0;
	int auds_strf_seen = 0;
	int auds_exist_flag = 0;
	long audio_bytes=0;
	int num_stream = 0;
	char data[256]={0};
	off_t oldpos=-1, newpos=-1;
#ifdef GS_FILESYSTEM
	RESEEK struseek, struheadoffset, strumovistart;
#endif

	AVI->pos = 0;
	if (lseek(AVI->fdes,AVI->pos,SEEK_SET) < 0)
	{
		printf("seek file err!\n");
		return (-1);
	}
	if( avi_read(AVI->fdes,data,12) != 12 )
	{
		printf("read file err!\n");
		return (-1);
	}
	if( strncasecmp(data  ,"RIFF",4) !=0 || strncasecmp(data+8,"AVI ",4) !=0 ) 
	{
		printf("data err!\n");
		return (-1);
	}
   /* Go through the AVI file and extract the header list,
      the start position of the 'movi' list and an optionally
      present idx1 tag */
      hdrl_data = 0;
	while(1)
	{
		memset(data,0,sizeof(data));
		if( avi_read(AVI->fdes,data,8) != 8 ) break; /* We assume it's EOF */
		newpos = lseek(AVI->fdes,0,SEEK_CUR);
		
		if(oldpos == newpos) {
			/* This is a broken AVI stream... */
			//return (-1);
			goto __exit_get_avi_head_info;
		}
		oldpos = newpos;

		n = str2ulong((unsigned char *)data+4);
		n = PAD_EVEN(n);

		if(strncasecmp(data,"LIST",4) == 0)
		{
			memset(data,0,sizeof(data));
			if( avi_read(AVI->fdes,data,4) != 4 ) 
				//ERR_EXIT(AVI_ERR_READ)
				goto __exit_get_avi_head_info;
			n -= 4;
			if(strncasecmp(data,"hdrl",4) == 0)
			{
				hdrl_len = n;
				hdrl_data = (unsigned char *) malloc(n);
				if(hdrl_data==0) ERR_EXIT(AVI_ERR_NO_MEM);
				
				header_offset = lseek(AVI->fdes,0,SEEK_CUR);
				if( avi_read(AVI->fdes,(char *)hdrl_data,n) != n ) //ERR_EXIT(AVI_ERR_READ)
				goto __exit_get_avi_head_info;
			}
			#if 0
			else if(strncasecmp(data,"movi",4) == 0)
			{
				AVI->movi_start = lseek(AVI->fdes,0,SEEK_CUR);
				/*这里n == 0  不知是否会出错*/
				if (lseek(AVI->fdes,n,SEEK_CUR)==(off_t)-1) break;
			}
			#endif
			else
				if (lseek(AVI->fdes,n,SEEK_CUR)==(off_t)-1) break;
		}
		/*如果是修复文件则可能没有idx1*/
		/*
		else if(strncasecmp(data,"idx1",4) == 0)
		{
			AVI->n_idx = AVI->max_idx = n/16;
			AVI->idx = (unsigned  char((*)[16]) ) malloc(n);
			if(AVI->idx==0) ERR_EXIT(AVI_ERR_NO_MEM)
			if(avi_read(AVI->fdes, (char *) AVI->idx, n) != n ) {
				free ( AVI->idx); AVI->idx=NULL;
				AVI->n_idx = 0;
			}
		}
		*/
		else if(strncasecmp(data,"JUNK",4) == 0)
		{
			lseek(AVI->fdes,0,SEEK_CUR);
			memset(data,0,sizeof(data));
			if( avi_read(AVI->fdes,data,12) != 12 ) //ERR_EXIT(AVI_ERR_READ)
				goto __exit_get_avi_head_info;
			n -= 12;
			if(0 == strncmp(data,"GrandStream",strlen("GrandStream")))
			{
				gs_flag = 1;
				if (lseek(AVI->fdes,n,SEEK_CUR)==(off_t)-1) break;
			}
			else
				//ERR_EXIT(AVI_ERR_READ)
				goto __exit_get_avi_head_info;
		}
		else
			lseek(AVI->fdes,n,SEEK_CUR);
	}

	
   	if(!hdrl_data) //ERR_EXIT(AVI_ERR_NO_HDRL)
   		goto __exit_get_avi_head_info;
   	//if(!AVI->movi_start) ERR_EXIT(AVI_ERR_NO_MOVI)

   	/* Interpret the header list */
   	for(i=0; i<hdrl_len; )
	{
		n = str2ulong(hdrl_data+i+4);
		n = PAD_EVEN(n);
		if(strncasecmp((char *)hdrl_data+i,"LIST",4)==0) 
		{
			i+= 12; 
			continue; 
		}
		else if(strncasecmp((char *)hdrl_data+i,"avih",4)==0)
		{
			i += 8;
			AVI->dwMicroSecPerFrame = str2ulong(hdrl_data+i);
			AVI->video_frames = str2ulong(hdrl_data+i+16);
			auds_exist_flag = str2ulong(hdrl_data+i+24);
			AVI->dwSuggestedBufferSize = str2ulong(hdrl_data+i+28);
			AVI->width = str2ulong(hdrl_data+i+32);
			AVI->height = str2ulong(hdrl_data+i+36);
			AVI->bt = str2ulong(hdrl_data+i+48);
			i += n;		/*64=8+56*/
			continue; 
		}
	 	else if(strncasecmp((char *)hdrl_data+i,"strh",4)==0)
		{
			i += 8; /*n=64*/
			if(strncasecmp((char *)hdrl_data+i,"vids",4) == 0 && !vids_strh_seen)
			{
				memcpy(AVI->compressor,hdrl_data+i+4,4);
				AVI->compressor[4] = 0;
				AVI->v_codech_off = header_offset + i+4;
				scale = str2ulong(hdrl_data+i+20);
				rate  = str2ulong(hdrl_data+i+24);
				if(scale!=0) AVI->fps = (double)rate/(double)scale;
				AVI->video_frames = str2ulong(hdrl_data+i+32);
				vids_strh_seen = 1;
				i += n;		/*72=8+64*/
				vids_strh_seen = 1;
           			lasttag = 1; /* vids */
				continue;
			}
			else if (strncasecmp ((char *)hdrl_data+i,"auds",4) ==0 && ! auds_strh_seen)
			{
				AVI->aptr=AVI->anum;
				++AVI->anum;
				if(AVI->anum > AVI_MAX_TRACKS) {
					fprintf(stderr, "error - only %d audio tracks supported\n", AVI_MAX_TRACKS);
					//return(-1);
					goto __exit_get_avi_head_info;
				}
				audio_bytes = str2ulong(hdrl_data+i+32);
				AVI->track[AVI->aptr].audio_strn = num_stream;
				// if samplesize==0 -> vbr
				AVI->track[AVI->aptr].a_vbr = !str2ulong(hdrl_data+i+44);
				AVI->track[AVI->aptr].padrate = str2ulong(hdrl_data+i+24);
				lasttag = 2; /* auds */

				AVI->track[AVI->aptr].a_codech_off = header_offset + i;
			 	auds_strh_seen = 1;
			}
			else if (strncasecmp ((const char*)hdrl_data+i,"iavs",4) ==0 && ! auds_strh_seen) 
			{
				fprintf(stderr, "AVILIB: error - DV AVI Type 1 no supported\n");
				//return (-1);
				goto __exit_get_avi_head_info;
			}
			else
				lasttag = 0;
			num_stream++;
		}
		else if(strncasecmp((char *)hdrl_data+i,"strf",4)==0)
		{
	         	i += 8; 		/*video:n=40/audio:n=16*/
			if(lasttag == 1)
			{
				#if 0
				alBITMAPINFOHEADER bih;
				memcpy(&bih, hdrl_data + i, sizeof(alBITMAPINFOHEADER));
				AVI->bitmap_info_header = (alBITMAPINFOHEADER *)malloc(str2ulong((unsigned char *)&bih.bi_size));
				if (AVI->bitmap_info_header != NULL)
					memcpy(AVI->bitmap_info_header, hdrl_data + i,
				 str2ulong((unsigned char *)&bih.bi_size));
				#endif
				AVI->width  = str2ulong(hdrl_data+i+4);
				AVI->height = str2ulong(hdrl_data+i+8);
				AVI->v_codecf_off = header_offset + i+16;
				memcpy(AVI->compressor2, hdrl_data+i+16, 4);
				AVI->compressor2[4] = 0;
				vids_strf_seen = 1;
				i += n;
				continue ;
			}
	         	else if(lasttag == 2)
			{
				
				#if 0
				alWAVEFORMATEX *wfe;
				char *nwfe;
				int wfes;

				if ((hdrl_len - i) < sizeof(alWAVEFORMATEX))
				  wfes = hdrl_len - i;
				else
				  wfes = sizeof(alWAVEFORMATEX);
				wfe = (alWAVEFORMATEX *)malloc(sizeof(alWAVEFORMATEX));
				if (wfe != NULL) 
				{
					memset(wfe, 0, sizeof(alWAVEFORMATEX));
					memcpy(wfe, hdrl_data + i, wfes);
					if (str2ushort((unsigned char *)&wfe->cb_size) != 0) 
					{
						nwfe = (char *)
						realloc(wfe, sizeof(alWAVEFORMATEX) + str2ushort((unsigned char *)&wfe->cb_size));
						if (nwfe != 0) 
						{

							off_t lpos = lseek(AVI->fdes, 0, SEEK_CUR);
							lseek(AVI->fdes, header_offset + i + sizeof(alWAVEFORMATEX),
							SEEK_SET);
							wfe = (alWAVEFORMATEX *)nwfe;
							nwfe = &nwfe[sizeof(alWAVEFORMATEX)];
							avi_read(AVI->fdes, nwfe,
							           str2ushort((unsigned char *)&wfe->cb_size));
							lseek(AVI->fdes, lpos, SEEK_SET);
						}
					}
					AVI->wave_format_ex[AVI->aptr] = wfe;
				}
				#endif
				AVI->track[AVI->aptr].a_fmt   = str2ushort(hdrl_data+i );
				AVI->track[AVI->aptr].a_codecf_off = header_offset + i;
				AVI->track[AVI->aptr].a_chans = str2ushort(hdrl_data+i+2);
				AVI->track[AVI->aptr].a_rate  = str2ulong (hdrl_data+i+4);
				//ThOe: read mp3bitrate
				AVI->track[AVI->aptr].mp3rate = 8*str2ulong(hdrl_data+i+8)/1000;
				AVI->track[AVI->aptr].a_bits  = str2ushort(hdrl_data+i+14);
				auds_strf_seen = 1;
				AVI->track[AVI->aptr].audio_bytes = audio_bytes*avi_sampsize(AVI);
				//printf("The aptr is %d ,audio byte is %u\n",AVI->aptr,AVI->track[AVI->aptr].audio_bytes);
				AVI_set_audio(AVI,AVI->track[AVI->aptr].a_chans,
					AVI->track[AVI->aptr].a_rate,AVI->track[AVI->aptr].a_bits,
					AVI->track[AVI->aptr].a_fmt);
			}
      		}
		else
		{
			i += 8;
			lasttag = 0;
		}
		//printf("adding %ld bytes\n", (long int)n);
		 i += n;
	}
__exit_get_avi_head_info:
	if(hdrl_data)
	{
		free(hdrl_data);
		hdrl_data = NULL;
	}
	if(auds_exist_flag == 2)
	{
		if(vids_strh_seen && vids_strf_seen  && auds_strh_seen && auds_strf_seen)
		{
			//printf("Have get video and audio info\n");
			return  0;
		}
	}
	else
	{
		if(vids_strh_seen && vids_strf_seen)
		{
			//printf("Have get video info no audio info\n");
			return  0;
		}
	}
  	return(-1);
}

int AVI_write_frame_index(avi_t *AVI,int bytes)
{
	long pos;

	pos = AVI->pos;
	int n;

	/* Check for maximum file length */

	if ( (AVI->pos + 8 + bytes + 8 + (AVI->n_idx+1)*16) > AVI->buf_len )
	{
		printf("avifile had beyond buf_len %d\n",AVI->buf_len);
		return (-1);
	}

	/* Add index entry */
	n = avi_add_index_entry(AVI,(unsigned char *)"00dc",0x10,AVI->pos,bytes);
	if(n)
	{
		return (-1);
	}
	AVI->pos += 8 + PAD_EVEN(bytes);
	AVI->last_pos = pos;
	AVI->last_len = bytes;
	AVI->video_frames++;
	//printf("*****************video  length is %d*************\n",bytes);
	return 0;
}

int AVI_write_audio_index(avi_t *AVI, int bytes)
{
	int n;

	/* Check for maximum file length */

	if ( (AVI->pos + 8 + bytes + 8 + (AVI->n_idx+1)*16) > AVI->buf_len )
	{
		printf("avifile had beyond buf_len %d\n",AVI->buf_len);
		return (-1);
	}

	/* Add index entry */
	n = avi_add_index_entry(AVI,(unsigned char *)"01wb",0x00,AVI->pos,bytes);
	if(n)
	{
		return (-1);
	}
	AVI->pos += 8 + PAD_EVEN(bytes);
	AVI->audio_bytes += bytes;
	//printf("*****************audio  length is %d*************\n",bytes);
	return 0;
}

int AVI_deformity_file(char  *file_name, int duration,unsigned int *tm_len)
{
	int ret = RECORD_DEFORM_SUCCESS;
	char data[16]={0};
	void *ptr = NULL;
	off_t oldpos=-1, newpos=-1;
	unsigned int  length = 0;
	avi_t			avi;
	avi_t			*pAvi = &avi;
	struct stat 		s;
	memset(&s,0,sizeof(struct stat));
	if(stat(file_name, &s) != 0)
	{
		printf("Can't get file stat\n");
		return RECORD_UNABLE_DEFORM;
	}
	length = s.st_size;
	if(length <= HEADERBYTES)
	{
		printf("deformity file len < 2048\n");
		return RECORD_UNABLE_DEFORM;
	}
	memset(&avi,0,sizeof(avi));
	pAvi->fdes = open(file_name, O_RDWR);
	if (pAvi->fdes < 0) 
	{
		printf("open file %s err!\n",file_name);
		return RECORD_UNABLE_DEFORM;
	}
	ret = avi_get_av_info_by_file_head(pAvi);
	if(ret < 0)
	{
		printf("Can't get av info\n");
		ret =  RECORD_DEFORM_FAIL;
		goto __exit_deformity_file;
	}
	if(((pAvi->height%288)==0) && pAvi->width >=352)
	{
		/* 大于 PAL CIF */
		pAvi->dwMicroSecPerFrame = 40000;
        	pAvi->dwSuggestedBufferSize =144008;
	}
	else 
	{
		/* 小于 PAL CIF */
		pAvi->dwMicroSecPerFrame = 33366;
        	pAvi->dwSuggestedBufferSize =120008;
	}
	
	ptr = malloc((duration*80+1024)*16);
	if(NULL == ptr) 
	{
		printf("Can't malloc\n");
		ret =  RECORD_DEFORM_FAIL;
		goto __exit_deformity_file;
	}
	pAvi->idx = (unsigned char((*)[16])) ptr;

	/* 限制AVI文件最大1GB */
	pAvi->buf_len = 1024*1024*1024;
	/*
	printf("The audio a_fmt is %d\n",pAvi->track[pAvi->aptr].a_fmt);
	printf("The audio a_chans is %d\n",pAvi->track[pAvi->aptr].a_chans);
	printf("The audio a_rate is %d\n",pAvi->track[pAvi->aptr].a_rate);
	printf("The audio mp3rate is %d\n",pAvi->track[pAvi->aptr].mp3rate);
	printf("The audio a_bits is %d\n",pAvi->track[pAvi->aptr].a_bits);
	AVI_set_audio(pAvi,pAvi->track[pAvi->aptr].a_chans,pAvi->track[pAvi->aptr].a_rate,
				pAvi->track[pAvi->aptr].a_bits,pAvi->track[pAvi->aptr].a_fmt);
	*/
	//AVI_set_audio(pAvi,1,8000,16, 1);
	pAvi->buf = NULL;
	pAvi->pos = HEADERBYTES;
	
	if (lseek(pAvi->fdes,pAvi->pos,SEEK_SET) < 0)
	{
		printf("lseek file %s err!\n",file_name);
		ret =  RECORD_DEFORM_FAIL;
		goto __exit_deformity_file;
	}
	pAvi->last_pos = 0;
	pAvi->last_len = 0;
	pAvi->video_frames = 0;
	pAvi->audio_bytes = 0;
	while(1)
	{
		memset(data,0,sizeof(data));
		if( avi_read(pAvi->fdes,data,8) != 8 ) break; /* We assume it's EOF */
		newpos = lseek(pAvi->fdes,0,SEEK_CUR);		/*+8*/
		if(oldpos == newpos) 
			break;
		oldpos = newpos;
		if(strncasecmp(data,"00dc",4) == 0)  /*video*/
		{	
			length = str2ulong((unsigned char *)data+4);
			ret = AVI_write_frame_index(pAvi,length);
			if(ret < 0)
			{
				printf("write frame err!\n");
				ret =  RECORD_DEFORM_FAIL;
				goto __exit_deformity_file;
			}
		}
		else if(strncasecmp(data,"01wb",4) == 0)  /*audio*/
		{
			length = str2ulong((unsigned char *)data+4);
			ret = AVI_write_audio_index(pAvi,length);
			if(ret < 0)
			{
				printf("write audio err!\n");
				ret =  RECORD_DEFORM_FAIL;
				goto __exit_deformity_file;
				
			}
		}
		else
		{			
			break;
		}
		if(length&1)
			length = PAD_EVEN(length);
		newpos = lseek(pAvi->fdes,length,SEEK_CUR);
	}
	if (lseek(pAvi->fdes,pAvi->pos,SEEK_SET) < 0)
	{
		printf("lseek file %s err!\n",file_name);
		ret = RECORD_DEFORM_FAIL;
	}
__exit_deformity_file:
	 if(pAvi->fps)
	 {
	 	*tm_len = pAvi->video_frames/pAvi->fps;
	 }
	AVI_close_fd(pAvi);
	return ret;
}

int AVI_init_file_header(avi_t *AVI)
{
	int njunk, sampsize, hasIndex,  idxerror;
	int movi_len, hdrl_start, strl_start;
	unsigned char AVI_header[HEADERBYTES];
	long nhb;

	movi_len = AVI->pos - HEADERBYTES + 4;
   
	idxerror = 0;
	hasIndex = 0;
  
	/* Prepare the file header */
	nhb = 0;

	/* The RIFF header */

	OUT4CC ("RIFF");
	OUTLONG(AVI->pos - 8);    /* # of bytes to follow */
	OUT4CC ("AVI ");

	/* Start the header list */

	OUT4CC ("LIST");
	OUTLONG(0);        /* Length of list in bytes, don't know yet */
	hdrl_start = nhb;  /* Store start position */
	OUT4CC ("hdrl");

	/* The main AVI header */

	/* The Flags in AVI File header */

	OUT4CC ("avih");
	OUTLONG(56);                 /* # of bytes to follow */
	OUTLONG(AVI->dwMicroSecPerFrame);       /* Microseconds per frame */
	OUTLONG(3600000);           /* MaxBytesPerSec, I hope this will never be used */
	OUTLONG(512);                  /* PaddingGranularity (whatever that might be) */
	                            /* Other sources call it 'reserved' */

	OUTLONG(2064);               /* Flags */
	OUTLONG(AVI->video_frames);  /* TotalFrames */
	OUTLONG(0);                  /* InitialFrames */
	if (AVI->audio_bytes)
	  { OUTLONG(2); }           /* Streams */
	else
	  { OUTLONG(1); }           /* Streams */
	OUTLONG(AVI->dwSuggestedBufferSize);                  /* SuggestedBufferSize */
	OUTLONG(AVI->width);         /* Width */
	OUTLONG(AVI->height);        /* Height */
	                            /* MS calls the following 'reserved': */
	OUTLONG(0);                  /* TimeScale:  Unit used to measure time */
	OUTLONG(0);                  /* DataRate:   Data rate of playback     */
	//OUTLONG(0);                  /* StartTime:  Starting time of AVI data */
	OUTLONG(AVI->bt);         /* StartTime:  Starting time of AVI data */
	OUTLONG(0);                  /* DataLength: Size of AVI data chunk    */


	/* Start the video stream list ---------------------------------- */

	OUT4CC ("LIST");
	OUTLONG(0);        /* Length of list in bytes, don't know yet */
	strl_start = nhb;  /* Store start position */
	OUT4CC ("strl");

	/* The video stream header */

	OUT4CC ("strh");
	OUTLONG(64);                 /* # of bytes to follow */
	OUT4CC ("vids");             /* Type */
	OUT4CC (AVI->compressor);    /* Handler */
	OUTLONG(0);                  /* Flags */
	OUTLONG(0);                  /* Reserved, MS says: wPriority, wLanguage */
	OUTLONG(0);                  /* InitialFrames */
	OUTLONG(1);                  /* Scale */
	OUTLONG(AVI->fps);                  /* Rate: Rate/Scale == samples/second */
	OUTLONG(0);                  /* Start */
	OUTLONG(AVI->video_frames);  /* Length */
	OUTLONG(-1);                 /* SuggestedBufferSize */
	OUTLONG(0);                  /* Quality */
	OUTLONG(0);                  /* SampleSize */
	OUTLONG(0);                  /* Frame */
	OUTLONG(0);                  /* Frame */
	OUTLONG(0);                  /* Frame */
	OUTLONG(0);                  /* Frame */

	/* The video stream format */

	OUT4CC ("strf");
	OUTLONG(40);                 /* # of bytes to follow */
	OUTLONG(40);                 /* Size */
	OUTLONG(AVI->width);         /* Width */
	OUTLONG(AVI->height);        /* Height */
	OUTSHRT(1);
	OUTSHRT(24);     /* Planes, Count */
	OUT4CC (AVI->compressor);    /* Compression */
	//OUTLONG(AVI->width*AVI->height);  /* SizeImage (in bytes?) */
	OUTLONG(144000);             /* SizeImage (in bytes?) */
	OUTLONG(0);                  /* XPelsPerMeter */
	OUTLONG(0);                  /* YPelsPerMeter */
	OUTLONG(0);                  /* ClrUsed: Number of colors used */
	OUTLONG(0);                  /* ClrImportant: Number of colors important */

	/* Finish stream list, i.e. put number of bytes in the list to proper pos */

	long2str(AVI_header+strl_start-4,nhb-strl_start);

	//if (AVI->a_chans && AVI->audio_bytes)
	if (AVI->a_chans)
	{
		sampsize = avi_sampsize(AVI);

		/* Start the audio stream list ---------------------------------- */

		OUT4CC ("LIST");
		OUTLONG(0);        /* Length of list in bytes, don't know yet */
		strl_start = nhb;  /* Store start position */
		OUT4CC ("strl");

		/* The audio stream header */

		OUT4CC ("strh");
		OUTLONG(64);            /* # of bytes to follow */
		OUT4CC ("auds");
		OUT4CC ("\0\0\0\0");
		OUTLONG(0);             /* Flags */
		OUTLONG(0);             /* Reserved, MS says: wPriority, wLanguage */
		OUTLONG(0);             /* InitialFrames */
		OUTLONG(sampsize);      /* Scale */
		OUTLONG(sampsize*AVI->a_rate); /* Rate: Rate/Scale == samples/second */
		OUTLONG(0);             /* Start */
		OUTLONG(AVI->audio_bytes/sampsize);   /* Length */
		OUTLONG(0);             /* SuggestedBufferSize */
		OUTLONG(-1);            /* Quality */
		OUTLONG(sampsize);      /* SampleSize */
		OUTLONG(0);             /* Frame */
		OUTLONG(0);             /* Frame */
		OUTLONG(0);             /* Frame */
		OUTLONG(0);             /* Frame */

		/* The audio stream format */

		OUT4CC ("strf");
		OUTLONG(16);                   /* # of bytes to follow */
		OUTSHRT(AVI->a_fmt);           /* Format */
		OUTSHRT(AVI->a_chans);         /* Number of channels */
		OUTLONG(AVI->a_rate);          /* SamplesPerSec */
		OUTLONG(sampsize*AVI->a_rate); /* AvgBytesPerSec */
		OUTSHRT(sampsize);             /* BlockAlign */
		OUTSHRT(AVI->a_bits);          /* BitsPerSample */

		/* Finish stream list, i.e. put number of bytes in the list to proper pos */

		long2str(AVI_header+strl_start-4,nhb-strl_start);

	}

	/* Finish header list */

	long2str(AVI_header+hdrl_start-4,nhb-hdrl_start);

	/* Calculate the needed amount of junk bytes, output junk */

	njunk = HEADERBYTES - nhb - 8 - 12;

	/* Safety first: if njunk <= 0, somebody has played with
	  HEADERBYTES without knowing what (s)he did.
	  This is a fatal error */

	if(njunk<=0)
	{
		printf("AVI_close_output_file: # of header bytes too small\n");
		return(1);
	}

	OUT4CC ("JUNK");
	OUTLONG(njunk);
	memset(AVI_header+nhb,0,njunk);
	strcpy(AVI_header+nhb,"GrandStream");
	nhb += njunk;

	/* Start the movi list */

	OUT4CC ("LIST");
	OUTLONG(movi_len); /* Length of list in bytes */
	OUT4CC ("movi");/*4: movi_len = AVI->pos - HEADERBYTES + 4;*/

	/* Output the header, truncate the file to the number of bytes
	  actually written, report an error if someting goes wrong */
	if ( lseek(AVI->fdes,0,SEEK_SET)<0 ||
	avi_write(AVI->fdes,(char *)AVI_header,HEADERBYTES)!=HEADERBYTES ||
	lseek(AVI->fdes,AVI->pos,SEEK_SET)<0)
	{
		return (-1);
	}
	if(idxerror) return (-1);
  	return 0;
}

int AVI_Init_fd_1(avi_t *AVI, int width, int height, int fps, const char *compressor, int duration, const char *file_name)
{
	int ret,fd;
	char recordFlag =1;
	void *ptr = NULL;

	/* apply for index space */
	ptr = malloc((duration*80+1024)*16);
	if(NULL == ptr) 
	{
		printf("malloc err!\n");
		return (-1);
	}
	AVI->pos = HEADERBYTES;
	AVI->idx = (unsigned char((*)[16]) ) ptr;

	/* 限制AVI文件最大1GB */
	AVI->buf_len = 1024*1024*1024;		
	/* write avi head */
	AVI_set_video(AVI, width, height, fps, compressor);
	//if(AUDIO_ENC_FMT_AAC == CFG_GetAudioEncType())
		//AVI_set_audio(AVI,1,16000,16,1);
	//else
		AVI_set_audio(AVI,1,8000,16, 1);

	AVI->fdes = open(file_name, O_RDWR | O_NONBLOCK | O_CREAT | O_TRUNC, 00644);
	if (AVI->fdes < 0) 
	{
		printf("create file %s err!\n",file_name);
		AVI_close_fd(AVI);
		return (-1);
	}
	usleep(10000);
	fd=open("/var/run/hostPlus",O_WRONLY |O_NONBLOCK | O_CREAT | O_TRUNC, 00644);
	//fd=open("/var/run/hostPlus",O_RDWR |O_NONBLOCK | O_CREAT | O_TRUNC, 00644);
	if(fd>=0)
	{
		write(fd,&recordFlag,1);
		fdatasync(fd);
		close(fd);
	}
	usleep(10000);
	ret = AVI_init_file_header(AVI);
	//ret =  AVI_output_file_fd(AVI);
	if(ret != 0)
	{
		printf("init file header err!\n");
		AVI_close_fd(AVI);
		return (-1);
	}
	AVI->pos = HEADERBYTES;
	if (lseek(AVI->fdes,AVI->pos,SEEK_SET) < 0)
	{
		printf("lseek file %s err!\n",file_name);
		AVI_close_fd(AVI);
		return (-1);
	}
	AVI->buf = NULL;
	AVI->bt = 0;
	AVI->et = 0;
	sync();
	return(1);
}

#ifdef AVI_READ
/*******************************************************************
 *                                                                 *
 *    Utilities for reading video and audio from an AVI File       *
 *                                                                 *
 *******************************************************************/

int AVI_close_1(avi_t *AVI)
{
   int ret;

   /* If the file was open for writing, the header and index still have
      to be written */

   if(AVI->mode == AVI_MODE_WRITE)
      ret = AVI_output_file_fd_1(AVI);
   else
      ret = 0;

   /* Even if there happened a error, we first clean up */
#ifdef FILE_OP
    fclose(AVI->fpFile);
#else
   close(AVI->fdes);
#endif
   if(AVI->idx) free(AVI->idx);
   if(AVI->video_index) free(AVI->video_index);
   if(AVI->audio_index) free(AVI->audio_index);
   free(AVI);

   return ret;
}

avi_t *AVI_open_input_file(char *filename, int getIndex)
{
   avi_t *AVI;
   long i, n, rate, scale, idx_type;
   unsigned char *hdrl_data;
   long hdrl_len;
   long nvi, nai, ioff;
   long tot;
   int lasttag = 0;
   int vids_strh_seen = 0;
   int vids_strf_seen = 0;
   int auds_strh_seen = 0;
   int auds_strf_seen = 0;
   int num_stream = 0;
   char data[256];
   int iFpos=0;
   /* Create avi_t structure */


   AVI = (avi_t *) malloc(sizeof(avi_t));
   if(AVI==NULL)
   {
      AVI_errno = AVI_ERR_NO_MEM;
      return 0;
   }
   memset((void *)AVI,0,sizeof(avi_t));

   AVI->mode = AVI_MODE_READ; /* open for reading */

   /* Open the file */
#ifdef FILE_OP
  AVI->fpFile= fopen(filename,"rb+");
   if(NULL  == AVI->fpFile)
   {
      AVI_errno = AVI_ERR_OPEN;
      free(AVI);
      return 0;
   }
#else
   AVI->fdes = open(filename,O_RDONLY);
   if(AVI->fdes < 0)
   {
      AVI_errno = AVI_ERR_OPEN;
      free(AVI);
      return 0;
   }

#endif
   /* Read first 12 bytes and check that this is an AVI file */

#ifdef  FILE_OP
      if( fread( data,12,1,AVI->fpFile) != SUCCESS ) ERR_EXIT(AVI_ERR_READ)

#else

   if( read(AVI->fdes,data,12) != 12 ) ERR_EXIT(AVI_ERR_READ)
   	
#endif

   if( strncasecmp(data  ,"RIFF",4) !=0 ||
       strncasecmp(data+8,"AVI ",4) !=0 ) ERR_EXIT(AVI_ERR_NO_AVI)

   /* Go through the AVI file and extract the header list,
      the start position of the 'movi' list and an optionally
      present idx1 tag */

   hdrl_data = 0;

   while(1)
   {

#ifdef FILE_OP
         if( fread(data,8,1,AVI->fpFile) != SUCCESS ) break; /* We assume it's EOF */
#else   
      if( read(AVI->fdes,data,8) != 8 ) break; /* We assume it's EOF */
#endif
      n = str2ulong((unsigned char *)data+4);
      n = PAD_EVEN(n);

      if(strncasecmp(data,"LIST",4) == 0)
      {
#ifdef FILE_OP
         if( fread(data,4,1,AVI->fpFile) != SUCCESS ) ERR_EXIT(AVI_ERR_READ)
#else
         if( read(AVI->fdes,data,4) != 4 ) ERR_EXIT(AVI_ERR_READ)
#endif		 	
         n -= 4;
         if(strncasecmp(data,"hdrl",4) == 0)
         {
            hdrl_len = n;
            hdrl_data = (unsigned char *) malloc(n);
            if(hdrl_data==0) ERR_EXIT(AVI_ERR_NO_MEM)
#ifdef FILE_OP
          if( fread( hdrl_data,n,1,AVI->fpFile) != SUCCESS ) ERR_EXIT(AVI_ERR_READ)

#else
            if( read(AVI->fdes,hdrl_data,n) != n ) ERR_EXIT(AVI_ERR_READ)
#endif				
         }
         else if(strncasecmp(data,"movi",4) == 0)
         {
#ifdef FILE_OP    
            fseek(AVI->fpFile,0,SEEK_CUR);
            AVI->movi_start = ftell(AVI->fpFile);
            fseek(AVI->fpFile,n,SEEK_CUR);
#else
              AVI->movi_start = lseek(AVI->fdes,0,SEEK_CUR);
            lseek(AVI->fdes,n,SEEK_CUR);
#endif
         }
         else
#ifdef FILE_OP		 	
            fseek(AVI->fpFile,n,SEEK_CUR);
#else
            lseek(AVI->fdes,n,SEEK_CUR);

#endif
      }
      else if(strncasecmp(data,"idx1",4) == 0)
      {
         /* n must be a multiple of 16, but the reading does not
            break if this is not the case */

         AVI->n_idx = AVI->max_idx = n/16;
         AVI->idx = (unsigned  char((*)[16]) ) malloc(n);
         if(AVI->idx==0) ERR_EXIT(AVI_ERR_NO_MEM)
#ifdef FILE_OP
                if( fread( AVI->idx,n,1,AVI->fpFile) != SUCCESS ) ERR_EXIT(AVI_ERR_READ)

#else
         if( read(AVI->fdes,AVI->idx,n) != n ) ERR_EXIT(AVI_ERR_READ)
#endif		 	
      }
      else
#ifdef FILE_OP	  	
         fseek(AVI->fpFile,n,SEEK_CUR);
#else
        lseek(AVI->fdes,n,SEEK_CUR);

#endif
   }

   if(!hdrl_data      ) ERR_EXIT(AVI_ERR_NO_HDRL)
   if(!AVI->movi_start) ERR_EXIT(AVI_ERR_NO_MOVI)

   /* Interpret the header list */

   for(i=0;i<hdrl_len;)
   {
      /* List tags are completly ignored */

      if(strncasecmp((char *)hdrl_data+i,"LIST",4)==0) { i+= 12; continue; }

      n = str2ulong(hdrl_data+i+4);
      n = PAD_EVEN(n);

      /* Interpret the tag and its args */

      if(strncasecmp((char *)hdrl_data+i,"strh",4)==0)
      {
         i += 8;
         if(strncasecmp((char *)hdrl_data+i,"vids",4) == 0 && !vids_strh_seen)
         {
            memcpy(AVI->compressor,hdrl_data+i+4,4);
            AVI->compressor[4] = 0;
            scale = str2ulong(hdrl_data+i+20);
            rate  = str2ulong(hdrl_data+i+24);
            if(scale!=0) AVI->fps = (double)rate/(double)scale;
            AVI->video_frames = str2ulong(hdrl_data+i+32);
            AVI->video_strn = num_stream;
            vids_strh_seen = 1;
            lasttag = 1; /* vids */
         }
         else if (strncasecmp ((char *)hdrl_data+i,"auds",4) ==0 && ! auds_strh_seen)
         {
            AVI->audio_bytes = str2ulong(hdrl_data+i+32)*avi_sampsize(AVI);
            AVI->audio_strn = num_stream;
            auds_strh_seen = 1;
            lasttag = 2; /* auds */
         }
         else
            lasttag = 0;
         num_stream++;
      }
      else if(strncasecmp((char *)hdrl_data+i,"strf",4)==0)
      {
         i += 8;
         if(lasttag == 1)
         {
            AVI->width  = str2ulong(hdrl_data+i+4);
            AVI->height = str2ulong(hdrl_data+i+8);
            vids_strf_seen = 1;
         }
         else if(lasttag == 2)
         {
            AVI->a_fmt   = str2ushort(hdrl_data+i  );
            AVI->a_chans = str2ushort(hdrl_data+i+2);
            AVI->a_rate  = str2ulong (hdrl_data+i+4);
            AVI->a_bits  = str2ushort(hdrl_data+i+14);
            auds_strf_seen = 1;
         }
         lasttag = 0;
      }
      else
      {
         i += 8;
         lasttag = 0;
      }

      i += n;
   }

   free(hdrl_data);

   if(!vids_strh_seen || !vids_strf_seen || AVI->video_frames==0) ERR_EXIT(AVI_ERR_NO_VIDS)

   AVI->video_tag[0] = AVI->video_strn/10 + '0';
   AVI->video_tag[1] = AVI->video_strn%10 + '0';
   AVI->video_tag[2] = 'd';
   AVI->video_tag[3] = 'b';

   /* Audio tag is set to "99wb" if no audio present */
   if(!AVI->a_chans) AVI->audio_strn = 99;

   AVI->audio_tag[0] = AVI->audio_strn/10 + '0';
   AVI->audio_tag[1] = AVI->audio_strn%10 + '0';
   AVI->audio_tag[2] = 'w';
   AVI->audio_tag[3] = 'b';

#ifdef FILE_OP
      fseek(AVI->fpFile,AVI->movi_start,SEEK_SET);
#else
      lseek(AVI->fdes,AVI->movi_start,SEEK_SET);

#endif
   /* get index if wanted */

   if(!getIndex) return AVI;

   /* if the file has an idx1, check if this is relative
      to the start of the file or to the start of the movi list */

   idx_type = 0;

   if(AVI->idx)
   {
      long pos, len;

      /* Search the first videoframe in the idx1 and look where
         it is in the file */

      for(i=0;i<AVI->n_idx;i++)
         if( strncasecmp((char*)AVI->idx[i],AVI->video_tag,3)==0 ) break;
      if(i>=AVI->n_idx) ERR_EXIT(AVI_ERR_NO_VIDS)

      pos = str2ulong(AVI->idx[i]+ 8);
      len = str2ulong(AVI->idx[i]+12);
#ifdef FILE_OP
      fseek(AVI->fpFile,pos,SEEK_SET);
      if(fread( data,8,1,AVI->fpFile)!=SUCCESS) ERR_EXIT(AVI_ERR_READ)
#else
     lseek(AVI->fdes,pos,SEEK_SET);
      if(read(AVI->fdes,data,8)!=8) ERR_EXIT(AVI_ERR_READ)
#endif
      if( strncasecmp((char*)data,(char*)AVI->idx[i],4)==0 && str2ulong((unsigned char*)data+4)==len )
      {
         idx_type = 1; /* Index from start of file */
      }
      else
      {
#ifdef FILE_OP      
         fseek(AVI->fpFile,pos+AVI->movi_start-4,SEEK_SET);
         if(fread( data,8,1,AVI->fpFile)!=SUCCESS) ERR_EXIT(AVI_ERR_READ)
#else
         lseek(AVI->fdes,pos+AVI->movi_start-4,SEEK_SET);
         if(read(AVI->fdes,data,8)!=8) ERR_EXIT(AVI_ERR_READ)
#endif
         if( strncasecmp(data,(char*)AVI->idx[i],4)==0 && str2ulong((unsigned char*)data+4)==len )
         {
            idx_type = 2; /* Index from start of movi list */
         }
      }
      /* idx_type remains 0 if neither of the two tests above succeeds */
   }

   if(idx_type == 0)
   {
      /* we must search through the file to get the index */
#ifdef FILE_OP
     fseek(AVI->fpFile, AVI->movi_start, SEEK_SET);
#else
      lseek(AVI->fdes, AVI->movi_start, SEEK_SET);

#endif
      AVI->n_idx = 0;

      while(1)
      {
#ifdef FILE_OP      
         if( fread( data,8,1,AVI->fpFile) != SUCCESS ) break;
#else
        if( read(AVI->fdes,data,8) != 8 ) break;

#endif
         n = str2ulong((unsigned char*)data+4);

         /* The movi list may contain sub-lists, ignore them */

         if(strncasecmp(data,"LIST",4)==0)
         {

#ifdef FILE_OP		 
            fseek(AVI->fpFile,4,SEEK_CUR);
#else
             lseek(AVI->fdes,4,SEEK_CUR);

#endif
            continue;
         }

         /* Check if we got a tag ##db, ##dc or ##wb */

         if( ( (data[2]=='d' || data[2]=='D') &&
               (data[3]=='b' || data[3]=='B' || data[3]=='c' || data[3]=='C') )
          || ( (data[2]=='w' || data[2]=='W') &&
               (data[3]=='b' || data[3]=='B') ) )
         {

#ifdef FILE_OP
                iFpos=ftell(AVI->fpFile);
                avi_add_index_entry(AVI,(unsigned char*)data,0,iFpos-8,n);

#else
            avi_add_index_entry(AVI,data,0,lseek(AVI->fdes,0,SEEK_CUR)-8,n);
#endif
         }
#ifdef FILE_OP
         fseek(AVI->fpFile,PAD_EVEN(n),SEEK_CUR);
#else
                 lseek(AVI->fdes,PAD_EVEN(n),SEEK_CUR);

#endif
      }
      idx_type = 1;
   }

   /* Now generate the video index and audio index arrays */

   nvi = 0;
   nai = 0;

   for(i=0;i<AVI->n_idx;i++)
   {
      if(strncasecmp((char *)AVI->idx[i],AVI->video_tag,3) == 0) nvi++;
      if(strncasecmp((char *)AVI->idx[i],AVI->audio_tag,4) == 0) nai++;
   }

   AVI->video_frames = nvi;
   AVI->audio_chunks = nai;

   if(AVI->video_frames==0) ERR_EXIT(AVI_ERR_NO_VIDS)
   AVI->video_index = (video_index_entry *) malloc(nvi*sizeof(video_index_entry));
   if(AVI->video_index==0) ERR_EXIT(AVI_ERR_NO_MEM)
   if(AVI->audio_chunks)
   {
      AVI->audio_index = (audio_index_entry *) malloc(nai*sizeof(audio_index_entry));
      if(AVI->audio_index==0) ERR_EXIT(AVI_ERR_NO_MEM)
   }

   nvi = 0;
   nai = 0;
   tot = 0;
   ioff = idx_type == 1 ? 8 : AVI->movi_start+4;

   for(i=0;i<AVI->n_idx;i++)
   {
      if(strncasecmp((char *)AVI->idx[i],AVI->video_tag,3) == 0)
      {
         AVI->video_index[nvi].pos = str2ulong(AVI->idx[i]+ 8)+ioff;
         AVI->video_index[nvi].len = str2ulong(AVI->idx[i]+12);
         nvi++;
      }
      if(strncasecmp((char *)AVI->idx[i],AVI->audio_tag,4) == 0)
      {
         AVI->audio_index[nai].pos = str2ulong(AVI->idx[i]+ 8)+ioff;
         AVI->audio_index[nai].len = str2ulong(AVI->idx[i]+12);
         AVI->audio_index[nai].tot = tot;
         tot += AVI->audio_index[nai].len;
         nai++;
      }
   }

   AVI->audio_bytes = tot;

   /* Reposition the file */
#ifdef FILE_OP
      fseek(AVI->fpFile,AVI->movi_start,SEEK_SET);

#else
   lseek(AVI->fdes,AVI->movi_start,SEEK_SET);
#endif
   AVI->video_pos = 0;

   return AVI;
}

long AVI_video_frames(avi_t *AVI)
{
   return AVI->video_frames;
}
int  AVI_video_width(avi_t *AVI)
{
   return AVI->width;
}
int  AVI_video_height(avi_t *AVI)
{
   return AVI->height;
}
double AVI_frame_rate(avi_t *AVI)
{
   return AVI->fps;
}
char* AVI_video_compressor(avi_t *AVI)
{
   return AVI->compressor;
}

int AVI_audio_channels(avi_t *AVI)
{
   return AVI->a_chans;
}
int AVI_audio_bits(avi_t *AVI)
{
   return AVI->a_bits;
}
int AVI_audio_format(avi_t *AVI)
{
   return AVI->a_fmt;
}
long AVI_audio_rate(avi_t *AVI)
{
   return AVI->a_rate;
}
long AVI_audio_bytes(avi_t *AVI)
{
   return AVI->audio_bytes;
}

long AVI_frame_size(avi_t *AVI, long frame)
{
   if(AVI->mode==AVI_MODE_WRITE) { AVI_errno = AVI_ERR_NOT_PERM; return -1; }
   if(!AVI->video_index)         { AVI_errno = AVI_ERR_NO_IDX;   return -1; }

   if(frame < 0 || frame >= AVI->video_frames) return 0;
   return(AVI->video_index[frame].len);
}

int AVI_seek_start(avi_t *AVI)
{
   if(AVI->mode==AVI_MODE_WRITE) { AVI_errno = AVI_ERR_NOT_PERM; return -1; }
#ifdef FILE_OP
   fseek(AVI->fpFile,AVI->movi_start,SEEK_SET);

#else
   lseek(AVI->fdes,AVI->movi_start,SEEK_SET);
#endif
   AVI->video_pos = 0;
   return 0;
}

int AVI_set_video_position(avi_t *AVI, long frame)
{
   if(AVI->mode==AVI_MODE_WRITE) { AVI_errno = AVI_ERR_NOT_PERM; return -1; }
   if(!AVI->video_index)         { AVI_errno = AVI_ERR_NO_IDX;   return -1; }

   if (frame < 0 ) frame = 0;
   AVI->video_pos = frame;
   return 0;
}
      

long AVI_read_frame(avi_t *AVI, char *vidbuf)
{
   long n;

   if(AVI->mode==AVI_MODE_WRITE) { AVI_errno = AVI_ERR_NOT_PERM; return -1; }
   if(!AVI->video_index)         { AVI_errno = AVI_ERR_NO_IDX;   return -1; }

   if(AVI->video_pos < 0 || AVI->video_pos >= AVI->video_frames) return 0;
   n = AVI->video_index[AVI->video_pos].len;

#ifdef  FILE_OP

     fseek(AVI->fpFile, AVI->video_index[AVI->video_pos].pos, SEEK_SET);
   if (fread(vidbuf,n,1,AVI->fpFile) != SUCCESS)
#else
   lseek(AVI->fdes, AVI->video_index[AVI->video_pos].pos, SEEK_SET);
   if (read(AVI->fdes,vidbuf,n) != n)
#endif   	
   {
      AVI_errno = AVI_ERR_READ;
      return -1;
   }

   AVI->video_pos++;

   return n;
}

int AVI_set_audio_position(avi_t *AVI, long byte)
{
   long n0, n1, n;

   if(AVI->mode==AVI_MODE_WRITE) { AVI_errno = AVI_ERR_NOT_PERM; return -1; }
   if(!AVI->audio_index)         { AVI_errno = AVI_ERR_NO_IDX;   return -1; }

   if(byte < 0) byte = 0;

   /* Binary search in the audio chunks */

   n0 = 0;
   n1 = AVI->audio_chunks;

   while(n0<n1-1)
   {
      n = (n0+n1)/2;
      if(AVI->audio_index[n].tot>byte)
         n1 = n;
      else
         n0 = n;
   }

   AVI->audio_posc = n0;
   AVI->audio_posb = byte - AVI->audio_index[n0].tot;

   return 0;
}

long AVI_read_audio(avi_t *AVI, char *audbuf, long bytes)
{
   long nr, pos, left, todo;

   if(AVI->mode==AVI_MODE_WRITE) { AVI_errno = AVI_ERR_NOT_PERM; return -1; }
   if(!AVI->audio_index)         { AVI_errno = AVI_ERR_NO_IDX;   return -1; }

   nr = 0; /* total number of bytes read */

   while(bytes>0)
   {
      left = AVI->audio_index[AVI->audio_posc].len - AVI->audio_posb;
      if(left==0)
      {
         if(AVI->audio_posc>=AVI->audio_chunks-1) return nr;
         AVI->audio_posc++;
         AVI->audio_posb = 0;
         continue;
      }
      if(bytes<left)
         todo = bytes;
      else
         todo = left;
      pos = AVI->audio_index[AVI->audio_posc].pos + AVI->audio_posb;


#ifdef FILE_OP 
         fseek(AVI->fpFile, pos, SEEK_SET);
      if (fread( audbuf+nr,todo,1,AVI->fpFile) != SUCCESS)

#else	  
      lseek(AVI->fdes, pos, SEEK_SET);
      if (read(AVI->fdes,audbuf+nr,todo) != todo)
#endif	  	
      {
         AVI_errno = AVI_ERR_READ;
         return -1;
      }
      bytes -= todo;
      nr    += todo;
      AVI->audio_posb += todo;
   }

   return nr;
}

/* AVI_read_data: Special routine for reading the next audio or video chunk
                  without having an index of the file. */

int AVI_read_data(avi_t *AVI, char *vidbuf, long max_vidbuf,
                              char *audbuf, long max_audbuf,
                              long *len)
{

/*
 * Return codes:
 *
 *    1 = video data read
 *    2 = audio data read
 *    0 = reached EOF
 *   -1 = video buffer too small
 *   -2 = audio buffer too small
 */

   int n;
   char data[8];

   if(AVI->mode==AVI_MODE_WRITE) return 0;

   while(1)
   {
      /* Read tag and length */

#ifdef FILE_OP
          if( fread(data,8,1,AVI->fpFile) != SUCCESS ) return 0;

#else
      if( read(AVI->fdes,data,8) != 8 ) return 0;
#endif
      /* if we got a list tag, ignore it */

      if(strncasecmp(data,"LIST",4) == 0)
      {
#ifdef FILE_OP
       fseek(AVI->fpFile,4,SEEK_CUR);

#else	  
         lseek(AVI->fdes,4,SEEK_CUR);
#endif		 
         continue;
      }

      n = PAD_EVEN(str2ulong((unsigned char*)data+4));

      if(strncasecmp(data,AVI->video_tag,3) == 0)
      {
         *len = n;
         AVI->video_pos++;
         if(n>max_vidbuf)
         {
#ifdef FILE_OP
        fseek(AVI->fpFile,n,SEEK_CUR);

#else		 
            lseek(AVI->fdes,n,SEEK_CUR);
#endif			
            return -1;
         }
#ifdef FILE_OP
               if(fread(vidbuf,n,1,AVI->fpFile) != SUCCESS ) return 0;

#else		 
         if(read(AVI->fdes,vidbuf,n) != n ) return 0;
#endif		 
         return 1;
      }
      else if(strncasecmp(data,AVI->audio_tag,4) == 0)
      {
         *len = n;
         if(n>max_audbuf)
         {
#ifdef FILE_OP
            fseek(AVI->fpFile,n,SEEK_CUR);

#else		 
            lseek(AVI->fdes,n,SEEK_CUR);
#endif			
            return -2;
         }
#ifdef  FILE_OP
         if(fread( audbuf,n,1,AVI->fpFile) != SUCCESS ) return 0;

#else		 
         if(read(AVI->fdes,audbuf,n) != n ) return 0;
#endif		 
         return 2;
         break;
      }
      else
#ifdef FILE_OP
               if(fseek(AVI->fpFile,n,SEEK_CUR)<0)  return 0;

#else
         if(lseek(AVI->fdes,n,SEEK_CUR)<0)  return 0;
#endif		 
   }
}

/* AVI_print_error: Print most recent error (similar to perror) */

char *(avi_errors[]) =
{
  /*  0 */ "avilib - No Error",
  /*  1 */ "avilib - AVI file size limit reached",
  /*  2 */ "avilib - Error opening AVI file",
  /*  3 */ "avilib - Error reading from AVI file",
  /*  4 */ "avilib - Error writing to AVI file",
  /*  5 */ "avilib - Error writing index (file may still be useable)",
  /*  6 */ "avilib - Error closing AVI file",
  /*  7 */ "avilib - Operation (read/write) not permitted",
  /*  8 */ "avilib - Out of memory (malloc failed)",
  /*  9 */ "avilib - Not an AVI file",
  /* 10 */ "avilib - AVI file has no header list (corrupted?)",
  /* 11 */ "avilib - AVI file has no MOVI list (corrupted?)",
  /* 12 */ "avilib - AVI file has no video data",
  /* 13 */ "avilib - operation needs an index",
  /* 14 */ "avilib - Unkown Error"
};
static int num_avi_errors = sizeof(avi_errors)/sizeof(char*);

static char error_string[4096];

void AVI_print_error(char *str)
{
   int aerrno;

   aerrno = (AVI_errno>=0 && AVI_errno<num_avi_errors) ? AVI_errno : num_avi_errors-1;

   fprintf(stderr,"%s: %s\n",str,avi_errors[aerrno]);

   /* for the following errors, perror should report a more detailed reason: */

   if(AVI_errno == AVI_ERR_OPEN ||
      AVI_errno == AVI_ERR_READ ||
      AVI_errno == AVI_ERR_WRITE ||
      AVI_errno == AVI_ERR_WRITE_INDEX ||
      AVI_errno == AVI_ERR_CLOSE )
   {
      perror("REASON");
   }
}

char *AVI_strerror()
{
   int aerrno;

   aerrno = (AVI_errno>=0 && AVI_errno<num_avi_errors) ? AVI_errno : num_avi_errors-1;

   if(AVI_errno == AVI_ERR_OPEN ||
      AVI_errno == AVI_ERR_READ ||
      AVI_errno == AVI_ERR_WRITE ||
      AVI_errno == AVI_ERR_WRITE_INDEX ||
      AVI_errno == AVI_ERR_CLOSE )
   {
      sprintf(error_string,"%s - %s",avi_errors[aerrno],strerror(errno));
      return error_string;
   }
   else
   {
      return avi_errors[aerrno];
   }
}

#endif


#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */
