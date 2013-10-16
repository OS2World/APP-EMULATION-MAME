/********************************************************************************
*										*
*	Define size independent data types and operations.			*
*										*
*   The following types must be supported by all platforms:			*
*										*
*	UINT8  - Unsigned 8-bit Integer		INT8  - Signed 8-bit integer	*
*	UINT16 - Unsigned 16-bit Integer	INT16 - Signed 16-bit integer	*
*	UINT32 - Unsigned 32-bit Integer	INT32 - Signed 32-bit integer	*
*	UINT64 - Unsigned 64-bit Integer	INT64 - Signed 64-bit integer	*
*										*
*										*
*   The macro names for the artithmatic operations are composed as follows:	*
*										*
*   XXX_R_A_B, where XXX - 3 letter operation code (ADD, SUB, etc.)		*
*			 R   - The type	of the result				*
*			 A   - The type of operand 1				*
*		         B   - The type of operand 2 (if binary operation)	*
*										*
*		     Each type is one of: U8,8,U16,16,U32,32,U64,64		*
*										*
********************************************************************************/


/***************************** DOS Section ************************************/

#ifndef OSD_CPU_H
#define OSD_CPU_H

#if defined DJGPP || defined UNIX || defined OS2

typedef unsigned char	UINT8;
typedef unsigned short	UINT16;
typedef unsigned int	UINT32;
typedef signed char 	INT8;
typedef signed short	INT16;
typedef signed int	INT32;

#ifdef UNIX

#ifdef __LP64__
typedef signed   long	INT64;
typedef unsigned long	UINT64;
#else
typedef signed   long long	INT64;
typedef unsigned long long	UINT64;
#endif

#else
__extension__	typedef signed long long	INT64;
__extension__	typedef unsigned long long	UINT64;
#endif

/* Combine to 32-bit integers into a 64-bit integer */
#define COMBINE_64_32_32(A,B)     ((((UINT64)(A))<<32) | (B))
#define COMBINE_U64_U32_U32(A,B)  COMBINE_64_32_32(A,B)

/* Return upper 32 bits of a 64-bit integer */
#define HI32_32_64(A)		  (((UINT64)(A)) >> 32)
#define HI32_U32_U64(A)		  HI32_32_64(A)

/* Return lower 32 bits of a 64-bit integer */
#define LO32_32_64(A)		  ((A) & 0xffffffff)
#define LO32_U32_U64(A)		  LO32_32_64(A)

#define DIV_64_64_32(A,B)	  ((A)/(B))
#define DIV_U64_U64_U32(A,B)  ((A)/(UINT32)(B))

#define MOD_32_64_32(A,B)	  ((A)%(B))
#define MOD_U32_U64_U32(A,B)  ((A)%(UINT32)(B))

#define MUL_64_32_32(A,B)	  ((A)*(INT64)(B))
#define MUL_U64_U32_U32(A,B)  ((A)*(UINT64)(UINT32)(B))

#endif

/***************************** Mac Section ************************************/

#ifdef macintosh

				typedef unsigned char		UINT8;
				typedef unsigned short		UINT16;
				typedef unsigned int		UINT32;
				typedef unsigned long long	UINT64;
				typedef signed char 		INT8;
				typedef signed short		INT16;
				typedef signed int			INT32;
				typedef signed long long	INT64;

/* Combine to 32-bit integers into a 64-bit integer */
#define COMBINE_64_32_32(A,B)     ((((UINT64)(A))<<32) | (B))
#define COMBINE_U64_U32_U32(A,B)  COMBINE_64_32_32(A,B)

/* Return upper 32 bits of a 64-bit integer */
#define HI32_32_64(A)		  (((UINT64)(A)) >> 32)
#define HI32_U32_U64(A)		  HI32_32_64(A)

/* Return lower 32 bits of a 64-bit integer */
#define LO32_32_64(A)		  ((A) & 0xffffffff)
#define LO32_U32_U64(A)		  LO32_32_64(A)

#define DIV_64_64_32(A,B)	  ((A)/(B))
#define DIV_U64_U64_U32(A,B)  ((A)/(UINT32)(B))

#define MOD_32_64_32(A,B)	  ((A)%(B))
#define MOD_U32_U64_U32(A,B)  ((A)%(UINT32)(B))

#define MUL_64_32_32(A,B)	  ((A)*(INT64)(B))
#define MUL_U64_U32_U32(A,B)  ((A)*(UINT64)(UINT32)(B))

#endif

/***************************** OS/2 Section ***********************************/
#define PI            3.14159265358979323846
#undef HAS_Z80 
#undef HAS_8080 
#undef HAS_8085A 
#undef HAS_H6280 
#undef HAS_M6502 
#undef HAS_M65C02 
#undef HAS_M6510 
#undef HAS_I86 
#undef HAS_I8035 
#undef HAS_I8039 
#undef HAS_I8048 
#undef HAS_N7751 
#undef HAS_M6800 
#undef HAS_M6801 
#undef HAS_M6802 
#undef HAS_M6803 
#undef HAS_M6808 
#undef HAS_HD63701 
#undef HAS_M6805 
#undef HAS_M68705 
#undef HAS_HD63705 
#undef HAS_M6309 
#undef HAS_M6809 
#undef HAS_M68000 
#undef HAS_M68010 
#undef HAS_M68020 
#undef HAS_T11 
#undef HAS_S2650 
#undef HAS_TMS34010 
#undef HAS_TMS9900 
#undef HAS_Z8000 
#undef HAS_TMS320C10 
#undef HAS_CCPU 
#undef HAS_CUSTOM 
#undef HAS_SAMPLES 
#undef HAS_DAC 
#undef HAS_AY8910 
#undef HAS_YM2203 
#undef HAS_YM2151 
#undef HAS_YM2151_ALT 
#undef HAS_YM2610 
#undef HAS_YM2612 
#undef HAS_YM3438 
#undef HAS_YM2413 
#undef HAS_YM3812 
#undef HAS_YM3526 
#undef HAS_SN76496 
#undef HAS_POKEY 
#undef HAS_NES 
#undef HAS_ASTROCADE 
#undef HAS_NAMCO 
#undef HAS_TMS5220 
#undef HAS_VLM5030 
#undef HAS_ADPCM 
#undef HAS_OKIM6295 
#undef HAS_MSM5205 
#undef HAS_UPD7759 
#undef HAS_HC55516 
#undef HAS_K007232 
#undef HAS_YM2610B

#define HAS_Z80 1
#define HAS_8080 1
#define HAS_8085A 1
#define HAS_H6280 1
#define HAS_M6502 1
#define HAS_M65C02 1
#define HAS_M6510 1
#define HAS_I86 1
#define HAS_I8035 1
#define HAS_I8039 1
#define HAS_I8048 1
#define HAS_N7751 1
#define HAS_M6800 1
#define HAS_M6801 1
#define HAS_M6802 1
#define HAS_M6803 1
#define HAS_M6808 1
#define HAS_HD63701 1
#define HAS_M6805 1
#define HAS_M68705 1
#define HAS_HD63705 1
#define HAS_M6309 1
#define HAS_M6809 1
#define HAS_M68000 1
#define HAS_M68010 1
#define HAS_M68020 1
#define HAS_T11 1
#define HAS_S2650 1
#define HAS_TMS34010 1
#define HAS_TMS9900 1
#define HAS_Z8000 1
#define HAS_TMS320C10 1
#define HAS_CCPU 1
#define HAS_CUSTOM 1
#define HAS_SAMPLES 1
#define HAS_DAC 1
#define HAS_AY8910 1
#define HAS_YM2203 1
#define HAS_YM2151 1
#define HAS_YM2151_ALT 1
#define HAS_YM2610 1
#define HAS_YM2610B 1
#define HAS_YM2612 1
#define HAS_YM3438 1
#define HAS_YM2413 1
#define HAS_YM3812 1
#define HAS_YM3526 1
#define HAS_SN76496 1
#define HAS_POKEY 1
#define HAS_NES 1
#define HAS_ASTROCADE 1
#define HAS_NAMCO 1
#define HAS_TMS5220 1
#define HAS_VLM5030 1
#define HAS_ADPCM 1
#define HAS_OKIM6295 1
#define HAS_MSM5205 1
#define HAS_UPD7759 1
#define HAS_HC55516 1
#define HAS_K007232 1
#define HAS_KONAMI 1
#define HAS_V20 1
#define HAS_V30 1
#define HAS_V33 1
#define HAS_K053260 1
#define HAS_RF5C68 1
#define HAS_SEGAPCM 1
#define HAS_TIA 1
#define HAS_YM2608 1
#define HAS_N2A03 1
#define HAS_CEM3394 1

#define printf os2printf

#define OSD_JOY_LEFT    1
#define OSD_JOY_RIGHT   2
#define OSD_JOY_UP      3
#define OSD_JOY_DOWN    4
#define OSD_JOY_FIRE1   5
#define OSD_JOY_FIRE2   6
#define OSD_JOY_FIRE3   7
#define OSD_JOY_FIRE4   8
#define OSD_JOY_FIRE5   9
#define OSD_JOY_FIRE6   10
#define OSD_JOY_FIRE7   11
#define OSD_JOY_FIRE8   12
#define OSD_JOY_FIRE9   13
#define OSD_JOY_FIRE10  14
#define OSD_JOY_FIRE    15      /* any of the first joystick fire buttons */
#define OSD_JOY2_LEFT   16
#define OSD_JOY2_RIGHT  17
#define OSD_JOY2_UP     18
#define OSD_JOY2_DOWN   19
#define OSD_JOY2_FIRE1  20
#define OSD_JOY2_FIRE2  21
#define OSD_JOY2_FIRE3  22
#define OSD_JOY2_FIRE4  23
#define OSD_JOY2_FIRE5  24
#define OSD_JOY2_FIRE6  25
#define OSD_JOY2_FIRE7  26
#define OSD_JOY2_FIRE8  27
#define OSD_JOY2_FIRE9  28
#define OSD_JOY2_FIRE10 29
#define OSD_JOY2_FIRE   30      /* any of the second joystick fire buttons */
#define OSD_JOY3_LEFT   31
#define OSD_JOY3_RIGHT  32
#define OSD_JOY3_UP     33
#define OSD_JOY3_DOWN   34
#define OSD_JOY3_FIRE1  35
#define OSD_JOY3_FIRE2  36
#define OSD_JOY3_FIRE3  37
#define OSD_JOY3_FIRE4  38
#define OSD_JOY3_FIRE5  39
#define OSD_JOY3_FIRE6  40
#define OSD_JOY3_FIRE7  41
#define OSD_JOY3_FIRE8  42
#define OSD_JOY3_FIRE9  43
#define OSD_JOY3_FIRE10 44
#define OSD_JOY3_FIRE   45      /* any of the third joystick fire buttons */
#define OSD_JOY4_LEFT   46
#define OSD_JOY4_RIGHT  47
#define OSD_JOY4_UP     48
#define OSD_JOY4_DOWN   49
#define OSD_JOY4_FIRE1  50
#define OSD_JOY4_FIRE2  51
#define OSD_JOY4_FIRE3  52
#define OSD_JOY4_FIRE4  53
#define OSD_JOY4_FIRE5  54
#define OSD_JOY4_FIRE6  55
#define OSD_JOY4_FIRE7  56
#define OSD_JOY4_FIRE8  57
#define OSD_JOY4_FIRE9  58
#define OSD_JOY4_FIRE10 59
#define OSD_JOY4_FIRE   60      /* any of the fourth joystick fire buttons */
#define OSD_MAX_JOY     60

/* We support 4 players for each analog control */
#define OSD_MAX_JOY_ANALOG	4
#define X_AXIS          1
#define Y_AXIS          2

/***************************** Common types ***********************************/

/******************************************************************************
 * Union of UINT8, UINT16 and UINT32 in native endianess of the target
 * This is used to access bytes and words in a machine independent manner.
 * The upper bytes h2 and h3 normally contain zero (16 bit CPU cores)
 * thus PAIR.d can be used to pass arguments to the memory system
 * which expects 'int' really.
 ******************************************************************************/
typedef union {
#ifdef LSB_FIRST
	struct { UINT8 l,h,h2,h3; } b;
	struct { UINT16 l,h; } w;
#else
	struct { UINT8 h3,h2,h,l; } b;
	struct { UINT16 h,l; } w;
#endif
	UINT32 d;
}	PAIR;

#endif	/* defined OSD_CPU_H */
