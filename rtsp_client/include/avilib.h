#ifndef __AVILIB_H__
#define __AVILIB_H__

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif


#define 	GS_FAIL					(-1)
#define 	GS_SUCCESS				(0)

typedef unsigned long			uLong;
typedef unsigned int 			u32;
typedef unsigned short 			u16;
typedef unsigned char 			u8;


#ifndef _M_IX86
typedef unsigned long long 		u64;
typedef long long				s64;
#else
typedef __init64				u64;
typedef __init64				s64;
#endif 

#define AVI_READ

#define	RECORD_DEFORM_SUCCESS			0
#define	RECORD_DEFORM_FAIL				(-1)
#define	RECORD_UNABLE_DEFORM			0x2

#define AVI_MODE_WRITE  0
#define AVI_MODE_READ   1

/* The error codes delivered by avi_open_input_file */

#define AVI_ERR_SIZELIM      1     /* The write of the data would exceed
                                      the maximum size of the AVI file.
                                      This is more a warning than an error
                                      since the file may be closed safely */

#define AVI_ERR_OPEN         2     /* Error opening the AVI file - wrong path
                                      name or file nor readable/writable */

#define AVI_ERR_READ         3     /* Error reading from AVI File */

#define AVI_ERR_WRITE        4     /* Error writing to AVI File,
                                      disk full ??? */
#define AVI_ERR_WRITE_INDEX  5     /* Could not write index to AVI file
										  during close, file may still be
										  usable */
	
#define AVI_ERR_CLOSE        6     /* Could not write header to AVI file
										  or not truncate the file during close,
										  file is most probably corrupted */
	
#define AVI_ERR_NOT_PERM     7     /* Operation not permitted:
										  trying to read from a file open
										  for writing or vice versa */

#define AVI_ERR_NO_MEM       8     /* malloc failed */

#define AVI_ERR_NO_AVI       9     /* Not an AVI file */

#define AVI_ERR_NO_HDRL     10     /* AVI file has no has no header list,
                                      corrupted ??? */
#define AVI_ERR_NO_MOVI     11     /* AVI file has no has no MOVI list,
										  corrupted ??? */
	
#define AVI_ERR_NO_VIDS     12     /* AVI file contains no video data */
	
#define AVI_ERR_NO_IDX      13     /* The file has been opened with
										  getIndex==0, but an operation has been
										  performed that needs an index */
/* Possible Audio formats */
	
#define WAVE_FORMAT_UNKNOWN             (0x0000)
#define WAVE_FORMAT_PCM                 (0x0001)
#define WAVE_FORMAT_ADPCM               (0x0002)
#define WAVE_FORMAT_IBM_CVSD            (0x0005)
#define WAVE_FORMAT_ALAW                (0x0006)
#define WAVE_FORMAT_MULAW               (0x0007)
#define WAVE_FORMAT_OKI_ADPCM           (0x0010)
#define WAVE_FORMAT_DVI_ADPCM           (0x0011)
#define WAVE_FORMAT_DIGISTD             (0x0015)
#define WAVE_FORMAT_DIGIFIX             (0x0016)
#define WAVE_FORMAT_YAMAHA_ADPCM        (0x0020)
#define WAVE_FORMAT_DSP_TRUESPEECH      (0x0022)
#define WAVE_FORMAT_GSM610              (0x0031)
#define IBM_FORMAT_MULAW                (0x0101)
#define IBM_FORMAT_ALAW                 (0x0102)
#define IBM_FORMAT_ADPCM                (0x0103)

#define AVI_MAX_TRACKS 8

typedef struct
{
   off_t pos;
   off_t len;
   off_t tot;
} audio_index_entry;

typedef struct _avisuperindex_entry {
    u64 qwOffset;           // absolute file offset
    u32 dwSize;                  // size of index chunk at this offset
    u32 dwDuration;              // time span in stream ticks
} avisuperindex_entry;

typedef struct _avistdindex_entry {
    u32 dwOffset;                // qwBaseOffset + this is absolute file offset
    u32 dwSize;                  // bit 31 is set if this is NOT a keyframe
} avistdindex_entry;

typedef struct _avistdindex_chunk {
    char           fcc[4];                 // ix##
    u32  dwSize;                 // size of this chunk
    u16 wLongsPerEntry;         // must be sizeof(aIndex[0])/sizeof(DWORD)
    u8  bIndexSubType;          // must be 0
    u8  bIndexType;             // must be AVI_INDEX_OF_CHUNKS
    u32  nEntriesInUse;          //
    char           dwChunkId[4];           // '##dc' or '##db' or '##wb' etc..
    u64 qwBaseOffset;       // all dwOffsets in aIndex array are relative to this
    u32  dwReserved3;            // must be 0
    avistdindex_entry *aIndex;
} avistdindex_chunk;

typedef struct _avisuperindex_chunk {
    char           fcc[4];
    u32  dwSize;                 // size of this chunk
    u16 wLongsPerEntry;         // size of each entry in aIndex array (must be 8 for us)
    u8  bIndexSubType;          // future use. must be 0
    u8  bIndexType;             // one of AVI_INDEX_* codes
    u32  nEntriesInUse;          // index of first unused member in aIndex array
    char           dwChunkId[4];           // fcc of what is indexed
    u32  dwReserved[3];          // meaning differs for each index type/subtype.
                                           // 0 if unused
    avisuperindex_entry *aIndex;           // where are the ix## chunks
    avistdindex_chunk **stdindex;          // the ix## chunks itself (array)
} avisuperindex_chunk;

typedef struct track_s
{

    long   a_fmt;             /* Audio format, see #defines below */
    long   a_chans;           /* Audio channels, 0 for no audio */
    long   a_rate;            /* Rate in Hz */
    long   a_bits;            /* bits per audio sample */
    long   mp3rate;           /* mp3 bitrate kbs*/
    long   a_vbr;             /* 0 == no Variable BitRate */
    long   padrate;	      /* byte rate used for zero padding */

    long   audio_strn;        /* Audio stream number */
    off_t  audio_bytes;       /* Total number of bytes of audio data */
    long   audio_chunks;      /* Chunks of audio data in the file */

    char   audio_tag[4];      /* Tag of audio data */
    long   audio_posc;        /* Audio position: chunk */
    long   audio_posb;        /* Audio position: byte within chunk */
 
    off_t  a_codech_off;       /* absolut offset of audio codec information */ 
    off_t  a_codecf_off;       /* absolut offset of audio codec information */ 

    audio_index_entry *audio_index;
    avisuperindex_chunk *audio_superindex;

} track_t;

typedef struct
{
  off_t key;
  off_t pos;
  off_t len;
} video_index_entry;

typedef struct
{
  u32  bi_size;
  u32  bi_width;
  u32  bi_height;
  u16  bi_planes;
  u16  bi_bit_count;
  u32  bi_compression;
  u32  bi_size_image;
  u32  bi_x_pels_per_meter;
  u32  bi_y_pels_per_meter;
  u32  bi_clr_used;
  u32  bi_clr_important;
} alBITMAPINFOHEADER;

typedef struct __attribute__((__packed__))
{
  u16  w_format_tag;
  u16  n_channels;
  u32  n_samples_per_sec;
  u32  n_avg_bytes_per_sec;
  u16  n_block_align;
  u16  w_bits_per_sample;
  u16  cb_size;
} alWAVEFORMATEX;


typedef struct
{
   unsigned char	*buf;	 /* 写入内存空间 */
   unsigned int		buf_len; /* 最大写入地址 */
   long				fdes;	 /* 写入文件描述符 */
   long				bt;		 /* 开始时间 */
   long				et;		 /* 结束时间 */

   long   width;             /* Width  of a video frame */
   long   height;            /* Height of a video frame */
   long	  fps;               /* Frames per second */
   char   compressor[8];     /* Type of compressor, 4 bytes + padding for 0 byte */
   char   compressor2[8];     /* Type of compressor, 4 bytes + padding for 0 byte */
   long   video_strn;        /* Video stream number */
   long   video_frames;      /* Number of video frames */
   char   video_tag[4];      /* Tag of video data */
   long   video_pos;         /* Number of next frame to be read
                                (if index present) */

   /*add by soctt.liao 2007-7-13*/
   long  dwMicroSecPerFrame;
   long  dwSuggestedBufferSize;

   long   a_fmt;             /* Audio format, see #defines below */
   long   a_chans;           /* Audio channels, 0 for no audio */
   long   a_rate;            /* Rate in Hz */
   long   a_bits;            /* bits per audio sample */
   long   audio_strn;        /* Audio stream number */
   long   audio_bytes;       /* Total number of bytes of audio data */
   long   audio_chunks;      /* Chunks of audio data in the file */
   char   audio_tag[4];      /* Tag of audio data */
   long   audio_posc;        /* Audio position: chunk */
   long   audio_posb;        /* Audio position: byte within chunk */

   long   pos;               /* position in file */
   long   n_idx;             /* number of index entries actually filled */
   long   max_idx;           /* number of index entries actually allocated */

   unsigned char (*idx)[16]; /* index entries (AVI idx1 tag) */
   //unsigned char idx[MAX_INDEX][16];
   long   last_pos;          /* Position of last frame written */
   long   last_len;          /* Length of last frame written */
   long    must_use_index;    /* Flag if frames are duplicated */
   long   movi_start;
   void		*idxPtr;
   long   mode;              /* 0 for reading, 1 for writing */
   u32 max_len;    /* maximum video chunk present */
   track_t track[AVI_MAX_TRACKS];  // up to AVI_MAX_TRACKS audio tracks supported
   off_t  v_codech_off;      /* absolut offset of video codec (strh) info */
   off_t  v_codecf_off;      /* absolut offset of video codec (strf) info */
   video_index_entry *video_index;
   #ifdef AVI_READ
   audio_index_entry * audio_index;
   #endif
   avisuperindex_chunk *video_superindex;  /* index of indices */
   int is_opendml;           /* set to 1 if this is an odml file with multiple index chunks */
   int total_frames;         /* total number of frames if dmlh is present */
   int anum;            // total number of audio tracks
   int aptr;            // current audio working track
   int comment_fd;      // Read avi header comments from this fd
   char *index_file;    // read the avi index from this file
   alBITMAPINFOHEADER *bitmap_info_header;
   alWAVEFORMATEX *wave_format_ex[AVI_MAX_TRACKS];
   void*		extradata;
   unsigned long	extradata_size;
} avi_t;

enum { PAL_CIF,PAL_FIELD,PAL_D1,NTSC_CIF,NTSC_FIELD,NTSC_D1,QCIF,VGA,QVGA };

int AVI_Init(avi_t *AVI, int file_size, int idx_size);
int AVI_Init_fd(avi_t *AVI, int width, int height, int fps, const char *compressor, int duration, const char *file_name);
void AVI_set_video(avi_t *AVI, int width, int height, int fps, const char *compressor);
void AVI_set_audio(avi_t *AVI, int channels, int rate, int bits, int format);
int AVI_write_frame(avi_t *AVI, unsigned char *data, int bytes, unsigned int rt);
int AVI_write_audio(avi_t *AVI, unsigned char *data, int bytes, unsigned int rt);
int AVI_output_file(avi_t *AVI);
int AVI_close(avi_t *AVI);
int AVI_close_fd(avi_t *AVI);
int AVI_close_fd_1(avi_t *AVI);
int AVI_deformity_file(char  *file_name, int duration,unsigned int *tm_len);
int AVI_Init_fd_1(avi_t *AVI, int width, int height, int fps, const char *compressor, int duration, const char *file_name);

#ifdef AVI_READ
int AVI_close_1(avi_t *AVI);
avi_t *AVI_open_input_file(char *filename, int getIndex);
long AVI_video_frames(avi_t *AVI);
int  AVI_video_width(avi_t *AVI);
int  AVI_video_height(avi_t *AVI);
double AVI_frame_rate(avi_t *AVI);
char* AVI_video_compressor(avi_t *AVI);

int  AVI_audio_channels(avi_t *AVI);
int  AVI_audio_bits(avi_t *AVI);
int  AVI_audio_format(avi_t *AVI);
long AVI_audio_rate(avi_t *AVI);
long AVI_audio_bytes(avi_t *AVI);

long AVI_frame_size(avi_t *AVI, long frame);
int  AVI_seek_start(avi_t *AVI);
int  AVI_set_video_position(avi_t *AVI, long frame);
long AVI_read_frame(avi_t *AVI, char *vidbuf);
int  AVI_set_audio_position(avi_t *AVI, long byte);
long AVI_read_audio(avi_t *AVI, char *audbuf, long bytes);

int  AVI_read_data(avi_t *AVI, char *vidbuf, long max_vidbuf,
                               char *audbuf, long max_audbuf,
                               long *len);

void AVI_print_error(char *str);
char *AVI_strerror();
char *AVI_syserror();

#endif

#ifdef __cplusplus
#if __cplusplus
extern "C"{
#endif
#endif /* __cplusplus */

#endif
