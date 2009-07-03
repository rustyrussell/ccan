#ifndef CCAN_CHARFLAG_H
#define CCAN_CHARFLAG_H

//All of these macros evaluate the argument exactly once

#define ccontrol(c)  (charflag(c) & CF_CONTROL) //Weird characters that shouldn't be in text
#define cspace(c)    (charflag(c) & CF_SPACE)   //Space, tab, vertical tab, form feed
#define creturn(c)   (charflag(c) & CF_RETURN)  //Newline
#define cwhite(c)    (charflag(c) & CF_WHITE)   //cspace or creturn
#define cdigit(c)    (charflag(c) & CF_DIGIT)   //0-9
#define cletter(c)   (charflag(c) & CF_LETTER)  //A-Za-z
#define chex(c)      (charflag(c) & CF_HEX)     //0-9A-Fa-f
#define csymbol(c)   (charflag(c) & CF_SYMBOL)
	// !"#$%&'()*+,-./:;<=>?@[\]^_`{|}~
	//If it's ASCII, prints a non-blank character, and is not a digit or letter, it's a symbol
#define cextended(c) (charflag(c) == 0)         //Characters >= 128

/* To test:

All charflag macros should evaluate exactly once

*/

extern unsigned char charflag[256];
#define charflag(c) (charflag[(unsigned int)(unsigned char)(c)])

#define CF_CONTROL ((unsigned char)  1)
#define CF_SPACE   ((unsigned char)  2)
#define CF_RETURN  ((unsigned char)  4)
#define CF_DIGIT   ((unsigned char)  8)
#define CF_LETTER  ((unsigned char) 16)
#define CF_HEX     ((unsigned char) 32)
#define CF_SYMBOL  ((unsigned char) 64)

#define CF_WHITE (CF_SPACE|CF_RETURN)

#endif
