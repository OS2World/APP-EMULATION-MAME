#define INCL_DOSMISC
#define INCL_PM
#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS
#define INCL_BASE
#define INCL_DOSFILEMGR
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_WININPUT
#undef ULONG
#include <os2.h>

extern int INITIAL_FREQ;          // 7980  44100
extern char SOUND_QUALITY;  // 8 or 16 bit

extern char **searchpath;
extern int numsearchdirs;

#include <stdio.h>
#include <malloc.h>
#include <dive.h>
#include <string.h>
#include <io.h>
#include <process.h>
#include <tmr0_ioc.h>
#include <math.h>

#include "mameresource.h"
#include "driver.h"
#include "osdepend.h"
#include "mame.h"
#include "os2_front.h"
#include "joyos2.h"
#include "gpmixer.h"

#ifndef __OS2__
#define __OS2__ 1
#endif

#define FOURCC_RGB3 0x33424752ul
#define FOURCC_R565 0x35363552ul
#define FOURCC_R555 0x35353552ul
#define FOURCC_LUT8 0x3854554cul
#define WM_VRNENABLE 0x7f
#define WM_VRNDISABLE 0x7e

#define GET_KEY WM_USER
#define WAIT_KEY WM_USER+1
#define EmuDone WM_USER+2
#define WM_RECREATE WM_USER+3
#define EmuStart WM_USER+4

unsigned char No_FM=1;
char *dirty_new;

unsigned char current_palette[1024];
unsigned char reported_palette[1024];

extern struct RunningMachine *Machine;

extern void set_ui_visarea (int xmin, int ymin, int xmax, int ymax);
extern const char *input_port_name( const struct InputPort *in );
extern int input_port_key( const struct InputPort *in );

extern int nocheat;

extern FILE *fp;

// DIVE / PM globals from frontend.cpp
extern HEV DiveBufferMutex;
extern HDIVE diveinst;
extern ULONG bufnum;
extern ULONG blitwidth, blitheight;
extern ULONG blitdepth;
extern HWND clientwin, framewin;
extern USHORT last_char;
extern char initialbmp;
extern char quitnotify;
extern char mousecapture;
extern USHORT centerx, centery;
extern HINI mameini;
extern char devicesenabled;
	// bit 0 - Keyboard
	// bit 1 - mouse
	// bit 2 - joystick
extern char mouseflip;
	// bit 0 - Flip X
	// bit 1 - Flip Y
extern char inpgamespec[];
extern char inpdef[];
extern char firekeys[];
extern char firerate[];
extern unsigned short GAME_TO_TRY;
extern char rapidtrigger;
extern char totalrapidkeys;
extern int *rapidkeys;
extern char rapidrate;
extern char custom_size;
extern char scanlines;
extern char fpstoggle;
extern HFILE timer;
extern char showfps;
extern char useTIMER0;
extern char autoskip, autoslow, maxskip;
extern char allowledflash;
extern char GPMIXERdevice;
extern unsigned long AUDIO_BUF_SIZE;
extern char thread_state;
extern char soundon;
extern char soundreallyon;

int autoframeskip = 0; 
// don't enable MAME's internal auto frameskip because it sucks

static char bkcolor = 0;

static char initrapidplz = 1;

/* audio related stuff */
#define NUMVOICES 16
static signed int attenuation = 0;
static void *sync_handle;
static char vfreqstart[NUMVOICES]; // Debugging only

int MasterVolume = 256;

static float fGamma = 1.0;
static int brightness = 100;
static char palchange = 0;
// Internal use only

// For frame rate regulation
static unsigned long starttime;
static unsigned long curframe;
static unsigned long *frametimes;
static unsigned long dropped=0;
static unsigned long totalframes=0;
static char curdropped=0;

char nonframeupdate = 0;

extern char keystatus[128];

int MAME_volumes[NUMVOICES];

char percentbarinit = 0;

enum {
  KEY_INVALID=0, KEY_ESC=1, KEY_1=2, KEY_2=3, KEY_3=4, KEY_4=5, KEY_5=6, KEY_6=7, KEY_7=8,
  KEY_8=9, KEY_9=10, KEY_0=11, KEY_MINUS=12, KEY_EQUAL=13, KEY_BACKSPACE=14, KEY_TAB=15,
  KEY_Q=16, KEY_W=17, KEY_E=18, KEY_R=19, KEY_T=20, KEY_Y=21, KEY_U=22, KEY_I=23, KEY_O=24,
  KEY_P=25, KEY_OBRACE=26, KEY_CBRACE=27, KEY_ENTER=28, KEY_LCONTROL=29, KEY_A=30, KEY_S=31,
  KEY_D=32, KEY_F=33, KEY_G=34, KEY_H=35, KEY_J=36, KEY_K=37, KEY_L=38, KEY_SCOLON=39,
  KEY_QUOTE=40, KEY_TILDE=41, KEY_LSHIFT=42, KEY_BSLASH=43, KEY_Z=44, KEY_X=45, KEY_C=46,
  KEY_V=47, KEY_B=48, KEY_N=49, KEY_M=50, KEY_COMMA=51, KEY_PERIOD=52, KEY_SLASH=53,
  KEY_RSHIFT=54, KEY_ASTERISK=55, KEY_ALT=56, KEY_SPACE=57, KEY_CLOCK=58, KEY_F1=59, 
  KEY_F2=60, KEY_F3=61, KEY_F4=62, KEY_F5=63, KEY_F6=64, KEY_F7=65, KEY_F8=66, KEY_F9=67,
  KEY_F10=68, KEY_NLOCK=69, KEY_SLOCK=70, KEY_PHOME=71, KEY_UP=72, KEY_PPGUP=73,
  KEY_PMINUS=74, KEY_LEFT=75, KEY_PCENTER=76, KEY_RIGHT=77, KEY_PPLUS=78, KEY_PEND=79,
  KEY_DOWN=80, KEY_PPGDN=81, KEY_PINSERT=82, KEY_PDELETE=83, KEY_84=84, KEY_85=85,
  KEY_86=86, KEY_F11=87, KEY_F12=88, KEY_89=89, KEY_PENTER=90, KEY_RCONTROL=91,
  KEY_PSLASH=92, KEY_PSCREEN=93, KEY_RALT=94, KEY_PAUSE=95, KEY_HOME=96, KEY_PUP=97,
  KEY_PGUP=98, KEY_PLEFT=99, KEY_PRIGHT=100, KEY_END=101, KEY_PDOWN=102, KEY_PGDN=103,
  KEY_INSERT=104, KEY_DELETE=105, KEY_MAX=106
};

static struct KeyboardInfo keylist[] = {
  { "ESC",	KEY_ESC,	KEYCODE_ESC       }, { "1",		KEY_1,		KEYCODE_1 },
  { "2",	KEY_2,		KEYCODE_2         }, { "3",		KEY_3,		KEYCODE_3 },
  { "4",	KEY_4,		KEYCODE_4         }, { "5",		KEY_5,		KEYCODE_5 },
  { "6",	KEY_6,		KEYCODE_6         }, { "7",		KEY_7,		KEYCODE_7 },
  { "8",	KEY_8,		KEYCODE_8         }, { "9",		KEY_9,		KEYCODE_9 },
  { "0",	KEY_0,		KEYCODE_0         }, { "-",		KEY_MINUS,	KEYCODE_MINUS },
  { "=",	KEY_EQUAL,	KEYCODE_EQUALS    }, { "BACKSPC",	KEY_BACKSPACE,	KEYCODE_BACKSPACE },
  { "TAB",	KEY_TAB,	KEYCODE_TAB       }, { "Q",		KEY_Q,		KEYCODE_Q },
  { "W",	KEY_W,		KEYCODE_W         }, { "E",		KEY_E,		KEYCODE_E },
  { "R",	KEY_R,		KEYCODE_R         }, { "T",		KEY_T,		KEYCODE_T },
  { "Y",	KEY_Y,		KEYCODE_Y         }, { "U",		KEY_U,		KEYCODE_U },
  { "I",	KEY_I,		KEYCODE_I         }, { "O",		KEY_O,		KEYCODE_O },
  { "[",	KEY_OBRACE,	KEYCODE_OPENBRACE }, { "]",		KEY_CBRACE,	KEYCODE_CLOSEBRACE },
  { "ENTER",	KEY_ENTER,	KEYCODE_ENTER     }, { "L CTRL",	KEY_LCONTROL,	KEYCODE_LCONTROL },
  { "A",	KEY_A,		KEYCODE_A         }, { "S",		KEY_S,		KEYCODE_S },
  { "D",	KEY_D,		KEYCODE_D         }, { "F",		KEY_F,		KEYCODE_F },
  { "G",	KEY_G,		KEYCODE_G         }, { "H",		KEY_H,		KEYCODE_H },
  { "J",	KEY_J,		KEYCODE_J         }, { "K",		KEY_K,		KEYCODE_K },
  { "L",	KEY_L,		KEYCODE_L         }, { ";",		KEY_SCOLON,	KEYCODE_COLON },
  { "'",	KEY_QUOTE,	KEYCODE_QUOTE     }, { "~",		KEY_TILDE,	KEYCODE_TILDE },
  { "L SHIFT",	KEY_LSHIFT,	KEYCODE_LSHIFT    }, { "\\",		KEY_BSLASH,	KEYCODE_BACKSLASH },
  { "Z",	KEY_Z,		KEYCODE_Z         }, { "X",		KEY_X,		KEYCODE_X },
  { "C",	KEY_C,		KEYCODE_C         }, { "V",		KEY_V,		KEYCODE_V },
  { "B",	KEY_B,		KEYCODE_B         }, { "N",		KEY_N,		KEYCODE_N },
  { "M",	KEY_M,		KEYCODE_M         }, { ",",		KEY_COMMA,	KEYCODE_COMMA },
  { ".",	KEY_PERIOD,	KEYCODE_STOP      }, { "/",		KEY_SLASH,	KEYCODE_SLASH },
  { "R SHIFT",	KEY_RSHIFT,	KEYCODE_RSHIFT    }, { "*",		KEY_ASTERISK,	KEYCODE_ASTERISK },
  { "L ALT",	KEY_ALT,	KEYCODE_LALT      }, { "SPACE",		KEY_SPACE,	KEYCODE_SPACE },
  { "CAPS LOCK",KEY_CLOCK,	KEYCODE_CAPSLOCK  }, { "F1",		KEY_F1,		KEYCODE_F1 },
  { "F2",	KEY_F2,		KEYCODE_F2        }, { "F3",		KEY_F3,		KEYCODE_F3 },
  { "F4",	KEY_F4,		KEYCODE_F4        }, { "F5",		KEY_F5,		KEYCODE_F5 },
  { "F6",	KEY_F6,		KEYCODE_F6        }, { "F7",		KEY_F7,		KEYCODE_F7 },
  { "F8",	KEY_F8,		KEYCODE_F8        }, { "F9",		KEY_F9,		KEYCODE_F9 },
  { "F10",	KEY_F10,	KEYCODE_F10       }, { "NUM LOCK",	KEY_NLOCK,	KEYCODE_NUMLOCK },
  { "SCRL LOCK",KEY_SLOCK,	KEYCODE_SCRLOCK   }, { "PAD HOME",	KEY_PHOME,	KEYCODE_7_PAD },
  { "UP",	KEY_UP,		KEYCODE_UP        }, { "PAD PGUP",	KEY_PPGUP,	KEYCODE_9_PAD },
  { "PAD -",	KEY_PMINUS,	KEYCODE_MINUS_PAD }, { "LEFT",		KEY_LEFT,	KEYCODE_LEFT },
  { "PAD 5",	KEY_PCENTER,	KEYCODE_5_PAD     }, { "RIGHT",		KEY_RIGHT,	KEYCODE_RIGHT },
  { "PAD +",	KEY_PPLUS,	KEYCODE_PLUS_PAD  }, { "PAD END",	KEY_PEND,	KEYCODE_1_PAD },
  { "DOWN",	KEY_DOWN,	KEYCODE_DOWN      }, { "PAD PGDN",	KEY_PPGDN,	KEYCODE_3_PAD },
  { "PAD INS",	KEY_PINSERT,	KEYCODE_0_PAD     }, { "PAD DEL",	KEY_PDELETE,	KEYCODE_DEL_PAD },
  { "F11",	KEY_F11,	KEYCODE_F11       }, { "F12",		KEY_F12,	KEYCODE_F12 },
  { "PAD ENTER",KEY_PENTER,	KEYCODE_ENTER_PAD }, { "R CTRL",	KEY_RCONTROL,	KEYCODE_RCONTROL },
  { "PAD /",	KEY_PSLASH,	KEYCODE_SLASH_PAD }, { "PRNT SCRN",	KEY_PSCREEN,	KEYCODE_PRTSCR },
  { "R ALT",	KEY_RALT,	KEYCODE_RALT      }, { "PAUSE",		KEY_PAUSE,	KEYCODE_PAUSE },
  { "HOME",	KEY_HOME,	KEYCODE_HOME      }, { "PGUP",		KEY_PGUP,	KEYCODE_PGUP },
  { "PAD LEFT",	KEY_PLEFT,	KEYCODE_4_PAD     }, { "PAD RIGHT",	KEY_PRIGHT,	KEYCODE_6_PAD },
  { "END",	KEY_END,	KEYCODE_END       }, { "PAD DOWN",	KEY_PDOWN,	KEYCODE_2_PAD },
  { "PGDN",	KEY_PGDN,	KEYCODE_PGDN      }, { "INSERT",	KEY_INSERT,	KEYCODE_INSERT },
  { "DELETE",	KEY_DELETE,	KEYCODE_DEL       }, { "?UNKNOWN?",	KEY_INVALID,	KEYCODE_NONE },
  { "P",	KEY_P,		KEYCODE_P         }, { "PAD UP",	KEY_PUP,	KEYCODE_8_PAD },
  { 0, 0, 0 }
};

static struct JoystickInfo joylist[] =
{
	{ "J1 LEFT",	OSD_JOY_LEFT,		JOYCODE_1_LEFT },
	{ "J1 RIGHT",	OSD_JOY_RIGHT,		JOYCODE_1_RIGHT },
	{ "J1 UP",	OSD_JOY_UP,		JOYCODE_1_UP },
	{ "J1 DOWN",	OSD_JOY_DOWN,		JOYCODE_1_DOWN },
	{ "J1 BUT1",	OSD_JOY_FIRE1,		JOYCODE_1_BUTTON1 },
	{ "J1 BUT2",	OSD_JOY_FIRE2,		JOYCODE_1_BUTTON2 },
	{ "J1 BUT3",	OSD_JOY_FIRE3,		JOYCODE_1_BUTTON3 },
	{ "J1 BUT4",	OSD_JOY_FIRE4,		JOYCODE_1_BUTTON4 },
	{ "J1 BUT5",	OSD_JOY_FIRE5,		JOYCODE_1_BUTTON5 },
	{ "J1 BUT6",	OSD_JOY_FIRE6,		JOYCODE_1_BUTTON6 },
	{ "J2 LEFT",	OSD_JOY2_LEFT,		JOYCODE_2_LEFT },
	{ "J2 RIGHT",	OSD_JOY2_RIGHT,		JOYCODE_2_RIGHT },
	{ "J2 UP",	OSD_JOY2_UP,		JOYCODE_2_UP },
	{ "J2 DOWN",	OSD_JOY2_DOWN,		JOYCODE_2_DOWN },
	{ "J2 BUT1",	OSD_JOY2_FIRE1,		JOYCODE_2_BUTTON1 },
	{ "J2 BUT2",	OSD_JOY2_FIRE2,		JOYCODE_2_BUTTON2 },
	{ "J2 BUT3",	OSD_JOY2_FIRE3,		JOYCODE_2_BUTTON3 },
	{ "J2 BUT4",	OSD_JOY2_FIRE4,		JOYCODE_2_BUTTON4 },
	{ "J2 BUT5",	OSD_JOY2_FIRE5,		JOYCODE_2_BUTTON5 },
	{ "J2 BUT6",	OSD_JOY2_FIRE6,		JOYCODE_2_BUTTON6 },
	{ "J3 LEFT",	OSD_JOY3_LEFT,		JOYCODE_3_LEFT },
	{ "J3 RIGHT",	OSD_JOY3_RIGHT,		JOYCODE_3_RIGHT },
	{ "J3 UP",	OSD_JOY3_UP,		JOYCODE_3_UP },
	{ "J3 DOWN",	OSD_JOY3_DOWN,		JOYCODE_3_DOWN },
	{ "J3 BUT1",	OSD_JOY3_FIRE1,		JOYCODE_3_BUTTON1 },
	{ "J3 BUT2",	OSD_JOY3_FIRE2,		JOYCODE_3_BUTTON2 },
	{ "J3 BUT3",	OSD_JOY3_FIRE3,		JOYCODE_3_BUTTON3 },
	{ "J3 BUT4",	OSD_JOY3_FIRE4,		JOYCODE_3_BUTTON4 },
	{ "J3 BUT5",	OSD_JOY3_FIRE5,		JOYCODE_3_BUTTON5 },
	{ "J3 BUT6",	OSD_JOY3_FIRE6,		JOYCODE_3_BUTTON6 },
	{ "J4 LEFT",	OSD_JOY4_LEFT,		JOYCODE_4_LEFT },
	{ "J4 RIGHT",	OSD_JOY4_RIGHT,		JOYCODE_4_RIGHT },
	{ "J4 UP",	OSD_JOY4_UP,		JOYCODE_4_UP },
	{ "J4 DOWN",	OSD_JOY4_DOWN,		JOYCODE_4_DOWN },
	{ "J4 BUT1",	OSD_JOY4_FIRE1,		JOYCODE_4_BUTTON1 },
	{ "J4 BUT2",	OSD_JOY4_FIRE2,		JOYCODE_4_BUTTON2 },
	{ "J4 BUT3",	OSD_JOY4_FIRE3,		JOYCODE_4_BUTTON3 },
	{ "J4 BUT4",	OSD_JOY4_FIRE4,		JOYCODE_4_BUTTON4 },
	{ "J4 BUT5",	OSD_JOY4_FIRE5,		JOYCODE_4_BUTTON5 },
	{ "J4 BUT6",	OSD_JOY4_FIRE6,		JOYCODE_4_BUTTON6 },
	{ 0,0,0 }
};

#define osd_key_pressed osd_is_key_pressed

int osd_is_key_pressed( int key ) {
	static char ESConce = 0;
	int i;
	char c;

	if ( quitnotify ) {
		if ( (key == KEY_ESC) && ESConce == 0 ) { ESConce = 1; return 1; }
	}

	ESConce = 0;

	if ( (c=(keystatus[ key>>3 ] & (1<<(key%8)))) ) { // Key is down currently
		for ( i=0; i<totalrapidkeys; i++ ) {
     			if ( key == rapidkeys[i] ) {
				if ( !rapidtrigger ) {
					rapidtrigger = 1;
					if ( rapidrate ) {
						WinStartTimer( WinQueryAnchorBlock( clientwin ), clientwin, 0, 1000 / rapidrate );
					}
					return 0;
				} else return 1;
			}
		}
	}
	return c != 0;
}

int osd_wait_keypress( void ) {
	HEV key_released;
	if ( quitnotify ) return KEY_ESC; // ESC
	DosCreateEventSem( "\\SEM32\\CharReady Semaphore", &key_released, 0, 0 );
	WinPostMsg( clientwin, GET_KEY, 0, MPFROMLONG( key_released ));
	DosWaitEventSem( key_released, -1 );
	WinPostMsg( clientwin, GET_KEY, 0, 0 );
	DosCloseEventSem( key_released );
	DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &starttime, 4 );
	curframe = 0;
	return last_char;
}

struct MAMEFILE {
	void *fp;
	char ptrtype;
	unsigned long len, currpos;
};

extern int load_zipped_file (const char *zipfile, const char *filename, unsigned char **buf, int *length);

void *osd_fopen( const char *directory, const char *name, int type, int canwrite ) {
	char buffer[2048], buf2[256], *extrapath;
	struct MAMEFILE *mf;
	char found = 0;
	unsigned int i;

	fprintf( fp,"Opening %s / %s.  Type %d, Write ? %d\n", directory, name, type, canwrite );
	fflush(fp);

	for (i=0; i<numsearchdirs; i++) {
		switch (type) {
			case OSD_FILETYPE_CONFIG:
				sprintf( buf2, "cfg\\%s.cfg", directory );
				name = directory;
				extrapath="\0";
				break;
			case OSD_FILETYPE_HIGHSCORE:
				sprintf( buf2, "hi\\%s.hi", directory );
				name = directory;
				extrapath="\0";
				break;
			case OSD_FILETYPE_ARTWORK:
				sprintf( buf2, "artwork\\%s", name );
				extrapath="\\artwork";
				break;
			case OSD_FILETYPE_SAMPLE:
				sprintf( buf2, "samples\\%s", name );
				extrapath="\\samples";
				break;
			default:
				sprintf( buf2, "%s\\%s", directory, name );
				extrapath="\0";
		} 
		sprintf( buffer, "%s\\%s", searchpath[i], buf2 );
		fprintf( fp, "  ==> Looking in %s\n", buffer );
		fflush(fp);
		mf = (struct MAMEFILE *)malloc( sizeof( struct MAMEFILE ) );
		mf->fp = fopen( buffer, (canwrite ? "w+b" : "rb" ) );
		if ( !mf->fp ) {
			sprintf( buffer, "%s\\%s\\%s", searchpath[i], directory, name );
			fprintf( fp, "  ==> Looking in %s\n", buffer );
			fflush(fp);
			mf->fp = fopen( buffer, (canwrite ? "w+b" : "rb" ) );
		}
		if ( !mf->fp ) {
			sprintf( buffer, "%s%s\\%s.zip", searchpath[i], extrapath, directory );
			fprintf( fp, "  ==> Looking in %s\n", buffer );
			fflush(fp);
			if ( access( buffer, 0 ) == 0 ) {
				if ( !(load_zipped_file( buffer, name, (unsigned char**)&(mf->fp), (unsigned int *)&(mf->len))) ) {
					found = 1;
					mf->currpos = 0;
					mf->ptrtype = 2; // ZIP file- uncompressed data block
					mf->len = 0;
					checksum_zipped_file(buffer, name, &(mf->len), &i );
					fprintf(fp, "Loaded `virtual file' of length %ld.\n", mf->len );
					break;
				}
			}
		} else {
			mf->currpos = 0;
			mf->ptrtype = 1; // regular file
			mf->len = filelength( fileno((FILE *)(mf->fp)));
			found = 1;
			break;
		}
	}

	if ( !found ) {
		free( mf );
		return NULL;
	}

	fprintf( fp, "  ==> Found it.\n" );
	fflush(fp);

	return mf;
}

extern unsigned int crc32 (unsigned int crc, const unsigned char *buf, unsigned int len);

int osd_fsize( void * file ) {
	return ((struct MAMEFILE *)file)->len;
}

unsigned int osd_fcrc( void *file ) {
	unsigned int ret;
	char *buffer;
	switch ( ((struct MAMEFILE *)file)->ptrtype ) {
		case 1:
			buffer = (unsigned char *)malloc( osd_fsize( file ) ); 
			fseek( (FILE*)((struct MAMEFILE *)file)->fp, 0, SEEK_SET );
			fread( buffer, 1, osd_fsize( file ), (FILE*)(((struct MAMEFILE *)file)->fp) );
			fseek( (FILE*)((struct MAMEFILE *)file)->fp, 0, SEEK_SET );
		break;
		case 2:
			buffer = (char *)(((struct MAMEFILE *)file)->fp);
		break;
		default:
			fprintf( fp, "BAD FILE ACCESS!  Error during fcrc.\n" );
			return 0;
	}
	ret = crc32( 0, buffer, osd_fsize( file ) );
	if ( ((struct MAMEFILE *)file)->ptrtype == 1 ) free( buffer );
	return ret;
}

int osd_fchecksum (const char *game, const char *filename, unsigned int *length, unsigned int *sum) {
	struct MAMEFILE *_fp;
	_fp = (struct MAMEFILE *)osd_fopen( game, filename, OSD_FILETYPE_ROM, 0 );
	if ( !_fp ) { os2printf( "Could not find %s for a checksum.\n", filename ); return -1; }
	*sum = osd_fcrc( _fp );
	*length = osd_fsize( _fp );
	osd_fclose( _fp );
	return 0;
}

int osd_fseek( void *file, int offset, int whence ) {
	if ( file ) {
		if ( !(((struct MAMEFILE *)file)->fp) ) return -1;
		switch ( ((struct MAMEFILE*)file)->ptrtype ) {
			case 1:
				return fseek( (FILE*)(((struct MAMEFILE *)file)->fp), offset, whence);
			break;
			case 2:
				fprintf(fp, "Seeking in `virtual file' - %ld bytes from ", offset );
				switch (whence) {
					case SEEK_SET:
						if ( offset > ((struct MAMEFILE*)file)->len ) offset = ((struct MAMEFILE*)file)->len;
						((struct MAMEFILE*)file)->currpos = offset;
						fprintf(fp, "the beginning.  New position = %ld.\n", ((struct MAMEFILE*)file)->currpos );
					break;
					case SEEK_END:
						if ( offset > ((struct MAMEFILE*)file)->len ) offset = ((struct MAMEFILE*)file)->len;
						((struct MAMEFILE*)file)->currpos = ((struct MAMEFILE*)file)->len - offset;
						fprintf(fp, "the end.  New position = %ld.\n", ((struct MAMEFILE*)file)->currpos );
					break;
					case SEEK_CUR:
						if ( offset + ((struct MAMEFILE*)file)->currpos >= ((struct MAMEFILE*)file)->len ) offset = ((struct MAMEFILE*)file)->len - ((struct MAMEFILE*)file)->currpos;
						if ( offset + ((struct MAMEFILE*)file)->currpos < 0 ) offset = -(((struct MAMEFILE*)file)->currpos);
						fprintf(fp, "%ld.  ", ((struct MAMEFILE*)file)->currpos );
						((struct MAMEFILE*)file)->currpos += offset;
						fprintf(fp, "New position = %ld.\n", ((struct MAMEFILE*)file)->currpos );
					break;
					default:
						fprintf( fp, "BAD FILE ACCESS!  Error during fseek.  Bad <whence> clause.\n" );
						return 0;
				}
			break;
		default:
			fprintf( fp, "BAD FILE ACCESS!  Error during fseek.\n" );
			return 0;
		}
	}
	return 0; // Success
}

void osd_fclose( void *_fp ) {
	if ( _fp ) {
		switch ( ((struct MAMEFILE *)_fp)->ptrtype ) {
			case 1:
				if ( ((struct MAMEFILE *)_fp)->fp ) {
					fclose( (FILE*)(((struct MAMEFILE *)_fp)->fp) );
				}
				fprintf( fp, "  ==> Closed file.\n" );
				fflush(fp);
			break;
			case 2:
				if ( ((struct MAMEFILE *)_fp)->fp ) {
					free( ((struct MAMEFILE *)_fp)->fp );
				}
				fprintf( fp, "  ==> Closed `virtual' file.\n" );
				fflush(fp);
			break;
			default:
				fprintf( fp, "BAD FILE ACCESS!  Error during fclose.\n" );
				return;
		}             
		free( _fp );
	}
}

int osd_faccess( const char *file, int filetype ) {
	char buffer[2048], buf2[256], *extrapath;
	char found = 0;
	unsigned int i, j;

	if ( file == NULL ) {
		fprintf( fp,"Repeat find was attempted.  Ignored.\n" );
		fflush(fp);
		return 0;
	}

	fprintf( fp,"Checking access to %s.  Type %d.\n", file, filetype );
	fflush(fp);

	for (i=0; i<numsearchdirs; i++) {
		switch (filetype) {
			case OSD_FILETYPE_CONFIG:
				sprintf( buf2, "cfg\\%s.cfg", file );
				extrapath="\0";
				break;
			case OSD_FILETYPE_HIGHSCORE:
				sprintf( buf2, "hi\\%s.hi", file );
				extrapath="\0";
				break;
			case OSD_FILETYPE_ARTWORK:
				sprintf( buf2, "artwork\\%s", file );
				extrapath="\\artwork";
				break;
			case OSD_FILETYPE_SAMPLE:
				sprintf( buf2, "samples\\%s", file );
				extrapath="\\samples";
				break;
			default:
				sprintf( buf2, "%s", file );
				extrapath="\0";
		} 
		sprintf( buffer, "%s%s\\%s", searchpath[i], extrapath, buf2 );
		fprintf( fp, "  ==> Looking in %s\n", buffer );
		fflush(fp);
		if ( access( buffer, 0 ) == 0 ) {
			found = 1;
			break;
		}
		sprintf( buffer, "%s%s\\%s.zip", searchpath[i], extrapath, buf2 );
		fprintf( fp, "  ==> Looking in %s\n", buffer );
		fflush(fp);
		if ( access( buffer, 0 ) == 0 ) {
			found = 1;
			break;
		}
	}

	if ( !found ) {
		fprintf( fp, "  ==> Not found as such.\n" );
		fflush(fp);
		return 0;
	}

	fprintf( fp, "  ==> Found it.\n" );
	fflush(fp);

	return 1;
}

int osd_fread( void *file, void *buffer, int length ) {
	if ( file ) {
		if ( !(((struct MAMEFILE *)file)->fp) ) return 0;
		switch ( ((struct MAMEFILE *)file)->ptrtype ) {
			case 1:
				return fread(buffer,1,length,(FILE*)(((struct MAMEFILE *)file)->fp));
			case 2:
				if ( ((struct MAMEFILE *)file)->currpos + length > ((struct MAMEFILE *)file)->len ) {
					length = ((struct MAMEFILE *)file)->len - ((struct MAMEFILE *)file)->currpos;
				}
				fprintf(fp, "Reading out of ZIP file: %ld bytes from current position %ld.\n", length, ((struct MAMEFILE *)file)->currpos );
				fflush(fp);
				memcpy( buffer, ((struct MAMEFILE *)file)->fp + ((struct MAMEFILE *)file)->currpos, length );
				((struct MAMEFILE *)file)->currpos += length;
				return length;
			default:
				fprintf( fp, "BAD FILE ACCESS!  Error during fread.\n" );
				return 0;
		}
	}
	return 0;
}

int osd_fread_swap(void *file,void *buffer,int length)
{
	int i;
	unsigned char *buf;
	unsigned char temp;
	int res;


	res = osd_fread(file,buffer,length);

	buf = buffer;
	for (i = 0;i < length;i+=2)
	{
		temp = buf[i];
		buf[i] = buf[i+1];
		buf[i+1] = temp;
	}

	return res;
}

int osd_fread_scatter(void *file,void *buffer,int length,int increment)
{
	unsigned char *buf = buffer;
	struct MAMEFILE *f = (struct MAMEFILE *)file;
	unsigned char tempbuf[4096];
	int totread,r,i;

	switch (f->ptrtype)
	{
		case 1:
			totread = 0;
			while (length)
			{
				r = length;
				if (r > 4096) r = 4096;
				r = fread(tempbuf,1,r,f->fp);
				f->currpos += r;
				if (r == 0) return totread;	/* error */
				for (i = 0;i < r;i++)
				{
					*buf = tempbuf[i];
					buf += increment;
				}
				totread += r;
				length -= r;
			}
			return totread;
			break;
		case 2:
			/* reading from the RAM image of a file */
			if (f->fp)
			{
				if (length + f->currpos > f->len)
					length = f->len - f->currpos;
				for (i = 0;i < length;i++)
				{
					*buf = ((unsigned char *)(f->fp))[f->currpos + i];
					buf += increment;
				}
				f->currpos += length;
				return length;
			}
			break;
	}

	return 0;
}

// Allow writing to ZIP file buffers, but changes will not be kept
// or stored back in the actual ZIP file.
int osd_fwrite( void *file, const void *buffer, int length ) {
	if ( file ) {
		if ( !(((struct MAMEFILE *)file)->fp) ) return 0;
		switch ( ((struct MAMEFILE *)file)->ptrtype ) {
			case 1:
				return fwrite(buffer,1,length,(FILE*)(((struct MAMEFILE *)file)->fp));
			case 2:
/*				if ( ((struct MAMEFILE *)file)->currpos + length > ((struct MAMEFILE *)file)->len ) {
					length = ((struct MAMEFILE *)file)->len - ((struct MAMEFILE *)file)->currpos;
				}
				memcpy( ((struct MAMEFILE *)file)->fp, buffer, length );
				return length; */
				return 0;
			default:
				fprintf( fp, "BAD FILE ACCESS!  Error during fwrite.\n" );
				return 0;
		}
	}
	return 0;
}

int osd_fwrite_swap(void *file,const void *buffer,int length)
{
	int i;
	unsigned char *buf;
	unsigned char temp;
	int res;


	buf = (unsigned char *)buffer;
	for (i = 0;i < length;i+=2)
	{
		temp = buf[i];
		buf[i] = buf[i+1];
		buf[i+1] = temp;
	}

	res = osd_fwrite(file,buffer,length);

	for (i = 0;i < length;i+=2)
	{
		temp = buf[i];
		buf[i] = buf[i+1];
		buf[i+1] = temp;
	}

	return res;
}

struct bmpdata {
	ULONG buffernum, linebytes, linetot;
	char ops_ok;
	char access_on;
	char safety;
	unsigned char *buffer;
};

// osd_bitmap *osd_create_bitmap( int height, int width ) {
struct osd_bitmap *osd_new_bitmap( int width, int height, int depth ) {
	ULONG i;
	struct osd_bitmap *ret;
	struct bmpdata *osd_specific;
	unsigned char *buffer;   // , debugbuffer[512];
	char safety = 0, mult = 1;

	if ( width == 0 ) width = 1;
	if ( height == 0 ) height = 1;
	// Fix bugs caused by allocating a 1x0 bitmap.

	if ( /*(Machine->orientation & ORIENTATION_ROTATE_90) || (Machine->orientation & ORIENTATION_ROTATE_270 ) ||*/ (Machine->orientation & ORIENTATION_SWAP_XY) ) {
		i = width; width = height; height = i;
	}

	fprintf( fp, "Allocating bitmap of %dx%d, %d bits per pixel.\n", width, height, depth );
	fflush(fp);

	if (width > 64) safety = 8; // Safety area around larger bitmaps

	ret = (struct osd_bitmap *)malloc( sizeof( struct osd_bitmap ) );
	osd_specific = (struct bmpdata *)malloc( sizeof( struct bmpdata ));
	osd_specific->safety = safety;
	ret->width = width;  ret->height = height;
	if ( depth != 8 && depth !=16 ) { 
		ret->depth = 8;
	} else {
		ret->depth = depth;
	}
	ret->_private = osd_specific;
	if ( scanlines ) {
		mult = 2; // double height for scan lines
	}

	if ( DiveAllocImageBuffer( diveinst, &(osd_specific->buffernum), blitdepth,
	   (width+(2*safety))*(ret->depth>>3), (height*mult)+(2*safety), width+(2*safety), NULL ) == 0 ) {
		if ( DiveBeginImageBufferAccess( diveinst, osd_specific->buffernum, (PBYTE *)&buffer,
		    &(osd_specific->linebytes), &(osd_specific->linetot) ) == 0 ) {
			osd_specific->access_on = 1;  osd_specific->ops_ok = 1;
			ret->line = (unsigned char **)malloc( (height+1) * mult * sizeof( unsigned char * ) );
			for ( i=0; i<=height; ++i ) {
				ret->line[i] = &(buffer[ ((width+(2*safety)) * (ret->depth>>3) * (ret->depth>>3) * ((i*mult)+safety)) + (safety*(ret->depth>>3)) ]);
			}
			osd_specific->buffer = buffer;
  		} else {
			WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, "Can't access DIVE buffer", "OSD", 0, MB_CANCEL | MB_ERROR | MB_SYSTEMMODAL | MB_MOVEABLE );
			fprintf( fp, "DIVE could not begin image buffer access for this bitmap.\n" );
			fflush(fp);
			osd_specific->access_on = 0;  osd_specific->ops_ok = 0;
			return 0;
	       	}
	} else {
		WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, "Can't allocate DIVE buffer", "OSD", 0, MB_CANCEL | MB_ERROR | MB_SYSTEMMODAL | MB_MOVEABLE );
		fprintf( fp, "DIVE could allocate space for this bitmap.\n" );
		fflush(fp);
		osd_specific->access_on = 0;  osd_specific->ops_ok = 0;
		return 0;
	}
//	WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, "Bitmap Create OK", "OSD", NULL, MB_OK | MB_INFORMATION | MB_SYSTEMMODAL | MB_MOVEABLE );
	osd_clearbitmap( ret );
	return ret;
}

void osd_clearbitmap( struct osd_bitmap *bmp ) {
	struct bmpdata *osdd;
	osdd = (struct bmpdata *)bmp->_private;
	if ( ! osdd->access_on ) {
		DiveBeginImageBufferAccess( diveinst, osdd->buffernum, (PBYTE *)&(osdd->buffer),
		    &(osdd->linebytes), &(osdd->linetot) );
		osdd->access_on = 1;
	}
	memset( osdd->buffer, bkcolor, ((bmp->width) + (2*osdd->safety)) * (bmp->depth>>3) * (bmp->depth>>3) * (bmp->height*(scanlines?2:1) + (2*osdd->safety)) );
	if ( bmp == Machine->scrbitmap ) {
		extern int bitmap_dirty;
		bitmap_dirty = 1;
	}
}

void osd_free_bitmap( struct osd_bitmap *bmp ) {
	struct bmpdata *osds; 

	if ( bmp == NULL ) return;
	// crash protection from freeing the same thing twice

	osds = (struct bmpdata *)bmp->_private;

	if ( bmp->_private == NULL ) return;

	fprintf( fp, "Bitmap free- " );
	fflush(fp);

	if ( osds->access_on && osds->buffernum != 0 ) {
		DiveEndImageBufferAccess( diveinst, osds->buffernum );
		fprintf( fp, "ending access, " );
		fflush(fp);
	}
	if ( osds->ops_ok ) {
		if ( osds->buffernum != 0 ) {
			DiveFreeImageBuffer( diveinst, osds->buffernum );
		} else {
			free( osds->buffer );
		}
		fprintf( fp, "freeing buffer (ops ok), " );
		fflush(fp);
	}
	if ( bmp->_private != NULL ) free( bmp->_private );

	free( bmp->line );

	free( bmp );
	fprintf( fp, "done.\n" );
	fflush(fp);
	bmp = NULL;
}

void osd_mark_dirty( int x1, int y1, int x2, int y2, int p5 ) {
	// Don't know what p5 does!  DOS code ignores it, so I will too.
//	RECTL rect;
//	rect.xLeft = x1; rect.yBottom = y2;
//	rect.xRight = x2; rect.yTop = y1;
//	WinInvalidateRect( clientwin, &rect, FALSE );

// Not worth making this function.  This does not improve performance.
}

void osd_led_w( int led, int on ) {
	static char leds[5] = {0,0,0,0,0};
	SHIFTSTATE ss;
	BYTE KeyState[257];
	ULONG ulAction, ulLength;
	HFILE hf;
	int vk, k;

	if ( allowledflash ) {
		DosOpen("KBD$", &hf, &ulAction, 0L, 0, FILE_OPEN, OPEN_ACCESS_READONLY|OPEN_SHARE_DENYNONE, 0);
		ulAction = 0;
		ulLength = sizeof(ss);
		DosDevIOCtl(hf, IOCTL_KEYBOARD, KBD_GETSHIFTSTATE, 0, 0, &ulAction, &ss, sizeof(ss), &ulLength);

		WinSetKeyboardStateTable(HWND_DESKTOP, KeyState, FALSE);

		switch ( led ) {
			case 0:
				vk = VK_NUMLOCK;
				k = NUMLOCK_ON;
			break;
			case 1:
				vk = VK_CAPSLOCK;
				k = CAPSLOCK_ON;
			break;
			default:
				vk = VK_SCRLLOCK;
				k = SCROLLLOCK_ON;
		}
		if ( on&1 ) {
			KeyState[vk] |= 0x01;
			ss.fsState |= k;
		} else {
			KeyState[vk] &= ~0x01;
			ss.fsState &= ~k;
		}
		/* seting keyboard state */
		WinSetKeyboardStateTable(HWND_DESKTOP, KeyState, TRUE);

		ulAction = sizeof(ss);
		ulLength = 0;
		DosDevIOCtl(hf, IOCTL_KEYBOARD, KBD_SETSHIFTSTATE, &ss, sizeof(ss), &ulAction, 0, 0, &ulLength);

		DosClose(hf);
	} else {
		if ( (!(on&1) && leds[led] ) || ((on&1) && !leds[led] )) {      
			// state changed

			leds[led] = on&1;

			((unsigned long *)leds)[0] += 0x01010101;

			WinSetMenuItemText( WinWindowFromID( framewin, FID_MENU ), 500, leds );

			((unsigned long *)leds)[0] -= 0x01010101;
		}
	}
}

extern JOYSTICK_STATUS jstick;
extern char mouseclick1, mouseclick2;


int osd_is_joy_pressed( int p1 ) {
	switch (p1) {
		case OSD_JOY_LEFT:
			return (jstick.Joy1X < -40 ); // Movement threshold ~30%
		case OSD_JOY_RIGHT:
			return (jstick.Joy1X > 40 );
		case OSD_JOY_UP:
			return (jstick.Joy1Y < -40 ); // Movement threshold ~30%
		case OSD_JOY_DOWN:
			return (jstick.Joy1Y > 40 );
		case OSD_JOY2_LEFT:
			return (jstick.Joy2X < -40 ); // Movement threshold ~30%
		case OSD_JOY2_RIGHT:
			return (jstick.Joy2X > 40 );
		case OSD_JOY2_UP:
			return (jstick.Joy2Y < -40 ); // Movement threshold ~30%
		case OSD_JOY2_DOWN:
			return (jstick.Joy2Y > 40 );
		case OSD_JOY_FIRE1:
			return (jstick.Joy1A || mouseclick1);
		case OSD_JOY_FIRE2:
			return (jstick.Joy1B || mouseclick2);
		case OSD_JOY_FIRE3:
			return (jstick.Joy2A);
		case OSD_JOY_FIRE4:
			return (jstick.Joy2B);
		case OSD_JOY2_FIRE1:
			return (jstick.Joy2A);
		case OSD_JOY2_FIRE2:
			return (jstick.Joy2B);
		case OSD_JOY_FIRE:
			return (jstick.Joy1A || jstick.Joy1B || jstick.Joy2A || jstick.Joy2B || mouseclick1 || mouseclick2 );
		case OSD_JOY2_FIRE:
			return (jstick.Joy2A || jstick.Joy2B);
	}
	return 0;
}

void osd_analogjoy_read( int player, int *x, int *y ) {
	*x = jstick.Joy1X;
	*y = jstick.Joy1Y;
}

void osd_trak_read( int player, int *dx, int *dy ) {
	static SHORT oldx = 0, oldy = 0;
	POINTL newpos;

	if ( !(devicesenabled & 2) ) {
		*dx = *dy = 0;
		return;
	}

	WinQueryPointerPos( HWND_DESKTOP, &newpos );

	switch ( mouseflip ) {
		case 0:
			*dx = newpos.x - oldx;
			*dy = oldy - newpos.y;
		break;
		case 1:
			*dx = oldx - newpos.x;
			*dy = oldy - newpos.y;
		break;
		case 2:
  			*dx = newpos.x - oldx;
			*dy = newpos.y - oldy;
		break;
		case 3:
			*dx = oldx - newpos.x;
			*dy = newpos.y - oldy;
		break;
		default:
			*dx = *dy = 0;
	}

	if ( mousecapture ){
		WinSetPointerPos( HWND_DESKTOP, centerx, centery );
		oldx = centerx; oldy = centery;
	} else {
		oldx = newpos.x; oldy = newpos.y;
	}
}

extern char hackedpause;

int osd_init( void ) {
	struct DEVICE_DRIVER driver;
	char *drivername;
	char buffer[256];
	struct InputPort *in;
	ULONG total,temp,temp2;
	struct InputPort *entry[40];
	unsigned long rc;

	fprintf( fp, "OSD interface initializing for %s.\n", drivers[GAME_TO_TRY]->description );
	fflush(fp);

	percentbarinit = 0;

	for (temp=0; temp<128; temp++) keystatus[temp] = 0;
	for (temp=0; temp<NUMVOICES; temp++) vfreqstart[temp] = 0;

	dropped = 0; totalframes = 0; curdropped = 0;
	DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &starttime, 4 );
	curframe = 0;
	frametimes = (unsigned long *)malloc( Machine->drv->frames_per_second * sizeof(unsigned long) );

	for ( temp=1; temp<=Machine->drv->frames_per_second; temp++ ) {
		frametimes[temp-1] = (unsigned long)((double)(((double)1000 / 
		    (double)Machine->drv->frames_per_second)) * (double)temp);
	}

	nonframeupdate = 0;

	brightness = 100;
	hackedpause = 0;

	dirty_new = (char *)malloc( 1600 );

	if ( soundon ) {
		sync_handle = NULL;

		fprintf(fp,"Looking for GPMIXER device drivers.\n" );

		if ( GPMIXERdevice >= AudioGetNumberOfDevices() ) {
			os2printf( "INI file contained invalid data for the audio device to use.  Using default audio device." );
			GPMIXERdevice = 0;
		}

		rc = AudioInitialize(GPMIXERdevice, Machine->sample_rate, AUDIO_BUF_SIZE, SOUND_QUALITY, NUMVOICES );

		fprintf(fp,"Initializing GPMIXER for %ldHz %d bit Mono audio!  Using a %ld byte audio buffer.  rc=%d\n", 
		    Machine->sample_rate, SOUND_QUALITY, AUDIO_BUF_SIZE, rc );

		if ( rc ) {
			fprintf(fp,"Aborting sound initialization.\n");
			soundon = 0;
			Machine->sample_rate = 0;
		}
	} else { Machine->sample_rate = 0; }

	soundreallyon = soundon;

	// Had to move rapid fire keys setup out of here because the input ports
	// are no longer known when this function is called.

	totalrapidkeys = 0;
	initrapidplz = 1;

	// Now instead, we just ask kindly for another routine to init it for us and
	// hope it only does so once.

	temp = 1; rapidrate = 10;
	PrfQueryProfileData( mameini, inpdef, firerate, &rapidrate, &temp );
	temp = 1;
	PrfQueryProfileData( mameini, buffer, firerate, &rapidrate, &temp );
 	return 0;
}

void osd_exit( void ) {
	int n;
	fprintf(fp,"OSD interface shutting down for %s.\n", drivers[GAME_TO_TRY]->description );
	fflush(fp);
	if ( soundreallyon ) {
		PlaySampleSyncCleanup( &sync_handle );
		AudioClose();
	}
	if ( rapidkeys ) { free( rapidkeys ); rapidkeys = NULL; totalrapidkeys = 0; }
	if ( dirty_new ) { free( dirty_new ); dirty_new = NULL; }
	free( frametimes );
	frametimes = NULL;
}

struct osd_bitmap *screen;

void osd_close_display( void ) {
	fprintf(fp, "Closing the display and freeing DIVE bitmap.\n" );
	fflush(fp);
	osd_free_bitmap( screen );
	initialbmp = 1;
	blitwidth = 640; blitheight = 480; blitdepth = FOURCC_R565;
	bufnum = 0;

	WinSendMsg( clientwin, WM_RECREATE, 0, 0 );
	WinSendMsg( clientwin, WM_VRNENABLE, 0, 0 );
	WinPostMsg( clientwin, WM_PAINT, 0, 0 );
}

char vector_game = 0;

void osd_allocate_colors(unsigned int totalcolors,const unsigned char *palette,unsigned short *pens) {
	int i;
	fprintf( fp, "Allocating colors: %d total.\n", totalcolors);
	fflush(fp);
	if ( Machine->drv->video_attributes & VIDEO_SUPPORTS_16BIT) {
		for (i = 0; i < 32768l; i++)
		{
			pens[i] = i;
			bkcolor = 0;
		}
		Machine->uifont->colortable[0] = 0;
		Machine->uifont->colortable[1] = 0xffff;
		Machine->uifont->colortable[2] = 0xffff;
		Machine->uifont->colortable[3] = 0;
	} else {
		for (i = 0;i < 256;i++)
		{
			current_palette[i*4] = current_palette[(i*4)+1] = current_palette[(i*4)+2] = current_palette[(i*4)+3] = 0;
			reported_palette[i*4] = reported_palette[(i*4)+1] = reported_palette[(i*4)+2] = reported_palette[(i*4)+3] = 0;
		}
		for (i = 0;i < totalcolors;i++) {
			pens[i] = i;
			current_palette[(i*4)+2] = palette[3*i];
			current_palette[(i*4)+1] = palette[(3*i)+1];
			current_palette[(i*4)+0] = palette[(3*i)+2];
			current_palette[(i*4)+3] = 0;
			reported_palette[(i*4)+2] = palette[3*i];
			reported_palette[(i*4)+1] = palette[(3*i)+1];
			reported_palette[(i*4)+0] = palette[(3*i)+2];
			reported_palette[(i*4)+3] = 0;
		}
		if ( totalcolors <= 254 ) {
			bkcolor = 254;
			// Define a nice white if we can.
			current_palette[(255*4)+3] = current_palette[(255*4)+2] = current_palette[(255*4)+1] = current_palette[(255*4)] = 255;
			reported_palette[(255*4)+3] = reported_palette[(255*4)+2] = reported_palette[(255*4)+1] = reported_palette[(255*4)] = 255;
			Machine->uifont->colortable[0] = 254;
			Machine->uifont->colortable[1] = 255;
			Machine->uifont->colortable[2] = 255;
			Machine->uifont->colortable[3] = 254;

		} else {
			bkcolor = 0;  // Default to 0 if we don't find anything good.
			for ( i = 0; i < totalcolors; i++ ) {
				if ( current_palette[(i*4)+2] == 0 && current_palette[(i*4)+1] == 0 && current_palette[i*4] == 0 ) {
					// If we have a nice black already, use that one.
					bkcolor = i;
					break;
				}
			}
			// assume we at least have <1> color to play with
			current_palette[(255*4)+0] = reported_palette[(255*4)+0] = 255;
			current_palette[(255*4)+1] = reported_palette[(255*4)+1] = 255;
			current_palette[(255*4)+2] = reported_palette[(255*4)+2] = 255;
			current_palette[(255*4)+3] = reported_palette[(255*4)+3] = 0;
			Machine->uifont->colortable[0] = bkcolor;
			Machine->uifont->colortable[1] = 255;
			Machine->uifont->colortable[2] = 255;
			Machine->uifont->colortable[3] = bkcolor;
		} 
		DiveSetSourcePalette( diveinst, 0, 256, (PBYTE)current_palette );
	}
	palchange = 1;
}

extern unsigned int gameX, gameY;

struct osd_bitmap *osd_create_display(int width,int height,int attributes) {
	struct bmpdata *osdd;
	RECTL framesize, clisize;
	SWP swp;
	int iYPos, iYDiff;

	WinSendMsg( clientwin, EmuStart, 0, 0 );

	if (Machine->drv->video_attributes & VIDEO_TYPE_VECTOR) {
		vector_game = 1;
	} else {
		vector_game = 0;
	}

	WinSendMsg(clientwin, WM_VRNDISABLE, 0, 0 ); // Stop all blitting here, just in case.

	if ( Machine->orientation & ORIENTATION_SWAP_XY ) {
		fprintf( fp, "Display swaps X and Y.\n" );
		blitwidth = Machine->drv->visible_area.max_y - Machine->drv->visible_area.min_y;
		blitheight = Machine->drv->visible_area.max_x - Machine->drv->visible_area.min_x;
		gameX = Machine->drv->visible_area.min_y + 8 /* Safety size */;
		gameY = Machine->drv->visible_area.min_x + 8 /* Safety size */;
	} else {
		blitwidth = Machine->drv->visible_area.max_x - Machine->drv->visible_area.min_x;
		blitheight = Machine->drv->visible_area.max_y - Machine->drv->visible_area.min_y;
		gameX = Machine->drv->visible_area.min_x + 8 /* Safety size */;
		gameY = Machine->drv->visible_area.min_y + 8 /* Safety size */;
/*		blitwidth = Machine->drv->screen_width;
		blitheight = Machine->drv->screen_height;
		gameX = ((width+16)>>1)-(blitwidth>>1);   // center in screen
		gameY = ((height+16)>>1)-(blitheight>>1); // center in screen */
	}

	if ( vector_game ) { blitwidth = width = 640; blitheight = height = 480; gameX = gameY = 9; }

	if ( /*totalcolors > 254 && */ (Machine->drv->video_attributes & VIDEO_SUPPORTS_16BIT)) { 
		fprintf(fp, "Creating 16 bit display, size %dx%d.\n",width,height );
		blitdepth = FOURCC_R555;
		screen = osd_new_bitmap( width, height, 16 );
	} else {
		fprintf(fp, "Creating 8 bit display, size %dx%d.\n",width,height );
		blitdepth = FOURCC_LUT8;
		screen = osd_new_bitmap( width, height, 8 );
	}

	if ( !screen ) fprintf( fp, "Screen bitmap creation FAILED!\n" );
	fflush(fp);

	osdd = (struct bmpdata *)screen->_private;

	osd_clearbitmap( screen ); 

// Free up that initial pretty colored bitmap... carefully.

	DosRequestMutexSem( DiveBufferMutex, -1 );
	DiveFreeImageBuffer( diveinst, bufnum );
	bufnum = osdd->buffernum;
	initialbmp = 0;
	DosReleaseMutexSem( DiveBufferMutex );

	if ( !custom_size ) {
		WinQueryWindowRect( framewin, &framesize );
	WinQueryWindowRect( clientwin, &clisize );

		WinSetWindowPos( framewin, HWND_TOP, 0, 0, (blitwidth*(scanlines?2:1)) + (framesize.xRight-framesize.xLeft-clisize.xRight+clisize.xLeft),
		    (blitheight*(scanlines?2:1)) + (framesize.yTop-framesize.yBottom-clisize.yTop+clisize.yBottom), SWP_SIZE );

		WinQueryWindowPos( framewin, &swp );
		WinQueryWindowRect( framewin, &framesize );
		WinQueryWindowRect( clientwin, &clisize );

		/* blit height plus the height of the frame elements */
		iYPos = (blitheight*(scanlines?2:1)) + (framesize.yTop - framesize.yBottom - clisize.yTop + clisize.yBottom);

		iYDiff = swp.y;
		/* If the top of the window is above the desktop... */
		if ((iYPos + swp.y) > (centery<<1))
		  /* then make it just hit the top. */
		  iYDiff = (centery<<1) - iYPos;
		WinSetWindowPos( framewin, HWND_TOP, 0, 0, blitwidth + (framesize.xRight-framesize.xLeft-clisize.xRight+clisize.xLeft),
		  blitheight + (framesize.yTop-framesize.yBottom-clisize.yTop+clisize.yBottom), SWP_SIZE );
		WinSetWindowPos( framewin, HWND_TOP, swp.x, iYDiff,
		  blitwidth + (framesize.xRight - framesize.xLeft - clisize.xRight + clisize.xLeft),
		  iYPos, SWP_SIZE|SWP_MOVE);
		// do this duplication to make sure the height is correct when the width changes (menu related)
	}
	set_ui_visarea( gameX-8, gameY-8, gameX+blitwidth-9, gameY+blitheight-9 );
	WinPostMsg(framewin, WM_VRNENABLE, 0L, 0L);
	WinPostMsg(framewin, WM_PAINT, 0L, 0L);
	return screen;
}

void osd_set_mastervolume( int vol ) {
	// volume was changed to attenuation by Nicola... ????
	// instead of a straightforward 0-255, the "volume" is in dB.

	float volume;

	attenuation = vol;

	volume = 256.0;	/* range is 0-256 */
	while (vol++ < 0)
		volume /= 1.122018454;	/* = (10 ^ (1/20)) = 1dB */

	MasterVolume = (signed int) volume;

//	ASetAudioMixerValue(AUDIO_MIXER_MASTER_VOLUME,MasterVolume);
}

int osd_get_mastervolume(void) {
	return attenuation;
}

int osd_skip_this_frame( void ) {
	ULONG currentmscount;
	int ret = 0;

	curframe++;
	if ( curframe >= Machine->drv->frames_per_second ) {
		DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &starttime, 4 );
		curframe = 0;
	}

	if ( !starttime ) {
		DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &starttime, 4 );
	}

	if ( autoskip ) {
		DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &currentmscount, 4 );

		if ( starttime+frametimes[curframe] < currentmscount ) {
			// Need to skip this frame.  We're running behind.
			if ( curdropped < maxskip || maxskip == 0 ) {
				curdropped++;
				dropped++;
				ret = 1;

				// Back-adjust the start time so we don't continually skip for
				// getting behind once.
//				starttime += currentmscount - (starttime+frametimes[curframe]);
			} else {
				curdropped = 0;
				ret = 0;
				// We've skipped as many frames in a row as the user wants us
				// to.  Draw this frame even though we're behind.
			}
		} else {
			// No need to skip this frame.  We're on or ahead of schedule.
			curdropped = 0;
			ret = 0;
		}

	} else {
		// The user does not want regulation of frame skipping.
		curdropped = 0;
		ret = 0;
	}

	totalframes++;
	if ( totalframes > 16777216 ) {
		// Wrap this counter after 3.2 straight days at 60fps  ;-)
		totalframes = 0;
		dropped = 0;
	}

	return ret;
}

// The "nonframeupdate" business is a hack to keep the CPU from racing
// out of control while MAME is on pause.  It reduces the frame rate to
// only 5 fps instead of the 30 or 60 it normally is.  It sucks that it
// has to update at all, but this is the way usrintrf.c was designed.

void osd_update_video_and_audio( void ) {
	ULONG targetmscount;
	ULONG currentmscount, size;
   	char buf[25];
	int i;

	if ( hackedpause ) {
		DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &starttime, 4 );
		curframe = 0;
	}

	if ( autoslow || hackedpause ) {
		DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &currentmscount, 4 );
		if ( starttime+frametimes[curframe] > currentmscount ) {
			// Slow down a bit.  We're too fast.
			if ( useTIMER0 ) {
				size = sizeof( int );
				targetmscount = (starttime+frametimes[curframe]) - currentmscount;
				if ( (DosDevIOCtl(timer, HRT_IOCTL_CATEGORY, HRT_BLOCKUNTIL,
					&targetmscount, size, &size, NULL, 0, NULL)) != 0) {
					fprintf( fp, "DevIOCtl call to TIMER0 device driver failed!\n" );
					fclose( fp );
					exit(1);
				}
			} else {
				targetmscount = starttime+frametimes[curframe];
				do {
					DosQuerySysInfo( QSV_MS_COUNT, QSV_MS_COUNT, &currentmscount, 4 );
					DosSleep(0);
				} while ( currentmscount < targetmscount );
			}
		}
	}

	if ( showfps && !nonframeupdate ) {
		unsigned short color;
		int trueorientation = Machine->orientation;
		Machine->orientation = ORIENTATION_DEFAULT;
		if ( blitdepth != FOURCC_LUT8 ) color = 0xffff; else color = 255;
		if ( totalframes ) {
			sprintf(buf,"%3d fps (dropped %ld = %d%%)  ", Machine->drv->frames_per_second,dropped,dropped*100/totalframes);
		} else {
			sprintf(buf,"%3d fps", Machine->drv->frames_per_second);
		}
		for (i=0; i<strlen(buf); ++i)
		    drawgfx(Machine->scrbitmap,Machine->uifont,buf[i],color,0,0,gameX-8+(i*Machine->uifont->width),gameY-8,0,TRANSPARENCY_NONE,0);
		Machine->orientation = trueorientation;
	}

	if ( palchange ) {
		palchange = 0;
		DiveSetSourcePalette( diveinst, 0, 256, (PBYTE)current_palette );
		WinPostMsg( framewin, WM_REALIZEPALETTE, 0, 0 );
	}

	WinSendMsg( clientwin, WM_PAINT, 0, 0 );
}

void osd_poll_joystick( void ) {
	// Not needed in this implementation.  Joystick is polled periodically by a timer.
}

void osd_update_audio( void ) {
	// Not needed for GPMIXER interface.  It is handled automagically by another thread.
}

void osd_play_sample( int channel, signed char *data, int len, int freq, int volume, int loop ) {
        ULONG rc;

	if (soundreallyon == 0 || channel >= NUMVOICES) return;

	if (!vfreqstart[channel]) {
		vfreqstart[channel] = 1;
		fprintf(fp,"Channel %d initial freq = %ld, 8 bits.\n", channel, freq );
	}

	/* backwards compatibility with old 0-255 volume range */
	if (volume > 100) volume = volume * 25 / 255;

	volume = (volume * 255) / 100;
	if ( volume > 255 ) volume = 255;

	rc = PlaySample( data, channel, len, 8, freq, volume, loop, 0 );
	if ( rc ) {
		fprintf( fp, "Error playing a sound sample! rc=%ld", rc );
	}
}

void osd_play_sample_16(int channel,signed short *data,int len,int freq,int volume,int loop) {
        ULONG rc;

	if (soundreallyon == 0 || channel >= NUMVOICES) return;

	if (!vfreqstart[channel]) {
		vfreqstart[channel] = 1;
		fprintf(fp,"Channel %d initial freq = %ld, 16 bits.\n", channel, freq);
	}

	/* backwards compatibility with old 0-255 volume range */
	if (volume > 100) volume = volume * 25 / 255;

	volume = (volume * 255) / 100;
	if ( volume > 255 ) volume = 255;

	rc = PlaySample( data, channel, len, 16, freq, volume, loop, 0 );
	if ( rc ) {
		fprintf( fp, "Error playing a sound sample! rc=%ld", rc );
	}
}

void osd_play_streamed_sample(int channel,signed char *data,int len,int freq,int volume,int pan) {
        ULONG rc;

	if (soundreallyon == 0 || channel >= NUMVOICES) return;

	if (!vfreqstart[channel]) {
		vfreqstart[channel] = 1;
		fprintf(fp,"Channel %d initial freq = %ld, 8 bits.\n", channel, freq);
	}

	/* backwards compatibility with old 0-255 volume range */
	if (volume > 100) volume = volume * 25 / 255;

	volume = (volume * 255) / 100;
	if ( volume > 255 ) volume = 255;

	rc = PlaySampleSync( data, channel, len, 8, freq, volume, 1, 0, &sync_handle );

	if ( rc ) {
		fprintf( fp, "Error playing a sound sample! rc=%ld", rc );
	}
}

void osd_play_streamed_sample_16(int channel,signed short *data,int len,int freq,int volume,int pan) {
        ULONG rc;

	if (soundreallyon == 0 || channel >= NUMVOICES) return;

	if (!vfreqstart[channel]) {
		vfreqstart[channel] = 1;
		fprintf(fp,"Channel %d initial freq = %ld, 16 bits.\n", channel, freq);
	}

	/* backwards compatibility with old 0-255 volume range */
	if (volume > 100) volume = volume * 25 / 255;

	volume = (volume * 255) / 100;
	if ( volume > 255 ) volume = 255;

	rc = PlaySampleSync( data, channel, len, 16, freq, volume, 1, 0, &sync_handle );

	if ( rc ) {
		fprintf( fp, "Error playing a sound sample! rc=%ld", rc );
	}
}


void osd_ym2203_update( void ) {

}

void osd_stop_sample( int channel ) {
	if (soundreallyon == 0 || channel >= NUMVOICES) return;

	StopChannelNow( channel );
//	AStopVoice(hVoice[channel]);
}

void osd_set_sample_volume( int channel, int volume ) {
	if (soundreallyon == 0 || channel >= NUMVOICES) return;

	/* backwards compatibility with old 0-255 volume range */
	if (volume > 100) volume = volume * 25 / 255;

	volume = (volume * 893) / 100;
	if ( volume > 255 ) volume = 255;

	AudioSetSampleVolume( channel, volume );
}

void osd_set_sample_freq( int channel, int freq ) {
	if (soundreallyon == 0 || channel >= NUMVOICES) return;

	AudioSetSampleFrequency( channel, freq );
}

void osd_restart_sample(int channel) {
	if (soundreallyon == 0 || channel >= NUMVOICES) return;

//	AStartVoice(hVoice[channel]);
}


int osd_get_sample_status(int channel) {
	ULONG stopped=0;
	if (soundreallyon == 0 || channel >= NUMVOICES) return -1;

//	AGetVoiceStatus(hVoice[channel], (LPBOOL)&stopped);
	return 1;
}

void osd_get_pen( int p1, unsigned char *p2, unsigned char *p3, unsigned char *p4 ) {
	if ( blitdepth == FOURCC_LUT8 ) {
		*p2 = reported_palette[(p1*4)+2];
		*p3 = reported_palette[(p1*4)+1];
		*p4 = reported_palette[(p1*4)+0];
	} else {
		*p2 = (p1&0xf800)>>11;
		*p3 = (p1&0x07e0)>>5;
		*p4 = (p1&0x001f);
	}
}

const char *osd_joy_name( int joycode ) {
	static char *joynames[] = {
		"Left", "Right", "Up", "Down", "Button A",
		"Button B", "Button C", "Button D", "Button E", "Button F",
		"Button G", "Button H", "Button I", "Button J", "Any Button",
		"J2 Left", "J2 Right", "J2 Up", "J2 Down", "J2 Button A",
		"J2 Button B", "J2 Button C", "J2 Button D", "J2 Button E", "J2 Button F",
		"J2 Button G", "J2 Button H", "J2 Button I", "J2 Button J", "J2 Any Button",
		"J3 Left", "J3 Right", "J3 Up", "J3 Down", "J3 Button A",
		"J3 Button B", "J3 Button C", "J3 Button D", "J3 Button E", "J3 Button F",
		"J3 Button G", "J3 Button H", "J3 Button I", "J3 Button J", "J3 Any Button",
		"J4 Left", "J4 Right", "J4 Up", "J4 Down", "J4 Button A",
		"J4 Button B", "J4 Button C", "J4 Button D", "J4 Button E", "J4 Button F",
		"J4 Button G", "J4 Button H", "J4 Button I", "J4 Button J", "J4 Any Button"
	};

	if (joycode == 0) return "None";
	else if (joycode <= OSD_MAX_JOY) return (char *)joynames[joycode-1];
	else return "Unknown";
}

void osd_modify_pen( int p1, unsigned char tr, unsigned char tg, unsigned char tb ) {
	if ( blitdepth != FOURCC_LUT8 ) return;
	reported_palette[(p1*4)+2]=tr;
	reported_palette[(p1*4)+1]=tg;
	reported_palette[(p1*4)+0]=tb;
	reported_palette[(p1*4)+3]=0;

	current_palette[(p1*4)  ] = 255 * brightness * pow(tb / 255.0, 1.0 / fGamma) / 100;
	current_palette[(p1*4)+1] = 255 * brightness * pow(tg / 255.0, 1.0 / fGamma) / 100;
	current_palette[(p1*4)+2] = 255 * brightness * pow(tr / 255.0, 1.0 / fGamma) / 100;
	current_palette[(p1*4)+3] = 0;

	palchange = 1;
}

void osd_opl_control( int chip, int reg ) {
	// I'm a dummy function put here to make the MAME core happy
}

void osd_opl_write( int chip, int data ) {
	// I'm a dummy function put here to make the MAME core happy
}

unsigned long osd_cycles( void ) {
	return 0;
}

// I don't take no stinkin orders from the MAME core !!!
void osd_set_config(int def_samplerate, int def_samplebits) {}
void osd_save_config(int frameskip, int samplerate, int samplebits) {}

void osd_sound_enable(int enable) {
	int i; unsigned char rc;

	if ( !soundreallyon ) return;
	if ( enable ) {
//		ASetAudioMixerValue(AUDIO_MIXER_MASTER_VOLUME,MasterVolume);
		fprintf(fp,"Sound enabled\n");
	} else {
		fprintf(fp,"Sound disabled\n");
		for ( i=0; i<NUMVOICES; ++i ) {
			rc = StopChannelAfterLoop(i);
			if (  rc != AUDIO_OK ) fprintf(fp,"Error %d.\n",rc);
		}
	}
}

int osd_get_config_frameskip(int def_frameskip) { return 0; }

// Not implemented... stick to the basics, please!!
void osd_profiler(int type) {}

// Assumes R,B are 0-31, G is 0-63.
#define makecol( r, g, b ) ((unsigned short)(((unsigned short)(b))&(unsigned short)31)|((((unsigned short)(g))&(unsigned short)63)<<5)|((((unsigned short)(r))&(unsigned short)31)<<11))

void fix_palette( void ) {
	unsigned short curcol;

	if (Machine->drv->video_attributes & VIDEO_SUPPORTS_16BIT ) {
		// Can't modify palette in 16 bpp, so modify pens
		// Hopefully that does the trick.
/*		for ( curcol = 0; curcol < 32768; curcol++ ) {
			Machine->pens[curcol] = makecol( 31 * brightness * pow((curcol&31)/31.0, 1.0/fGamma) / 100,
			    63 * brightness * pow(((curcol>>5)&63)/63.0, 1.0/fGamma) / 100,
			    31 * brightness * pow(((curcol>>11)&31)/31.0, 1.0/fGamma) / 100 );
		} */
		// Well.. it was bugged up, so forget it for now.
		return;
	}

	// Leave the UI colors alone.  Only touch 0->253.
	for ( curcol = 0; curcol < 254; curcol++ ) {
		current_palette[(curcol*4)  ] = 255 * brightness * pow(reported_palette[(curcol*4)  ] / 255.0, 1.0 / fGamma) / 100;
		current_palette[(curcol*4)+1] = 255 * brightness * pow(reported_palette[(curcol*4)+1] / 255.0, 1.0 / fGamma) / 100;
		current_palette[(curcol*4)+2] = 255 * brightness * pow(reported_palette[(curcol*4)+2] / 255.0, 1.0 / fGamma) / 100;
		current_palette[(curcol*4)+3] = 255 * brightness * pow(reported_palette[(curcol*4)+3] / 255.0, 1.0 / fGamma) / 100;
	}

	DiveSetSourcePalette( diveinst, 0, 256, (PBYTE)current_palette );
	WinPostMsg( framewin, WM_REALIZEPALETTE, 0, 0 );
}

void osd_set_gamma(float _gamma) {
	fGamma = _gamma;
	fix_palette();
}

float osd_get_gamma(void) {return fGamma;}

void osd_set_brightness(int _brightness) {
	brightness = _brightness;
	fix_palette();
}

int osd_get_brightness(void) { return brightness; }

void osd_save_snapshot(void) {}

#define SET_MAXMIN_VAL WM_USER
#define SET_CURRENT_VAL WM_USER+1
#define ADD_TO_CURRENT_VAL WM_USER+2

int osd_display_loading_rom_message( const char *name, int current, int total ) {
	extern HWND StartupDlgHWND;
	WinSetWindowText(WinWindowFromID(StartupDlgHWND, CurrentLoadingRom), name);
	if ( !percentbarinit ) {
		WinSendDlgItemMsg(StartupDlgHWND,StartupPercent,
		    SET_MAXMIN_VAL,MPFROMLONG(total),MPFROMLONG(0));
		percentbarinit = 1;
	}
	WinSendDlgItemMsg(StartupDlgHWND,StartupPercent,
	    ADD_TO_CURRENT_VAL,MPFROMLONG(1),0);
	return 0;
}

// 0 means unpause
// 1 means pause
// 2 means fake pause (OSD initiated thread freeze)
// 3 means fake unpause (OSD initiated thread thaw)
void osd_pause(int newstate) {
	static char oldstate = 0, prevbright = 100;
	if ( oldstate != newstate ) {
		if ( oldstate == 1 && (newstate == 2 || newstate == 3)) return;
		if ( oldstate == 2 && newstate == 3 ) {
			oldstate = 0;
			osd_set_brightness( prevbright );
			osd_update_video_and_audio();
			WinSendMsg( clientwin, WM_COMMAND, MPFROMLONG(PauseGame), MPFROMSHORT((short)0) );
			return;
		}
		if ( oldstate == 0 && newstate == 3 ) return;
		oldstate = newstate;
		if ( newstate ) { prevbright = osd_get_brightness(); osd_set_brightness( 65 ); }
		else osd_set_brightness( prevbright );
		osd_update_video_and_audio();
		WinSendMsg( clientwin, WM_COMMAND, MPFROMLONG(PauseGame), MPFROMSHORT((short)newstate!=0) );
	}
}

void osd_customize_inputport_defaults(struct ipd *defaults) {
	// I dunno WTF this is supposed to be  :-/
}

int MapMAMEKeyToOSD( int MAMEkey ) {
	int i = 0;
	while (keylist[i].code != 0) {
		if ( keylist[i].standardcode == MAMEkey ) {
			return keylist[i].code;
		}
		++i;
	}
	return 0;
}

const struct KeyboardInfo *osd_get_key_list(void) {
	if ( initrapidplz ) {
		char buffer[256];
		struct InputPort *in;
		ULONG total,temp;
		struct InputPort *entry[40];

		in = Machine->input_ports;
		total = 0;
		while (in->type != IPT_END) {
			if (input_port_name(in) != 0 && input_port_key(in) != IP_KEY_NONE && (in->type & IPF_UNUSED) == 0
				&& (((in->type & 0xff) < IPT_ANALOG_START) || ((in->type & 0xff) > IPT_ANALOG_END))
				&& !(nocheat && (in->type & IPF_CHEAT))) {
					entry[total] = in;
					total++;
			}
			in++;
		}

		sprintf( buffer, inpgamespec, drivers[GAME_TO_TRY]->name );
		temp = sizeof( int );
		totalrapidkeys = 0;
		total = 0;

		PrfQueryProfileSize( mameini, buffer, firekeys, &total );
 
		if ( rapidkeys ) { free( rapidkeys ); rapidkeys = NULL; }

		if ( total ) {
			rapidkeys = (int *)malloc( total );
			temp = total;
			PrfQueryProfileData( mameini, buffer, firekeys, rapidkeys, &temp );
			total /= sizeof(int);
			totalrapidkeys = total;
			rapidtrigger = 0;
			for ( temp=0; temp<total; temp++ ) {
				fprintf( fp, "Rapid fire key %d enabled: %d M%d E%d\n", temp, MapMAMEKeyToOSD(input_port_key(entry[ rapidkeys[temp] ])), input_port_key(entry[ rapidkeys[temp] ]), rapidkeys[temp] );
				fflush(fp);
				rapidkeys[temp] = MapMAMEKeyToOSD(input_port_key(entry[ rapidkeys[temp] ]));  // Convert from index to scan codes
			}
		}
		initrapidplz = 0;
	}

	return (struct KeyboardInfo *)&keylist;
}

const struct JoystickInfo *osd_get_joy_list(void) {
	return (struct JoystickInfo *)&joylist;
}

// This is a load of crap that was an after-thought from the DOS
// team that I already implemented myself  :P
// Needless to say, I'm not implementing it here.
void osd_joystick_start_calibration( void ) {}
void osd_joystick_calibrate( void ) {}
char *osd_joystick_calibrate_next (void) {return "Please use the calibration routine under the input options menu item.";}
void osd_joystick_end_calibration( void ) {}
int osd_joystick_needs_calibration( void ) {return 0;}
void osd_poll_joysticks( void ) {}
int osd_key_invalid(int keycode) { return 0; }

