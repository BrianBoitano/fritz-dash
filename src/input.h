// Fritz Dash - input bits, one per controller button the game uses.
#ifndef FRITZ_INPUT_H
#define FRITZ_INPUT_H

#define IN_CROSS    1
#define IN_START    2
#define IN_CIRCLE   4
#define IN_UP       8
#define IN_DOWN     16
#define IN_L1       32
#define IN_R1       64
#define IN_LEFT     128
#define IN_RIGHT    256
#define IN_SQUARE   512
#define IN_TRIANGLE 1024
#define IN_L2       2048
#define IN_R2       4096
#define IN_TOUCH    8192
#define IN_RS_LEFT  16384       // right stick pushed left, and so on
#define IN_RS_RIGHT 32768
#define IN_RS_UP    65536
#define IN_RS_DOWN  131072
#define IN_L3       262144
#define IN_P1_CROSS 1048576     // X on the first controller only (2 player race)
#define IN_P2_CROSS 2097152     // X on the second controller
#define IN_P2_HERE  4194304     // a second controller is connected

#endif
