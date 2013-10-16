// SpiceWare Joystick Support
// by Darrell Spice Jr.         February 16, 1997

#define INCL_DOSFILEMGR         // include needed for DosOpen call
#define INCL_DOSDEVICES         // include needed for DosDevIOCtl call
#define INCL_DOSDEVIOCTL        // include needed for DosDevIOCtl call
#include <os2.h>
#include <stdio.h>
// #include <io.h>                 // included for file i/o
#include "joyos2.h"


long int SWjoymin[4]={0xffff,0xffff,0xffff,0xffff}; // Calibration Information
long int SWjoymax[4]={0,0,0,0};                     // Calibration Information

long int SWlow   = -1; // low range value
long int SWhigh  = 1;  // high range value
long int SWrange = 3;  // number of discrete values(inclusive) between SWlow and SWhigh

HFILE SWhGame;     // game port file handle

//-----------------------------------------------------------------------------
// JoystickInit(a)
//--------------------
// This routine will initialize the joystick calibration information.  It will
// load this information from the file JOYSTICK.CLB, created by the
// JoystickSaveCalibration routine or it will default it to min = 0xffff,
// and max = 0.  As the joystick is read, the min and max values will self
// adjust.  This function will normally be passed a value of 0.
//
// NOTE: You may wish to have a re-calibrate option in your program, and
//       have it call this function passing a value of 1
//
// NOTE: This function is optional if you do not wish to use saved calibration
//       information.
//
// parameters passed
//   a = 0, attempt to load in saved joystick calibration information
//   a = 1, default the calibration information, min = 0xffff, max = 0
//
// returns TRUE if calibration info loaded
//         FALSE if defaults are loaded
//-----------------------------------------------------------------------------
int JoystickInit(int a)
{
   GAME_STATUS_STRUCT gameStatus;      // joystick readings
   ULONG dataLen = sizeof(gameStatus); // length of gameStatus
   APIRET rc = 0;                      // return code from DosDevIOCtl
   FILE *cal;

    if (a == 0)    // load JOYSTICK.CLB routine
    {
       cal = fopen( "joystick.clb", "ra" );

       if (cal)
       {
          fscanf( cal, "%d\n%d\n%d\n%d\n%d\n%d\n%d\%d\n", &(SWjoymin[0]), &(SWjoymin[1]), &(SWjoymin[2]), &(SWjoymin[3]), &(SWjoymax[0]), &(SWjoymax[1]), &(SWjoymax[2]), &(SWjoymax[3]) );
          fclose( cal );
       }
       // check if values look ok, in case user messed with the joystick.clb file
       if ((SWjoymin[0] < 0) || (SWjoymin[1] < 0) || (SWjoymin[2] < 0) || (SWjoymin[3] < 0) ||
           (SWjoymax[0] < 0) || (SWjoymax[1] < 0) || (SWjoymax[2] < 0) || (SWjoymax[3] < 0) ||
           (SWjoymin[0] > 0xffff) || (SWjoymin[1] > 0xffff) || (SWjoymin[2] > 0xffff) || (SWjoymin[3] > 0xffff) ||
           (SWjoymin[0] > 0xffff) || (SWjoymin[1] > 0xffff) || (SWjoymin[2] > 0xffff) || (SWjoymin[3] > 0xffff))
           a = 1;
       if (SWjoymin[0] == 0)   // if value is 0, then last time we ran there was no joystick
       {                       // plugged in.  If this is the case, reset the min/max
          SWjoymin[0] = 0xffff;// values in case there is now a joystick
          SWjoymax[0] = 0;
       }
       if (SWjoymin[1] == 0)
       {
          SWjoymin[1] = 0xffff;
          SWjoymax[1] = 0;
       }
       if (SWjoymin[2] == 0)
       {
          SWjoymin[2] = 0xffff;
          SWjoymax[2] = 0;
       }
       if (SWjoymin[3] == 0)
       {
          SWjoymin[3] = 0xffff;
          SWjoymax[3] = 0;
       }

       rc = 1; // set as an invalid reading
       if (SWhGame) // if port is open, then take a reading
          rc = DosDevIOCtl(SWhGame, IOCTL_CAT_USER, GAME_GET_STATUS, NULL, 0, NULL, &gameStatus, dataLen, &dataLen);
       else         // otherwise open, read, close
       {
          if (JoystickOn() == 0)
          {
             rc = DosDevIOCtl(SWhGame, IOCTL_CAT_USER, GAME_GET_STATUS, NULL, 0, NULL, &gameStatus, dataLen, &dataLen);
             JoystickOff();
          }
       }
       if (rc == 0) // if a valid reading, then check for missing joysticks
       {
          if (gameStatus.curdata.A.x == 0)  // check if joystick 1 is not plugged in
          {                                 // and reset min/max if it isn't
             SWjoymin[0] = SWjoymin[1] = 0xffff;
             SWjoymax[0] = SWjoymax[1] = 0;
          }
          if (gameStatus.curdata.B.x == 0)  // check if joystick 2 is not plugged in
          {                                 // and reset min/max if it isn't
             SWjoymin[2] = SWjoymin[3] = 0xffff;
             SWjoymax[2] = SWjoymax[3] = 0;
          }
       }

       if (a == 0) return TRUE;
    }
    // load defaults.  Happens if a = 1, or if JOYSTICK.CLB load failed
    SWjoymin[0] = SWjoymin[1] = SWjoymin[2] = SWjoymin[3] = 0xffff;
    SWjoymax[0] = SWjoymax[1] = SWjoymax[2] = SWjoymax[3] = 0;
    return FALSE;
}




//-----------------------------------------------------------------------------
// JoystickSaveCalibration()
//------------------------------
// This routine saves the min/max calibration information to the file
// JOYSTICK.CLB so it can be recalled later.
//
// NOTE: This function is optional if you do not wish to use saved calibration
//       information.
//
// parameters passed, none
//
// returns TRUE if information was saved
//         FALSE if information not saved
//-----------------------------------------------------------------------------
int JoystickSaveCalibration(void)
{
    FILE *cal;
    cal = fopen( "joystick.clb", "wa" );
    if (cal)
    {
       fprintf( cal, "%d\n%d\n%d\n%d\n%d\n%d\n%d\%d\n", SWjoymin[0], SWjoymin[1], SWjoymin[2], SWjoymin[3], SWjoymax[0], SWjoymax[1], SWjoymax[2], SWjoymax[3] );
       fclose(cal);
       return TRUE;
    }
    return FALSE;
}




//-----------------------------------------------------------------------------
// JoystickRange(int low, int high)
//---------------------------------
// This routine sets the range of values to be returned by the JoystickValues
// function.  If the low value is greater or equal to than the high value,
// the default range of -1 to 1 (for digital style joysticks) will be used.
// Typical ranges would be -1 to 1 for direction only results, -5 to 5 for
// speed and directly results from an analog joystick, or 0 to 600 for a paddle
// style game such as breakout.
//
// NOTE: This function is optional if you only wish to receive "digital" values
//       of -1, 0, and 1 for each axis on the joystick
//       information.
//
// parameters passed
//   low  = lowest value to be returned by JoystickValues
//   high = highest value to be returned by JoystickValues
//
// returns TRUE if range accepted
//         FALSE if range defaulted to -1 to 1
//-----------------------------------------------------------------------------
int JoystickRange(int low, int high)
{
    if (low < high)
    {
       SWlow   = low;
       SWhigh  = high;
       SWrange = high - low + 1; //add 1 because its an inclusive range
       return TRUE;
    }
    else
    {
       SWlow   = -1; // default low range value
       SWhigh  = 1;  // default high range value
       SWrange = 3;  // three discrete default values (-1, 0, 1)
       return FALSE;
    }
}




//-----------------------------------------------------------------------------
// JoystickOn()
//-------------
// this routine opens the joystick port for reading
//
// NOTE: this function must be used for JoystickValues to work
//
// NOTE: I've experienced a problem using DosStartSession while the joystick
//       port is open.  The port should be closed by calling JoystickOff()
//       before the DosStartSession call, and then reopened by calling
//       JoystickOn() after the call.
//
// returns 0 if port active
//         DosOpen error value if port open failed
//-----------------------------------------------------------------------------
int JoystickOn(void)
{
   ULONG action;    // return value from DosOpen

   return DosOpen(GAMEPDDNAME, &SWhGame, &action, 0, FILE_READONLY, FILE_OPEN,
                  OPEN_ACCESS_READONLY | OPEN_SHARE_DENYNONE, NULL);
}




//-----------------------------------------------------------------------------
// JoystickOff()
//--------------
// This routine closes the joystick port.
//
// parameters, none
//
// returns TRUE if port closed
//         FALSE if close failed
//-----------------------------------------------------------------------------
int JoystickOff(void)
{
   APIRET rc = 0;       // return code from DosClose

   if (SWhGame)
     rc = DosClose(SWhGame);

   if (rc == 0)
      return TRUE;
   else
      return FALSE;
}




//-----------------------------------------------------------------------------
// JoystickValues(stick)
//----------------------
// This routine reads the values from the joystick port, and returns values
// via the passed JOYSTICK_STATUS variable pointer.  The X and Y axis values
// are adjusted to have values ranging from SWlow to SWhigh (inclusive).
//
// parameters passed
//   pointer to a JOYSTICK_STATUS structure
//
// returns
//      TRUE if valid values are passed back via the JOYSTICK_STATUS structure,
//      FALSE if the joystick could not be read
//-----------------------------------------------------------------------------
int JoystickValues(JOYSTICK_STATUS * stick)
{
   static GAME_STATUS_STRUCT gameStatus;      // joystick readings
   static ULONG dataLen = sizeof(gameStatus); // length of gameStatus
   static APIRET rc = 0;                      // return code from DosDevIOCtl

   if (SWhGame == 0) return FALSE; // exit if game port is not opened

   rc = DosDevIOCtl(SWhGame, IOCTL_CAT_USER, GAME_GET_STATUS, NULL, 0, NULL, &gameStatus, dataLen, &dataLen);
   if (rc != 0) return FALSE;      // exit if reading failed;

   if (gameStatus.curdata.A.x < SWjoymin[0]) SWjoymin[0] = gameStatus.curdata.A.x;  // adjust minimum values
   if (gameStatus.curdata.A.y < SWjoymin[1]) SWjoymin[1] = gameStatus.curdata.A.y;
   if (gameStatus.curdata.B.x < SWjoymin[2]) SWjoymin[2] = gameStatus.curdata.B.x;
   if (gameStatus.curdata.B.y < SWjoymin[3]) SWjoymin[3] = gameStatus.curdata.B.y;

   if (SWjoymax[0] == 0) SWjoymax[0] = SWjoymin[0] + 1;
   if (SWjoymax[1] == 0) SWjoymax[1] = SWjoymin[1] + 1;
   if (SWjoymax[2] == 0) SWjoymax[2] = SWjoymin[2] + 1;
   if (SWjoymax[3] == 0) SWjoymax[3] = SWjoymin[3] + 1;

   if (gameStatus.curdata.A.x > SWjoymax[0]) SWjoymax[0] = gameStatus.curdata.A.x;  // adjust maximum values
   if (gameStatus.curdata.A.y > SWjoymax[1]) SWjoymax[1] = gameStatus.curdata.A.y;
   if (gameStatus.curdata.B.x > SWjoymax[2]) SWjoymax[2] = gameStatus.curdata.B.x;
   if (gameStatus.curdata.B.y > SWjoymax[3]) SWjoymax[3] = gameStatus.curdata.B.y;

   // calculate joystick X and Y values based on range of values requested
   if (gameStatus.curdata.A.x == 0)  // check if joystick 1 is not plugged in
      stick->Joy1X = stick->Joy1Y = SWlow + SWrange / 2; // give a middle reading for missing joystick
   else
   {
      stick->Joy1X = ((gameStatus.curdata.A.x - SWjoymin[0]) * SWrange) / (SWjoymax[0] - SWjoymin[0]) + SWlow;
      stick->Joy1Y = ((gameStatus.curdata.A.y - SWjoymin[1]) * SWrange) / (SWjoymax[1] - SWjoymin[1]) + SWlow;
   }

   if (gameStatus.curdata.B.x == 0)  // check if joystick 2 is not plugged in
      stick->Joy2X = stick->Joy2Y = SWlow + SWrange / 2; // give a middle reading for missing joystick
   else
   {
      stick->Joy2X = ((gameStatus.curdata.B.x - SWjoymin[2]) * SWrange) / (SWjoymax[2] - SWjoymin[2]) + SWlow;
      stick->Joy2Y = ((gameStatus.curdata.B.y - SWjoymin[3]) * SWrange) / (SWjoymax[3] - SWjoymin[3]) + SWlow;
   }

   if(stick->Joy1X > SWhigh) stick->Joy1X = SWhigh; // make sure high range is
   if(stick->Joy1Y > SWhigh) stick->Joy1Y = SWhigh; // not exceeded.  This can
   if(stick->Joy2X > SWhigh) stick->Joy2X = SWhigh; // happen due to rounding
   if(stick->Joy2Y > SWhigh) stick->Joy2Y = SWhigh; // errors

   stick->Joy1A = !(gameStatus.curdata.butMask & 0x10); // firebutton values
   stick->Joy1B = !(gameStatus.curdata.butMask & 0x20); // are TRUE if
   stick->Joy2A = !(gameStatus.curdata.butMask & 0x40); // pressed, else
   stick->Joy2B = !(gameStatus.curdata.butMask & 0x80); // they are FALSE

   return TRUE;
}
