#include "charflag.h"

#define C CF_CONTROL
#define S CF_SPACE
#define R CF_RETURN
#define D CF_DIGIT
#define L CF_LETTER
#define H CF_HEX
#define Y CF_SYMBOL

unsigned char charflag[256] = {
   C,C,C,C,C,C,C,C,C,
   S, // \t
   R, // \n
   S, // \v
   S, // \f
   R, // \r
   C,C,C,C,C,C,C,C,C,C,C,C,C,C,C,C,C,C,
   S, // space
   Y,   // !
   Y,   // "
   Y,   // #
   Y,   // $
   Y,   // %
   Y,   // &
   Y,   // '
   Y,   // (
   Y,   // )
   Y,   // *
   Y,   // +
   Y,   // ,
   Y,   // -
   Y,   // .
   Y,   // /
   D|H, // 0
   D|H, // 1
   D|H, // 2
   D|H, // 3
   D|H, // 4
   D|H, // 5
   D|H, // 6
   D|H, // 7
   D|H, // 8
   D|H, // 9
   Y,   // :
   Y,   // ;
   Y,   // <
   Y,   // =
   Y,   // >
   Y,   // ?
   Y,   // @
   L|H, // A
   L|H, // B
   L|H, // C
   L|H, // D
   L|H, // E
   L|H, // F
   L,   // G
   L,   // H
   L,   // I
   L,   // J
   L,   // K
   L,   // L
   L,   // M
   L,   // N
   L,   // O
   L,   // P
   L,   // Q
   L,   // R
   L,   // S
   L,   // T
   L,   // U
   L,   // V
   L,   // W
   L,   // X
   L,   // Y
   L,   // Z
   Y,   // [
   Y,   // \ (backslash)
   Y,   // ]
   Y,   // ^
   Y,   // _
   Y,   // `
   L|H, // a
   L|H, // b
   L|H, // c
   L|H, // d
   L|H, // e
   L|H, // f
   L,   // g
   L,   // h
   L,   // i
   L,   // j
   L,   // k
   L,   // l
   L,   // m
   L,   // n
   L,   // o
   L,   // p
   L,   // q
   L,   // r
   L,   // s
   L,   // t
   L,   // u
   L,   // v
   L,   // w
   L,   // x
   L,   // y
   L,   // z
   Y,   // {
   Y,   // |
   Y,   // }
   Y,   // ~
   C,   // DEL
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

#undef C
#undef S
#undef R
#undef D
#undef L
#undef H
#undef Y
