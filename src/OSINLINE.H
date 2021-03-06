#ifndef __OSINLINE__
#define __OSINLINE__

/* What goes herein depends heavily on the OS. */

#define DIRTY_H 256
#define DIRTY_V 1600/16

// extern char *dirty_new;
#define osd_mark_vector_dirty(x,y) // dirty_new[(y)/16 * DIRTY_H + (x)/16] = 1

#if 0
#define vec_mult _vec_mult

#ifndef __IBMCPP__
inline int _vec_mult(int x, int y)
{
	int result;
	asm (
			"movl  %1    , %0    ; "
			"imull %2            ; "    /* do the multiply */
			"movl  %%edx , %%eax ; "
			:  "=&a" (result)           /* the result has to go in eax */
			:  "mr" (x),                /* x and y can be regs or mem */
			   "mr" (y)
			:  "%edx", "%cc"            /* clobbers edx and flags */
		);
	return result;
}
#else
inline int _vec_mult( int x, int y ) {
	int result = 0;
	result = (x&65536)*(y&65536);
	result |= (((x>>16)&65536)*((y>>16)&65536))<<16;
	return result;
}
#endif
#endif

#endif /* __OSINLINE__ */
