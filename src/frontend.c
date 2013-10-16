#define INCL_PM                
#define INCL_DOSMODULEMGR
#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS
#define INCL_DOSMISC
#define INCL_BASE
#include <os2.h>
#include <stdio.h>
#include <malloc.h>
#include <process.h>
#include <stdarg.h>

#include "tmr0_ioc.h"
#include "dive.h"
#include "osdepend.h"
#include "os2_front.h"
#include "mameresource.h"
#include "mame.h"
#include "joyos2.h"
#include "driver.h"
#include "audit.h"
#include "gpmixer.h"

#define VERSION "Version .36 Beta 7"
#define VERNUM		  3600700
#define VERNUM_36b6	  3600600
#define VERNUM_36b5	  3600500
#define VERNUM_36b4	  3600400
#define VERNUM_35b11	  3501101
#define VERNUM_35b11pre	 03501100 // I screwed up this version and did it in octal :-/

/*
#define F44K 44100
#define F33K 33075 
#define F22K 22050
#define F11K 11025
#define F8K 8000
*/
#define F44K 44100
#define F33K 33060 
#define F22K 22020
#define F11K 10980
#define F8K 7980

#define FOURCC_RGB3 0x33424752ul
#define FOURCC_R565 0x35363552ul
#define FOURCC_LUT8 0x3854554cul
#define FOURCC_SCRN 0

// MAME Main Window constants
#define WM_VRNDISABLE 0x7e
#define WM_VRNENABLE 0x7f
#define GET_KEY WM_USER
#define WAIT_KEY WM_USER+1
#define EmuDone WM_USER+2
#define WM_RECREATE WM_USER+3
#define EmuStart WM_USER+4

FILE *fp;

ULONG APIENTRY WinSetVisibleRegionNotify( HWND win, BOOL bool );
ULONG APIENTRY WinQueryVisibleRegion( HWND win, HRGN hrgn );

extern const char *input_port_name(const struct InputPort *in);
extern int input_port_key(const struct InputPort *in);

int nocheat;
extern FILE *errorlog;

TID emu_thread_id = 0;
char thread_state = 0;

char quitnotify = 0;

ULONG emu_thread_pclass = PRTYC_REGULAR;
LONG emu_thread_pdelta = 0;

// DIVE / PM globals.
HEV DiveBufferMutex = 0;
unsigned int gameX = 0, gameY = 0;
HINI mameini = 0;
HDIVE diveinst = 0;
ULONG bufnum = 0;
ULONG blitwidth=640, blitheight=480;
HWND clientwin, framewin;
USHORT last_char = 0;
ULONG blitdepth = FOURCC_R565;
char pauseonfocuschange = 1;
char initialbmp = 1;
char mousecapture = 1;
USHORT centerx, centery; // Center of the desktop
HWND StartupDlgHWND = 0;
char devicesenabled = 3;
	// bit 0 - Keyboard
	// bit 1 - mouse
	// bit 2 - joystick
	// bit 3 - mouse functions in abs mode adding to jstick
char mouseflip = 0;
	// bit 0 - Filp X
	// bit 1 - Flip Y
char rapidtrigger = 0; // 1 if rapid fire key is held down
char totalrapidkeys = 0;
int *rapidkeys = NULL; // list of rapid fire keys (no longer than totalrapidkeys)
char rapidrate = 10; // rapid fire rate (pulses per second)
char custom_size = 0; // Is a custom window size being used for this game?
char scanlines = 0; // should we use scanlines or not
char scanwarn = 1; // should we annoy the user telling them to restart
char warnDART = 1, GPMIXERdevice = 0;
char wantstochangescanmode = 0;
JOYSTICK_STATUS jstick;
char joydetected = 0;
// int fpscounter = 0, fpsstable = 0;
unsigned char fpstoggle = 0;
char showfps = 0;
HFILE  timer;
char mouseclick1=0, mouseclick2=0;
char useTIMER0=0;
char autoskip=1,maxskip=3,manskip=0,autoslow=1;
int frameskip=0;  // Needed for cheat.c - not used elsewhere
char soundon=1, soundreallyon=0, allowledflash = 1;
char hackedpause=0, fakepause=0;
extern char MasterVolume; // in os2.c
extern char nonframeupdate;

int INITIAL_FREQ = 44100;
char SOUND_QUALITY = 16;
unsigned long AUDIO_BUF_SIZE = 0;
char soundchangewarn = 1;

extern char vector_game;  // Is this a vector game?

unsigned short GAME_TO_TRY=0xffff; // ATETRIS 0x1a7

extern struct GameOptions options;

char **searchpath;
int numsearchdirs;

char keystatus[128] = {0}; // bitmap of keyboard status.
// I know no one out there has a 128 * 8 key keyboard, but so what..

// Globalize strings in order to save some space.
char inpdef[] = "MAME Input Options (default)";
char inpgamespec[] = "MAME Input Options - %8s";
char keyen[] = "Keyboard Enabled";
char firekeys[] = "Rapid Fire Keys";
char firerate[] = "Rapid Fire Rate";
char mouseen[] = "Mouse Enabled";
char mousegrab[] = "Mouse Grab Enabled";
char mouseflipx[] = "Mouse Flip X-Axis";
char mouseflipy[] = "Mouse Flip Y-Axis";
char windowparam[] = "Window parameters";
char joyen[] = "Joystick Enabled";
char mouseasjoystick[] = "Mouse functions as joystick";

char showwindowprintf = 1;
static char showcat = 1;

extern void *oldgames[];
extern char *newgames[];

#ifndef __IBMCPP__

// Apparently IBM VAC doesn't like this form of definition.
static MRESULT EXPENTRY (*OldListBoxProc) ( HWND, ULONG, MPARAM, MPARAM );
static MRESULT EXPENTRY (*OldFrameWinProc) ( HWND, ULONG, MPARAM, MPARAM );
static MRESULT EXPENTRY (*OldRadioProc) ( HWND, ULONG, MPARAM, MPARAM );

#else
PFNWP OldListBoxProc, OldFrameWinProc, OldRadioProc;
#endif

struct winp {
	unsigned short x, y, cx, cy;
};

#define FIND_FAILED 1048576

ULONG getIdx( char *name ) {
	ULONG cdriver, namelen;
	namelen = strlen(name);
	for ( cdriver=0; drivers[cdriver]; ++cdriver ) {
		if ( strlen( drivers[cdriver]->name ) == namelen &&
		  stricmp( drivers[cdriver]->name, name ) == 0 )
			return cdriver;
	}
	return FIND_FAILED;
}

void os2printf( const char *fmt, ... ) {
	va_list args;  
	char buffer[1024];
	char temp;

	va_start( args, fmt );
	vsprintf( (char *)buffer, fmt, args );
	temp = pauseonfocuschange;
	pauseonfocuschange = 0;

	// Strip off newline characters in case there are extras.
	while ( strlen( buffer ) && ( buffer[ strlen( buffer )-1 ] == 0xd || buffer[ strlen( buffer )-1 ] == 0xa) ) {
		buffer[ strlen( buffer )-1 ] = 0;
	}

	fprintf(fp,"%s\n",buffer);
	fflush(fp);
	if ( showwindowprintf ) {
		WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, (char *)buffer, "MAME for OS/2",
		    0, MB_OK | MB_INFORMATION | MB_MOVEABLE );
	}
	va_end( args );
	pauseonfocuschange = temp;
}

verify_printf_proc verifyprintf = (verify_printf_proc)&os2printf;

char remap_key( char scancode ) {
	switch ( scancode ) {
		case 102:		return 80;
		case 97:		return 72;
		case 99:		return 75;
		case 100:		return 77;
		case 80:		return 102;
		case 72:		return 97;
		case 75:		return 99;
		case 77:		return 100;
	}
	return scancode;
}

void EmulatorThread( void *junk ) {
	HAB ab;
	HMQ messq;
	unsigned short i;

	char buffer[64];
	ULONG temp, yesno;
	char olddeven = devicesenabled;

	devicesenabled = 0;

	scanlines = wantstochangescanmode;

	sprintf( buffer, inpgamespec, drivers[GAME_TO_TRY]->name );

	temp = 1; yesno = 1;
	PrfQueryProfileData( mameini, inpdef, mouseen, &yesno, &temp );
	temp = 1;
	PrfQueryProfileData( mameini, buffer, mouseen, &yesno, &temp );
	devicesenabled |= (yesno != 0)<<1;

	if ( yesno ) {
		temp = 1; yesno = 1;
		PrfQueryProfileData( mameini, inpdef, mousegrab, &yesno, &temp );
		temp = 1;
		PrfQueryProfileData( mameini, buffer, mousegrab, &yesno, &temp );
		mousecapture = (yesno != 0);

		temp = 1; yesno = 0;
		PrfQueryProfileData( mameini, inpdef, mouseflipx, &yesno, &temp );
		temp = 1;
		PrfQueryProfileData( mameini, buffer, mouseflipx, &yesno, &temp );
		mouseflip = (yesno != 0);

		temp = 1; yesno = 0;
		PrfQueryProfileData( mameini, inpdef, mouseflipy, &yesno, &temp );
		temp = 1;
		PrfQueryProfileData( mameini, buffer, mouseflipy, &yesno, &temp );
		mouseflip |= (yesno != 0)<<1;

		temp = 1; yesno = 0;
		PrfQueryProfileData( mameini, inpdef, mouseasjoystick, &yesno, &temp );
		temp = 1;
		PrfQueryProfileData( mameini, buffer, mouseasjoystick, &yesno, &temp );
		devicesenabled |= (yesno != 0)<<3;
	} else mousecapture = 0;

	temp = 1; yesno = 1;
	PrfQueryProfileData( mameini, inpdef, joyen, &yesno, &temp );
	temp = 1;
	PrfQueryProfileData( mameini, buffer, joyen, &yesno, &temp );

	devicesenabled |= (yesno != 0)<<2;
	if ( yesno && !(olddeven & 4) ) {
		JoystickInit(0);
		JoystickRange(-128, 128);
		if ( JoystickOn() ) devicesenabled &= 11;
	}

	ab = WinInitialize( 0 );
	messq = WinCreateMsgQueue( ab, 0 );

	memset( &options, 0, sizeof(struct GameOptions) );

	options.cheat = !nocheat;
	options.samplerate = INITIAL_FREQ;
	options.samplebits = SOUND_QUALITY;
	options.gui_host = 1;
	options.no_fm = 1;
	options.use_samples = 1;
	options.use_emulated_ym3812 = 1;
	options.use_artwork = 1;

	for (i=0; i<128; ++i) keystatus[i]=0;
	// unstick any stuck keys

	WinSetWindowText( WinWindowFromID( framewin, FID_TITLEBAR ), "M.A.M.E. for OS/2" );
	for (i=0; i<4; i++) osd_led_w( i, 0 );

	if ( run_game ( GAME_TO_TRY/*, &options*/ ) ) {
		// couldn't start the game
		WinSendMsg( clientwin, EmuStart, 0, 0 );
		WinSendMsg( clientwin, EmuDone, 0, 0 );
	}
	WinSetWindowText( WinWindowFromID( framewin, FID_TITLEBAR ), "M.A.M.E. for OS/2" );
	WinDestroyMsgQueue (messq) ;
	WinTerminate (ab);

	emu_thread_id = 0; thread_state = 3;
	WinPostMsg( framewin, EmuDone, 0, 0 );
	_endthread();
}

char profile_update_FUCKING_HACK = 0;

MRESULT EXPENTRY BitmapScaler(HWND win, ULONG message, MPARAM mp1, MPARAM mp2) {
	static HBITMAP hbm;
	HBITMAP hbmtemp;
	HPS hps;
	RECTL dest;
	int i;

	switch (message) {
		case WM_CREATE:
			hps = WinGetPS( win );
			if ( showcat ) {
				hbm = GpiLoadBitmap( hps, NULLHANDLE, 1, 0, 0 );
			} else {
				hbm = GpiLoadBitmap( hps, NULLHANDLE, 5, 0, 0 );
			}
			WinReleasePS( hps );
		break;
		case WM_PAINT:
			WinQueryWindowRect( win, &dest );
			hps = WinGetPS( win );
			WinDrawBitmap( hps, hbm, NULL, (PPOINTL)&dest, 0, 0, DBM_NORMAL | DBM_STRETCH );
			WinReleasePS( hps );
		break;
		case WM_DESTROY:
			GpiDeleteBitmap( hbm );
		break;
		case WM_USER:
			hps = WinGetPS( win );
			i = LONGFROMMP( mp1 );
			hbmtemp = GpiLoadBitmap( hps, NULLHANDLE, i, 0, 0 );
			if ( hbmtemp ) {
				GpiDeleteBitmap( hbm );
				hbm = hbmtemp;
			}
			WinReleasePS( hps );
			WinSendMsg( win, WM_PAINT, NULL, NULL );
		break;
	}
	return WinDefWindowProc( win, message, mp1, mp2 );
}

MRESULT EXPENTRY QuickListDlg(HWND win, ULONG message, MPARAM mp1, MPARAM mp2);

MRESULT EXPENTRY AddOneToListProc( HWND win, ULONG message, MPARAM mp1, MPARAM mp2 ) {
	int i;
	RECTL rectl, rectl2;
	char buf[10] = {0};
	switch (message) {
		case WM_INITDLG:
			WinQueryWindowRect( HWND_DESKTOP, &rectl );
			WinQueryWindowRect( win, &rectl2 );
			WinSetWindowPos( win, 0, (rectl.xRight -
			    (rectl2.xRight-rectl2.xLeft)-rectl.xLeft) / 2, 
			    (rectl.yTop-(rectl2.yTop-rectl2.yBottom)-rectl.yBottom) / 2,
			    0, 0, SWP_MOVE );
			options.gui_host = 1;
		break;
		case WM_COMMAND:
			if ( SHORT1FROMMP( mp1 ) == DID_OK ) {
				WinQueryDlgItemText( win, GameToAdd, 10, buf );
				for (i = 0; drivers[i]; i++)
					if ( strlen(drivers[i]->name) == strlen(buf) && strnicmp( buf, drivers[i]->name, strlen(drivers[i]->name) ) == 0) break;
				if ( drivers[i] != 0 ) {
					extern int gUnzipQuiet;
					gUnzipQuiet = 1;
					if ( VerifyRomSet( i, verifyprintf ) ) {
						os2printf( "I could not initialize this game." );
						gUnzipQuiet = 0;
						return 0; // Do not close this window after this error
					}
					gUnzipQuiet = 0;
					PrfWriteProfileData( mameini, "Quick List", buf, &i, 2 );
					os2printf( "%s was added successfully.", buf );
					fprintf( fp, "One game was added to the quick list.  Name: %s.\n", buf );
					WinEnableMenuItem( WinWindowFromID( framewin, FID_MENU ), OpenGame, FALSE );
					profile_update_FUCKING_HACK = 1;
					WinDlgBox( HWND_DESKTOP, HWND_DESKTOP, QuickListDlg, 0, QuickList, NULL );
					profile_update_FUCKING_HACK = 0;
					WinEnableMenuItem( WinWindowFromID( framewin, FID_MENU ), OpenGame, TRUE );
				} else {
					os2printf( "The game entered is not supported by this version of MAME." );
					return 0; // Do not close this window after this error
				}
			}
		break;
	}
	return WinDefDlgProc( win, message, mp1, mp2 );
}

#define SET_MAXMIN_VAL WM_USER
#define SET_CURRENT_VAL WM_USER+1
#define ADD_TO_CURRENT_VAL WM_USER+2

MRESULT EXPENTRY PercentProc(HWND win, ULONG message, MPARAM mp1, MPARAM mp2) {
	ULONG oldval, newval, minval, maxval;
	ULONG oldcol, newcol, t1, t2;
	RECTL rect, rect2, rect3;
	HPS hps;
	switch ( message ) {
		case WM_CREATE:
			WinSetWindowULong( win, 0, 1 ); // Prevent divide by zero
			WinSetWindowULong( win, 4, 0 );
			WinSetWindowULong( win, 8, 0 );
		break;
		case WM_PAINT:
			hps = WinBeginPaint( win, 0, &rect );
			newval = WinQueryWindowULong( win, 8 );
			maxval = WinQueryWindowULong( win, 0 );
			minval = WinQueryWindowULong( win, 4 );
			WinQueryWindowRect( win, &rect2 );
			if ( newval*100/(maxval-minval) < 25 ) newcol = CLR_DARKRED; else
			if ( newval*100/(maxval-minval) < 50 ) newcol = CLR_RED; else
			if ( newval*100/(maxval-minval) < 75 ) newcol = CLR_YELLOW; else
			    newcol = CLR_GREEN;

			newval = newval * (rect2.xRight - rect2.xLeft) / (maxval-minval);
			// Now in coordinates

			if ( rect.xLeft < newval ) {
				// Some overlap on active percent.  Draw part of the bar.
				rect3.xLeft = rect.xLeft;  rect3.xRight = newval;
				rect3.yBottom = rect.yBottom;  rect3.yTop = rect.yTop;
				WinFillRect( hps, &rect3, newcol );
			}

			if ( rect.xRight > newval ) {
				// Need to paint some blank space
				rect3.xLeft = newval+1; rect3.xRight = rect.xRight;
				rect3.yBottom = rect.yBottom;  rect3.yTop = rect.yTop;
				WinFillRect( hps, &rect3, CLR_NEUTRAL );
			}
		break;
		case SET_MAXMIN_VAL:
			WinSetWindowULong( win, 0, LONGFROMMP(mp1) );
			WinSetWindowULong( win, 4, LONGFROMMP(mp2) );
		break;
		case SET_CURRENT_VAL:
			WinSetWindowULong( win, 8, LONGFROMMP(mp1) );
			WinInvalidateRect( win, NULL, FALSE );
		break;
		case ADD_TO_CURRENT_VAL:
			oldval = WinQueryWindowULong( win, 8 );
			newval = oldval+LONGFROMMP(mp1);
			WinSetWindowULong( win, 8, newval );
			maxval = WinQueryWindowULong( win, 0 );
			minval = WinQueryWindowULong( win, 4 );
			if ( oldval*100/(maxval-minval) < 25 ) oldcol = CLR_DARKRED; else
			if ( oldval*100/(maxval-minval) < 50 ) oldcol = CLR_RED; else
			if ( oldval*100/(maxval-minval) < 75 ) oldcol = CLR_YELLOW; else
			    oldcol = CLR_GREEN;
			if ( newval*100/(maxval-minval) < 25 ) newcol = CLR_DARKRED; else
			if ( newval*100/(maxval-minval) < 50 ) newcol = CLR_RED; else
			if ( newval*100/(maxval-minval) < 75 ) newcol = CLR_YELLOW; else
			    newcol = CLR_GREEN;
			if ( oldcol != newcol ) {
				// Redraw the whole thing
				WinInvalidateRect( win, NULL, FALSE );
			} else {
				// Just add the new part
				WinQueryWindowRect( win, &rect );
				t1 = rect.xLeft; t2 = rect.xRight;
				rect.xLeft += (t2-t1)*oldval/(maxval-minval);
				rect.xRight =  t1+((t2-t1)*newval/(maxval-minval));
				WinInvalidateRect( win, &rect, FALSE );
			}
		break;
	}
	return WinDefWindowProc( win, message, mp1, mp2 );
}

MRESULT EXPENTRY AnimatedROM(HWND win, ULONG message, MPARAM mp1, MPARAM mp2) {
	static HPOINTER animation[9];
	static int current = 0;
	static HDC hdc;
	static HPS hps2;
	static HBITMAP bmp;
	static RECTL winsize;

	HPS hps = 0;
	int i;
	switch ( message ) {
    		case WM_CREATE: {
			DEVOPENSTRUC dop = { 0, "DISPLAY", NULL, 0, 0, 0, 0, 0, 0 };
			BITMAPINFOHEADER2 bmpi = { 0 };
			SIZEL size = { 0, 0 };

			hdc = DevOpenDC( WinQueryAnchorBlock( win ), OD_MEMORY, "*", 5, (PDEVOPENDATA)&dop, NULLHANDLE );
			hps2 = GpiCreatePS( WinQueryAnchorBlock( win ), hdc, &size, PU_PELS | GPIA_ASSOC );

			WinQueryWindowRect( win, &winsize );
			bmpi.cbFix = sizeof(BITMAPINFOHEADER2);
			bmpi.cx = winsize.xRight;
			bmpi.cy = winsize.yTop;
			bmpi.cPlanes = 1;
			bmpi.cBitCount = 8;
			bmpi.ulCompression = BCA_UNCOMP;
			bmp = GpiCreateBitmap( hps2, (PBITMAPINFOHEADER2)&bmpi, 0, 0, NULL );

			GpiSetBitmap( hps2, bmp );

			for (i=2; i<11; i++) animation[i-2] = WinLoadPointer( HWND_DESKTOP, 0, i );
			GpiCreateLogColorTable( hps2, 0, LCOLF_RGB, 0, 0, NULL );
			GpiSetColor( hps2, WinQuerySysColor(HWND_DESKTOP, SYSCLR_DIALOGBACKGROUND, 0) );
			WinStartTimer( WinQueryAnchorBlock( win ), win, 0, 100 );
		}
		break;
		case WM_CLOSE:
			WinStopTimer( WinQueryAnchorBlock( win ), win, 0 );
			GpiSetBitmap( hps2, 0 );
			GpiDeleteBitmap( bmp );
			GpiDestroyPS( hps2 );
			DevCloseDC( hdc );
			for (i=0; i<9; i++) WinDestroyPointer( animation[i] );
		break;
		case WM_TIMER: {
			POINTL ptl;
			ptl.x = 0; ptl.y = 0;
			GpiMove( hps2, &ptl );
			ptl.x = winsize.xRight; ptl.y = winsize.yTop;
			GpiBox( hps2, DRO_FILL, &ptl, 0, 0 );
			WinDrawPointer( hps2, 0, 0, animation[ current++ ], DP_NORMAL );
			WinInvalidateRect( win, NULL, TRUE );
			if ( current >= 9 ) current = 0;
		}
		break;
		case WM_PAINT: {
			POINTL pts[3];
			RECTL rectl = {0};
			HPS hps;

	                hps = WinBeginPaint( win, NULLHANDLE, &rectl );

			pts[0].x = rectl.xLeft; pts[0].y = rectl.yBottom;
			pts[1].x = rectl.xRight;  pts[1].y = rectl.yTop;
			pts[2].x = rectl.xLeft; pts[2].y = rectl.yBottom;

			GpiBitBlt( hps, hps2, 3, pts, ROP_SRCCOPY, BBO_IGNORE );
			WinEndPaint( hps );
		}
		break;
	}
	return WinDefWindowProc( win, message, mp1, mp2 );
}

MRESULT EXPENTRY QuarterPopper(HWND win, ULONG message, MPARAM mp1, MPARAM mp2) {
	static HPOINTER animation[8];
	static int current = 0;
	static HDC hdc;
	static HPS hps2;
	static HBITMAP bmp;
	static RECTL winsize;
	int i;
	switch ( message ) {
    		case WM_CREATE: {
			DEVOPENSTRUC dop = { 0, "DISPLAY", NULL, 0, 0, 0, 0, 0, 0 };
			BITMAPINFOHEADER2 bmpi = { 0 };
			SIZEL size = { 0, 0 };

			hdc = DevOpenDC( WinQueryAnchorBlock( win ), OD_MEMORY, "*", 5, (PDEVOPENDATA)&dop, NULLHANDLE );
			hps2 = GpiCreatePS( WinQueryAnchorBlock( win ), hdc, &size, PU_PELS | GPIA_ASSOC );

			WinQueryWindowRect( win, &winsize );
			bmpi.cbFix = sizeof(BITMAPINFOHEADER2);
			bmpi.cx = winsize.xRight;
			bmpi.cy = winsize.yTop;
			bmpi.cPlanes = 1;
			bmpi.cBitCount = 8;
			bmpi.ulCompression = BCA_UNCOMP;
			bmp = GpiCreateBitmap( hps2, (PBITMAPINFOHEADER2)&bmpi, 0, 0, NULL );

			GpiSetBitmap( hps2, bmp );

			for (i=13; i<=20; i++) animation[i-13] = WinLoadPointer( HWND_DESKTOP, 0, i );
			GpiCreateLogColorTable( hps2, 0, LCOLF_RGB, 0, 0, NULL );
			GpiSetColor( hps2, WinQuerySysColor(HWND_DESKTOP, SYSCLR_DIALOGBACKGROUND, 0) );
			WinStartTimer( WinQueryAnchorBlock( win ), win, 0, 75 );
		}
		break;
		case WM_CLOSE:
			WinStopTimer( WinQueryAnchorBlock( win ), win, 0 );
			GpiSetBitmap( hps2, 0 );
			GpiDeleteBitmap( bmp );
			GpiDestroyPS( hps2 );
			DevCloseDC( hdc );
			for (i=0; i<8; i++) WinDestroyPointer( animation[i] );
		break;
		case WM_TIMER: {
			POINTL ptl;
			ptl.x = 0; ptl.y = 0;
			GpiMove( hps2, &ptl );
			ptl.x = winsize.xRight; ptl.y = winsize.yTop;
			GpiBox( hps2, DRO_FILL, &ptl, 0, 0 );
			if ( current >= 8 ) { current++; } else
			    WinDrawPointer( hps2, 0, 0, animation[ current++ ], DP_NORMAL );
			WinInvalidateRect( win, NULL, TRUE );
			if ( current >= 16 ) current = 0;
		}
		break;
		case WM_PAINT: {
			POINTL pts[3];
			RECTL rectl = {0};
			HPS hps;

	                hps = WinBeginPaint( win, NULLHANDLE, &rectl );

			pts[0].x = rectl.xLeft; pts[0].y = rectl.yBottom;
			pts[1].x = rectl.xRight;  pts[1].y = rectl.yTop;
			pts[2].x = rectl.xLeft; pts[2].y = rectl.yBottom;

			GpiBitBlt( hps, hps2, 3, pts, ROP_SRCCOPY, BBO_IGNORE );
			WinEndPaint( hps );
		}
		break;
	}
	return WinDefWindowProc( win, message, mp1, mp2 );
}

HWND progresswin;
#define DirSearchDone		WM_USER
#define DirSearchError		WM_USER+1
#define NewDir			WM_USER+2
#define ProcessError		WM_USER+3
#define NewSliderVal		WM_USER+4
#define SliderBounds		WM_USER+5
#define VerifyDone		WM_USER+6

static ULONG dir_thread_id = 0;
static char quick_exit_dir_search = 0;
static char newgamesearchonly = 0;

#define APPROX_GAMES_SUPPORTED 2048

void DirSearchThread( void *junk ) {
	HDIR findhandle = HDIR_CREATE;
	FILEFINDBUF3 found;
	ULONG findnum = 1, rc;
	unsigned char *temp, present[APPROX_GAMES_SUPPORTED/8]; 
	unsigned long i, j, path, numpresent = 0;
	HAB ab;
	HMQ messq;
	char nametemp[1024];

	// Just in case we didn't do this yet.

	options.gui_host = 1;

	ab = WinInitialize( 0 );
	messq = WinCreateMsgQueue( ab, 0 );
	// Do this so I can do WinSendMsg instead of WinPostMsg so it updates
	// the status window in "real time".

	memset( present, 0, APPROX_GAMES_SUPPORTED/8 );

	if ( !newgamesearchonly ) {
		for ( path=0; path<numsearchdirs; path++ ) {
			sprintf( nametemp, "%s\\*", searchpath[path] );
			findhandle = HDIR_CREATE;
			findnum = 1;
			if ( DosFindFirst( nametemp, &findhandle, MUST_HAVE_DIRECTORY | FILE_ARCHIVED | FILE_DIRECTORY |
			    FILE_SYSTEM | FILE_HIDDEN | FILE_READONLY, &found, sizeof( FILEFINDBUF3 ), &findnum,
			    FIL_STANDARD ) ) {
				goto nexttry;
		        }

			do {
				findnum = 1;
				if ( found.achName[0] == '.' ) continue;
				temp = (char *)malloc( strlen(found.achName) + 1 );
				if ( temp ) {
					strcpy( temp, found.achName );
					WinSendMsg( progresswin, NewDir, MPFROMP(temp), 0 );
				}
				for (i = 0; drivers[i]; i++)
					if ( stricmp( found.achName, drivers[i]->name ) == 0) break;
				if ( drivers[i] != 0 && !(present[i/8]&(1<<(i%8))) ) {
					// We support this game, mark the ROMs for testing.
					present[i/8] |= 1<<(i%8);
					numpresent++;
				}
			} while ( !quick_exit_dir_search && DosFindNext( findhandle, &found, sizeof( FILEFINDBUF3 ), &findnum ) == 0 );

nexttry:
			DosFindClose( findhandle );
			if ( quick_exit_dir_search ) { goto outthread; }
			findnum = 1;
			findhandle = HDIR_CREATE;
			sprintf( nametemp, "%s\\*.zip", searchpath[path] );
			if ( DosFindFirst( nametemp, &findhandle, FILE_ARCHIVED | FILE_SYSTEM | FILE_HIDDEN | FILE_READONLY,
			    &found, sizeof( FILEFINDBUF3 ), &findnum, FIL_STANDARD ) ) {
				goto nexttry2;
			}
			do {
				findnum = 1;
				if ( found.achName[0] == '.' ) continue;
				temp = (char *)malloc( strlen(found.achName) + 1 );
				if ( temp ) {
					strcpy( temp, found.achName );
					WinSendMsg( progresswin, NewDir, MPFROMP(temp), 0 );
				}
				for (i = 0; drivers[i]; i++)
					if ( strnicmp( found.achName, drivers[i]->name, strlen(drivers[i]->name) ) == 0 && strlen(drivers[i]->name) == (strlen(found.achName) - 4) ) break;
				if ( drivers[i] != 0 && !(present[i/8]&(1<<(i%8))) ) {
					// We support this game, mark the ROMs for testing.
					present[i/8] |= 1<<(i%8);
					numpresent++;
				}
			} while ( !quick_exit_dir_search && DosFindNext( findhandle, &found, sizeof( FILEFINDBUF3 ), &findnum ) == 0 );

nexttry2:
			DosFindClose( findhandle );
			findhandle = HDIR_CREATE;
			findnum = 1;
			if ( quick_exit_dir_search ) { goto outthread; }
		}

		WinSendMsg( progresswin, DirSearchDone, 0, 0 );

		if ( numpresent == 0 ) {
			WinSendMsg( progresswin, ProcessError, MPFROMP( "Could not find any games!" ),
			    MPFROMP( "Error building quick list..." ) );
			goto outthread;
		}
	} else {
		for ( i=0; newgames[i]; ++i ) {
			findhandle = getIdx( newgames[i] );
			if ( findhandle != FIND_FAILED ) {
				temp = (char *)malloc( strlen(drivers[findhandle]->name) + 1 );
				if ( temp ) {
					strcpy( temp, drivers[findhandle]->name );
					WinSendMsg( progresswin, NewDir, MPFROMP(temp), 0 );
				}
				present[findhandle/8] |= 1<<(findhandle%8);
				numpresent++;
			}
		}

		WinSendMsg( progresswin, DirSearchDone, 0, 0 );
	}

	i=0;
	{
	unsigned long numfailed = 0;
	char tempbuf[256];
	WinSendMsg( progresswin, SliderBounds, 0, MPFROMLONG(numpresent) );
	showwindowprintf = 0;
	while ( !quick_exit_dir_search && drivers[i] != 0 ) {
		if ( present[i/8] & (1<<(i%8)) ) {
			temp = (char *)malloc( strlen(drivers[i]->name) + 1 );
			if ( temp ) {
				strcpy( temp, drivers[i]->name );
				WinSendMsg( progresswin, NewDir, MPFROMP(temp), 0 );
			}
			fprintf( fp, "Checking %s... ", drivers[i]->name );
			if ( (rc = VerifyRomSet( i, verifyprintf )) ) {
				char *reason;
				// init failed.  This game is not available.
				present[i/8] &= ~(1<<(i%8));
				switch ( rc ) {
					case CORRECT:   reason = "I dunno?!"; break;
					case INCORRECT: reason = "Set was incorrect."; break;
					case NOTFOUND:  reason = "Part or all of the set was not found."; break;
					case CLONE_NOTFOUND: reason = "Part or all of clone was not found."; break;
				}
				fprintf( fp, "failed.  Reason: %s\n", reason );
				numfailed++;
			} else fprintf( fp, "passed!\n" );
			WinSendMsg( progresswin, NewSliderVal, MPFROMLONG(1), 0 );
		}
		++i;
	}
	if ( quick_exit_dir_search ) { goto outthread; }
	showwindowprintf = 1;
	if ( numfailed && !newgamesearchonly ) {
		sprintf( tempbuf, "%d games did not pass validity check and were not added to the quick list.  See DEBUG.LOG for details.", numfailed );
		WinSendMsg( progresswin, ProcessError, MPFROMP( tempbuf ), MPFROMP( "Quick List Warning" ) );
	}
	WinSendMsg( progresswin, VerifyDone, 0, 0 );

	if ( !newgamesearchonly ) {
		PrfWriteProfileData( mameini, "Quick List", NULL, NULL, 0 );
	}
	// Remove old quick list entries

	WinSendMsg( progresswin, SliderBounds, 0, MPFROMLONG(numpresent-numfailed) );

	} // Pop those unneeded locals off the stack.  Sorry this is so sloppy.

	i=0;

	while ( drivers[i] != 0 ) {
		if ( present[i/8] & (1<<(i%8)) ) {
			temp = (char *)malloc( 9 );
			for (j=0; j<9; j++) temp[j]=0;
			strcpy( temp, drivers[i]->name );
			PrfWriteProfileData( mameini, "Quick List", temp, &i, 2 );
			WinSendMsg( progresswin, NewDir, MPFROMP(temp), 0 );
			WinSendMsg( progresswin, NewSliderVal, MPFROMLONG(1), 0 );
		}
		++i;
	}

outthread:
	WinSendMsg( progresswin, WM_CLOSE, 0, 0 );
	WinDestroyMsgQueue (messq) ;
	WinTerminate (ab);
	dir_thread_id = 0;
   	_endthread();
}

int allowradioswitch = 0;

MRESULT EXPENTRY NoInputRadioButton(HWND win, ULONG message, MPARAM mp1, MPARAM mp2) {
	if ( message == WM_PAINT || allowradioswitch ) {
		return (*OldRadioProc) (win,message,mp1,mp2);
	} else return 0;
}

MRESULT EXPENTRY ProgressDlgProc(HWND win, ULONG message, MPARAM mp1, MPARAM mp2) {
	RECTL rectl, rectl2;
	switch ( message ) {
    		case WM_INITDLG:
			WinQueryWindowRect( HWND_DESKTOP, &rectl );
			WinQueryWindowRect( win, &rectl2 );
			WinSetWindowPos( win, 0, (rectl.xRight -
			    (rectl2.xRight-rectl2.xLeft)-rectl.xLeft) / 2, 
			    (rectl.yTop-(rectl2.yTop-rectl2.yBottom)-rectl.yBottom) / 2,
			    0, 0, SWP_MOVE );
			WinCheckButton( win, ReadingDirList, TRUE );
			progresswin = win;
			quick_exit_dir_search = 0;
			dir_thread_id = _beginthread( DirSearchThread, NULL, 65536, NULL );
			allowradioswitch = 0;
			OldRadioProc = WinSubclassWindow( WinWindowFromID( win, ReadingDirList ),
			  NoInputRadioButton );
			WinSubclassWindow( WinWindowFromID( win, VerifyingROMs), NoInputRadioButton );
			WinSubclassWindow( WinWindowFromID( win, RecordingInfo), NoInputRadioButton );
//			fprintf(fp, "INIT: tid=%ld\n", dir_thread_id ); fflush(fp);
		break;

		case WM_CLOSE:
//			fprintf(fp, "WMCLOSE: tid=%ld\n", dir_thread_id ); fflush(fp);
			if ( dir_thread_id != 0 ) { quick_exit_dir_search = 1; }
		break;

		case WM_COMMAND:
			if ( SHORT1FROMMP( mp1 ) == DID_CANCEL ) {
//				fprintf(fp, "DIDCANCEL: tid=%ld\n", dir_thread_id ); fflush(fp);
				if ( dir_thread_id != 0 ) { quick_exit_dir_search = 1; }
			}
		break;

		case DirSearchDone:
			allowradioswitch = 1;
			WinCheckButton( win, VerifyingROMs, TRUE );
			allowradioswitch = 0;
		break;

		case VerifyDone:
			allowradioswitch = 1;
			WinCheckButton( win, RecordingInfo, TRUE );
			allowradioswitch = 0;
			WinSendDlgItemMsg( progresswin, PercentBar, SET_CURRENT_VAL, 0, 0 );
		break;

		case NewDir:
			WinSetDlgItemText( win, CurrentDirText, (const char *)PVOIDFROMMP(mp1) );
			free( PVOIDFROMMP(mp1) );
		break;

		case ProcessError:
			WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, (char *)PVOIDFROMMP(mp1), 
			    (char *)PVOIDFROMMP(mp2), 0, MB_OK | MB_INFORMATION | MB_MOVEABLE | MB_SYSTEMMODAL );
		break;

		case NewSliderVal:
			WinSendDlgItemMsg( win, PercentBar, ADD_TO_CURRENT_VAL, mp1, 0 );
		break;

		case SliderBounds:
			WinSendDlgItemMsg( win, PercentBar, SET_MAXMIN_VAL, mp2, mp1 );
		break;
	}
	return WinDefDlgProc( win, message, mp1, mp2 );
}

static char splashscreenstays = 0;

MRESULT EXPENTRY SplashScreenProc(HWND win, ULONG message, MPARAM mp1, MPARAM mp2) {
	RECTL rectl, rectl2;
	switch ( message ) {
		case WM_INITDLG:
			WinQueryWindowRect( HWND_DESKTOP, &rectl );
			WinQueryWindowRect( win, &rectl2 );
			WinSetWindowPos( win, 0, (rectl.xRight -
			    (rectl2.xRight-rectl2.xLeft)-rectl.xLeft) / 2, 
			    (rectl.yTop-(rectl2.yTop-rectl2.yBottom)-rectl.yBottom) / 2,
			    0, 0, SWP_MOVE );
		break;
		case WM_CHAR:
			if ( !splashscreenstays ) {
	  			WinDismissDlg( win, 0 );
			} else return 0;
		break;
	}
	return WinDefDlgProc( win, message, mp1, mp2 );
}

MRESULT EXPENTRY SplashScreenScroller( HWND win, ULONG message, MPARAM mp1, MPARAM mp2) {
	static HPS hps2=0;
	static HBITMAP bmp=0, foros2bmp=0, verbmp=0, portedbmp=0;
	static HDC hdc;
	static SWP swp;
	static RECTL winrect;
	static int sequence;
	static int sequence2;
	static int offsetx, offsetx2, offsetx3, offsety;
	HPS hps;
	POINTL pts[3];
	RECTL rect;

	switch (message) {
		case WM_CREATE: {
			BITMAPINFOHEADER bmpinf = { 0 };
			DEVOPENSTRUC dop = { 0, "DISPLAY", NULL, 0, 0, 0, 0, 0, 0 };
			BITMAPINFOHEADER2 bmpi = { 0 };
			SIZEL size = { 0, 0 };

			hdc = DevOpenDC( WinQueryAnchorBlock( win ), OD_MEMORY, "*", 5, (PDEVOPENDATA)&dop, NULLHANDLE );
//			fprintf( fp, "SS: hdc = %ld\n", hdc );
			hps2 = GpiCreatePS( WinQueryAnchorBlock( win ), hdc, &size, PU_PELS | GPIT_MICRO | GPIA_ASSOC );
//			fprintf( fp, "SS: hps2 = %ld\n", hps2 );

			WinQueryWindowPos( win, &swp );
			winrect.xLeft = winrect.yBottom = 0;
			winrect.xRight = swp.cx;
			winrect.yTop = swp.cy;

			bmpi.cbFix = sizeof(BITMAPINFOHEADER2);
			bmpi.cx = swp.cx;
			bmpi.cy = swp.cy;
			bmpi.cPlanes = 1;
			bmpi.cBitCount = 8;
			bmpi.ulCompression = BCA_UNCOMP;
			bmp = GpiCreateBitmap( hps2, (PBITMAPINFOHEADER2)&bmpi, 0, 0, NULL );

			GpiSetBitmap( hps2, bmp );
			foros2bmp = GpiLoadBitmap( hps2, NULLHANDLE, 2, 0, 0 );
			verbmp = GpiLoadBitmap( hps2, NULLHANDLE, 3, 0, 0 );
			portedbmp = GpiLoadBitmap( hps2, NULLHANDLE, 4, 0, 0 );

			bmpinf.cbFix = sizeof( BITMAPINFOHEADER );
			GpiQueryBitmapParameters( foros2bmp, &bmpinf );
			offsetx =  (swp.cx >> 1)-(bmpinf.cx >> 1);
			offsety =  0;
			memset( &bmpinf, 0, sizeof( BITMAPINFOHEADER ) );
			bmpinf.cbFix = sizeof( BITMAPINFOHEADER );
			GpiQueryBitmapParameters( verbmp, &bmpinf );
			offsetx2 = (swp.cx >> 1)-(bmpinf.cx >> 1);
			memset( &bmpinf, 0, sizeof( BITMAPINFOHEADER ) );
			bmpinf.cbFix = sizeof( BITMAPINFOHEADER );
			GpiQueryBitmapParameters( portedbmp, &bmpinf );
			offsetx3 = (swp.cx >> 1)-(bmpinf.cx >> 1);

			GpiSetColor( hps2, CLR_BLACK );
			GpiSetBackColor( hps2, CLR_BLACK );

			sequence = 0; sequence2 = 0;
			if ( !showcat ) sequence2 = 7;
			WinStartTimer( WinQueryAnchorBlock(win), win, 0, 50 );
		}
		break;
		case WM_PAINT:
			pts[0].x = 0; pts[0].y = 0;
			pts[1].x = swp.cx;  pts[1].y = swp.cy;
			pts[2].x = 0; pts[2].y = 0;

	                hps = WinBeginPaint( win, NULLHANDLE, NULL );
			if ( hps && hps2 ) GpiBitBlt( hps, hps2, 3, pts, ROP_SRCCOPY, BBO_IGNORE );
			WinEndPaint( hps );
		break;
		case WM_TIMER:
			switch (sequence2) {
				case 0:
					splashscreenstays = 1;
					GpiCreateLogColorTable( hps2, 0, LCOLF_RGB,
					  0, 0, 0 );
					WinStopTimer( WinQueryAnchorBlock(win), win, 0 );
					WinDrawText( hps2, 25,
					  "In Loving Memory of Katie",
					  &winrect, 0xffffff, 0,
					  DT_CENTER | DT_VCENTER | DT_ERASERECT );
					WinInvalidateRect( win, NULL, TRUE );
					sequence2++; sequence = 0xff;
					WinStartTimer( WinQueryAnchorBlock(win), win, 0, 2000 );
				break;
				case 1:
					WinStopTimer( WinQueryAnchorBlock(win), win, 0 );
					WinStartTimer( WinQueryAnchorBlock(win), win, 0, 25 );
					sequence2++;
				case 2:
					WinDrawText( hps2, 25,
					  "In Loving Memory of Katie",
					  &winrect, sequence | (sequence<<8) |
					  (sequence<<16), 0,
					  DT_CENTER | DT_VCENTER | DT_ERASERECT );
					WinInvalidateRect( win, NULL, TRUE );
					sequence--;
					if ( sequence == 0 ) sequence2++;
				break;
				case 3:
					WinStopTimer( WinQueryAnchorBlock(win), win, 0 );
					WinDrawText( hps2, 28,
					  "June 1986 - October 16, 1999",
					  &winrect, 0xffffff, 0,
					  DT_CENTER | DT_VCENTER | DT_ERASERECT );
					WinInvalidateRect( win, NULL, TRUE );
					sequence2++; sequence = 0xff;
					WinStartTimer( WinQueryAnchorBlock(win), win, 0, 2000 );
				break;
				case 4:
					WinStopTimer( WinQueryAnchorBlock(win), win, 0 );
					WinStartTimer( WinQueryAnchorBlock(win), win, 0, 25 );
					sequence2++;
				case 5:
					WinDrawText( hps2, 28,
					  "June 1986 - October 16, 1999",
					  &winrect, sequence | (sequence<<8) |
					  (sequence<<16), 0,
					  DT_CENTER | DT_VCENTER | DT_ERASERECT );
					WinInvalidateRect( win, NULL, TRUE );
					sequence--;
					if ( sequence == 0 ) sequence2++;
				break;
				case 6:
					WinStopTimer( WinQueryAnchorBlock(win), win, 0 );
					WinSendMsg( WinWindowFromID( 
					  WinQueryWindow(win, QW_PARENT), 101 ),
					  WM_USER, MPFROMLONG(5), 0 );
					sequence2++;
					WinStartTimer( WinQueryAnchorBlock(win), win, 0, 50 );
				break;
				case 7:
					splashscreenstays = 0;
					pts[0].x = 0; pts[0].y = 0;
					pts[1].x = swp.cx;  pts[1].y = swp.cy;
					rect.xLeft = swp.cx - sequence;
					rect.xRight = swp.cx;
					rect.yBottom = offsety;
					rect.yTop = swp.cy;
					GpiMove(hps2, &pts[0] );
					GpiBox(hps2, DRO_OUTLINEFILL, &(pts[1]), 0, 0);
					WinDrawBitmap( hps2, foros2bmp, NULL, (PPOINTL)&rect, 0, 0,
					    DBM_STRETCH );
					sequence+=10;

					WinInvalidateRect( win, NULL, TRUE );

					if ( sequence > swp.cx-offsetx ) {
							sequence2++;
							sequence = 0;
					}
				break;
				case 8:
					pts[0].x = 0; pts[0].y = 0;
					pts[1].x = swp.cx;  pts[1].y = swp.cy;
					rect.xLeft = offsetx;
					rect.xRight = swp.cx-sequence;
					rect.yBottom = offsety;
					rect.yTop = swp.cy;
					GpiMove(hps2, &pts[0] );
					GpiBox(hps2, DRO_OUTLINEFILL, &(pts[1]), 0, 0);
					WinDrawBitmap( hps2, foros2bmp, NULL, (PPOINTL)&rect, 0, 0,
					    DBM_STRETCH );
					sequence+=10;

					WinInvalidateRect( win, NULL, TRUE );

					if ( sequence > offsetx) {
							sequence2++;
							sequence = 0;
					}
					break;
				case 9:
					WinStopTimer( WinQueryAnchorBlock(win), win, 0 );
					pts[0].x = 0; pts[0].y = 0;
					pts[1].x = swp.cx;  pts[1].y = swp.cy;
					rect.xLeft = offsetx;
					rect.xRight = swp.cx-offsetx;
					rect.yBottom = offsety;
					rect.yTop = swp.cy;
					GpiMove(hps2, &pts[0] );
					GpiBox(hps2, DRO_OUTLINEFILL, &(pts[1]), 0, 0);
					WinDrawBitmap( hps2, foros2bmp, NULL, (PPOINTL)&rect, 0, 0,
					    DBM_STRETCH );
					WinInvalidateRect( win, NULL, TRUE );
					WinStartTimer( WinQueryAnchorBlock(win), win, 0, 1000 );
					sequence2++;
				break;
				case 10:
					WinStopTimer( WinQueryAnchorBlock(win), win, 0 );
					WinStartTimer( WinQueryAnchorBlock(win), win, 0, 50 );
					sequence2++;
					// Intentional fall-through to case 3
				case 11:
					pts[0].x = 0; pts[0].y = 0;
					pts[1].x = swp.cx;  pts[1].y = swp.cy;
					GpiMove(hps2, &pts[0] );
					GpiBox(hps2, DRO_OUTLINEFILL, &(pts[1]), 0, 0);
					rect.xLeft = offsetx;
					rect.xRight = swp.cx-offsetx;
					rect.yBottom = offsety + sequence;
					rect.yTop = swp.cy;
					WinDrawBitmap( hps2, foros2bmp, NULL, (PPOINTL)&rect, 0, 0,
					    DBM_STRETCH );
					rect.xLeft = offsetx2;
					rect.xRight = swp.cx-offsetx2;
					rect.yBottom = 0;
					rect.yTop = sequence;
					WinDrawBitmap( hps2, verbmp, NULL, (PPOINTL)&rect, 0, 0,
					    DBM_STRETCH );

					WinInvalidateRect( win, NULL, TRUE );
					sequence++;

					if ( sequence > swp.cy ) {
							sequence2++;
							sequence = 0;
						}
					break;
				break;
				case 12:
					WinStopTimer( WinQueryAnchorBlock(win), win, 0 );
					WinStartTimer( WinQueryAnchorBlock(win), win, 0, 1000 );
					sequence2++;
				break;
				case 13:
					WinStopTimer( WinQueryAnchorBlock(win), win, 0 );
					WinStartTimer( WinQueryAnchorBlock(win), win, 0, 50 );
					sequence2++;
					// Intentional fall-through to case 5
				case 14:
					pts[0].x = 0; pts[0].y = 0;
					pts[1].x = swp.cx;  pts[1].y = swp.cy;
					GpiMove(hps2, &pts[0] );
					GpiBox(hps2, DRO_OUTLINEFILL, &(pts[1]), 0, 0);
					rect.xLeft = offsetx2;
					rect.xRight = swp.cx-offsetx2;
					rect.yBottom = offsety + sequence;
					rect.yTop = swp.cy;
					WinDrawBitmap( hps2, verbmp, NULL, (PPOINTL)&rect, 0, 0,
					    DBM_STRETCH );
					rect.xLeft = offsetx3;
					rect.xRight = swp.cx-offsetx3;
					rect.yBottom = 0;
					rect.yTop = sequence;
					WinDrawBitmap( hps2, portedbmp, NULL, (PPOINTL)&rect, 0, 0,
					    DBM_STRETCH );

					WinInvalidateRect( win, NULL, TRUE );
					sequence++;

					if ( sequence > swp.cy ) {
							sequence2++;
							sequence = 0;
						}
					break;
				break;
				case 15:
					WinStopTimer( WinQueryAnchorBlock(win), win, 0 );
					WinStartTimer( WinQueryAnchorBlock(win), win, 0, 1000 );
					sequence2++;
				break;
				case 16:
					WinStopTimer( WinQueryAnchorBlock(win), win, 0 );
					WinSendMsg( WinQueryWindow( win, QW_PARENT ), WM_CHAR, 0, 0 );
				break;
			}
		break;
		case WM_CLOSE:
			GpiSetBitmap( hps2, 0 );
			GpiDeleteBitmap( bmp );
			GpiDeleteBitmap( foros2bmp );
			GpiDeleteBitmap( verbmp );
			GpiDeleteBitmap( portedbmp );
			GpiDestroyPS( hps2 );
			DevCloseDC( hdc );
	}
	return WinDefWindowProc( win, message, mp1, mp2 );
}

#define RECAL WM_USER

MRESULT EXPENTRY Calibrator(HWND win, ULONG message, MPARAM mp1, MPARAM mp2) {
	static ULONG w,h, lx, ly;
	static char olddeven = 1;
	HPS hps;
	RECTL rectl;
	POINTL p,p2;
	switch ( message ) {
		case WM_CREATE:
			if ( joydetected ) {
				WinStartTimer( WinQueryAnchorBlock(win), win, 0, 100 );
				if ( !(devicesenabled & 4) ) {
					olddeven = 0;
					JoystickInit(0);
					JoystickRange(-128, 128);
					if ( JoystickOn() ) devicesenabled &= 11; else devicesenabled |= 4;
				} else olddeven = 1;
				WinQueryWindowRect( win, &rectl );
				w = rectl.xRight - rectl.xLeft;
				h = rectl.yTop - rectl.yBottom;
			}
		break;
		case WM_SIZE:
			w = SHORT1FROMMP( mp2 );
			h = SHORT2FROMMP( mp2 );
		break;
		case WM_PAINT:
			hps = WinBeginPaint( win, 0, &rectl );
			GpiErase( hps );
			WinEndPaint( hps );
		break;
		case WM_TIMER:
			if ( joydetected ) {
				JoystickValues(&jstick);
				p.x = jstick.Joy1X;
				p.y = jstick.Joy1Y;
				hps = WinGetPS( win );
				if ( ((128+p.x)*w/256) != lx || ((128-p.y)*h/256) != ly ) {
					GpiErase( hps );
				}
				ly = (128-p.y)*h/256;
				p2.x = 0; p2.y = ly;
				GpiMove( hps, &p2 );
				p2.x = w; p2.y = ly;
				GpiLine( hps, &p2 );
				lx = (128+p.x)*w/256;
				p2.x = lx; p2.y = 0;
				GpiMove( hps, &p2 );
				p2.x = lx; p2.y = h;
				GpiLine( hps, &p2 );
				WinReleasePS( win );
			}
		break;
		case WM_CLOSE:
			if ( joydetected ) WinStopTimer( WinQueryAnchorBlock(win), win, 0 );
			if ( !olddeven ) {
				JoystickSaveCalibration();
				JoystickOff();
				devicesenabled &= 11;
			}
		break;
		case RECAL:
			if ( joydetected && (devicesenabled & 4) ) {
				JoystickOff();
				JoystickInit(1);  // Don't load cal data
				JoystickRange(-128, 128);
				if ( JoystickOn() ) devicesenabled &= 11;
			}
		break;
	}
	return WinDefWindowProc( win, message, mp1, mp2 );
}

MRESULT EXPENTRY InputOptionsProc(HWND win, ULONG message, MPARAM mp1, MPARAM mp2) {
	ULONG yesno = 1;
	static struct InputPort *entry[40]; // Leave this initialized for the DID_OK command

	switch ( message ) {
		case WM_INITDLG:
		{
			char buffer[64];
			ULONG temp = 1,i;
			struct InputPort *in;
			int total;
			int *rapidkeys;
			RECTL rectl, rectl2;

			WinQueryWindowRect( HWND_DESKTOP, &rectl );
			WinQueryWindowRect( win, &rectl2 );
			WinSetWindowPos( win, 0, (rectl.xRight -
			    (rectl2.xRight-rectl2.xLeft)-rectl.xLeft) / 2, 
			    (rectl.yTop-(rectl2.yTop-rectl2.yBottom)-rectl.yBottom) / 2,
			    0, 0, SWP_MOVE );

			if ( GAME_TO_TRY != 0xffff && Machine->input_ports != NULL ) {
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

				for (i = 0;i < total;i++) {
//					sprintf( buffer, "%s  (%s)", osd_key_name(input_port_key(entry[i])), input_port_name(entry[i]) );
					sprintf( buffer, "%s", input_port_name(entry[i]) );
					WinSendDlgItemMsg( win, RapidFireList, LM_INSERTITEM, 
					    MPFROMSHORT( LIT_END ), MPFROMP(buffer));
				}

				sprintf( buffer, inpgamespec, drivers[GAME_TO_TRY]->name );
				temp = sizeof( int );
				i = 0;
				PrfQueryProfileSize( mameini, buffer, firekeys, &i );
				if ( i ) {
					rapidkeys = (int *)malloc( i );
					temp = i;
					PrfQueryProfileData( mameini, buffer, firekeys, rapidkeys, &temp );
					i /= sizeof(int);
					while ( i-- > 0 ) {
						WinSendDlgItemMsg( win, RapidFireList, LM_SELECTITEM, 
						    MPFROMSHORT( rapidkeys[i] ), MPFROMSHORT( TRUE ) );
					}
					free( rapidkeys );
				}
			}

			if ( GAME_TO_TRY != 0xffff ) {
				temp = 1;
				PrfQueryProfileData( mameini, inpdef, keyen, &yesno, &temp );
				temp = 1;
			} else {
				strcpy( buffer, inpdef );
			}
			PrfQueryProfileData( mameini, buffer, keyen, &yesno, &temp );
			// Search defaults first, replace with game specs if available.  Have hard-wired
			// fallback if neither exist.
			WinCheckButton( win, KeyboardEnable, yesno );

			WinEnableControl( win, RapidFireRate, yesno );
			WinEnableControl( win, RapidFireList, yesno );

			WinSendDlgItemMsg( win, RapidFireRate, SPBM_OVERRIDESETLIMITS,
			     MPFROMLONG( 100 ), MPFROMLONG( 1 ) );

			temp = 1; yesno = 10;
			PrfQueryProfileData( mameini, inpdef, firerate, &yesno, &temp );
			temp = 1;
			PrfQueryProfileData( mameini, buffer, firerate, &yesno, &temp );
			WinSendDlgItemMsg( win, RapidFireRate, SPBM_SETCURRENTVALUE,
			     MPFROMLONG( yesno ), 0 );

			temp = 1; yesno = 1;
			PrfQueryProfileData( mameini, inpdef, mouseen, &yesno, &temp );
			temp = 1;
			PrfQueryProfileData( mameini, buffer, mouseen, &yesno, &temp );
			WinCheckButton( win, MouseEnable, yesno );

			WinEnableControl( win, MouseGrab, yesno );
			WinEnableControl( win, FlipMouseX, yesno );
			WinEnableControl( win, FlipMouseY, yesno );

			temp = 1; yesno = 1;
			PrfQueryProfileData( mameini, inpdef, mousegrab, &yesno, &temp );
			temp = 1;
			PrfQueryProfileData( mameini, buffer, mousegrab, &yesno, &temp );
			WinCheckButton( win, MouseGrab, yesno );

			temp = 1; yesno = 0;
			PrfQueryProfileData( mameini, inpdef, mouseflipx, &yesno, &temp );
			temp = 1;
			PrfQueryProfileData( mameini, buffer, mouseflipx, &yesno, &temp );
			WinCheckButton( win, FlipMouseX, yesno );

			temp = 1; yesno = 0;
			PrfQueryProfileData( mameini, inpdef, mouseflipy, &yesno, &temp );
			temp = 1;
			PrfQueryProfileData( mameini, buffer, mouseflipy, &yesno, &temp );
			WinCheckButton( win, FlipMouseY, yesno );

			temp = 1; yesno = 0;
			PrfQueryProfileData( mameini, inpdef, mouseasjoystick, &yesno, &temp );
			temp = 1;
			PrfQueryProfileData( mameini, buffer, mouseasjoystick, &yesno, &temp );
			WinCheckButton( win, MouseAsJoystick, yesno );

			if ( joydetected ) {
				WinEnableControl( win, JoyEnable, TRUE );

				temp = 1; yesno = 1;
				PrfQueryProfileData( mameini, inpdef, joyen, &yesno, &temp );
				temp = 1;
				PrfQueryProfileData( mameini, buffer, joyen, &yesno, &temp );
				WinCheckButton( win, JoyEnable, yesno );
				WinEnableControl( win, Calibrate, yesno );
			}
		}
		break;
		case WM_CONTROL:
			switch ( SHORT1FROMMP( mp1 ) ) {
				case KeyboardEnable:
					yesno = WinQueryButtonCheckstate( win, KeyboardEnable );
					WinEnableControl( win, RapidFireRate, yesno );
					WinEnableControl( win, RapidFireList, yesno );
				break;
				case MouseEnable:
					yesno = WinQueryButtonCheckstate( win, MouseEnable );
					if ( yesno == 0 ) {
						WinCheckButton( win, MouseGrab, 0 );
						WinCheckButton( win, FlipMouseX, 0 );
						WinCheckButton( win, FlipMouseY, 0 );
						WinCheckButton( win, MouseAsJoystick, 0 );
					}
					WinEnableControl( win, MouseGrab, yesno );
					WinEnableControl( win, FlipMouseX, yesno );
					WinEnableControl( win, FlipMouseY, yesno );
					WinEnableControl( win, MouseAsJoystick, yesno );
				break;
				case JoyEnable:
					yesno = WinQueryButtonCheckstate( win, JoyEnable );
					WinEnableControl( win, Calibrate, yesno );
				break;
			}
		break;
		case WM_COMMAND:
			if ( SHORT1FROMMP( mp1 ) == Calibrate ) {
				WinSendDlgItemMsg( win, 120, RECAL, 0, 0 );
				return 0;
			} else
			if ( SHORT1FROMMP( mp1 ) == DID_OK ) {
				char buffer[64];
				USHORT cindex = LIT_FIRST;
				int selected[40];
				int cselected = 0, prevdev;

				prevdev = devicesenabled;

				if ( GAME_TO_TRY != 0xffff ) {
					sprintf( buffer, inpgamespec, drivers[GAME_TO_TRY]->name );
				} else {
					strcpy( buffer, inpdef );
				}

				while ( (cindex = LONGFROMMR( WinSendDlgItemMsg( win, RapidFireList, LM_QUERYSELECTION,
				    MPFROM2SHORT( cindex, 0 ), MPFROMLONG( 0 )))) != 0xffff ) {
					selected[ cselected++ ] = cindex;
				}

				if ( cselected ) {
					PrfWriteProfileData( mameini, buffer, firekeys, selected, cselected*sizeof(int) );
				}

				if ( rapidkeys ) {
					free( rapidkeys );
				}

				rapidkeys = (int *)malloc( cselected * sizeof(int) );
				for ( yesno=0; yesno<cselected; yesno++ ) {
					extern int MapMAMEKeyToOSD( int MAMEkey );
					rapidkeys[yesno] = MapMAMEKeyToOSD(input_port_key(entry[selected[yesno]])); // update current game
				}
				totalrapidkeys=cselected;

				yesno = WinQueryButtonCheckstate( win, KeyboardEnable );
				PrfWriteProfileData( mameini, buffer, keyen, &yesno, 1 );
				devicesenabled = (yesno != 0 );

				WinSendDlgItemMsg( win, RapidFireRate, SPBM_QUERYVALUE,
				    MPFROMP( &yesno ), MPFROM2SHORT( 0, SPBQ_ALWAYSUPDATE ) );
				PrfWriteProfileData( mameini, buffer, firerate, &yesno, 1 );
				rapidrate = yesno; // update currently running game

				yesno = WinQueryButtonCheckstate( win, MouseEnable );
				PrfWriteProfileData( mameini, buffer, mouseen, &yesno, 1 );
				devicesenabled |= (yesno != 0 )<<1;

				yesno = WinQueryButtonCheckstate( win, MouseGrab );
				PrfWriteProfileData( mameini, buffer, mousegrab, &yesno, 1 );
				mousecapture = (yesno != 0);

				yesno = WinQueryButtonCheckstate( win, FlipMouseX );
				PrfWriteProfileData( mameini, buffer, mouseflipx, &yesno, 1 );
				mouseflip = (yesno != 0);

				yesno = WinQueryButtonCheckstate( win, FlipMouseY );
				PrfWriteProfileData( mameini, buffer, mouseflipy, &yesno, 1 );
				mouseflip |= (yesno != 0)<<1;

				yesno = WinQueryButtonCheckstate( win, MouseAsJoystick );
				PrfWriteProfileData( mameini, buffer, mouseflipy, &yesno, 1 );
				devicesenabled |= (yesno != 0)<<3;

				yesno = WinQueryButtonCheckstate( win, JoyEnable );
				PrfWriteProfileData( mameini, buffer, joyen, &yesno, 1 );
				devicesenabled |= (yesno != 0)<<2;

				if ( yesno && !( prevdev & 4 ) ) {
					JoystickInit(0);
					JoystickRange(-128, 128);
					if ( JoystickOn() ) devicesenabled &= 11; else devicesenabled |= 4;
				}
				if ( !yesno && (prevdev & 4) ) {
					JoystickOff();
					JoystickSaveCalibration();
					devicesenabled &= 11;
				}
			}
		break;
	}
	return WinDefDlgProc( win, message, mp1, mp2 );
}

MRESULT EXPENTRY ListBoxWDblClick(HWND win, ULONG message, MPARAM mp1, MPARAM mp2) {
	if ( message == WM_BUTTON1DBLCLK ) {
		// Make sure something is selected before sending DID_OK
		// Prevents a problem if the list is empty but double-clicked
		if ( SHORT1FROMMR( WinSendMsg( win, LM_QUERYSELECTION, 
		    MPFROMSHORT( LIT_FIRST ), MPFROMLONG( 0 ) )) != 0xffff ) {
			WinPostMsg( WinQueryWindow( win, QW_PARENT ), WM_COMMAND, MPFROMLONG(DID_OK), 0 );
		}
	}
	return (*OldListBoxProc) (win, message, mp1, mp2 ); 
}

MRESULT EXPENTRY FrameWinKeyBypass( HWND win, ULONG message, MPARAM mp1, MPARAM mp2 ) {
	if ( message == WM_CHAR && emu_thread_id && !thread_state ) { return 0; }
 	if ( message == WM_TRANSLATEACCEL && emu_thread_id && !thread_state ) { return 0; }
	// Don't pass ALT to menu if the emulator is running.
	return (*OldFrameWinProc) (win, message, mp1, mp2 ); 
}

MRESULT EXPENTRY AboutDlg(HWND win, ULONG message, MPARAM mp1, MPARAM mp2) {
	RECTL rectl, rectl2;
	switch (message) {
		case WM_INITDLG:
			WinQueryWindowRect( HWND_DESKTOP, &rectl );
			WinQueryWindowRect( win, &rectl2 );
			WinSetWindowPos( win, 0, (rectl.xRight -
			    (rectl2.xRight-rectl2.xLeft)-rectl.xLeft) / 2, 
			    (rectl.yTop-(rectl2.yTop-rectl2.yBottom)-rectl.yBottom) / 2,
			    0, 0, SWP_MOVE );
		break;
	}
	return WinDefDlgProc( win, message, mp1, mp2 );
}

void UpdateIniFile( int oldgameidx ) {
	ULONG newprofsize, profsize, gameidx, newidx;
	USHORT *favorites, *newfavs;
	char *profileinf, *curchar, temp[9];

	// First update the quick list
	PrfQueryProfileSize( mameini, "Quick List", NULL, &profsize );
	if ( profsize ) {
		profileinf = (char *)malloc( profsize );
		newprofsize = 0;
		PrfQueryProfileData( mameini, "Quick List", NULL, profileinf, &profsize );
		curchar = profileinf;
		while ( *curchar != 0 ) {
			strcpy( temp, curchar );
			curchar += strlen( temp ) + 1;
			if ( strcmp( "Favorites", temp ) == 0 ) break;
			gameidx = 0xffff;
			profsize = 2;
			PrfQueryProfileData( mameini, "Quick List", temp, &gameidx, &profsize );
			if ( gameidx != 0xffff ) {
				newidx = getIdx( *((char**)((oldgames[oldgameidx])+(gameidx*4))) );
				if ( newidx != FIND_FAILED ) {
					newidx &= 0xffff;
					newprofsize+=2;
					PrfWriteProfileData( mameini, "Quick List", temp, &newidx, 2 );
				}
			}
		}
		free( profileinf );
	}

	// Next update the favorites list
	PrfQueryProfileSize( mameini, "Quick List", "Favorites", &profsize );
	if ( profsize ) {
		favorites = (USHORT *)malloc( profsize );
		newfavs = (USHORT *)malloc( profsize );
		newprofsize = 0;
		PrfQueryProfileData( mameini, "Quick List", "Favorites", favorites, &profsize );
		for ( gameidx=0; gameidx<(profsize/2); gameidx++ ) {
			newidx = getIdx( *((char**)((oldgames[oldgameidx])+(favorites[gameidx]*4))) );
			if ( newidx != FIND_FAILED ) {
				newfavs[ newprofsize ] = newidx;
				newprofsize++;
			}
		}
		PrfWriteProfileData( mameini, "Quick List", "Favorites", newfavs, newprofsize<<1 );
		free(favorites);
		free(newfavs);
	}
}

#define WM_REINITDLG WM_USER
#define SAVELIST WM_USER+1

MRESULT EXPENTRY QuickListDlg(HWND win, ULONG message, MPARAM mp1, MPARAM mp2) {
	static char favs[256]; // bitmapped... 8*256 total possible just in case.
	static char synclists = 1;
	char *profileinf, *curchar, temp[9];
	USHORT *favorites;
	ULONG profsize = 0, gameidx;
	USHORT idx, selectedone = 0,i;
	RECTL rectl, rectl2;

	switch (message) {
		case WM_INITDLG:
			WinQueryWindowRect( HWND_DESKTOP, &rectl );
			WinQueryWindowRect( win, &rectl2 );
			WinSetWindowPos( win, 0, (rectl.xRight -
			    (rectl2.xRight-rectl2.xLeft)-rectl.xLeft) / 2, 
			    (rectl.yTop-(rectl2.yTop-rectl2.yBottom)-rectl.yBottom) / 2,
			    0, 0, SWP_MOVE );
			for (i=0; i<256; ++i) favs[i] = 0;
			OldListBoxProc = WinSubclassWindow( WinWindowFromID( win, QuickListList ),
			    ListBoxWDblClick );
			WinSubclassWindow( WinWindowFromID( win, FavoritesListList ), ListBoxWDblClick );
		case WM_REINITDLG:
			PrfQueryProfileSize( mameini, "Quick List", "Favorites", &profsize );
			if ( profsize ) {
				favorites = (USHORT *)malloc( profsize );
				PrfQueryProfileData( mameini, "Quick List", "Favorites", favorites, &profsize );
				for ( i=0; i<(profsize/2); i++ ) {
					favs[ (favorites[i]/8) ] |= 1 << (favorites[i]%8);
					idx = LONGFROMMR( WinSendDlgItemMsg( win, FavoritesListList, LM_INSERTITEM, 
					    MPFROMSHORT( LIT_SORTASCENDING ), MPFROMP(drivers[favorites[i]]->description) ));
					WinSendDlgItemMsg( win, FavoritesListList, LM_SETITEMHANDLE, 
					    MPFROMSHORT( idx ), MPFROMLONG( favorites[i] ) );
				}
				free(favorites);
			}
			PrfQueryProfileSize( mameini, "Quick List", NULL, &profsize );
			if ( profsize ) {
				profileinf = (char *)malloc( profsize );
				PrfQueryProfileData( mameini, "Quick List", NULL, profileinf, &profsize );
				curchar = profileinf;
				while ( *curchar != 0 ) {
					strcpy( temp, curchar );
					curchar += strlen( temp ) + 1;
					if ( strcmp( "Favorites", temp ) == 0 ) break;
					gameidx = 0xffff;
					profsize = 2;
					PrfQueryProfileData( mameini, "Quick List", temp, &gameidx, &profsize );
					idx = LONGFROMMR( WinSendDlgItemMsg( win, QuickListList, LM_INSERTITEM, 
					    MPFROMSHORT( LIT_SORTASCENDING ), MPFROMP(drivers[gameidx]->description) ));
					WinSendDlgItemMsg( win, QuickListList, LM_SETITEMHANDLE, 
					    MPFROMSHORT( idx ), MPFROMLONG( gameidx ) );
					if ( (GAME_TO_TRY != 0xffff && gameidx == GAME_TO_TRY) ||
					    (GAME_TO_TRY == 0xffff && gameidx == 0x1a7 )) { // ATETRIS
						// Select the most recently chosen one by default.
						WinSendDlgItemMsg( win, QuickListList, LM_SELECTITEM, 
						    MPFROMSHORT( idx ), MPFROMSHORT( TRUE ) );
						selectedone = idx + 1;
					}
				}
				free( profileinf );

				if ( !selectedone ) {
					WinSendDlgItemMsg( win, QuickListList, LM_SELECTITEM, 
					    MPFROMSHORT( 0 ), MPFROMLONG( TRUE ) );
				}
				// Select the first one if none have been selected yet.

				idx = SHORT1FROMMR( WinSendDlgItemMsg( win, QuickListList, LM_QUERYSELECTION, 
				    MPFROMSHORT( LIT_FIRST ), MPFROMLONG( 0 ) ));

				if ( idx < 6 ) idx = 0; else idx -= 6;
				// Center in list box

				WinSendDlgItemMsg( win, QuickListList, LM_SETTOPINDEX, 
				    MPFROMSHORT( idx ), MPFROMLONG( 0 ) );
			}
			if ( profile_update_FUCKING_HACK ) {
				WinSendMsg( win, WM_COMMAND, MPFROMLONG(DID_CANCEL), 0 );
			}
		break;
		case WM_COMMAND:
			switch ( LONGFROMMP( mp1 ) ) {
				case AddToFav:
					idx = SHORT1FROMMR( WinSendDlgItemMsg( win, QuickListList, LM_QUERYSELECTION, 
					    MPFROMSHORT( LIT_FIRST ), MPFROMLONG( 0 ) ));
					if ( idx == 0xffff ) {
						WinAlarm( HWND_DESKTOP, WA_ERROR );
						return 0;
					}
					gameidx = LONGFROMMR( WinSendDlgItemMsg( win, QuickListList,
					    LM_QUERYITEMHANDLE, MPFROMSHORT( idx ), MPFROMLONG( 0 ) ));
					if ( (favs[ gameidx / 8 ] & (1<<(gameidx%8))) == 0 ) {
						idx = LONGFROMMR( WinSendDlgItemMsg( win, FavoritesListList, LM_INSERTITEM, 
						    MPFROMSHORT( LIT_SORTASCENDING ), MPFROMP(drivers[gameidx]->description) ));
						WinSendDlgItemMsg( win, FavoritesListList, LM_SETITEMHANDLE, 
						    MPFROMSHORT( idx ), MPFROMLONG( gameidx ) );
					}
					favs[ gameidx / 8 ] |= (1<<(gameidx%8));
					idx = SHORT1FROMMR(WinSendDlgItemMsg( win, FavoritesListList, LM_QUERYITEMCOUNT, 0, 0 ));
					for ( i=0; i<idx; i++ ) {
						selectedone = LONGFROMMR( WinSendDlgItemMsg( win, FavoritesListList,
						    LM_QUERYITEMHANDLE, MPFROMSHORT( i ), 0 ));
						if ( gameidx == selectedone ) {
							WinSendDlgItemMsg( win, FavoritesListList, LM_SELECTITEM, 
							    MPFROMSHORT( i ), MPFROMSHORT( TRUE ) );
							break;
						}
					}
					if ( gameidx != selectedone ) i = 0;
					if ( i < 6 ) i = 0; else i -= 6;
					// Center in list box

					WinSendDlgItemMsg( win, FavoritesListList, LM_SETTOPINDEX, 
					    MPFROMSHORT( i ), MPFROMLONG( 0 ) );
					return 0;
				break;
				case AddToFavAndRun:
					WinSendMsg( win, WM_COMMAND, MPFROMSHORT(AddToFav), 0 );
					WinSendMsg( win, WM_COMMAND, MPFROMSHORT(RunGame), 0 );
				break;
				case RemoveFromFav:
					idx = SHORT1FROMMR( WinSendDlgItemMsg( win, FavoritesListList, LM_QUERYSELECTION, 
					    MPFROMSHORT( LIT_FIRST ), MPFROMLONG( 0 ) ));
					if ( idx == 0xffff ) {
						WinAlarm( HWND_DESKTOP, WA_ERROR );
						return 0;
					}
					gameidx = LONGFROMMR( WinSendDlgItemMsg( win, FavoritesListList,
					    LM_QUERYITEMHANDLE, MPFROMSHORT( idx ), 0 ));
					favs[ gameidx / 8 ] &= ~(1<<( gameidx % 8));
					WinSendDlgItemMsg( win, FavoritesListList, LM_SELECTITEM, 
					    MPFROMSHORT( idx ), MPFROMSHORT( FALSE ) );
					WinSendDlgItemMsg( win, FavoritesListList, LM_DELETEITEM, MPFROMSHORT( idx ), 0 );
					i = SHORT1FROMMR(WinSendDlgItemMsg( win, FavoritesListList, LM_QUERYITEMCOUNT, 0, 0 ));
					if ( i > idx ) {
						WinSendDlgItemMsg( win, FavoritesListList, LM_SELECTITEM, 
						    MPFROMSHORT( idx ), MPFROMSHORT( TRUE ) );
					} else {
						if ( i > 0 ) {
							WinSendDlgItemMsg( win, FavoritesListList, LM_SELECTITEM, 
							    MPFROMSHORT( --idx ), MPFROMSHORT( TRUE ) );
						}
					}
					i = SHORT1FROMMR(WinSendDlgItemMsg( win, FavoritesListList, LM_QUERYTOPINDEX, 0, 0 ));
					if ( i > 0 ) {
						WinSendDlgItemMsg( win, FavoritesListList, LM_SETTOPINDEX, 
						    MPFROMSHORT( i ), MPFROMLONG( 0 ) );
					}
					return 0;
				break;
				case ReScan:
					WinShowWindow( win, FALSE );
					WinEnableMenuItem( WinWindowFromID( framewin, FID_MENU ), CreateQList, FALSE );
					WinDlgBox( HWND_DESKTOP, HWND_DESKTOP, ProgressDlgProc, 0, Progress, NULL );
					WinEnableMenuItem( WinWindowFromID( framewin, FID_MENU ), CreateQList, TRUE );
					WinSendDlgItemMsg( win, QuickListList, LM_DELETEALL, 0, 0 );
     					WinSendMsg( win, WM_REINITDLG, 0, 0 );
					WinShowWindow( win, TRUE );
					WinSetFocus( HWND_DESKTOP, win );
					return 0;
				break;
				case DID_OK:
				case RunGame:
					idx = SHORT1FROMMR( WinSendDlgItemMsg( win, QuickListList, LM_QUERYSELECTION, 
					    MPFROMSHORT( LIT_FIRST ), MPFROMLONG( 0 ) ));
					if ( idx == 0xffff ) {
						WinAlarm( HWND_DESKTOP, WA_ERROR );
						break;
					}
					GAME_TO_TRY = LONGFROMMR( WinSendDlgItemMsg( win, QuickListList,                                
					    LM_QUERYITEMHANDLE, MPFROMSHORT( idx ), MPFROMLONG( 0 ) ));

					pauseonfocuschange |= 2;
					StartupDlgHWND = WinLoadDlg( HWND_DESKTOP, HWND_DESKTOP, AboutDlg, 0, StartupProgress, NULL );

					emu_thread_id = _beginthread( EmulatorThread, NULL, 65536, NULL );
					DosSetPriority( PRTYS_THREAD, emu_thread_pclass, emu_thread_pdelta, emu_thread_id );

					WinSendMsg( win, SAVELIST, 0, 0 );
					WinDestroyWindow( win );
				break;
				case DID_CANCEL:
					WinSendMsg( win, SAVELIST, 0, 0 );
					WinDestroyWindow( win );
				break;
			}
		break;
		case WM_CLOSE:
		case SAVELIST:
			favorites = (USHORT *)malloc(4096);
			idx = 0;
			for (i=0; i<2048; ++i) {
				if ( favs[i/8] & (1<<(i%8)) ) { favorites[idx] = i; idx++; }
			}
			if ( idx ) PrfWriteProfileData( mameini, "Quick List", "Favorites", favorites, idx*2 );
			free( favorites );
		break;
		case WM_CONTROL:
			if ( !synclists ) break;
			switch ( SHORT1FROMMP( mp1 ) ) {
				case QuickListList:
					if ( SHORT2FROMMP( mp1 ) == LN_SELECT ) {
						idx = SHORT1FROMMR( WinSendDlgItemMsg( win, FavoritesListList, LM_QUERYSELECTION, 
						    MPFROMSHORT( LIT_FIRST ), 0 ));
						synclists = 0;
						WinSendDlgItemMsg( win, FavoritesListList, LM_SELECTITEM, MPFROMSHORT( idx ), MPFROMSHORT( FALSE ) );
						idx = SHORT1FROMMR( WinSendMsg( LONGFROMMP( mp2 ), LM_QUERYSELECTION, 
						    MPFROMSHORT( LIT_FIRST ), 0 ));
						gameidx = SHORT1FROMMR( WinSendMsg( LONGFROMMP( mp2 ), LM_QUERYITEMHANDLE, 
						    MPFROMSHORT( idx ), 0 ));
						selectedone = SHORT1FROMMR(WinSendDlgItemMsg( win, FavoritesListList, LM_QUERYITEMCOUNT, 0, 0 ));
						for (i=0; i<selectedone; ++i) {
							if ( SHORT1FROMMR( WinSendDlgItemMsg( win, FavoritesListList, LM_QUERYITEMHANDLE,
							    MPFROMSHORT( i ), 0 )) == gameidx ) {
								WinSendDlgItemMsg( win, FavoritesListList, LM_SELECTITEM, MPFROMSHORT( i ), MPFROMSHORT( TRUE ) );
								synclists = 1;
								break;
							}
						}
						synclists = 1;
					}
				break;
				case FavoritesListList:
					if ( SHORT2FROMMP( mp1 ) == LN_SELECT ) {
						idx = SHORT1FROMMR( WinSendMsg( LONGFROMMP( mp2 ), LM_QUERYSELECTION, 
						    MPFROMSHORT( LIT_FIRST ), 0 ));
						gameidx = SHORT1FROMMR( WinSendMsg( LONGFROMMP( mp2 ), LM_QUERYITEMHANDLE, 
						    MPFROMSHORT( idx ), 0 ));
						selectedone = SHORT1FROMMR(WinSendDlgItemMsg( win, QuickListList, LM_QUERYITEMCOUNT, 0, 0 ));
						for (i=0; i<selectedone; ++i) {
							if ( SHORT1FROMMR( WinSendDlgItemMsg( win, QuickListList, LM_QUERYITEMHANDLE,
							    MPFROMSHORT( i ), 0 )) == gameidx ) {
								synclists = 0;
								WinSendDlgItemMsg( win, QuickListList, LM_SELECTITEM, MPFROMSHORT( i ), MPFROMSHORT( TRUE ) );
								synclists = 1;
								break;
							}
						}
					}
				break;
			}
		break;
	}
	return WinDefDlgProc( win, message, mp1, mp2 );
}

MRESULT EXPENTRY FrameRegulationsDlg(HWND win, ULONG message, MPARAM mp1, MPARAM mp2) {
	int tmp;
	RECTL rectl, rectl2;
	switch (message) {
		case WM_INITDLG:
			WinQueryWindowRect( HWND_DESKTOP, &rectl );
			WinQueryWindowRect( win, &rectl2 );
			WinSetWindowPos( win, 0, (rectl.xRight -
			    (rectl2.xRight-rectl2.xLeft)-rectl.xLeft) / 2, 
			    (rectl.yTop-(rectl2.yTop-rectl2.yBottom)-rectl.yBottom) / 2,
			    0, 0, SWP_MOVE );
			WinCheckButton( win, AutoSkip, autoskip );
			WinCheckButton( win, AutoSlow, autoslow );
			WinSendDlgItemMsg( win, MaxSkip, SPBM_OVERRIDESETLIMITS,
			     MPFROMLONG( 10 ), MPFROMLONG( 0 ) );
			WinSendDlgItemMsg( win, MaxSkip, SPBM_SETCURRENTVALUE,
			     MPFROMLONG( maxskip ), 0 );
			WinEnableControl( win, MaxSkip, autoskip );
		break;
		case WM_COMMAND:
			switch ( SHORT1FROMMP(mp1) ) {
				case DID_OK:
					WinSendDlgItemMsg( win, MaxSkip, SPBM_QUERYVALUE,
					    MPFROMP( &tmp ), MPFROM2SHORT( 0, SPBQ_ALWAYSUPDATE ) );
					// Function returns a 4 byte INT.  Can't directly store in a
					// char without clobbering data.
					maxskip = tmp;
					tmp = SHORT1FROMMR(WinSendDlgItemMsg( win, AutoSkip, BM_QUERYCHECK, 0, 0 ));
					autoskip = tmp;
					tmp = SHORT1FROMMR(WinSendDlgItemMsg( win, AutoSlow, BM_QUERYCHECK, 0, 0 ));
					autoslow = tmp;
				break;
			}
		break;
		case WM_CONTROL: {
			char askiptemp;
			askiptemp = SHORT1FROMMR(WinSendDlgItemMsg( win, AutoSkip, BM_QUERYCHECK, 0, 0 ));
			WinEnableControl( win, MaxSkip, askiptemp );
   		}
		break;
	}
	return WinDefDlgProc( win, message, mp1, mp2 );
}

MRESULT EXPENTRY AdvancedAudioDlg(HWND win, ULONG message, MPARAM mp1, MPARAM mp2) {
	static char nopropagate = 0;
	static unsigned long tmpfreq = 0, tmpssize = 0;
	RECTL rectl, rectl2;
	long int i;
	ULONG idx;
	struct DEVICE_DRIVER aud_drv;
	char buftemp[32];

	switch (message) {
		case WM_INITDLG:
			nopropagate = 0;
			if ( INITIAL_FREQ > F33K ) {
				WinCheckButton( win, 118, TRUE );
				tmpfreq = F44K;
			} else if ( INITIAL_FREQ > F22K ) {
				WinCheckButton( win, 119, TRUE );
				tmpfreq = F33K;
			} else if ( INITIAL_FREQ > F11K ) {
				WinCheckButton( win, 120, TRUE );
				tmpfreq = F22K;
			} else if ( INITIAL_FREQ > F8K ) {
				WinCheckButton( win, 121, TRUE );
				tmpfreq = F11K;
			} else { WinCheckButton( win, 122, TRUE ); tmpfreq = F8K; }


			if ( SOUND_QUALITY == 8 ) {
				WinCheckButton( win, 125, TRUE );
				tmpssize = 8;
			} else { WinCheckButton( win, 126, TRUE ); tmpssize = 16; }

			WinQueryWindowRect( HWND_DESKTOP, &rectl );
			WinQueryWindowRect( win, &rectl2 );
			WinSetWindowPos( win, 0, (rectl.xRight -
			    (rectl2.xRight-rectl2.xLeft)-rectl.xLeft) / 2, 
			    (rectl.yTop-(rectl2.yTop-rectl2.yBottom)-rectl.yBottom) / 2,
			    0, 0, SWP_MOVE );
			for (i=0; i<AudioGetNumberOfDevices(); ++i) {
				if ( AudioGetDeviceDriver( i, &aud_drv ) == AUDIO_OK ) {
					idx = LONGFROMMR( WinSendDlgItemMsg( win, SoundDriverListBox, LM_INSERTITEM, 
					    MPFROMSHORT( LIT_SORTASCENDING ), MPFROMP(aud_drv.name) ));
					WinSendDlgItemMsg( win, SoundDriverListBox, LM_SETITEMHANDLE, 
					    MPFROMSHORT( idx ), MPFROMLONG( i ) );
					if ( i == GPMIXERdevice ) {
						WinSendDlgItemMsg( win, SoundDriverListBox, LM_SELECTITEM, 
						    MPFROMSHORT( idx ), MPFROMSHORT( TRUE ) );
					}
				}
			}

		break;
		case WM_CONTROL:
			switch SHORT1FROMMP( mp1 ) {
				case 118: // For any frequency or sound quality change
					tmpfreq = F44K;
					goto fadjust;
				case 119:
					tmpfreq = F33K;
					goto fadjust;
				case 120:
					tmpfreq = F22K;
					goto fadjust;
				case 121:
					tmpfreq = F11K;
					goto fadjust;
				case 122:
					tmpfreq = F8K;
					goto fadjust;
				case 125:
					tmpssize = 8;
					goto fadjust;
				case 126:
					tmpssize = 16;
				fadjust:
					// Hold the latency as constant over these various changes
					WinSendDlgItemMsg( win, BufferSizeText, SPBM_QUERYVALUE, 
					    MPFROMP(&idx), MPFROM2SHORT(0,SPBQ_ALWAYSUPDATE) );

					idx = SHORT1FROMMR( WinSendDlgItemMsg( win, SoundDriverListBox, 
					    LM_QUERYSELECTION, MPFROMSHORT( LIT_FIRST ), MPFROMLONG( 0 ) ));
					i = LONGFROMMR( WinSendDlgItemMsg( win, SoundDriverListBox,
					    LM_QUERYITEMHANDLE, MPFROMSHORT( idx ), 0 ));
					if ( AudioGetDeviceDriver( i, &aud_drv ) == AUDIO_OK ) {
						nopropagate = 1;
						WinSendDlgItemMsg( win, LatencyText, SPBM_SETLIMITS, 
						    MPFROMLONG(aud_drv.max_bufsize*1000/(tmpfreq*(tmpssize>>3))), 
						    MPFROMLONG(aud_drv.min_bufsize*1000/(tmpfreq*(tmpssize>>3))) );
						WinSendDlgItemMsg( win, BufferSizeText, SPBM_QUERYVALUE, 
						    MPFROMP(&idx), MPFROM2SHORT(0,SPBQ_ALWAYSUPDATE) );
						WinSendDlgItemMsg( win, LatencyText, SPBM_SETCURRENTVALUE, 
						    MPFROMLONG(idx*1000/(tmpfreq*(tmpssize>>3))), 0 );
						WinSendDlgItemMsg( win, BufferSizeSlider, 
						    SBM_SETPOS, MPFROMSHORT(idx/4), 0 );
						nopropagate = 0;
					} else WinAlarm( HWND_DESKTOP, WA_WARNING );
				break;
				case SoundDriverListBox:
					if ( SHORT2FROMMP( mp1 ) == LN_SELECT ) {
						// A list box item was selected
						// Update the description appropriately
						idx = SHORT1FROMMR( WinSendDlgItemMsg( win, SoundDriverListBox, 
						    LM_QUERYSELECTION, MPFROMSHORT( LIT_FIRST ), MPFROMLONG( 0 ) ));
						i = LONGFROMMR( WinSendDlgItemMsg( win, SoundDriverListBox,
						    LM_QUERYITEMHANDLE, MPFROMSHORT( idx ), 0 ));
						if ( AudioGetDeviceDriver( i, &aud_drv ) == AUDIO_OK ) {
							WinSetWindowText( WinWindowFromID( win, DescriptionText ),
							    aud_drv.description );
							sprintf( buftemp, "%ld", aud_drv.min_bufsize );
							WinSetWindowText( WinWindowFromID( win, MinBufferSizeText ),
							    buftemp );
							sprintf( buftemp, "%ld", aud_drv.max_bufsize );
							WinSetWindowText( WinWindowFromID( win, MaxBufferSizeText ),
							    buftemp );
							nopropagate = 1;
							WinSendDlgItemMsg( win, BufferSizeText, SPBM_SETLIMITS, 
							    MPFROMLONG(aud_drv.max_bufsize), MPFROMLONG(aud_drv.min_bufsize) );
							if ( AUDIO_BUF_SIZE == 0 ) {
								AUDIO_BUF_SIZE = aud_drv.suggested_bufsize;
							}

							WinSendDlgItemMsg( win, BufferSizeText, SPBM_SETCURRENTVALUE, 
							    MPFROMLONG(AUDIO_BUF_SIZE), 0 );
							WinSendDlgItemMsg( win, LatencyText, SPBM_SETLIMITS, 
							    MPFROMLONG(aud_drv.max_bufsize*1000/(tmpfreq*(tmpssize>>3))), 
							    MPFROMLONG(aud_drv.min_bufsize*1000/(tmpfreq*(tmpssize>>3))) );
							WinSendDlgItemMsg( win, LatencyText, SPBM_SETCURRENTVALUE, 
							    MPFROMLONG(AUDIO_BUF_SIZE*1000/(tmpfreq*(tmpssize>>3))), 0 );
							WinSendDlgItemMsg( win, BufferSizeSlider, SBM_SETSCROLLBAR, 
							    MPFROMSHORT(AUDIO_BUF_SIZE/4), 
							    MPFROM2SHORT(aud_drv.min_bufsize/4,aud_drv.max_bufsize/4) );
							nopropagate = 0;
						} else {
							WinSetWindowText( WinWindowFromID( win, DescriptionText ),
							    "Could not get a device driver description?!" );
							WinSetWindowText( WinWindowFromID( win, MinBufferSizeText ),
							    "I dunno" );
							WinSetWindowText( WinWindowFromID( win, MaxBufferSizeText ),
							    "I dunno" );
						}
					}
				break;
				case BufferSizeText:
					if ( !nopropagate ) {
						nopropagate = 1;

						WinSendDlgItemMsg( win, BufferSizeText, SPBM_QUERYVALUE, 
						    MPFROMP(&idx), MPFROM2SHORT(0,SPBQ_ALWAYSUPDATE) );

						if ( SHORT2FROMMP(mp1) == SPBN_DOWNARROW ) {
							idx-=4;
						}

						if ( idx % 4 ) {
							idx += 4 - (idx%4);
						}

						WinSendDlgItemMsg( win, BufferSizeText, SPBM_SETCURRENTVALUE, 
						    MPFROMLONG(idx), 0 );

						WinSendDlgItemMsg( win, LatencyText, SPBM_SETCURRENTVALUE, 
						    MPFROMLONG(idx*1000/(tmpfreq*(tmpssize>>3))), 0 );
						WinSendDlgItemMsg( win, BufferSizeSlider, 
						    SBM_SETPOS, MPFROMSHORT(idx/4), 0 );

						nopropagate = 0;
					}
				break;
				case LatencyText:
					if ( !nopropagate ) {
						if ( SHORT2FROMMP(mp1) == SPBN_CHANGE ) {
							nopropagate = 1;
							WinSendDlgItemMsg( win, LatencyText, SPBM_QUERYVALUE, 
							    MPFROMP(&idx), MPFROM2SHORT(0,SPBQ_ALWAYSUPDATE) );
							idx = (idx*(tmpfreq*(tmpssize>>3)))/1000;
							if ( idx%4 ) {
								idx += 4 - (idx%4);
							}
							WinSendDlgItemMsg( win, BufferSizeText, SPBM_SETCURRENTVALUE, 
							    MPFROMLONG(idx), 0 );
							WinSendDlgItemMsg( win, BufferSizeSlider, 
							    SBM_SETPOS, MPFROMSHORT(idx/4), 0 );
							nopropagate = 0;
						}
					}
				break;
				
			}
		break;
		case WM_HSCROLL:
			if ( SHORT1FROMMP(mp1) == BufferSizeSlider ) {
				idx = SHORT1FROMMR( WinSendDlgItemMsg( win, SoundDriverListBox, 
				    LM_QUERYSELECTION, MPFROMSHORT( LIT_FIRST ), MPFROMLONG( 0 ) ));
				i = LONGFROMMR( WinSendDlgItemMsg( win, SoundDriverListBox,
				    LM_QUERYITEMHANDLE, MPFROMSHORT( idx ), 0 ));
				if ( AudioGetDeviceDriver( i, &aud_drv ) == AUDIO_OK ) {
					idx = SHORT1FROMMR( WinSendDlgItemMsg( win, BufferSizeSlider, 
					    SBM_QUERYPOS, 0, 0 ) );
					switch( (USHORT)(SHORT2FROMMP( mp2 )) ) {
						case SB_LINELEFT:
							WinSendDlgItemMsg( win, BufferSizeSlider, 
							    SBM_SETPOS, MPFROMSHORT(idx - 1), 0 );
						break;
						case SB_LINERIGHT:
							WinSendDlgItemMsg( win, BufferSizeSlider, 
							    SBM_SETPOS, MPFROMSHORT(idx + 1), 0 );
						break;
						case SB_PAGELEFT:
							WinSendDlgItemMsg( win, BufferSizeSlider, 
							    SBM_SETPOS, MPFROMSHORT(idx - 128), 0 );
						break;
						case SB_PAGERIGHT:
							WinSendDlgItemMsg( win, BufferSizeSlider, 
							    SBM_SETPOS, MPFROMSHORT(idx + 128), 0 );
						break;
					}
				}
				if ( !nopropagate ) {
					nopropagate = 1;
					if ( SHORT1FROMMP( mp2 ) == 0 ) {
						WinSendDlgItemMsg( win, BufferSizeText, SPBM_SETCURRENTVALUE, 
						    MPFROMLONG((unsigned long)(idx*4)), 0 );
						WinSendDlgItemMsg( win, LatencyText, SPBM_SETCURRENTVALUE, 
						    MPFROMLONG((unsigned long)(idx*4000)/(tmpfreq*(tmpssize>>3))), 0 );
					} else {
						WinSendDlgItemMsg( win, BufferSizeText, SPBM_SETCURRENTVALUE, 
						    MPFROMLONG((unsigned long)(SHORT1FROMMP(mp2)*4)), 0 );
						WinSendDlgItemMsg( win, LatencyText, SPBM_SETCURRENTVALUE, 
						    MPFROMLONG((unsigned long)(SHORT1FROMMP(mp2)*4000)/(tmpfreq*(tmpssize>>3))), 0 );
					}
					nopropagate = 0;
				}
			}
		break;
		case WM_COMMAND:
			if ( SHORT1FROMMP( mp1 ) == DID_OK ) {
				idx = SHORT1FROMMR( WinSendDlgItemMsg( win, SoundDriverListBox, 
				    LM_QUERYSELECTION, MPFROMSHORT( LIT_FIRST ), MPFROMLONG( 0 ) ));
				GPMIXERdevice = LONGFROMMR( WinSendDlgItemMsg( win, SoundDriverListBox,
				    LM_QUERYITEMHANDLE, MPFROMSHORT( idx ), 0 ));
				WinSendDlgItemMsg( win, BufferSizeText, SPBM_QUERYVALUE, 
				    MPFROMP(&AUDIO_BUF_SIZE), MPFROM2SHORT(0,SPBQ_ALWAYSUPDATE) );
				if ( WinQueryButtonCheckstate( win, 118 ) ) INITIAL_FREQ = F44K;
				if ( WinQueryButtonCheckstate( win, 119 ) ) INITIAL_FREQ = F33K;
				if ( WinQueryButtonCheckstate( win, 120 ) ) INITIAL_FREQ = F22K;
				if ( WinQueryButtonCheckstate( win, 121 ) ) INITIAL_FREQ = F11K;
				if ( WinQueryButtonCheckstate( win, 122 ) ) INITIAL_FREQ = F8K;
				if ( WinQueryButtonCheckstate( win, 125 ) ) SOUND_QUALITY = 8;
				if ( WinQueryButtonCheckstate( win, 126 ) ) SOUND_QUALITY = 16;
			}
		break;
	}
	return WinDefDlgProc( win, message, mp1, mp2 );
}

MRESULT EXPENTRY ROMLocationsProc(HWND win, ULONG message, MPARAM mp1, MPARAM mp2) {
	ULONG idx, i;
	RECTL rectl, rectl2;
	char tmpbuf[ 1024 ];
	switch (message) {
		case WM_INITDLG:
			WinQueryWindowRect( HWND_DESKTOP, &rectl );
			WinQueryWindowRect( win, &rectl2 );
			WinSetWindowPos( win, 0, (rectl.xRight -
			    (rectl2.xRight-rectl2.xLeft)-rectl.xLeft) / 2, 
			    (rectl.yTop-(rectl2.yTop-rectl2.yBottom)-rectl.yBottom) / 2,
			    0, 0, SWP_MOVE );
			for (i=0; i<numsearchdirs; i++) {
				idx = LONGFROMMR( WinSendDlgItemMsg( win, ROMLocationListBox, LM_INSERTITEM, 
				    MPFROMSHORT( 0 ), MPFROMP(searchpath[i]) ));
			}
		break;
		case WM_COMMAND:
			switch ( LONGFROMMP(mp1) ) {
				case AddROMLoc:
					WinQueryDlgItemText( win, ROMLocationText, 1024, tmpbuf );
					idx = LONGFROMMR( WinSendDlgItemMsg( win, ROMLocationListBox, LM_INSERTITEM, 
					    MPFROMSHORT( 0 ), MPFROMP(tmpbuf) ));
					return 0;
				break;
				case RemoveROMLoc:
					idx = SHORT1FROMMR( WinSendDlgItemMsg( win, ROMLocationListBox, 
					     LM_QUERYSELECTION, MPFROMSHORT( LIT_FIRST ), MPFROMLONG( 0 ) ));
					WinSendDlgItemMsg( win, ROMLocationListBox, LM_DELETEITEM, 
					    MPFROMSHORT( idx ), 0 );
					return 0;
				break;
				case DID_OK:
					for ( i=0; i<numsearchdirs; ++i ) {
						free( searchpath[i] );
					}
					idx = SHORT1FROMMR( WinSendDlgItemMsg( win, ROMLocationListBox, 
					     LM_QUERYITEMCOUNT, MPFROMSHORT( 0 ), MPFROMLONG( 0 ) ));

					if ( idx != numsearchdirs ) {
						free( searchpath );
						searchpath = (char **) malloc( idx );
						numsearchdirs = idx;
					}

					for (i=0; i<idx; ++i) {
						WinSendDlgItemMsg( win, ROMLocationListBox, 
						    LM_QUERYITEMTEXT, MPFROM2SHORT( i, 1024 ), 
						    MPFROMP( tmpbuf ) );
						searchpath[i] = (char *)malloc( strlen(tmpbuf)+1 );
						strcpy( searchpath[i], tmpbuf );
					}
			}
		break;
	}
	return WinDefDlgProc( win, message, mp1, mp2 );
}

extern void InitCheat(void);
extern void StopCheat(void);
// extern int key_to_pseudo_code(int k);

MRESULT EXPENTRY MameMain(HWND win, ULONG message, MPARAM mp1, MPARAM mp2) {
	static PVOID framebuffer = NULL;
	static ULONG err = 1;
	static HEV key_released = 0, key_pressed = 0;
	static unsigned short trygame = 0xffff;
	static char vrnenable = 0;
	static float sw, sh;
	static int timerthread = 0, resetcummouse = 1;
	static int cummousex = 0, cummousey = 0;
	static SETUP_BLITTER BlSet;

	RGNRECT rgnCtl;
	UCHAR scancode;
	PBYTE imagebuf = NULL;
	HPS hps = 0;
	RECTL rectl;
	ULONG linebytes, linetot, i, j;
	RECTL rcls[50];
	HRGN hrgn;
	SWP swp;
	POINTL pointl;
	int iYPos, iYDiff;

	switch ( message ) {
		case WM_SIZE:
			custom_size = 1;
		break;
		case WM_TIMER:
			switch ( SHORT1FROMMP( mp1 ) ) {
			case 0: // Rapid fire trigger
				rapidtrigger = 0;
				WinStopTimer( WinQueryAnchorBlock( win ), win, 0 );
			break;
			case 1: // Joystick polling
				if ( thread_state ) {
					resetcummouse = 1;
					return 0;
				}
				JoystickValues(&jstick);
				if ( (devicesenabled & 2) && (devicesenabled & 8)) {
					if ( resetcummouse ) {
						resetcummouse = 0;
						cummousex = cummousey = 0;
					}
					osd_trak_read( 0, (int*)&(pointl.x), (int*)&(pointl.y) );
					cummousex += pointl.x;
					cummousey += pointl.y;
					if ( cummousex > 128 ) cummousex = 128;
					if ( cummousey > 128 ) cummousey = 128;
					if ( cummousex < -128 ) cummousex = -128;
					if ( cummousey < -128 ) cummousey = -128;
					jstick.Joy1X += cummousex;
					jstick.Joy1Y += cummousey;
				}
			break;
		}
		break;
		case WM_MOUSEMOVE:
			if ( mousecapture  && emu_thread_id && !thread_state ) {
				WinSetPointer( HWND_DESKTOP, 0 );
				return 0;
			}
		break;
		case WM_BUTTON1DOWN:
			if ( devicesenabled & 2 ) mouseclick1 = 1;
		break;
		case WM_BUTTON2DOWN:
			if ( devicesenabled & 2 ) mouseclick2 = 1;
		break;
		case WM_BUTTON1UP:
			mouseclick1 = 0;
		break;
		case WM_BUTTON2UP:
			mouseclick2 = 0;
		break;
		case EmuStart: {
			struct winp parm;
			char buf[9];
			err = 0;
			thread_state = 0;
			resetcummouse = 1;
			WinEnableMenuItem( WinWindowFromID( framewin, FID_MENU ), PauseGame, TRUE );
			WinEnableMenuItem( WinWindowFromID( framewin, FID_MENU ), ResetGame, TRUE );
			WinEnableMenuItem( WinWindowFromID( framewin, FID_MENU ), GameSettings, TRUE );
			WinEnableMenuItem( WinWindowFromID( framewin, FID_MENU ), ActualSize, TRUE );
			WinEnableMenuItem( WinWindowFromID( framewin, FID_MENU ), DoubleSize, TRUE );
			WinSetMenuItemText( WinWindowFromID( framewin, FID_MENU ), PauseGame, "~Pause\t[P]" );
			pauseonfocuschange &= 1;
			parm.x = 0xffff; i = sizeof( struct winp );
			sprintf( buf, "%8s", drivers[GAME_TO_TRY]->name );
			PrfQueryProfileData( mameini, windowparam, buf, &parm, &i );
			if ( parm.x != 0xffff ) {
				WinSetWindowPos( framewin, HWND_TOP, parm.x, parm.y, parm.cx, parm.cy, SWP_SIZE | SWP_MOVE );
				custom_size = 1;
			} else custom_size = 0;
			if ( StartupDlgHWND ) {
				WinDismissDlg( StartupDlgHWND, 0 );
				WinDestroyWindow( StartupDlgHWND );
				StartupDlgHWND = 0;
			}

			if ( devicesenabled & 4 ) {
				WinStartTimer( WinQueryAnchorBlock( win ), win, 1, 50 );
			}
			if ( mousecapture ) {
				WinSetCapture( HWND_DESKTOP, win );
				WinSetPointer( HWND_DESKTOP, 0 );
			} }
		break;
		case EmuDone: {
			struct winp parm;
			char buf[9];
			trygame = GAME_TO_TRY;
			GAME_TO_TRY = 0xffff;
			fprintf( fp, "Emulation thread reports that the emulation has terminated.\n" );
			if ( devicesenabled & 4 ) {
				WinStopTimer( WinQueryAnchorBlock(win), win, 1 );
			}
			if ( timerthread != 0 ) { DosKillThread( timerthread ); timerthread = 0; }
			WinEnableMenuItem( WinWindowFromID( framewin, FID_MENU ), PauseGame, FALSE );
			WinEnableMenuItem( WinWindowFromID( framewin, FID_MENU ), ResetGame, FALSE );
			WinEnableMenuItem( WinWindowFromID( framewin, FID_MENU ), GameSettings, FALSE );
			WinEnableMenuItem( WinWindowFromID( framewin, FID_MENU ), ActualSize, FALSE );
			WinEnableMenuItem( WinWindowFromID( framewin, FID_MENU ), DoubleSize, FALSE );
			if ( custom_size ) {
				WinQueryWindowPos( framewin, &swp );
				sprintf( buf, "%8s", drivers[trygame]->name );
				parm.x = swp.x;  parm.y = swp.y;  parm.cx = swp.cx;  parm.cy = swp.cy;
				PrfWriteProfileData( mameini, windowparam, buf, &parm, sizeof(struct winp) );
			}
			custom_size = 0;
			if ( mousecapture ) {
				WinSetCapture( HWND_DESKTOP, 0 );
			}
			switch ( quitnotify ) {
			case 1:
				fprintf( fp, "Trying to create quick list again.\n" );
				quitnotify = 0;
				WinPostMsg( win, WM_COMMAND, MPFROMLONG( CreateQList ), 0 );
			break;
			case 2:
				quitnotify = 0;
				if ( thread_state == 3 ) {
					GAME_TO_TRY = trygame;
					pauseonfocuschange |= 2;
					StartupDlgHWND = WinLoadDlg( HWND_DESKTOP, HWND_DESKTOP, AboutDlg, 0, StartupProgress, NULL );
					emu_thread_id = _beginthread( EmulatorThread, NULL, 65536, NULL );
					DosSetPriority( PRTYS_THREAD, emu_thread_pclass, emu_thread_pdelta, emu_thread_id );
					thread_state = 0;
					WinSetMenuItemText( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), PauseGame, "~Pause\t[P]" );
				}
			break;
			case 3:
				quitnotify = 0;
				fprintf( fp, "Restarting game.\n" );
				WinPostMsg( win, WM_COMMAND, MPFROMLONG( OpenGame ), 0 );
			break;
			} }
		break;
		case WM_REALIZEPALETTE:
			if ( diveinst && blitdepth == FOURCC_LUT8)  {
				BYTE palette[1024];
				GpiQueryRealColors( hps, 0, 0, 256, (PLONG)palette );
				DiveSetDestinationPalette( diveinst, 0, 256, palette );
			}
		break;
		case GET_KEY:
			key_released = LONGFROMMP( mp2 );
		break;
		case WAIT_KEY:
			key_pressed = LONGFROMMP( mp2 );
		break;
		case WM_FOCUSCHANGE:
			// Turn off the keyboard LEDs if needed.
/*			if ( mousecapture && emu_thread_id && thread_state == 0) {
				// Playing (unpaused) and emulator thread exists
				if ( !SHORT1FROMMP( mp2 ) ) {
					// Losing focus, release mouse, clear keyboard LEDs if needed.
					WinSetCapture( HWND_DESKTOP, 0 );
				} else {
					// Getting focus, grab the mouse
					WinSetCapture( HWND_DESKTOP, win );
					WinSetPointer( HWND_DESKTOP, 0 );
				}
			} */
			if ( !pauseonfocuschange || (pauseonfocuschange & 2) ) break;
			if ( emu_thread_id && SHORT1FROMMP( mp2 ) == FALSE &&
			    LONGFROMMP( mp1 ) != win &&
			    LONGFROMMP( mp1 ) != framewin &&
			    LONGFROMMP( mp1 ) != WinWindowFromID( framewin, FID_SYSMENU ) &&
			    LONGFROMMP( mp1 ) != WinWindowFromID( framewin, FID_MENU ) &&
			    LONGFROMMP( mp1 ) != WinWindowFromID( framewin, FID_MINMAX ) ) {
				// If it is playing, send a pause signal if we're losing the focus.
				osd_pause(2);
				osd_led_w( 0, 0 );
				osd_led_w( 1, 0 );
				osd_led_w( 2, 0 );
				DosSuspendThread( emu_thread_id );
			} else if ( emu_thread_id && ( SHORT1FROMMP( mp2 ) == TRUE )) {
				DosResumeThread( emu_thread_id );
				osd_pause(3);
				// Wait for the user to unpause.
			}
		break;
		case WM_CHAR:
			if ( !emu_thread_id ) return WinDefWindowProc( win, message, mp1, mp2 );
			// Make sure ALT key doesn't get "stuck" by ignoring all keyed input until
			// the emulator is running.

			if ( (SHORT1FROMMP( mp1 ) & KC_SCANCODE) == 0 ) {
				if ( emu_thread_id && thread_state == 0 ) { return 0; }
				// If emulator is running.. don't pass this keystroke to the PM
			}
			if ( (SHORT1FROMMP( mp1 ) & KC_KEYUP) == 0 ) {
				scancode = CHAR4FROMMP(mp1);
				scancode = remap_key( scancode );

				if ( key_pressed != 0 ) DosPostEventSem( key_pressed );
				key_pressed = 0;

				keystatus[ scancode/8 ] |= 1 << (scancode%8);
			} else {
				if ( key_released != 0 ) DosPostEventSem( key_released );
				key_released = 0;
				scancode = remap_key( CHAR4FROMMP(mp1) );

				keystatus[ scancode/8 ] &= ~(1 << (scancode%8));
//				{ extern char memory[]; memory[ scancode ] = 0; memory[key_to_pseudo_code(scancode)] = 0; }
				last_char = scancode;
			}
			if ( emu_thread_id && thread_state == 0 ) { return 0; }
			// If emulator is running.. don't pass this keystroke to the PM
		break;
		case WM_CREATE:
			WinQueryWindowRect( HWND_DESKTOP, &(rcls[0]) );
			centerx = (rcls[0].xRight - rcls[0].xLeft) / 2;
			centery = (rcls[0].yTop - rcls[0].yBottom) / 2;
			err = DiveOpen( &diveinst, FALSE, framebuffer );
			if ( err ) {
				WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, "Could not initialize a DIVE instance.  Unable to display graphics.", "DIVE error", 0, MB_CANCEL | MB_ERROR | MB_SYSTEMMODAL | MB_MOVEABLE );
				break;
			}
		case WM_RECREATE: // Don't have to reopen DIVE to recreate
			DosRequestMutexSem( DiveBufferMutex, -1 );
			err = DiveAllocImageBuffer( diveinst, &bufnum, FOURCC_R565,
			   640, 480, 1280, NULL );
			if ( err ) {
				WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, "Could allocate an R565 DIVE image buffer.  Unable to display graphics.", "DIVE error", 0, MB_CANCEL | MB_ERROR | MB_SYSTEMMODAL | MB_MOVEABLE );
				DiveClose( diveinst );
				DosReleaseMutexSem( DiveBufferMutex );
				break;
			}
			err = DiveBeginImageBufferAccess( diveinst, bufnum, &imagebuf,
			    &linebytes, &linetot );
			if ( err ) {
				WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, "Error accessing the DIVE image buffer.  Unable to display graphics.", "DIVE error", 0, MB_CANCEL | MB_ERROR | MB_SYSTEMMODAL | MB_MOVEABLE );
				DiveFreeImageBuffer( diveinst, bufnum );
				DiveClose( diveinst );
				DosReleaseMutexSem( DiveBufferMutex );
				break;
			}
			{ unsigned short *tmp; 
			tmp = (unsigned short *)imagebuf;
			for ( i = 0; i < linetot; ++i ) {
				for ( j=0; j < linebytes; j += 2 ) {
					*tmp = ((i*32/linetot) << 11) | ((i*64/linetot) << 5) | (i*32/linetot);
					tmp++;
				}
			} }
			err = DiveEndImageBufferAccess( diveinst, bufnum );
			if ( err ) {
				WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, "Error manipulating the DIVE image buffer.  Unable to display graphics.", "DIVE error", 0, MB_CANCEL | MB_ERROR | MB_SYSTEMMODAL | MB_MOVEABLE );
				DiveFreeImageBuffer( diveinst, bufnum );
				DiveClose( diveinst );
				DosReleaseMutexSem( DiveBufferMutex );
				break;
			}
			DosReleaseMutexSem( DiveBufferMutex );

		break;

		case WM_CLOSE:
			if ( emu_thread_id ) {
				WinSendMsg( win, EmuDone, 0, 0 );
				// Make sure settings are saved
			}
			if ( emu_thread_id != 0 ) DosKillThread( emu_thread_id );
			if ( timerthread != 0 ) DosKillThread( timerthread );
			DosRequestMutexSem( DiveBufferMutex, -1 );
			DiveFreeImageBuffer( diveinst, bufnum );
			bufnum = 0;
			DiveClose( diveinst );
			diveinst = 0;
			DosReleaseMutexSem( DiveBufferMutex );
		break;

		case WM_COMMAND:
			switch ( LONGFROMMP(mp1) ) { 
				// Ok, I got lazy and didn't make defines for all of these... so shoot me.
				// I did label them appropriately however, and this is the only place they're used.

				case 259: // Audio Options
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), 259, FALSE );
					WinDlgBox( HWND_DESKTOP, HWND_DESKTOP, AdvancedAudioDlg, 0, AdvancedAudioDialog, NULL );
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), 259, TRUE );
					if ( soundchangewarn && emu_thread_id && soundon ) {
						os2printf( "NOTE:  Change will not take effect until the emulation is restarted or a new game is picked.  (This warning will only be display once per session.)" );
						soundchangewarn = 0;
					}
				break;
				case 211: // Allow LEDs to flash
					allowledflash = 1 - allowledflash;
					WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 211, allowledflash );
				break;
				case 257: // No sound
					if ( soundon ) { 
						Machine->sample_rate = 0;
					}
					soundon = 1 - soundon;
					WinCheckMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), 257, !soundon );
					if ( soundchangewarn && emu_thread_id && soundon ) {
						// Sound disable happens immediately, but sound on requires a reset.
						os2printf( "This change will not take effect until this game is restarted or another game is selected." );
						soundchangewarn = 0;
					}
				break;
				case 210: // Cheat
					nocheat = !nocheat;
					WinCheckMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), 210, !nocheat );
					if ( !emu_thread_id ) break;
					if ( nocheat ) StopCheat(); else InitCheat();
				break;
				case 407: // Frame rate regulation
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), 407, FALSE );
					WinDlgBox( HWND_DESKTOP, HWND_DESKTOP, FrameRegulationsDlg, 0, FrameSkipOptions, NULL );
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), 407, TRUE );
				break;
/*				case 250:  // 44 KHz sound selected
					INITIAL_FREQ = F44K;
					goto checkmenu;
				case 251:  // 33 KHz sound selected
					INITIAL_FREQ = F33K;
					goto checkmenu;
				case 252:  // 22 KHz sound selected
					INITIAL_FREQ = F22K;
					goto checkmenu;
				case 253:  // 11 KHz sound selected
					INITIAL_FREQ = F11K;
					goto checkmenu;
				case 254:  // 8 KHz sound selected
					INITIAL_FREQ = F8K;
					checkmenu:
					WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 250, INITIAL_FREQ == F44K );
					WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 251, INITIAL_FREQ == F33K );
					WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 252, INITIAL_FREQ == F22K );
					WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 253, INITIAL_FREQ == F11K );
					WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 254, INITIAL_FREQ == F8K );
					if ( soundchangewarn && emu_thread_id ) {
						os2printf( "This change will not take effect until this game is restarted or another game is selected." );
						soundchangewarn = 0;
					}
				break;
				case 256: // 16 bit sound
					SOUND_QUALITY = 16;
					goto checkmenu2;
				case 255: // 8 bit sound
					SOUND_QUALITY = 8;
					checkmenu2:
					WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 256, SOUND_QUALITY == 16 );
					WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 255, SOUND_QUALITY == 8 );
					if ( soundchangewarn && emu_thread_id ) {
						os2printf( "This change will not take effect until this game is restarted or another game is selected." );
						soundchangewarn = 0;
					}
					break; */
				case ResetGame:
					if ( MBID_YES != WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, "Are you sure you want to do a complete reset?", "Confirmation", 0, MB_YESNO | MB_ICONQUESTION | MB_SYSTEMMODAL | MB_MOVEABLE ) )
					    break;
					quitnotify = 2;
					trygame = GAME_TO_TRY;
					{ extern int usres; // defined in cpuintrf.c
					   usres = 1; } // Force CPU loop to quit.
					last_char = 1;
					if ( key_released != 0 ) DosPostEventSem( key_released );
					key_released = 0;
					if ( key_pressed != 0 ) DosPostEventSem( key_pressed );
					key_pressed = 0;
					// Simulate pressing ESC to force end of emulation

				break;
				case OpenGame:
					if ( emu_thread_id ) {
						// If there is an emulator thread, we have to stop and restart it after this
						// action finishes.
						trygame = GAME_TO_TRY;
						if ( MBID_YES != WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, "This will end your current game.  Are you sure?", "Annoying confirmation", 0, MB_YESNO | MB_ICONQUESTION | MB_SYSTEMMODAL | MB_MOVEABLE ) )
 	    					    break;
						{ extern int usres; // defined in cpuintrf.c
						   usres = 1; } // Force CPU loop to quit.

						quitnotify = 3;

						last_char = 1;
						if ( key_released != 0 ) DosPostEventSem( key_released );
						key_released = 0;
						if ( key_pressed != 0 ) DosPostEventSem( key_pressed );
						key_pressed = 0;

						// Simulate pressing ESC to force end of emulation
						break;
					}

					GAME_TO_TRY = trygame;
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), OpenGame, FALSE );
					WinDlgBox( HWND_DESKTOP, HWND_DESKTOP, QuickListDlg, 0, QuickList, NULL );
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), OpenGame, TRUE );
				break;
				case CreateQList:
					if ( emu_thread_id ) {
						// If there is an emulator thread, we have to stop and restart it after this
						// action finishes.
						if ( MBID_YES != WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, "I'll have to stop and reset the emulation engine to proceed.  Is this ok?", "Emulation Engine Warning", 0, MB_YESNO | MB_ICONQUESTION | MB_SYSTEMMODAL | MB_MOVEABLE ) )
 	    					    break;
						trygame = GAME_TO_TRY;
						{ extern int usres; // defined in cpuintrf.c
						   usres = 1; } // Force CPU loop to quit.

						quitnotify = 1;

						last_char = 1;
						if ( key_released != 0 ) DosPostEventSem( key_released );
						key_released = 0;
						if ( key_pressed != 0 ) DosPostEventSem( key_pressed );
						key_pressed = 0;

						// Simulate pressing ESC to force end of emulation
						break;
					}

					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), CreateQList, FALSE );
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), AddToQList, FALSE );
					WinDlgBox( HWND_DESKTOP, HWND_DESKTOP, ProgressDlgProc, 0, Progress, NULL );
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), CreateQList, TRUE );
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), AddToQList, TRUE );

					if ( thread_state == 3 ) {
						pauseonfocuschange |= 2;
						GAME_TO_TRY = trygame;
						StartupDlgHWND = WinLoadDlg( HWND_DESKTOP, HWND_DESKTOP, AboutDlg, 0, StartupProgress, NULL );
						emu_thread_id = _beginthread( EmulatorThread, NULL, 65536, NULL );
						DosSetPriority( PRTYS_THREAD, emu_thread_pclass, emu_thread_pdelta, emu_thread_id );
						thread_state = 0;
						WinSetMenuItemText( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), PauseGame, "~Pause\t[P]" );
					}
				break;
				case AddToQList: {
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), CreateQList, FALSE );
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), AddToQList, FALSE );
					WinDlgBox( HWND_DESKTOP, HWND_DESKTOP, AddOneToListProc, 0, AddOneDlg, NULL );
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), CreateQList, TRUE );
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), AddToQList, TRUE );
				}
				break;
				case ExitMame:
					WinSendMsg (win, WM_CLOSE, 0L, 0L) ;
				break;
				case ROMSearchPath:
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), ROMSearchPath, FALSE );
					WinDlgBox( HWND_DESKTOP, HWND_DESKTOP, ROMLocationsProc, 0, ROMLocations, NULL );
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), ROMSearchPath, TRUE );
				break;
				case AboutMame:
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), AboutMame, FALSE );
					WinDlgBox( HWND_DESKTOP, HWND_DESKTOP, AboutDlg, 0, AboutDialog, NULL );
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), AboutMame, TRUE );
				break;
				case PauseGame:
					// Simulate P key press and release
//					keystatus[ 25/8 ] |= 1 << (25%8);

//					if ( thread_state && emu_thread_id ) {
					if ( !SHORT1FROMMP(mp2) ) {
						if ( mousecapture ) { WinSetCapture( HWND_DESKTOP, win ); WinSetPointer( HWND_DESKTOP, 0 ); }
						if ( thread_state > 1 ) break;
						if ( soundreallyon ) { extern void osd_sound_enable( int ); osd_sound_enable(1); }
						WinSetMenuItemText( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), PauseGame, "~Pause\t[P]" );
						WinSetWindowText( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_TITLEBAR ), "M.A.M.E. for OS/2" );
						thread_state = 0;
						nonframeupdate = 0;
						hackedpause = 0;
					} else {
						if ( !emu_thread_id ) break;
						if ( mousecapture ) WinSetCapture( HWND_DESKTOP, 0 );
						if ( soundreallyon ) { extern void osd_sound_enable( int ); osd_sound_enable(0); }
						WinSetMenuItemText( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), PauseGame, "Un~pause\t[P]" );
						WinSetWindowText( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_TITLEBAR ), "M.A.M.E. for OS/2 - <PAUSED>" );
						thread_state = 1;
						nonframeupdate = 1;
						hackedpause = 1;
						// hack to make the high CPU utilization slow down quicker.
					}
				break;
				case InputDeviceOptions:
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), InputDeviceOptions, FALSE );
					WinDlgBox( HWND_DESKTOP, HWND_DESKTOP, InputOptionsProc, 0, InputDialog, NULL );
					WinEnableMenuItem( WinWindowFromID( WinQueryWindow( win, QW_PARENT), FID_MENU ), InputDeviceOptions, TRUE );
				break;
				case DoubleSize:
					i = 2*blitwidth;
					j = 2*blitheight;
					custom_size = 1;
					goto actual;
				case ActualSize:
					i = blitwidth;
					j = blitheight;
					custom_size = 0;
actual:
					WinQueryWindowRect( framewin, &(rcls[0]) );
					WinQueryWindowRect( clientwin, &(rcls[1]) );
					WinQueryWindowPos( framewin, &swp );

					/* blit height plus the height of the frame elements */
					iYPos = j + (rcls[0].yTop - rcls[0].yBottom - rcls[1].yTop + rcls[1].yBottom);

					iYDiff = swp.y;
					/* If the top of the window is above the desktop... */
					if ((iYPos + swp.y) > (centery<<1))
					  /* then make it just hit the top. */
					  iYDiff = (centery<<1) - iYPos;
					WinSetWindowPos( framewin, HWND_TOP, 0, 0, i + (rcls[0].xRight-rcls[0].xLeft-rcls[1].xRight+rcls[1].xLeft),
					    j + (rcls[0].yTop-rcls[0].yBottom-rcls[1].yTop+rcls[1].yBottom), SWP_SIZE );
					WinSetWindowPos( framewin, HWND_TOP, swp.x, iYDiff,
					  i + (rcls[0].xRight - rcls[0].xLeft - rcls[1].xRight + rcls[1].xLeft),
					  iYPos, SWP_SIZE|SWP_MOVE);
					// do this duplication to make sure the height is correct when the width changes (menu related)
				break;
				case ScreenGrab: {
					LONG format[2]; // Closest PM bitmap format to screen
					BITMAPINFO2 bmpi = {0};
					HBITMAP bmp;
					HPS hps2;
					DEVOPENSTRUC dop = { 0, "DISPLAY", NULL, 0, 0, 0, 0, 0, 0 };
					SIZEL size = {0, 0};
					HDC hdc;
					POINTL pts[3];
					hps = WinGetPS( win );
					hdc = DevOpenDC( WinQueryAnchorBlock( win ), OD_MEMORY, "*", 5, (PDEVOPENDATA)&dop, NULLHANDLE );
					hps2 = GpiCreatePS( WinQueryAnchorBlock( win ), hdc, &size, PU_PELS | GPIA_ASSOC );

					GpiQueryDeviceBitmapFormats( hps, 2, format );
					WinQueryWindowPos( win, &swp );
					bmpi.cbFix = sizeof(BITMAPINFO2);
					bmpi.cx = swp.cx;
					bmpi.cy = swp.cy;
					bmpi.cPlanes = 1; // format[0];
					bmpi.cBitCount = 24; // format[1];
					bmp = GpiCreateBitmap( hps, (PBITMAPINFOHEADER2)&bmpi, 0, 0, NULL );

					GpiSetBitmap( hps2, bmp );
					pts[0].x = 0; pts[0].y = 0;
					pts[1].x = swp.cx;  pts[1].y = swp.cy;
					pts[2].x = 0; pts[2].y = 0;

					GpiBitBlt( hps2, hps, 3, pts, ROP_SRCCOPY, BBO_IGNORE );
					WinOpenClipbrd( WinQueryAnchorBlock( win ) );
					WinSetClipbrdOwner( WinQueryAnchorBlock( win ), win );
					WinEmptyClipbrd( WinQueryAnchorBlock( win ) );
					WinSetClipbrdData( WinQueryAnchorBlock( win ), bmp, CF_BITMAP, CFI_HANDLE );
					WinAlarm( HWND_DESKTOP, WA_WARNING );
					WinSetClipbrdOwner( WinQueryAnchorBlock( win ), NULLHANDLE );
					WinCloseClipbrd( WinQueryAnchorBlock( win ) );
					WinReleasePS( hps );
					GpiSetBitmap( hps2, 0 );
					GpiDestroyPS( hps2 );
					DevCloseDC( hdc );
				}
				break;
				case PauseFocusChg:
					pauseonfocuschange = 1 - (pauseonfocuschange&1);
					WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), PauseFocusChg, pauseonfocuschange );
				break;
				case ScanLines:
					wantstochangescanmode = 1 - wantstochangescanmode;
					WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), ScanLines, wantstochangescanmode );
					if ( scanwarn && emu_thread_id ) {
						os2printf( "NOTE:  Change will not take effect until the emulation is restarted or a new game is picked.  (This warning will only be display once per session.)" );
						scanwarn = 0;
					}
				break;
				case ShowFPS:
					showfps = 1 - showfps;
					if ( emu_thread_id ) osd_clearbitmap( Machine->scrbitmap );
					WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), ShowFPS, showfps );
				break;
				case UseTIMER0:
					if ( useTIMER0 ) {
						useTIMER0 = 0;
						if ( timer ) { DosClose( timer ); timer = 0; }
					} else {
						ULONG action;
						ULONG ulOpenFlag = OPEN_ACTION_OPEN_IF_EXISTS;
						ULONG ulOpenMode = OPEN_FLAGS_FAIL_ON_ERROR | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE;
						if ( MBID_YES == WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, "NOTE:  Using TIMER0 will give you much more accurate frame rate regulation however it may prevent other concurrent DOS applications and WinOS/2 from functioning.  Are you sure you want to enable it?", "Enabling use of TIMER0 device", 0, MB_YESNO | MB_ICONQUESTION | MB_SYSTEMMODAL | MB_MOVEABLE ) ) {
						if ( DosOpen( "TIMER0$  ", &timer, &action, 0, 0, ulOpenFlag, ulOpenMode, NULL )) {
							os2printf( "Could not open TIMER0 device driver." );
							fprintf( fp, "Could not open TIMER0 device driver.\n" );
							return 0;
						} } else return 0;
						useTIMER0 = 1;
					}
					WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), UseTIMER0, useTIMER0 );
				break;
			}
		break;

		case WM_PAINT:
			if ( !vrnenable ) WinSendMsg( win, WM_VRNENABLE, 0, 0 );
			if ( err ) {
				hps = WinBeginPaint( win, 0, &rectl );
				GpiSetBackColor( hps, CLR_BLACK );
				GpiSetBackMix( hps, BM_OVERPAINT );
				WinEndPaint( hps );
			} else {
				if ( bufnum ) {
					DosRequestMutexSem( DiveBufferMutex, -1 );
					if (!err) err = DiveBlitImage( diveinst, bufnum, DIVE_BUFFER_SCREEN );
					if ( err ) {
						struct bmpdata {
							ULONG buffernum, linebytes, linetot;
							char ops_ok;
							char access_on;
							char safety;
							unsigned char *buffer;
						};
						fprintf( fp, "DIVE blit error!  Return code: %ld.\n", err );
						if ( err == 4097 ) {  // Source format error
							extern struct osd_bitmap *screen;
							fprintf( fp, "Source format is invalid.  Blitting buffer #%ld.  Struct len= %ld, fInvert = %ld, Blitdepth = %c%c%c%c, Size %ldx%ld, SrcPos %ld,%ld, Dither %ld, Dest FCC %lx, Dest size %ldx%ld, pos %ld,%ld, Screen Pos %ld,%ld, Rect %ld.\n", bufnum, BlSet.ulStructLen, BlSet.fInvert, (char)(BlSet.fccSrcColorFormat&255), (char)((BlSet.fccSrcColorFormat>>8)&255), (char)((BlSet.fccSrcColorFormat>>16)&255), (char)((BlSet.fccSrcColorFormat>>24)&255), BlSet.ulSrcWidth, BlSet.ulSrcHeight, BlSet.ulSrcPosX, BlSet.ulSrcPosY, BlSet.ulDitherType, BlSet.fccDstColorFormat, BlSet.ulDstWidth, BlSet.ulDstHeight, BlSet.lDstPosX, BlSet.lDstPosY, BlSet.lScreenPosX, BlSet.lScreenPosY, BlSet.ulNumDstRects );
							fprintf( fp, "Additional information: osd_bitmap size = %dx%d, depth = %dbpp, private = %lx, line 0 start = %lx, DIVE buffnum = %ld, Bytes/Line = %ld, # lines = %ld, Ops ok = %d, Access on = %d, Safety = %d, buffer start = %lx.\n",screen->width,screen->height, screen->depth, (unsigned long)(screen->_private), (unsigned long)(&(screen->line[0][0])), ((struct bmpdata *)(screen->_private))->buffernum, ((struct bmpdata *)(screen->_private))->linebytes, ((struct bmpdata *)(screen->_private))->linetot, ((struct bmpdata *)(screen->_private))->ops_ok, ((struct bmpdata *)(screen->_private))->access_on, ((struct bmpdata *)(screen->_private))->safety, (unsigned long)(((struct bmpdata *)(screen->_private))->buffer) );
							BlSet.fccSrcColorFormat = FOURCC_LUT8;
							err = DiveSetupBlitter(diveinst, &BlSet);
							if ( err ) { fprintf( fp, "DIVE setup blitter call failed!  Return code was %ld.\n", err ); }
							err = DiveBlitImage( diveinst, bufnum, DIVE_BUFFER_SCREEN );
							if ( err ) {
								fprintf( fp, "DIVE blitter call failed again!  Return code was %ld.\n", err );
								hps = WinBeginPaint( win, 0, &rectl );
								GpiErase( hps );
								WinEndPaint( hps );
							}
						}
					}
					DosReleaseMutexSem( DiveBufferMutex );
				}
			}
		break;

		case WM_VRNDISABLE:
			vrnenable = 0;
			DiveSetupBlitter(diveinst, 0);
			err = 1;
		break;

		case WM_VRNENABLE:
			vrnenable = 1;
			hps=WinGetPS(win);
			if ( hps == 0 ) { err=1; break; }
			hrgn=GpiCreateRegion(hps, 0L, NULL);
			if (hrgn) {
				err = WinQueryVisibleRegion(win, hrgn);
				rgnCtl.ircStart=0;
				rgnCtl.crc=50;
				rgnCtl.ulDirection=RECTDIR_LFRT_TOPBOT;
				err = GpiQueryRegionRects(hps, hrgn, NULL, &rgnCtl, rcls);
				if ( err ) { 
					err = 0; 
					WinQueryWindowPos(win, &swp);
					pointl.x=swp.x;
					pointl.y=swp.y;
					WinMapWindowPoints(WinQueryWindow( win, QW_PARENT ), HWND_DESKTOP, (POINTL*)&pointl, 1);
					BlSet.ulStructLen = sizeof( SETUP_BLITTER );
					BlSet.fInvert = 0;
					BlSet.fccSrcColorFormat = blitdepth;
					BlSet.ulSrcWidth = blitwidth;
					BlSet.ulSrcHeight = blitheight;
					  // if scanlines are enabled, mult by 2.
					if ( !initialbmp ) {
						BlSet.ulSrcHeight = (scanlines?2*BlSet.ulSrcHeight:BlSet.ulSrcHeight);
						BlSet.ulSrcPosX = gameX; // Safety margin of 8 should not be visible
						BlSet.ulSrcPosY = gameY; // Safety margin of 8 should not be visible
					} else {
						BlSet.ulSrcPosX = 0;
						BlSet.ulSrcPosY = 0;
					}
					BlSet.ulDitherType = 1;
					BlSet.fccDstColorFormat = FOURCC_SCRN;
					BlSet.ulDstWidth=swp.cx;
					BlSet.ulDstHeight=swp.cy;
					if ( swp.cx != 0 && swp.cy != 0 ) {
						sw = blitwidth / swp.cx;  sh = blitheight / swp.cy;
					} else sw = sh = 0;
					BlSet.lDstPosX = 0;
					BlSet.lDstPosY = 0;
					BlSet.lScreenPosX=pointl.x;
					BlSet.lScreenPosY=pointl.y;
					BlSet.ulNumDstRects=rgnCtl.crcReturned;
					BlSet.pVisDstRects=rcls;
					if ( rgnCtl.crcReturned == 0 ) { 
						// We don't have a visible region currently... This is not an error,
						// so just don't Blit.
						err = 0;
						DiveSetupBlitter( diveinst, NULL );
						GpiDestroyRegion(hps, hrgn);
						break;
					}
					err = DiveSetupBlitter(diveinst, &BlSet);
					if ( err ) { fprintf( fp, "DIVE setup blitter call failed!  Return code was %ld.\n", err ); }
					GpiDestroyRegion(hps, hrgn);
				} else err = 1;
			}
			WinReleasePS( hps );
		break;
	}
	return WinDefWindowProc( win, message, mp1, mp2 );
}

extern void diag( void );

int main( int argc, char *argv[] ) {
	HAB ab;
	HMQ messq;
	QMSG qmsg;
	ULONG temp,temp2,version;

	ULONG frameflgs= FCF_TITLEBAR | FCF_SYSMENU | FCF_SIZEBORDER | 
	    FCF_MINMAX | FCF_SHELLPOSITION | FCF_TASKLIST | FCF_ICON | FCF_MENU;

	ULONG action;
	ULONG ulOpenFlag = OPEN_ACTION_OPEN_IF_EXISTS;
	ULONG ulOpenMode = OPEN_FLAGS_FAIL_ON_ERROR | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE;

	char *tempbuf, *tempbuf2, rescanwarning = 0;

	ab = WinInitialize( 0 );

	mameini = PrfOpenProfile( ab, "MAMEOS2.INI" );

	fp = fopen( "Debug.Log", "w" );
//	errorlog = fp;

	DosCreateMutexSem( "\\SEM32\\MAMEDiveBufferMutex", &DiveBufferMutex, 0, 0 );
	if ( !DiveBufferMutex ) {
		fprintf( fp, "Critical error!:  Cannot create a mutex semaphore for accessing the DIVE buffer.\n" );
		return 1;
	}

	if ( argc > 1 && stricmp( argv[1], "-diag" )==0 ) {
		// start diagnostics
		diag();
		return 0;
	} else if ( argc > 1 && strcmp( argv[1], "@!#?@!" )==0 ) {
		for (GAME_TO_TRY = 0; drivers[GAME_TO_TRY]; GAME_TO_TRY++)
			fprintf(fp, "%s\n", drivers[GAME_TO_TRY]->name );
		fclose(fp);
		return 0;
	}
	temp = 1;
	PrfQueryProfileData( mameini, "General options", "Use TIMER0 device for better timing", &useTIMER0, &temp );

	if ( useTIMER0 ) {
		if ( DosOpen( "TIMER0$  ", &timer, &action, 0, 0, ulOpenFlag, ulOpenMode, NULL )) {
			printf( "Could not open TIMER0 device driver." );
			return 1;
		}
	}

	messq = WinCreateMsgQueue( ab, 0 );

	temp = sizeof( unsigned long );
	version = 0;
	PrfQueryProfileData( mameini, "MAME Version", "Most recent", &version, &temp );

	WinRegisterClass( ab, "MAME for OS/2 Main window", MameMain, 
	     CS_SIZEREDRAW | CS_MOVENOTIFY, 0 );
	WinRegisterClass( ab, "MAME Percent Bar", PercentProc, 0, 12 );
	WinRegisterClass( ab, "AnimatedROM", AnimatedROM, 0, 0 );
	WinRegisterClass( ab, "MAME Quarter Popper", QuarterPopper, 0, 0 );
	WinRegisterClass( ab, "Calibrator", Calibrator, 0, 0 );
	WinRegisterClass( ab, "SplashScreenScroller", SplashScreenScroller, 0, 0 );
	WinRegisterClass( ab, "BitmapScaler", BitmapScaler, 0, 0 );

	if ( !version ) {
		// If this INI file has no version number then it is too old to use.
		// Delete it and restore defaults.

		PrfCloseProfile( mameini );
		unlink( "MAMEOS2.INI" );
		mameini = PrfOpenProfile( ab, "MAMEOS2.INI" );
		version = VERNUM;
		os2printf( "The current version of MAMEOS2.INI does not contain version information.  It is too old to use.  A new one will be generated and you will have to re-scan your ROMs.  Your settings have also been reset.  Be sure to review them." );
		rescanwarning = 1;
		version = VERNUM_35b11pre;
	}

	if ( version == VERNUM_35b11pre ) {
		PrfWriteProfileData( mameini, "General options", "ROM Search Path", ".,roms\0", 7 );
		version = VERNUM_35b11;
	}

	if ( version == VERNUM_36b5 ) {
		UpdateIniFile( 0 );
		os2printf( "The contents of your quick list and favorites list were automatically updated so that they are pointing to the same games.  Newly supported games should be added using the Add One Game option or rescanning your quick list." );
		rescanwarning = 1;
	}

	if ( version == VERNUM_36b6 ) {
		UpdateIniFile( 1 );
		os2printf( "The contents of your quick list and favorites list were automatically updated so that they are pointing to the same games.  About to scan for newly supported games..." );
		rescanwarning = 1;

		newgamesearchonly = 1;
		WinDlgBox( HWND_DESKTOP, HWND_DESKTOP, ProgressDlgProc, 0, Progress, NULL );
		newgamesearchonly = 0;
	}

	if ( version != VERNUM ) {
		// This INI file was made or last updated by a different version of
		// MAME for OS/2.  Warn the user.
		if ( !rescanwarning ) {
			os2printf( "The current version of MAMEOS2.INI was last used with a different version of MAME (%ld).  You may have to re-scan your ROMs before using this version.  Hopefully you won't have to do this again in future releases.", version );
		}
		version = VERNUM;
		rescanwarning = 1;
	}

	PrfWriteProfileData( mameini, "MAME Version", "Most recent", &version, sizeof(unsigned long) );

	temp = 1;
	PrfQueryProfileData( mameini, "General options", "Show memorial message", &showcat, &temp );

	clientwin = WinLoadDlg( HWND_DESKTOP, HWND_DESKTOP, SplashScreenProc, 0, SplashScreen, NULL );
 	WinProcessDlg( clientwin );

	PrfQueryProfileSize( mameini, "General options", "ROM Search Path", &temp );
	if ( !temp ) {
		os2printf( "Bah!  There was no search path for ROMs found in your MAMEOS2.INI file!  I'll make a default one now.  This should not normally happen unless you are screwing around with the MAMEOS2.INI file or it got corrupted." );
		PrfWriteProfileData( mameini, "General options", "ROM Search Path", ".,roms\0", 7 );
		PrfQueryProfileSize( mameini, "General options", "ROM Search Path", &temp );
	}
	tempbuf = (char *)malloc( temp );
	PrfQueryProfileData( mameini, "General options", "ROM Search Path", tempbuf, &temp );
	numsearchdirs = 1; tempbuf2 = tempbuf;
	while ( tempbuf2[0] != 0 && (tempbuf2 = strchr( tempbuf2+1, ',' )) ) {
		numsearchdirs++;
	}
	searchpath = (char **)malloc( sizeof(char*) * numsearchdirs );
	tempbuf2 = strtok( tempbuf, "," );
	for ( temp=0; tempbuf2 && temp<numsearchdirs; temp++ ) {
		searchpath[temp] = (char *)malloc( strlen(tempbuf2)+1 );
		strcpy( searchpath[temp], tempbuf2 );
		tempbuf2 = strtok( NULL, "," );
	}
	free( tempbuf );
	tempbuf = tempbuf2 = NULL;

	temp = 1;
	PrfQueryProfileData( mameini, "General options", "Enable scan lines", &scanlines, &temp );
	wantstochangescanmode = scanlines;
	temp = 1;
	PrfQueryProfileData( mameini, "General options", "Pause game on focus change", &pauseonfocuschange, &temp );
	temp = 1;
	PrfQueryProfileData( mameini, "General options", "Show FPS counter", &showfps, &temp );
	temp = 2;
	PrfQueryProfileData( mameini, "General options", "Sound Sampling Frequency", &INITIAL_FREQ, &temp );
	temp = 1;
	PrfQueryProfileData( mameini, "General options", "Sound Quality", &SOUND_QUALITY, &temp );
	temp = 1;
	PrfQueryProfileData( mameini, "General options", "Auto Frame Skipping", &autoskip, &temp );
	temp = 1;
	PrfQueryProfileData( mameini, "General options", "Max Frame Skipping", &maxskip, &temp );
	temp = 1;
	PrfQueryProfileData( mameini, "General options", "Auto Slow Down", &autoslow, &temp );
	temp = 1;
	nocheat = 0;
	PrfQueryProfileData( mameini, "General options", "Cheat", &nocheat, &temp );
	nocheat = !nocheat;
	temp = 1;
	PrfQueryProfileData( mameini, "General options", "Game sound enabled", &soundon, &temp );
	temp = 1;
	PrfQueryProfileData( mameini, "General options", "Allow keyboard LEDs to flash", &allowledflash, &temp );
	temp = 1;
	PrfQueryProfileData( mameini, "General options", "GPMIXER device to use", &GPMIXERdevice, &temp );
	temp = 4;
	PrfQueryProfileData( mameini, "General options", "GPMIXER audio buffer size", &AUDIO_BUF_SIZE, &temp );
	if ( !AUDIO_BUF_SIZE ) {
		struct DEVICE_DRIVER aud_drv;
		if ( GPMIXERdevice >= AudioGetNumberOfDevices() ) {
			os2printf( "INI file contained invalid data for the audio device to use.  Using default audio device." );
			GPMIXERdevice = 0;
		}
		AudioGetDeviceDriver( GPMIXERdevice, &aud_drv );
		AUDIO_BUF_SIZE = aud_drv.suggested_bufsize;
	}

	DosSetPriority( PRTYS_THREAD, PRTYC_REGULAR, 1, 1 );
	DosSetPriority( PRTYS_THREAD, PRTYC_REGULAR, 1, 2 );
	// Boost the priority of this thread (the main dispatcher)

	JoystickInit(0);
	if ( !JoystickOn() ) joydetected = 1;
	JoystickOff();

	framewin = WinCreateStdWindow( HWND_DESKTOP, WS_VISIBLE | WS_ANIMATE,
	    &frameflgs, "MAME for OS/2 Main window", "M.A.M.E. for OS/2",
	    0, 0, 1, &clientwin );
	WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), PauseFocusChg, pauseonfocuschange );
	WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), ScanLines, scanlines );
	WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), ShowFPS, showfps );
	WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), UseTIMER0, useTIMER0 );
	WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 250, INITIAL_FREQ > F33K );
	WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 251, INITIAL_FREQ < F44K && INITIAL_FREQ > F22K );
	WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 252, INITIAL_FREQ < F33K && INITIAL_FREQ > F11K );
	WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 253, INITIAL_FREQ < F22K && INITIAL_FREQ > F8K );
	WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 254, INITIAL_FREQ < F11K );
	WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 256, SOUND_QUALITY == 16 );
	WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 255, SOUND_QUALITY == 8 );
	WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 210, !nocheat );
	WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 257, !soundon );
	WinCheckMenuItem( WinWindowFromID( framewin, FID_MENU ), 211, allowledflash );
	WinSetVisibleRegionNotify( clientwin, TRUE );
	WinPostMsg(framewin, WM_VRNENABLE, 0L, 0L);
	WinPostMsg(framewin, WM_REALIZEPALETTE, 0L, 0L);
	OldFrameWinProc = WinSubclassWindow( framewin, FrameWinKeyBypass );

	if ( argc > 1 ) {
		for (GAME_TO_TRY = 0; drivers[GAME_TO_TRY]; GAME_TO_TRY++)
			if (stricmp(argv[1],drivers[GAME_TO_TRY]->name) == 0) break;
		if ( drivers[GAME_TO_TRY] == 0 ) {
			WinMessageBox( HWND_DESKTOP, HWND_DESKTOP, "The chosen game is not supported by this version of MAME.", "Game initialization", 0, MB_CANCEL | MB_ERROR | MB_SYSTEMMODAL | MB_MOVEABLE );
			GAME_TO_TRY = 0xffff;
		} else {
			pauseonfocuschange |= 2;
			StartupDlgHWND = WinLoadDlg( HWND_DESKTOP, HWND_DESKTOP, AboutDlg, 0, StartupProgress, NULL );

			emu_thread_id = _beginthread( EmulatorThread, NULL, 65536, NULL );
			DosSetPriority( PRTYS_THREAD, emu_thread_pclass, emu_thread_pdelta, emu_thread_id );
		}
	}

	if ( WinQueryFocus( HWND_DESKTOP != clientwin ) ) {
		WinSetFocus( HWND_DESKTOP, clientwin );
	}

	while (WinGetMsg (ab, &qmsg, NULLHANDLE, 0, 0))
	    WinDispatchMsg (ab, &qmsg) ;
	WinDestroyWindow (framewin) ;

	if ( devicesenabled & 4 ) {
		JoystickOff();
		JoystickSaveCalibration();
	}

	WinDestroyMsgQueue (messq) ;

	temp2 = 0;
	for ( temp=0; temp<numsearchdirs; temp++ ) {
		temp2 += strlen( searchpath[temp] )+1;
	}
	tempbuf = (char *)malloc( temp2 );
	strcpy( tempbuf, searchpath[0] );
	for ( temp=1; temp<numsearchdirs; temp++ ) {
		strcat( tempbuf, "," );
		strcat( tempbuf, searchpath[temp] );
		free( searchpath[temp] );
	}
	PrfWriteProfileData( mameini, "General options", "ROM Search Path", tempbuf, temp2 );
	free( searchpath );
	free( tempbuf );

	showcat = 0;
	PrfWriteProfileData( mameini, "General options", "Show memorial message", &showcat, 1 );
	PrfWriteProfileData( mameini, "General options", "Enable scan lines", &wantstochangescanmode, 1 );
	PrfWriteProfileData( mameini, "General options", "Pause game on focus change", &pauseonfocuschange, 1 );
	PrfWriteProfileData( mameini, "General options", "Show FPS counter", &showfps, 1 );
	PrfWriteProfileData( mameini, "General options", "Use TIMER0 device for better timing", &useTIMER0, 1 );
	PrfWriteProfileData( mameini, "General options", "Sound Sampling Frequency", &INITIAL_FREQ, 2 );
	PrfWriteProfileData( mameini, "General options", "Sound Quality", &SOUND_QUALITY, 1 );
	PrfWriteProfileData( mameini, "General options", "Auto Frame Skipping", &autoskip, 1 );
	PrfWriteProfileData( mameini, "General options", "Max Frame Skipping", &maxskip, 1 );
	PrfWriteProfileData( mameini, "General options", "Auto Slow Down", &autoslow, 1 );
	PrfWriteProfileData( mameini, "General options", "Game sound enabled", &soundon, 1 );
	nocheat = !nocheat;
	PrfWriteProfileData( mameini, "General options", "Cheat", &nocheat, 1 );
	PrfWriteProfileData( mameini, "General options", "Allow keyboard LEDs to flash", &allowledflash, 1 );
	PrfWriteProfileData( mameini, "General options", "GPMIXER device to use", &GPMIXERdevice, 1 );
	PrfWriteProfileData( mameini, "General options", "GPMIXER audio buffer size", &AUDIO_BUF_SIZE, 4 );

	PrfCloseProfile( mameini );
	WinTerminate (ab) ;

	DosCloseMutexSem( DiveBufferMutex );

	fclose(fp);
	DosClose( timer );
	return 0;
}
