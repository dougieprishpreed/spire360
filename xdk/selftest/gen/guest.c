#include "w2c2_base.h"

#include "guest.h"

const U8 m5_guestData0[]={
0x72,0x65,0x61,0x64,0x79,0,0x2d,0x2b,0x20,0x20,0x20,0x30,0x58,0x30,0x78,0,0x2d,0x30,
0x58,0x2b,0x30,0x58,0x20,0x30,0x58,0x2d,0x30,0x78,0x2b,0x30,0x78,0x20,0x30,0x78,0,0x2f,
0x73,0x63,0x72,0x61,0x74,0x63,0x68,0x2f,0x62,0x2e,0x74,0x78,0x74,0,0x2f,0x73,0x63,0x72,
0x61,0x74,0x63,0x68,0x2f,0x61,0x2e,0x74,0x78,0x74,0,0x74,0x72,0x61,0x70,0,0x6e,0x61,
0x6e,0,0x6d,0x61,0x74,0x68,0,0x2f,0x73,0x63,0x72,0x61,0x74,0x63,0x68,0,0x65,0x6f,
0x66,0,0x69,0x6e,0x66,0,0x68,0x65,0x6c,0x6c,0x6f,0x20,0x73,0x70,0x69,0x72,0x65,0,
0x65,0x73,0x63,0x61,0x70,0x65,0,0x64,0x69,0x72,0x20,0x6e,0x6f,0x6e,0x65,0,0x74,0x69,
0x6d,0x65,0,0x66,0x69,0x6c,0x65,0,0x2f,0x2e,0x2e,0x2f,0x2e,0x2e,0x2f,0x65,0x74,0x63,
0x2f,0x70,0x61,0x73,0x73,0x77,0x64,0,0x66,0x69,0x6c,0x65,0x20,0x6f,0x70,0x65,0x6e,0x2d,
0x77,0x72,0x69,0x74,0x65,0x2d,0x66,0x61,0x69,0x6c,0x65,0x64,0,0x62,0x6c,0x6f,0x63,0x6b,
0x65,0x64,0,0x77,0x62,0,0x72,0x62,0,0x61,0x62,0,0x72,0x77,0x61,0,0x2f,0x64,
0x61,0x74,0x61,0,0x53,0x50,0x49,0x52,0x45,0x5f,0x54,0x45,0x53,0x54,0,0x4f,0x50,0x45,
0x4e,0,0x4e,0x41,0x4e,0,0x49,0x4e,0x46,0,0x65,0x78,0x69,0x74,0x33,0,0x70,0x32,
0,0x2e,0,0x2d,0,0x28,0x6e,0x75,0x6c,0x6c,0x29,0,0x67,0x72,0x6f,0x77,0x20,0,
0x65,0x63,0x68,0x6f,0x20,0,0x67,0x72,0x6f,0x77,0x20,0x25,0x6c,0x64,0x20,0x25,0x6c,0x75,
0xa,0,0x65,0x6e,0x76,0x20,0x25,0x64,0x20,0x25,0x73,0x20,0x25,0x73,0xa,0,0x65,0x63,
0x68,0x6f,0x20,0x25,0x73,0xa,0,0x75,0x6e,0x6b,0x6e,0x6f,0x77,0x6e,0x20,0x25,0x73,0xa,
0,0x65,0x73,0x63,0x61,0x70,0x65,0x20,0x25,0x73,0xa,0,0x6d,0x61,0x74,0x68,0x20,0x25,
0x6c,0x6c,0x78,0x20,0x25,0x6c,0x6c,0x78,0x20,0x25,0x2e,0x31,0x37,0x67,0x20,0x25,0x30,0x32,
0x78,0x25,0x30,0x32,0x78,0x25,0x30,0x32,0x78,0x25,0x30,0x32,0x78,0x20,0x25,0x6c,0x6c,0x64,
0x20,0x25,0x64,0x20,0x25,0x2e,0x36,0x66,0xa,0,0x67,0x72,0x6f,0x77,0x20,0x66,0x61,0x69,
0x6c,0x20,0x61,0x74,0x20,0x25,0x6c,0x64,0xa,0,0x64,0x69,0x72,0x20,0x25,0x64,0x20,0x25,
0x6c,0x64,0xa,0,0x67,0x75,0x65,0x73,0x74,0x3a,0x20,0x73,0x74,0x61,0x72,0x74,0x65,0x64,
0xa,0,0x66,0x69,0x6c,0x65,0x20,0x25,0x73,0x7c,0x25,0x73,0x7c,0x25,0x6c,0x6c,0x64,0x7c,
0x25,0x64,0xa,0,0x70,0x32,0x20,0x25,0x64,0x20,0x25,0x64,0x20,0x6e,0x3d,0x25,0x64,0x20,
0x66,0x69,0x72,0x73,0x74,0x3d,0x25,0x64,0x20,0x77,0x61,0x69,0x74,0x65,0x64,0x3d,0x25,0x64,
0xa,0,0x63,0x6c,0x65,0x61,0x6e,0x75,0x70,0x20,0x25,0x64,0xa,0,0x74,0x69,0x6d,0x65,
0x20,0x25,0x64,0x20,0x25,0x64,0xa,0,0x72,0x65,0x6e,0x61,0x6d,0x65,0x20,0x25,0x64,0x20,
0x25,0x64,0xa,0,0x53,0x75,0x70,0x70,0x6f,0x72,0x74,0x20,0x66,0x6f,0x72,0x20,0x66,0x6f,
0x72,0x6d,0x61,0x74,0x74,0x69,0x6e,0x67,0x20,0x6c,0x6f,0x6e,0x67,0x20,0x64,0x6f,0x75,0x62,
0x6c,0x65,0x20,0x76,0x61,0x6c,0x75,0x65,0x73,0x20,0x69,0x73,0x20,0x63,0x75,0x72,0x72,0x65,
0x6e,0x74,0x6c,0x79,0x20,0x64,0x69,0x73,0x61,0x62,0x6c,0x65,0x64,0x2e,0xa,0x54,0x6f,0x20,
0x65,0x6e,0x61,0x62,0x6c,0x65,0x20,0x69,0x74,0x2c,0x20,0x61,0x64,0x64,0x20,0x2d,0x6c,0x63,
0x2d,0x70,0x72,0x69,0x6e,0x74,0x73,0x63,0x61,0x6e,0x2d,0x6c,0x6f,0x6e,0x67,0x2d,0x64,0x6f,
0x75,0x62,0x6c,0x65,0x20,0x74,0x6f,0x20,0x74,0x68,0x65,0x20,0x6c,0x69,0x6e,0x6b,0x20,0x63,
0x6f,0x6d,0x6d,0x61,0x6e,0x64,0x2e,0xa,0,0,1,0,0,0,0,0,0,0,
6,0,0,0,2,0,0,0,0,0,0,0,3,0,0,0,0,0,
0,0,1,0,0,0,0,0,0,0,4,0,0,0,0,0,0,0,
7,0,0,0,0,0,0,0,0x14,0,0,0,0,0x60,0,0,0,0x20,
0,0,0,0x40,0,0,0,0x80,0,0,0,0xc0,0,0,0,0xc0,0,0,
0,0xa0,0,0,0x78,0xf,0,0,0xf0,0xf,0,0,0x68,0x10,0,0,0x53,0x75,
0x63,0x63,0x65,0x73,0x73,0,0x49,0x6c,0x6c,0x65,0x67,0x61,0x6c,0x20,0x62,0x79,0x74,0x65,
0x20,0x73,0x65,0x71,0x75,0x65,0x6e,0x63,0x65,0,0x44,0x6f,0x6d,0x61,0x69,0x6e,0x20,0x65,
0x72,0x72,0x6f,0x72,0,0x52,0x65,0x73,0x75,0x6c,0x74,0x20,0x6e,0x6f,0x74,0x20,0x72,0x65,
0x70,0x72,0x65,0x73,0x65,0x6e,0x74,0x61,0x62,0x6c,0x65,0,0x4e,0x6f,0x74,0x20,0x61,0x20,
0x74,0x74,0x79,0,0x50,0x65,0x72,0x6d,0x69,0x73,0x73,0x69,0x6f,0x6e,0x20,0x64,0x65,0x6e,
0x69,0x65,0x64,0,0x4f,0x70,0x65,0x72,0x61,0x74,0x69,0x6f,0x6e,0x20,0x6e,0x6f,0x74,0x20,
0x70,0x65,0x72,0x6d,0x69,0x74,0x74,0x65,0x64,0,0x4e,0x6f,0x20,0x73,0x75,0x63,0x68,0x20,
0x66,0x69,0x6c,0x65,0x20,0x6f,0x72,0x20,0x64,0x69,0x72,0x65,0x63,0x74,0x6f,0x72,0x79,0,
0x4e,0x6f,0x20,0x73,0x75,0x63,0x68,0x20,0x70,0x72,0x6f,0x63,0x65,0x73,0x73,0,0x46,0x69,
0x6c,0x65,0x20,0x65,0x78,0x69,0x73,0x74,0x73,0,0x56,0x61,0x6c,0x75,0x65,0x20,0x74,0x6f,
0x6f,0x20,0x6c,0x61,0x72,0x67,0x65,0x20,0x66,0x6f,0x72,0x20,0x64,0x61,0x74,0x61,0x20,0x74,
0x79,0x70,0x65,0,0x4e,0x6f,0x20,0x73,0x70,0x61,0x63,0x65,0x20,0x6c,0x65,0x66,0x74,0x20,
0x6f,0x6e,0x20,0x64,0x65,0x76,0x69,0x63,0x65,0,0x4f,0x75,0x74,0x20,0x6f,0x66,0x20,0x6d,
0x65,0x6d,0x6f,0x72,0x79,0,0x52,0x65,0x73,0x6f,0x75,0x72,0x63,0x65,0x20,0x62,0x75,0x73,
0x79,0,0x49,0x6e,0x74,0x65,0x72,0x72,0x75,0x70,0x74,0x65,0x64,0x20,0x73,0x79,0x73,0x74,
0x65,0x6d,0x20,0x63,0x61,0x6c,0x6c,0,0x52,0x65,0x73,0x6f,0x75,0x72,0x63,0x65,0x20,0x74,
0x65,0x6d,0x70,0x6f,0x72,0x61,0x72,0x69,0x6c,0x79,0x20,0x75,0x6e,0x61,0x76,0x61,0x69,0x6c,
0x61,0x62,0x6c,0x65,0,0x49,0x6e,0x76,0x61,0x6c,0x69,0x64,0x20,0x73,0x65,0x65,0x6b,0,
0x43,0x72,0x6f,0x73,0x73,0x2d,0x64,0x65,0x76,0x69,0x63,0x65,0x20,0x6c,0x69,0x6e,0x6b,0,
0x52,0x65,0x61,0x64,0x2d,0x6f,0x6e,0x6c,0x79,0x20,0x66,0x69,0x6c,0x65,0x20,0x73,0x79,0x73,
0x74,0x65,0x6d,0,0x44,0x69,0x72,0x65,0x63,0x74,0x6f,0x72,0x79,0x20,0x6e,0x6f,0x74,0x20,
0x65,0x6d,0x70,0x74,0x79,0,0x43,0x6f,0x6e,0x6e,0x65,0x63,0x74,0x69,0x6f,0x6e,0x20,0x72,
0x65,0x73,0x65,0x74,0x20,0x62,0x79,0x20,0x70,0x65,0x65,0x72,0,0x4f,0x70,0x65,0x72,0x61,
0x74,0x69,0x6f,0x6e,0x20,0x74,0x69,0x6d,0x65,0x64,0x20,0x6f,0x75,0x74,0,0x43,0x6f,0x6e,
0x6e,0x65,0x63,0x74,0x69,0x6f,0x6e,0x20,0x72,0x65,0x66,0x75,0x73,0x65,0x64,0,0x48,0x6f,
0x73,0x74,0x20,0x69,0x73,0x20,0x75,0x6e,0x72,0x65,0x61,0x63,0x68,0x61,0x62,0x6c,0x65,0,
0x41,0x64,0x64,0x72,0x65,0x73,0x73,0x20,0x69,0x6e,0x20,0x75,0x73,0x65,0,0x42,0x72,0x6f,
0x6b,0x65,0x6e,0x20,0x70,0x69,0x70,0x65,0,0x49,0x2f,0x4f,0x20,0x65,0x72,0x72,0x6f,0x72,
0,0x4e,0x6f,0x20,0x73,0x75,0x63,0x68,0x20,0x64,0x65,0x76,0x69,0x63,0x65,0x20,0x6f,0x72,
0x20,0x61,0x64,0x64,0x72,0x65,0x73,0x73,0,0x4e,0x6f,0x20,0x73,0x75,0x63,0x68,0x20,0x64,
0x65,0x76,0x69,0x63,0x65,0,0x4e,0x6f,0x74,0x20,0x61,0x20,0x64,0x69,0x72,0x65,0x63,0x74,
0x6f,0x72,0x79,0,0x49,0x73,0x20,0x61,0x20,0x64,0x69,0x72,0x65,0x63,0x74,0x6f,0x72,0x79,
0,0x54,0x65,0x78,0x74,0x20,0x66,0x69,0x6c,0x65,0x20,0x62,0x75,0x73,0x79,0,0x45,0x78,
0x65,0x63,0x20,0x66,0x6f,0x72,0x6d,0x61,0x74,0x20,0x65,0x72,0x72,0x6f,0x72,0,0x49,0x6e,
0x76,0x61,0x6c,0x69,0x64,0x20,0x61,0x72,0x67,0x75,0x6d,0x65,0x6e,0x74,0,0x41,0x72,0x67,
0x75,0x6d,0x65,0x6e,0x74,0x20,0x6c,0x69,0x73,0x74,0x20,0x74,0x6f,0x6f,0x20,0x6c,0x6f,0x6e,
0x67,0,0x53,0x79,0x6d,0x62,0x6f,0x6c,0x69,0x63,0x20,0x6c,0x69,0x6e,0x6b,0x20,0x6c,0x6f,
0x6f,0x70,0,0x46,0x69,0x6c,0x65,0x6e,0x61,0x6d,0x65,0x20,0x74,0x6f,0x6f,0x20,0x6c,0x6f,
0x6e,0x67,0,0x54,0x6f,0x6f,0x20,0x6d,0x61,0x6e,0x79,0x20,0x6f,0x70,0x65,0x6e,0x20,0x66,
0x69,0x6c,0x65,0x73,0x20,0x69,0x6e,0x20,0x73,0x79,0x73,0x74,0x65,0x6d,0,0x4e,0x6f,0x20,
0x66,0x69,0x6c,0x65,0x20,0x64,0x65,0x73,0x63,0x72,0x69,0x70,0x74,0x6f,0x72,0x73,0x20,0x61,
0x76,0x61,0x69,0x6c,0x61,0x62,0x6c,0x65,0,0x42,0x61,0x64,0x20,0x66,0x69,0x6c,0x65,0x20,
0x64,0x65,0x73,0x63,0x72,0x69,0x70,0x74,0x6f,0x72,0,0x4e,0x6f,0x20,0x63,0x68,0x69,0x6c,
0x64,0x20,0x70,0x72,0x6f,0x63,0x65,0x73,0x73,0,0x42,0x61,0x64,0x20,0x61,0x64,0x64,0x72,
0x65,0x73,0x73,0,0x46,0x69,0x6c,0x65,0x20,0x74,0x6f,0x6f,0x20,0x6c,0x61,0x72,0x67,0x65,
0,0x54,0x6f,0x6f,0x20,0x6d,0x61,0x6e,0x79,0x20,0x6c,0x69,0x6e,0x6b,0x73,0,0x4e,0x6f,
0x20,0x6c,0x6f,0x63,0x6b,0x73,0x20,0x61,0x76,0x61,0x69,0x6c,0x61,0x62,0x6c,0x65,0,0x52,
0x65,0x73,0x6f,0x75,0x72,0x63,0x65,0x20,0x64,0x65,0x61,0x64,0x6c,0x6f,0x63,0x6b,0x20,0x77,
0x6f,0x75,0x6c,0x64,0x20,0x6f,0x63,0x63,0x75,0x72,0,0x53,0x74,0x61,0x74,0x65,0x20,0x6e,
0x6f,0x74,0x20,0x72,0x65,0x63,0x6f,0x76,0x65,0x72,0x61,0x62,0x6c,0x65,0,0x50,0x72,0x65,
0x76,0x69,0x6f,0x75,0x73,0x20,0x6f,0x77,0x6e,0x65,0x72,0x20,0x64,0x69,0x65,0x64,0,0x4f,
0x70,0x65,0x72,0x61,0x74,0x69,0x6f,0x6e,0x20,0x63,0x61,0x6e,0x63,0x65,0x6c,0x65,0x64,0,
0x46,0x75,0x6e,0x63,0x74,0x69,0x6f,0x6e,0x20,0x6e,0x6f,0x74,0x20,0x69,0x6d,0x70,0x6c,0x65,
0x6d,0x65,0x6e,0x74,0x65,0x64,0,0x4e,0x6f,0x20,0x6d,0x65,0x73,0x73,0x61,0x67,0x65,0x20,
0x6f,0x66,0x20,0x64,0x65,0x73,0x69,0x72,0x65,0x64,0x20,0x74,0x79,0x70,0x65,0,0x49,0x64,
0x65,0x6e,0x74,0x69,0x66,0x69,0x65,0x72,0x20,0x72,0x65,0x6d,0x6f,0x76,0x65,0x64,0,0x4c,
0x69,0x6e,0x6b,0x20,0x68,0x61,0x73,0x20,0x62,0x65,0x65,0x6e,0x20,0x73,0x65,0x76,0x65,0x72,
0x65,0x64,0,0x50,0x72,0x6f,0x74,0x6f,0x63,0x6f,0x6c,0x20,0x65,0x72,0x72,0x6f,0x72,0,
0x42,0x61,0x64,0x20,0x6d,0x65,0x73,0x73,0x61,0x67,0x65,0,0x4e,0x6f,0x74,0x20,0x61,0x20,
0x73,0x6f,0x63,0x6b,0x65,0x74,0,0x44,0x65,0x73,0x74,0x69,0x6e,0x61,0x74,0x69,0x6f,0x6e,
0x20,0x61,0x64,0x64,0x72,0x65,0x73,0x73,0x20,0x72,0x65,0x71,0x75,0x69,0x72,0x65,0x64,0,
0x4d,0x65,0x73,0x73,0x61,0x67,0x65,0x20,0x74,0x6f,0x6f,0x20,0x6c,0x61,0x72,0x67,0x65,0,
0x50,0x72,0x6f,0x74,0x6f,0x63,0x6f,0x6c,0x20,0x77,0x72,0x6f,0x6e,0x67,0x20,0x74,0x79,0x70,
0x65,0x20,0x66,0x6f,0x72,0x20,0x73,0x6f,0x63,0x6b,0x65,0x74,0,0x50,0x72,0x6f,0x74,0x6f,
0x63,0x6f,0x6c,0x20,0x6e,0x6f,0x74,0x20,0x61,0x76,0x61,0x69,0x6c,0x61,0x62,0x6c,0x65,0,
0x50,0x72,0x6f,0x74,0x6f,0x63,0x6f,0x6c,0x20,0x6e,0x6f,0x74,0x20,0x73,0x75,0x70,0x70,0x6f,
0x72,0x74,0x65,0x64,0,0x4e,0x6f,0x74,0x20,0x73,0x75,0x70,0x70,0x6f,0x72,0x74,0x65,0x64,
0,0x41,0x64,0x64,0x72,0x65,0x73,0x73,0x20,0x66,0x61,0x6d,0x69,0x6c,0x79,0x20,0x6e,0x6f,
0x74,0x20,0x73,0x75,0x70,0x70,0x6f,0x72,0x74,0x65,0x64,0x20,0x62,0x79,0x20,0x70,0x72,0x6f,
0x74,0x6f,0x63,0x6f,0x6c,0,0x41,0x64,0x64,0x72,0x65,0x73,0x73,0x20,0x6e,0x6f,0x74,0x20,
0x61,0x76,0x61,0x69,0x6c,0x61,0x62,0x6c,0x65,0,0x4e,0x65,0x74,0x77,0x6f,0x72,0x6b,0x20,
0x69,0x73,0x20,0x64,0x6f,0x77,0x6e,0,0x4e,0x65,0x74,0x77,0x6f,0x72,0x6b,0x20,0x75,0x6e,
0x72,0x65,0x61,0x63,0x68,0x61,0x62,0x6c,0x65,0,0x43,0x6f,0x6e,0x6e,0x65,0x63,0x74,0x69,
0x6f,0x6e,0x20,0x72,0x65,0x73,0x65,0x74,0x20,0x62,0x79,0x20,0x6e,0x65,0x74,0x77,0x6f,0x72,
0x6b,0,0x43,0x6f,0x6e,0x6e,0x65,0x63,0x74,0x69,0x6f,0x6e,0x20,0x61,0x62,0x6f,0x72,0x74,
0x65,0x64,0,0x4e,0x6f,0x20,0x62,0x75,0x66,0x66,0x65,0x72,0x20,0x73,0x70,0x61,0x63,0x65,
0x20,0x61,0x76,0x61,0x69,0x6c,0x61,0x62,0x6c,0x65,0,0x53,0x6f,0x63,0x6b,0x65,0x74,0x20,
0x69,0x73,0x20,0x63,0x6f,0x6e,0x6e,0x65,0x63,0x74,0x65,0x64,0,0x53,0x6f,0x63,0x6b,0x65,
0x74,0x20,0x6e,0x6f,0x74,0x20,0x63,0x6f,0x6e,0x6e,0x65,0x63,0x74,0x65,0x64,0,0x4f,0x70,
0x65,0x72,0x61,0x74,0x69,0x6f,0x6e,0x20,0x61,0x6c,0x72,0x65,0x61,0x64,0x79,0x20,0x69,0x6e,
0x20,0x70,0x72,0x6f,0x67,0x72,0x65,0x73,0x73,0,0x4f,0x70,0x65,0x72,0x61,0x74,0x69,0x6f,
0x6e,0x20,0x69,0x6e,0x20,0x70,0x72,0x6f,0x67,0x72,0x65,0x73,0x73,0,0x53,0x74,0x61,0x6c,
0x65,0x20,0x66,0x69,0x6c,0x65,0x20,0x68,0x61,0x6e,0x64,0x6c,0x65,0,0x51,0x75,0x6f,0x74,
0x61,0x20,0x65,0x78,0x63,0x65,0x65,0x64,0x65,0x64,0,0x4d,0x75,0x6c,0x74,0x69,0x68,0x6f,
0x70,0x20,0x61,0x74,0x74,0x65,0x6d,0x70,0x74,0x65,0x64,0,0x43,0x61,0x70,0x61,0x62,0x69,
0x6c,0x69,0x74,0x69,0x65,0x73,0x20,0x69,0x6e,0x73,0x75,0x66,0x66,0x69,0x63,0x69,0x65,0x6e,
0x74,0,0,0,0x75,2,0x4e,0,0xd6,1,0xe2,4,0xb9,4,0x18,1,0x8e,5,
0xed,2,0x16,4,0xf2,0,0x97,3,1,3,0x38,5,0xaf,1,0x82,1,0x4f,3,
0x2f,4,0x1e,0,0xd4,5,0xa2,0,0x12,3,0x1e,3,0xc2,1,0xde,3,8,0,
0xac,5,0,1,0x64,2,0xf1,1,0x65,5,0x34,2,0x8c,2,0xcf,2,0x2d,3,
0x4c,4,0xe3,5,0x9f,2,0xf8,4,0x1c,5,8,5,0xb1,2,0x4b,5,0x15,2,
0x78,0,0x52,2,0x3c,3,0xf1,3,0xe4,0,0xc3,3,0x7d,4,0xcc,0,0xaa,3,
0x79,5,0x24,2,0x6e,1,0x6d,3,0x22,4,0xab,4,0x44,0,0xfb,1,0xae,0,
0x83,3,0x60,0,0xe5,1,7,4,0x94,4,0x5e,4,0x2b,0,0x58,1,0x39,1,
0x92,0,0xc2,5,0x9b,1,0x43,2,0x46,1,0xf6,5,0,0,0,0,0,0,
0x19,0,0xa,0,0x19,0x19,0x19,0,0,0,0,5,0,0,0,0,0,0,
9,0,0,0,0,0xb,0,0,0,0,0,0,0,0,0x19,0,0x11,0xa,
0x19,0x19,0x19,3,0xa,7,0,1,0x1b,9,0xb,0x18,0,0,9,6,0xb,0,
0,0xb,0,6,0x19,0,0,0,0x19,0x19,0x19
};

const U8 m5_guestData1[]={
0xe,0,0,0,0,0,0,0,0,0x19,0,0xa,0xd,0x19,0x19,0x19,0,0xd,
0,0,2,0,9,0xe,0,0,0,9,0,0xe,0,0,0xe
};

const U8 m5_guestData2[]={0xc};

const U8 m5_guestData3[]={
0x13,0,0,0,0,0x13,0,0,0,0,9,0xc,0,0,0,0,0,0xc,
0,0,0xc
};

const U8 m5_guestData4[]={0x10};

const U8 m5_guestData5[]={
0xf,0,0,0,4,0xf,0,0,0,0,9,0x10,0,0,0,0,0,0x10,
0,0,0x10
};

const U8 m5_guestData6[]={0x12};

const U8 m5_guestData7[]={
0x11,0,0,0,0,0x11,0,0,0,0,9,0x12,0,0,0,0,0,0x12,
0,0,0x12,0,0,0x1a,0,0,0,0x1a,0x1a,0x1a
};

const U8 m5_guestData8[]={0x1a,0,0,0,0x1a,0x1a,0x1a,0,0,0,0,0,0,9};

const U8 m5_guestData9[]={0x14};

const U8 m5_guestData10[]={
0x17,0,0,0,0,0x17,0,0,0,0,9,0x14,0,0,0,0,0,0x14,
0,0,0x14
};

const U8 m5_guestData11[]={0x16};

const U8 m5_guestData12[]={
0x15,0,0,0,0,0x15,0,0,0,0,9,0x16,0,0,0,0,0,0x16,
0,0,0x16,0,0,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x41,0x42,0x43,
0x44,0x45,0x46
};

const U8 m5_guestData13[]={0xff,0xff,0xff,0xff,0,0,0,0,5};

const U8 m5_guestData14[]={4};

const U8 m5_guestData15[]={2,0,0,0,1,0,0,0,8,0x13};

const U8 m5_guestData16[]={2,0,0,0,0,0,0,0,0xff,0xff,0xff,0xff};

const U8 m5_guestData17[]={0x78,0xf,0,0,0,0,0,0,9};

const U8 m5_guestData18[]={4};

const U8 m5_guestData19[]={3,0,0,0,0,0,0,0,1,0,0,0,0x18,0x13,0,0,0,4};

const U8 m5_guestData20[]={0xf0,0xf,0,0,0,0,0,0,5};

const U8 m5_guestData21[]={4};

const U8 m5_guestData22[]={5,0,0,0,1,0,0,0,0x28,0x17,0,0,0,4};

const U8 m5_guestData23[]={1,0,0,0,0,0,0,0,0xa};

const U8 m5_guestData24[]={0x68,0x10};

void f28(wasmModuleInstance* instance) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l0=0;
U32 l1=0;
U32 l2=0;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U64 l6=0;
U32 l7=0;
U64 l8=0;
U64 l9=0;
U32 l10=0;
U32 l11=0;
U32 l12=0;
U32 l13=0;
U32 l14=0;
U32 l15=0;
U64 l16=0;
U32 l17=0;
U32 l18=0;
U32 l19=0;
U32 l20=0;
U64 l21=0;
U32 l22=0;
U32 l23=0;
U32 l24=0;
U32 l25=0;
U32 l26=0;
U32 l27=0;
U32 l28=0;
U32 l29=0;
U32 l30=0;
U32 si0,si1,si2,si3,si4,si5,si6;
U64 sj0,sj1,sj2,sj3,sj4,sj5;
F64 sd1;
si0=4320U;
si0=i32_load(i->m0,si0);
si0=!(si0);
if(si0){
si0=4320U;
si1=1U;
i32_store(i->m0,si0,si1);
si0=i->g0;
si1=16U;
si0-=si1;
l14=si0;
i->g0=si0;
si0=l14;
si1=8U;
si0+=si1;
si1=l14;
si2=12U;
si1+=si2;
si0=i22_wasiX5FsnapshotX5Fpreview114_argsX5FsizesX5Fget(instance,si0,si1);
si1=65535U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l14;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
si1=1U;
si0+=si1;
l13=si0;
si0=!(si0);
if(si0){
goto L7;
}
si0=l14;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+12U);
si0=f29(instance,si0);
l0=si0;
si0=!(si0);
if(si0){
goto L6;
}
si0=l13;
si1=4U;
si0=f31(instance,si0,si1);
l13=si0;
si0=!(si0);
if(si0){
goto L5;
}
si0=l13;
si1=l0;
si0=i22_wasiX5FsnapshotX5Fpreview18_argsX5Fget(instance,si0,si1);
si1=65535U;
si0&=si1;
if(si0){
goto L4;
}
si0=l14;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l18=si0;
si0=i->g0;
si1=768U;
si0-=si1;
l3=si0;
i->g0=si0;
si0=1756U;
si0=i32_load(i->m0,si0);
l17=si0;
l0=si0;
si1=-1U;
i32_store(i->m0,(WasmMemoryAddress)si0+64U,si1);
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+44U);
if(si0){
si0=l0;
si1=10U;
i32_store(i->m0,(WasmMemoryAddress)si0+64U,si1);
}
L9:;
si0=l0;
si1=l0;
si1=i32_load(i->m0,si1);
si2=64U;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=1424U;
si1=15U;
si2=1U;
si3=1748U;
si3=i32_load(i->m0,si3);
si0=f66(instance,si0,si1,si2,si3);
si0=1024U;
f69(instance,si0);
si0=l3;
si1=304U;
si0+=si1;
si1=1752U;
si1=i32_load(i->m0,si1);
l22=si1;
si0=f53(instance,si0,si1);
if(si0){
si0=l3;
si1=303U;
si0+=si1;
l23=si0;
si0=l3;
si1=736U;
si0+=si1;
l19=si0;
si0=l3;
si1=304U;
si0+=si1;
si1=5U;
si0|=si1;
l20=si0;
si0=l3;
si1=40U;
si0+=si1;
l24=si0;
si0=l3;
si1=56U;
si0+=si1;
l25=si0;
si0=l3;
si1=-64U;
si0-=si1;
l26=si0;
si0=l3;
si1=72U;
si0+=si1;
l27=si0;
si0=l3;
si1=32U;
si0+=si1;
l28=si0;
L11:;
W2C2_LOOP_START
{
si0=l3;
si1=304U;
si0+=si1;
si0=f82(instance,si0);
l0=si0;
si0=!(si0);
if(si0){
goto L12;
}
L13:;
W2C2_LOOP_START
{
si0=l0;
si1=l23;
si0+=si1;
l1=si0;
si0=i32_load8_u(i->m0,si0);
si1=10U;
si0-=si1;
switch(si0){
case 0:
goto L14;
case 1:
goto L12;
case 2:
goto L12;
case 3:
goto L14;
default:
goto L12;
}
L14:;
si0=l1;
si1=0U;
i32_store8(i->m0,si0,si1);
si0=l0;
si1=1U;
si0-=si1;
l0=si0;
if(si0){
goto L13;
}
}
L12:;
si0=l3;
si1=304U;
si0+=si1;
si1=1276U;
si2=5U;
si0=f77(instance,si0,si1,si2);
si0=!(si0);
if(si0){
si0=l3;
si1=l20;
i32_store(i->m0,si0,si1);
si0=1310U;
si1=l3;
f67(instance,si0,si1);
goto L15;
}
L16:;
si0=l3;
si1=304U;
si0+=si1;
si1=1098U;
si2=5U;
si0=f77(instance,si0,si1,si2);
si0=!(si0);
if(si0){
si0=l24;
sj1=W2C2_LL(219043332164U);
i64_store(i->m0,si0,sj1);
si0=l3;
si1=48U;
si0+=si1;
sj1=W2C2_LL(73014444066U);
i64_store(i->m0,si0,sj1);
si0=l25;
sj1=W2C2_LL(-176366841446U);
i64_store(i->m0,si0,sj1);
si0=l26;
si1=-2U;
i32_store(i->m0,si0,si1);
si0=l27;
sj1=W2C2_LL(4609047870845172685U);
i64_store(i->m0,si0,sj1);
si0=l3;
sj1=W2C2_LL(81985529216486895U);
i64_store(i->m0,(WasmMemoryAddress)si0+560U,sj1);
si0=l3;
sj1=W2C2_LL(4599676419421066581U);
i64_store(i->m0,(WasmMemoryAddress)si0+704U,sj1);
si0=l3;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+560U);
l6=sj0;
si0=l3;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+560U);
l8=sj0;
si0=l28;
si1=l3;
sd1=f64_load(i->m0,(WasmMemoryAddress)si1+704U);
f64_store(i->m0,si0,sd1);
si0=l3;
sj1=l6;
sj2=W2C2_LL(3U);
sj1*=sj2;
i64_store(i->m0,(WasmMemoryAddress)si0+16U,sj1);
si0=l3;
sj1=l8;
sj2=W2C2_LL(7U);
sj1>>=(sj2&63);
i64_store(i->m0,(WasmMemoryAddress)si0+24U,sj1);
si0=1342U;
si1=l3;
si2=16U;
si1+=si2;
f67(instance,si0,si1);
goto L15;
}
L17:;
si0=l3;
si1=304U;
si0+=si1;
si1=1153U;
si2=5U;
si0=f77(instance,si0,si1,si2);
si0=!(si0);
if(si0){
si0=i->g0;
si1=16U;
si0-=si1;
l0=si0;
i->g0=si0;
si0=1103U;
si1=l0;
si2=12U;
si1+=si2;
si2=4824U;
si3=4828U;
si3=i32_load(i->m0,si3);
si0=f47(instance,si0,si1,si2,si3);
l2=si0;
si1=-1U;
si0=si0 == si1;
if(si0){
si0=4820U;
si1=44U;
i32_store(i->m0,si0,si1);
goto L19;
}
L20:;
si0=l2;
si1=4824U;
si1=i32_load(i->m0,si1);
l2=si1;
si2=l2;
si2=f82(instance,si2);
si0=i22_wasiX5FsnapshotX5Fpreview121_pathX5FcreateX5Fdirectory(instance,si0,si1,si2);
si1=65535U;
si0&=si1;
l2=si0;
if(si0){
si0=4820U;
si1=l2;
i32_store(i->m0,si0,si1);
}
L21:;
L19:;
si0=l0;
si1=16U;
si0+=si1;
i->g0=si0;
si0=1074U;
si1=1207U;
si0=f61(instance,si0,si1);
l0=si0;
si0=!(si0);
if(si0){
si0=1176U;
f69(instance,si0);
goto L15;
}
L22:;
si0=1120U;
si1=11U;
si2=1U;
si3=l0;
si0=f66(instance,si0,si1,si2,si3);
si0=l0;
f49(instance,si0);
si0=1074U;
si1=1213U;
si0=f61(instance,si0,si1);
l0=si0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+64U);
si1=33U;
si0=si0 == si1;
if(si0){
goto L24;
}
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l2=si0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+16U);
si0=si0 == si1;
if(si0){
goto L24;
}
si0=l0;
si1=l2;
si2=1U;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=l2;
si1=33U;
i32_store8(i->m0,si0,si1);
goto L23;
L24:;
si0=l0;
si1=33U;
f63(instance,si0,si1);
L23:;
si0=l0;
f49(instance,si0);
si0=l3;
si1=704U;
si0+=si1;
si1=63U;
si2=1074U;
si3=1210U;
si2=f61(instance,si2,si3);
l0=si2;
si0=f64(instance,si0,si1,si2);
si1=l3;
si2=704U;
si1+=si2;
si0+=si1;
si1=0U;
i32_store8(i->m0,si0,si1);
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+24U);
si0=si0 == si1;
if(si0){
goto L26;
}
si0=l0;
si1=0U;
si2=0U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+32U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
if(si0){
goto L26;
}
goto L25;
L26:;
si0=l0;
si1=0U;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l0;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+16U,sj1);
si0=l0;
sj1=W2C2_LL(6U);
si2=0U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+36U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType5);
sj0=((U64 (*)(wasmModuleInstance*,U32,U64,U32))entry.func)(entry.instance,si0,sj1,si2);
}
sj1=W2C2_LL(0U);
si0=(U64)((I64)sj0<(I64)sj1);
if(si0){
goto L25;
}
si0=l0;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+4U,sj1);
si0=l0;
si1=l0;
si1=i32_load(i->m0,si1);
si2=-17U;
si1&=si2;
i32_store(i->m0,si0,si1);
L25:;
si0=l19;
si1=5U;
si2=l0;
si0=f64(instance,si0,si1,si2);
si1=l3;
si0+=si1;
si1=736U;
si0+=si1;
si1=0U;
i32_store8(i->m0,si0,si1);
si0=l0;
f49(instance,si0);
si0=l3;
si1=560U;
si0+=si1;
l2=si0;
si0=i->g0;
si1=16U;
si0-=si1;
l0=si0;
i->g0=si0;
si0=1074U;
si1=l0;
si2=12U;
si1+=si2;
si2=4824U;
si3=4828U;
si3=i32_load(i->m0,si3);
si0=f47(instance,si0,si1,si2,si3);
l1=si0;
si1=-1U;
si0=si0 == si1;
if(si0){
si0=4820U;
si1=44U;
i32_store(i->m0,si0,si1);
goto L27;
}
L28:;
si0=l1;
si1=4824U;
si1=i32_load(i->m0,si1);
si2=l2;
si3=0U;
si0=f44(instance,si0,si1,si2,si3);
L27:;
si0=l0;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l3;
si1=128U;
si0+=si1;
si1=l3;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+584U);
si2=61440U;
si1&=si2;
si2=32768U;
si1=si1 == si2;
i32_store(i->m0,si0,si1);
si0=l3;
si1=l19;
i32_store(i->m0,(WasmMemoryAddress)si0+116U,si1);
si0=l3;
si1=l3;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+608U);
i64_store(i->m0,(WasmMemoryAddress)si0+120U,sj1);
si0=l3;
si1=l3;
si2=704U;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+112U,si1);
si0=1440U;
si1=l3;
si2=112U;
si1+=si2;
f67(instance,si0,si1);
si0=i->g0;
si1=16U;
si0-=si1;
l0=si0;
i->g0=si0;
si0=1074U;
si1=l0;
si2=8U;
si1+=si2;
si2=4832U;
si3=4836U;
si3=i32_load(i->m0,si3);
si0=f47(instance,si0,si1,si2,si3);
l5=si0;
si1=-1U;
si0=si0 == si1;
if(si0){
goto L30;
}
si0=4832U;
si0=i32_load(i->m0,si0);
l2=si0;
si0=1059U;
si1=l0;
si2=12U;
si1+=si2;
si2=4824U;
si3=4828U;
si3=i32_load(i->m0,si3);
si0=f47(instance,si0,si1,si2,si3);
l1=si0;
si1=-1U;
si0=si0 == si1;
if(si0){
goto L30;
}
si0=4824U;
si0=i32_load(i->m0,si0);
l4=si0;
si0=0U;
si1=l5;
si2=l2;
si3=l2;
si3=f82(instance,si3);
si4=l1;
si5=l4;
si6=l4;
si6=f82(instance,si6);
si1=i22_wasiX5FsnapshotX5Fpreview111_pathX5Frename(instance,si1,si2,si3,si4,si5,si6);
si2=65535U;
si1&=si2;
l4=si1;
si1=!(si1);
if(si1){
goto L31;
}
si0=4820U;
si1=l4;
i32_store(i->m0,si0,si1);
si0=-1U;
L31:;
goto L29;
L30:;
si0=4820U;
si1=44U;
i32_store(i->m0,si0,si1);
si0=0U;
L29:;
si0=l0;
si1=16U;
si0+=si1;
i->g0=si0;
si0=1074U;
si0=f45(instance,si0);
l0=si0;
si0=1059U;
si0=f45(instance,si0);
l1=si0;
si0=l3;
si1=l0;
si1=!(si1);
i32_store(i->m0,(WasmMemoryAddress)si0+96U,si1);
si0=l3;
si1=l1;
si1=!(si1);
i32_store(i->m0,(WasmMemoryAddress)si0+100U,si1);
si0=1518U;
si1=l3;
si2=96U;
si1+=si2;
f67(instance,si0,si1);
si0=i->g0;
si1=16U;
si0-=si1;
l0=si0;
i->g0=si0;
si0=1059U;
si1=l0;
si2=12U;
si1+=si2;
si2=4824U;
si3=4828U;
si3=i32_load(i->m0,si3);
si0=f47(instance,si0,si1,si2,si3);
l2=si0;
si1=-1U;
si0=si0 == si1;
if(si0){
si0=4820U;
si1=44U;
i32_store(i->m0,si0,si1);
goto L32;
}
L33:;
si0=l2;
si1=4824U;
si1=i32_load(i->m0,si1);
l2=si1;
si2=l2;
si2=f82(instance,si2);
si0=i22_wasiX5FsnapshotX5Fpreview116_pathX5FunlinkX5Ffile(instance,si0,si1,si2);
si1=65535U;
si0&=si1;
l2=si0;
if(si0){
si0=4820U;
si1=l2;
i32_store(i->m0,si0,si1);
}
L34:;
L32:;
si0=l0;
si1=16U;
si0+=si1;
i->g0=si0;
si0=i->g0;
si1=16U;
si0-=si1;
l0=si0;
i->g0=si0;
si0=1103U;
si1=l0;
si2=12U;
si1+=si2;
si2=4824U;
si3=4828U;
si3=i32_load(i->m0,si3);
si0=f47(instance,si0,si1,si2,si3);
l2=si0;
si1=-1U;
si0=si0 == si1;
if(si0){
si0=4820U;
si1=44U;
i32_store(i->m0,si0,si1);
goto L35;
}
L36:;
si0=l2;
si1=4824U;
si1=i32_load(i->m0,si1);
l2=si1;
si2=l2;
si2=f82(instance,si2);
si0=i22_wasiX5FsnapshotX5Fpreview121_pathX5FremoveX5Fdirectory(instance,si0,si1,si2);
si1=65535U;
si0&=si1;
l2=si0;
if(si0){
si0=4820U;
si1=l2;
i32_store(i->m0,si0,si1);
}
L37:;
L35:;
si0=l0;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l3;
si1=1103U;
si1=f45(instance,si1);
si1=!(si1);
i32_store(i->m0,(WasmMemoryAddress)si0+80U,si1);
si0=1494U;
si1=l3;
si2=80U;
si1+=si2;
f67(instance,si0,si1);
goto L15;
}
L18:;
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+304U);
l0=si0;
si1=7498084U;
si0=si0 == si1;
if(si0){
si0=i->g0;
si1=16U;
si0-=si1;
l2=si0;
i->g0=si0;
si0=1220U;
si1=l2;
si2=12U;
si1+=si2;
si2=4824U;
si3=4828U;
si3=i32_load(i->m0,si3);
si0=f47(instance,si0,si1,si2,si3);
l0=si0;
si1=-1U;
si0=si0 == si1;
if(si0){
si0=4820U;
si1=44U;
i32_store(i->m0,si0,si1);
si0=0U;
goto L42;
}
L43:;
si0=0U;
l1=si0;
si0=l0;
si1=4824U;
si1=i32_load(i->m0,si1);
si2=67117060U;
si0=f43(instance,si0,si1,si2);
l4=si0;
si1=-1U;
si0=si0 == si1;
if(si0){
goto L44;
}
si0=40U;
si0=f29(instance,si0);
l0=si0;
if(si0){
si0=l0;
si1=4096U;
si1=f29(instance,si1);
l1=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+16U,si1);
si0=l1;
si0=!(si0);
if(si0){
si0=l0;
f30(instance,si0);
si0=0U;
goto L46;
}
L47:;
si0=l4;
si1=l1;
si2=4096U;
sj3=W2C2_LL(0U);
si4=l0;
si5=28U;
si4+=si5;
si0=f40(instance,si0,si1,si2,sj3,si4);
l1=si0;
if(si0){
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
f30(instance,si0);
si0=l0;
f30(instance,si0);
si0=4820U;
si1=l1;
i32_store(i->m0,si0,si1);
si0=0U;
goto L46;
}
L48:;
si0=l0;
sj1=W2C2_LL(4294967296U);
i64_store(i->m0,(WasmMemoryAddress)si0+32U,sj1);
si0=l0;
sj1=W2C2_LL(17592186044416U);
i64_store(i->m0,(WasmMemoryAddress)si0+20U,sj1);
si0=l0;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+8U,sj1);
si0=l0;
si1=l4;
i32_store(i->m0,si0,si1);
si0=l0;
L46:;
}else{
si0=0U;
}
L45:;
l1=si0;
if(si0){
goto L44;
}
si0=l4;
si0=f36(instance,si0);
si0=0U;
l1=si0;
L44:;
si0=l1;
L42:;
l0=si0;
si0=l2;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l0;
l1=si0;
si0=!(si0);
if(si0){
goto L40;
}
si0=0U;
l5=si0;
si0=0U;
l10=si0;
L49:;
W2C2_LOOP_START
{
si0=i->g0;
si1=144U;
si0-=si1;
l11=si0;
i->g0=si0;
si0=l1;
si1=28U;
si0+=si1;
l29=si0;
L51:;
W2C2_LOOP_START
{
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+28U);
l2=si0;
si1=l1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+20U);
l0=si1;
si0-=si1;
l12=si0;
si1=23U;
si0=si0 <= si1;
if(si0){
si0=l2;
si1=l1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+24U);
l0=si1;
si0=si0 < si1;
if(si0){
goto L52;
}
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l4=si0;
goto L56;
}
L57:;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l4=si0;
si1=l0;
si0+=si1;
l2=si0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l15=si0;
si1=24U;
si0+=si1;
l7=si0;
si0=l15;
si0=!(si0);
if(si0){
si0=l1;
si1=l0;
si2=l7;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=3U;
goto L53;
}
L58:;
si0=l7;
si1=l12;
si0=si0 <= si1;
if(si0){
goto L55;
}
si0=l7;
si1=l1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+24U);
l2=si1;
si0=si0 <= si1;
if(si0){
si0=l2;
l0=si0;
goto L56;
}
L59:;
L60:;
W2C2_LOOP_START
{
si0=l2;
l0=si0;
si1=1U;
si0<<=(si1&31);
l2=si0;
si0=l0;
si1=l7;
si0=si0 < si1;
if(si0){
goto L60;
}
}
si0=l4;
si1=l0;
si0=f32(instance,si0,si1);
l4=si0;
si0=!(si0);
if(si0){
goto L52;
}
si0=l1;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l1;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+16U,si1);
L56:;
si0=l1;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+28U,si1);
si0=l1;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=l1;
si0=i32_load(i->m0,si0);
si1=l4;
si2=l0;
si3=l1;
sj3=i64_load(i->m0,(WasmMemoryAddress)si3+8U);
si4=l29;
si0=f40(instance,si0,si1,si2,sj3,si4);
l0=si0;
si0=!(si0);
if(si0){
goto L54;
}
si0=0U;
l4=si0;
si0=4820U;
si1=l0;
i32_store(i->m0,si0,si1);
goto L50;
L55:;
si0=l2;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+20U);
l12=si0;
si0=l2;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+8U);
l6=sj0;
si0=l2;
sj0=i64_load(i->m0,si0);
l8=sj0;
si0=l2;
si1=24U;
si0+=si1;
l30=si0;
si1=0U;
si2=l15;
si0=f76(instance,si0,si1,si2);
if(si0){
si0=l1;
si1=l0;
si2=l7;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=3U;
goto L53;
}
L61:;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+36U);
l0=si0;
si1=l15;
si2=10U;
si1+=si2;
l4=si1;
si0=si0 >= si1;
if(si0){
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+32U);
l4=si0;
goto L62;
}
L63:;
L64:;
W2C2_LOOP_START
{
si0=l0;
l2=si0;
si1=1U;
si0<<=(si1&31);
l0=si0;
si0=l2;
si1=l4;
si0=si0 < si1;
if(si0){
goto L64;
}
}
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+32U);
si1=l2;
si0=f32(instance,si0,si1);
l4=si0;
si0=!(si0);
if(si0){
goto L52;
}
si0=l1;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+36U,si1);
si0=l1;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+32U,si1);
L62:;
si0=l4;
si1=l12;
i32_store8(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l4;
si1=9U;
si0+=si1;
si1=l30;
si2=l15;
si0=f78(instance,si0,si1,si2);
l0=si0;
si1=l15;
si0+=si1;
si1=0U;
i32_store8(i->m0,si0,si1);
sj0=l6;
sj1=W2C2_LL(0U);
si0=sj0 != sj1;
if(si0){
goto L65;
}
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=46U;
si0-=si1;
l2=si0;
if(si0){
goto L66;
}
si0=l4;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+10U);
si1=46U;
si0-=si1;
l2=si0;
if(si0){
goto L66;
}
si0=l4;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+11U);
l2=si0;
L66:;
si0=l2;
si0=!(si0);
if(si0){
sj0=W2C2_LL(0U);
l6=sj0;
goto L65;
}
L67:;
si0=l1;
si0=i32_load(i->m0,si0);
l2=si0;
si1=-2U;
si0=si0 != si1;
if(si0){
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=47U;
si0=si0 != si1;
if(si0){
goto L69;
}
}
L70:;
si0=i->g0;
si1=16U;
si0-=si1;
l2=si0;
i->g0=si0;
si0=-1U;
l12=si0;
si0=l0;
si1=l2;
si2=12U;
si1+=si2;
si2=4824U;
si3=4828U;
si3=i32_load(i->m0,si3);
si0=f47(instance,si0,si1,si2,si3);
l0=si0;
si1=-1U;
si0=si0 == si1;
if(si0){
si0=4820U;
si1=44U;
i32_store(i->m0,si0,si1);
goto L71;
}
L72:;
si0=l0;
si1=4824U;
si1=i32_load(i->m0,si1);
si2=l11;
si3=1U;
si0=f44(instance,si0,si1,si2,si3);
l12=si0;
L71:;
si0=l2;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l12;
goto L68;
L69:;
si0=l2;
si1=l0;
si2=l11;
si3=1U;
si0=f44(instance,si0,si1,si2,si3);
L68:;
if(si0){
si0=1U;
si1=4820U;
si1=i32_load(i->m0,si1);
si2=44U;
si1=si1 != si2;
if(si1){
goto L53;
}
si0=l1;
si1=l1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+20U);
si2=l7;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=3U;
goto L53;
}
L73:;
si0=l11;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+8U);
l6=sj0;
si0=l11;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+24U);
si1=61440U;
si0&=si1;
si1=4096U;
si0-=si1;
si1=20U;
si0=I32_ROTL(si0,si1);
l0=si0;
si1=11U;
si0=si0 <= si1;
if(si0){
si0=l0;
si1=2U;
si0<<=(si1&31);
si1=1672U;
si0+=si1;
si0=i32_load(i->m0,si0);
}else{
si0=0U;
}
L74:;
l12=si0;
L65:;
si0=l4;
si1=l12;
i32_store8(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l4;
sj1=l6;
i64_store(i->m0,si0,sj1);
si0=l1;
sj1=l8;
i64_store(i->m0,(WasmMemoryAddress)si0+8U,sj1);
si0=l1;
si1=l1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+20U);
si2=l7;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
goto L50;
L54:;
si0=l1;
si1=0U;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=0U;
L53:;
si1=1U;
si0=si0 != si1;
if(si0){
goto L51;
}
L52:;
}
si0=0U;
l4=si0;
L50:;
si0=l11;
si1=144U;
si0+=si1;
i->g0=si0;
si0=l4;
l0=si0;
if(si0){
si0=l0;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+9U);
si1=46U;
si0=si0 == si1;
if(si0){
goto L49;
}
si0=l10;
si1=1U;
si0+=si1;
l10=si0;
si0=l0;
si1=9U;
si0+=si1;
si0=f82(instance,si0);
si1=l5;
si0+=si1;
l5=si0;
goto L49;
}
L75:;
}
si0=l1;
si0=i32_load(i->m0,si0);
l0=si0;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
f30(instance,si0);
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+32U);
f30(instance,si0);
si0=l1;
f30(instance,si0);
si0=l0;
si0=f36(instance,si0);
si0=l3;
si1=l5;
i32_store(i->m0,(WasmMemoryAddress)si0+148U,si1);
si0=l3;
si1=l10;
i32_store(i->m0,(WasmMemoryAddress)si0+144U,si1);
si0=1412U;
si1=l3;
si2=144U;
si1+=si2;
f67(instance,si0,si1);
goto L15;
}
L41:;
si0=l3;
si1=304U;
si0+=si1;
si1=1132U;
si2=7U;
si0=f77(instance,si0,si1,si2);
si0=!(si0);
if(si0){
si0=l3;
si1=1237U;
si2=1199U;
si3=1158U;
si4=1210U;
si3=f61(instance,si3,si4);
l0=si3;
si1=si3?si1:si2;
i32_store(i->m0,(WasmMemoryAddress)si0+160U,si1);
si0=1331U;
si1=l3;
si2=160U;
si1+=si2;
f67(instance,si0,si1);
si0=l0;
si0=!(si0);
if(si0){
goto L15;
}
si0=l0;
f49(instance,si0);
goto L15;
}
L76:;
si0=l3;
si1=304U;
si0+=si1;
si1=1270U;
si2=5U;
si0=f77(instance,si0,si1,si2);
si0=!(si0);
if(si0){
si0=l20;
l0=si0;
L78:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_s(i->m0,si0);
l2=si0;
si1=14U;
si0-=si1;
l1=si0;
si0=l0;
si1=1U;
si0+=si1;
l0=si0;
si0=l2;
si1=255U;
si0&=si1;
l4=si0;
si1=32U;
si0=si0 == si1;
if(si0){
goto L78;
}
si0=l1;
si1=-6U;
si0=si0 > si1;
if(si0){
goto L78;
}
}
si0=1U;
l5=si0;
si0=l4;
si1=43U;
si0-=si1;
switch(si0){
case 0:
goto L80;
case 1:
goto L82;
case 2:
goto L81;
default:
goto L82;
}
L82:;
si0=l0;
si1=1U;
si0-=si1;
l0=si0;
goto L79;
L81:;
si0=0U;
l5=si0;
L80:;
si0=l0;
si0=i32_load8_s(i->m0,si0);
l2=si0;
L79:;
si0=0U;
l1=si0;
si0=l2;
si1=48U;
si0-=si1;
l2=si0;
si1=9U;
si0=si0 <= si1;
if(si0){
si0=l0;
si1=1U;
si0+=si1;
l0=si0;
L84:;
W2C2_LOOP_START
{
si0=l1;
si1=10U;
si0*=si1;
si1=l2;
si0-=si1;
l1=si0;
si0=l0;
si0=i32_load8_s(i->m0,si0);
l2=si0;
si0=l0;
si1=1U;
si0+=si1;
l0=si0;
si0=l2;
si1=48U;
si0-=si1;
l2=si0;
si1=10U;
si0=si0 < si1;
if(si0){
goto L84;
}
}
}
L83:;
si0=0U;
si1=l1;
si0-=si1;
si1=l1;
si2=l5;
si0=si2?si0:si1;
l4=si0;
si1=2U;
si0<<=(si1&31);
si0=f29(instance,si0);
l2=si0;
si0=l4;
si1=0U;
si0=(U32)((I32)si0<=(I32)si1);
if(si0){
si0=0U;
l1=si0;
goto L85;
}
L86:;
si0=0U;
l0=si0;
si0=l2;
l1=si0;
L87:;
W2C2_LOOP_START
{
si0=l1;
si1=1048576U;
si1=f29(instance,si1);
l5=si1;
i32_store(i->m0,si0,si1);
si0=l5;
si0=!(si0);
if(si0){
si0=l3;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+176U,si1);
si0=1394U;
si1=l3;
si2=176U;
si1+=si2;
f67(instance,si0,si1);
goto L15;
}
L88:;
si0=l5;
si1=l0;
si2=1048576U;
si0=f79(instance,si0,si1,si2);
si0=l1;
si1=4U;
si0+=si1;
l1=si0;
si0=l4;
si1=l0;
si2=1U;
si1+=si2;
l0=si1;
si0=si0 != si1;
if(si0){
goto L87;
}
}
si0=0U;
l1=si0;
si0=l2;
l0=si0;
si0=l4;
l5=si0;
L89:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load(i->m0,si0);
l10=si0;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+12345U);
l7=si0;
si0=l10;
f30(instance,si0);
si0=l0;
si1=4U;
si0+=si1;
l0=si0;
si0=l1;
si1=l7;
si0+=si1;
l1=si0;
si0=l5;
si1=1U;
si0-=si1;
l5=si0;
if(si0){
goto L89;
}
}
L85:;
si0=l2;
f30(instance,si0);
si0=l3;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+196U,si1);
si0=l3;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+192U,si1);
si0=1282U;
si1=l3;
si2=192U;
si1+=si2;
f67(instance,si0,si1);
goto L15;
}
L77:;
si0=l3;
si1=304U;
si0+=si1;
si1=1148U;
si2=5U;
si0=f77(instance,si0,si1,si2);
si0=!(si0);
if(si0){
si0=l3;
si1=560U;
si0+=si1;
f34(instance,si0);
si0=i->g0;
si1=16U;
si0-=si1;
l0=si0;
i->g0=si0;
si0=l0;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=l0;
si1=2000000U;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
sj0=W2C2_LL(0U);
l8=sj0;
si0=i->g0;
si1=112U;
si0-=si1;
l1=si0;
i->g0=si0;
si0=l1;
si1=104U;
si0+=si1;
l5=si0;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=l1;
si1=80U;
si0+=si1;
l7=si0;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=l1;
si1=96U;
si0+=si1;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=l1;
si1=88U;
si0+=si1;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=l1;
si1=72U;
si0+=si1;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=l5;
si1=0U;
i32_store16(i->m0,si0,si1);
si0=l7;
si1=1668U;
si1=i32_load(i->m0,si1);
i32_store(i->m0,si0,si1);
si0=l1;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+64U,sj1);
si0=28U;
si1=l0;
l2=si1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l5=si1;
si2=999999999U;
si1=si1 > si2;
if(si1){
goto L91;
}
si0=l2;
sj0=i64_load(i->m0,si0);
l6=sj0;
sj1=W2C2_LL(0U);
si0=(U64)((I64)sj0<(I64)sj1);
if(si0){
goto L92;
}
si0=l1;
si1=0U;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=i->g0;
si1=48U;
si0-=si1;
l2=si0;
i->g0=si0;
si0=l1;
si1=20U;
si0+=si1;
l7=si0;
si1=0U;
i32_store(i->m0,si0,si1);
sj0=l6;
sj1=W2C2_LL(-9223372036854775808U);
sj0|=sj1;
si0=!(sj0);
if(si0){
goto L94;
}
sj0=l6;
l8=sj0;
sj1=W2C2_LL(2U);
si0=sj0 < sj1;
if(si0){
goto L93;
}
si0=l2;
si1=16U;
si0+=si1;
l11=si0;
si0=i->g0;
si1=16U;
si0-=si1;
l4=si0;
i->g0=si0;
si0=i->g0;
si1=16U;
si0-=si1;
l10=si0;
i->g0=si0;
sj0=W2C2_LL(-3761807510621650945U);
sj1=W2C2_LL(3671234140U);
l16=sj1;
sj2=W2C2_LL(4000000000U);
sj1*=sj2;
sj0-=sj1;
l9=sj0;
L95:;
W2C2_LOOP_START
{
sj0=l16;
sj1=W2C2_LL(4294967295U);
si0=sj0 > sj1;
if(si0){
sj0=l16;
sj1=W2C2_LL(1U);
sj0-=sj1;
l16=sj0;
sj0=l9;
sj1=W2C2_LL(4000000000U);
sj0+=sj1;
l9=sj0;
sj1=W2C2_LL(4294967296U);
si0=sj0 < sj1;
if(si0){
goto L95;
}
}
L96:;
}
sj0=W2C2_LL(-4U);
sj1=l16;
sj2=W2C2_LL(-1266874889709551616U);
sj1*=sj2;
sj0-=sj1;
l9=sj0;
sj1=l9;
sj2=W2C2_LL(4000000000U);
sj1=DIV_U(sj1,sj2);
l9=sj1;
sj2=W2C2_LL(4000000000U);
sj1*=sj2;
sj0-=sj1;
l21=sj0;
L97:;
W2C2_LOOP_START
{
sj0=l9;
sj1=W2C2_LL(4294967295U);
si0=sj0 > sj1;
if(si0){
sj0=l9;
sj1=W2C2_LL(1U);
sj0-=sj1;
l9=sj0;
sj0=l21;
sj1=W2C2_LL(4000000000U);
sj0+=sj1;
l21=sj0;
sj1=W2C2_LL(4294967296U);
si0=sj0 < sj1;
if(si0){
goto L97;
}
}
L98:;
}
si0=l4;
sj1=l9;
sj2=l16;
sj3=W2C2_LL(32U);
sj2<<=(sj3&63);
sj1+=sj2;
i64_store(i->m0,si0,sj1);
si0=l4;
sj1=W2C2_LL(9223372036U);
i64_store(i->m0,(WasmMemoryAddress)si0+8U,sj1);
si0=l10;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l4;
sj0=i64_load(i->m0,si0);
l9=sj0;
si0=l11;
si1=l4;
si2=8U;
si1+=si2;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,(WasmMemoryAddress)si0+8U,sj1);
si0=l11;
sj1=l9;
i64_store(i->m0,si0,sj1);
si0=l4;
si1=16U;
si0+=si1;
i->g0=si0;
sj0=l8;
si1=l2;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+16U);
si0=sj0 > sj1;
si1=l2;
si2=24U;
si1+=si2;
sj1=i64_load(i->m0,si1);
l8=sj1;
sj2=W2C2_LL(0U);
si1=sj1 < sj2;
sj2=l8;
si2=!(sj2);
si0=si2?si0:si1;
if(si0){
goto L94;
}
goto L93;
L94:;
si0=l7;
si1=1U;
i32_store(i->m0,si0,si1);
L93:;
si0=l2;
sj1=l6;
sj2=W2C2_LL(32U);
sj1>>=(sj2&63);
l8=sj1;
sj2=W2C2_LL(0U);
sj1*=sj2;
sj2=l6;
sj3=W2C2_LL(4294967295U);
sj2&=sj3;
sj3=W2C2_LL(1000000000U);
sj2*=sj3;
l9=sj2;
sj3=W2C2_LL(32U);
sj2>>=(sj3&63);
l6=sj2;
sj3=W2C2_LL(32U);
sj2>>=(sj3&63);
sj1+=sj2;
sj2=l8;
sj3=W2C2_LL(1000000000U);
sj2*=sj3;
sj3=l6;
sj4=W2C2_LL(4294967295U);
sj3&=sj4;
sj2+=sj3;
l6=sj2;
sj3=W2C2_LL(32U);
sj2>>=(sj3&63);
sj1+=sj2;
i64_store(i->m0,(WasmMemoryAddress)si0+8U,sj1);
si0=l2;
sj1=l9;
sj2=W2C2_LL(4294967295U);
sj1&=sj2;
sj2=l6;
sj3=W2C2_LL(32U);
sj2<<=(sj3&63);
sj1|=sj2;
i64_store(i->m0,si0,sj1);
si0=l1;
si1=l2;
si2=8U;
si1+=si2;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,(WasmMemoryAddress)si0+8U,sj1);
si0=l1;
si1=l2;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,si0,sj1);
si0=l2;
si1=48U;
si0+=si1;
i->g0=si0;
sj0=W2C2_LL(-1U);
l8=sj0;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
sj1=W2C2_LL(0U);
si2=l1;
si3=8U;
si2+=si3;
sj2=i64_load(i->m0,si2);
l9=sj2;
sj3=W2C2_LL(1U);
sj2&=sj3;
sj1-=sj2;
l6=sj1;
sj2=l9;
sj1^=sj2;
sj2=W2C2_LL(0U);
si1=sj1 != sj2;
si0|=si1;
if(si0){
goto L92;
}
sj0=l6;
sj1=W2C2_LL(0U);
si0=(U64)((I64)sj0<(I64)sj1);
if(si0){
goto L92;
}
sj0=W2C2_LL(-1U);
sj1=W2C2_LL(-1U);
si2=l1;
sj2=i64_load(i->m0,si2);
l8=sj2;
si3=l5;
sj3=(U64)(si3);
sj2+=sj3;
l9=sj2;
sj3=W2C2_LL(0U);
sj4=l8;
sj5=l9;
si4=sj4 > sj5;
sj4=(U64)(si4);
sj5=l6;
sj4+=sj5;
l8=sj4;
sj5=W2C2_LL(1U);
sj4&=sj5;
sj3-=sj4;
l6=sj3;
sj4=W2C2_LL(0U);
si3=(U64)((I64)sj3<(I64)sj4);
sj1=si3?sj1:sj2;
sj2=l6;
sj3=l8;
sj2^=sj3;
sj3=W2C2_LL(0U);
si2=sj2 != sj3;
sj0=si2?sj0:sj1;
l8=sj0;
L92:;
si0=l1;
sj1=l8;
i64_store(i->m0,(WasmMemoryAddress)si0+88U,sj1);
si0=58U;
si1=0U;
si2=l1;
si3=-64U;
si2-=si3;
si3=l1;
si4=24U;
si3+=si4;
si4=1U;
si5=l1;
si6=60U;
si5+=si6;
si2=i22_wasiX5FsnapshotX5Fpreview111_pollX5Foneoff(instance,si2,si3,si4,si5);
si3=65535U;
si2&=si3;
si3=l1;
si3=i32_load16_u(i->m0,(WasmMemoryAddress)si3+32U);
si2|=si3;
si0=si2?si0:si1;
L91:;
l5=si0;
si0=l1;
si1=112U;
si0+=si1;
i->g0=si0;
si0=l5;
l2=si0;
if(si0){
si0=4820U;
si1=l2;
i32_store(i->m0,si0,si1);
}
L99:;
si0=l0;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l3;
si1=704U;
si0+=si1;
f34(instance,si0);
si0=i->g0;
si1=16U;
si0-=si1;
l0=si0;
i->g0=si0;
si0=l0;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+8U,sj1);
si0=0U;
sj1=W2C2_LL(1000000000U);
si2=l0;
si3=8U;
si2+=si3;
si0=f37(instance,si0,sj1,si2);
si0=l0;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+8U);
sj1=W2C2_LL(1000000000U);
sj0=DIV_U(sj0,sj1);
l6=sj0;
si0=l0;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l3;
sj1=l6;
sj2=W2C2_LL(1600000000U);
si1=(U64)((I64)sj1>(I64)sj2);
i32_store(i->m0,(WasmMemoryAddress)si0+208U,si1);
si0=l3;
si1=l3;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+712U);
si2=l3;
si2=i32_load(i->m0,(WasmMemoryAddress)si2+568U);
si1=(U32)((I32)si1>(I32)si2);
si2=l3;
sj2=i64_load(i->m0,(WasmMemoryAddress)si2+704U);
si3=l3;
sj3=i64_load(i->m0,(WasmMemoryAddress)si3+560U);
si2=(U64)((I64)sj2>(I64)sj3);
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+212U,si1);
si0=1506U;
si1=l3;
si2=208U;
si1+=si2;
f67(instance,si0,si1);
goto L15;
}
L90:;
si0=l0;
si1=7761509U;
si0=si0 == si1;
if(si0){
si0=0U;
l5=si0;
si0=3952U;
si0=i32_load(i->m0,si0);
si1=-1U;
si0=si0 == si1;
if(si0){
si0=i->g0;
si1=16U;
si0-=si1;
l2=si0;
i->g0=si0;
si0=l2;
si1=12U;
si0+=si1;
si1=l2;
si2=8U;
si1+=si2;
si0=i22_wasiX5FsnapshotX5Fpreview117_environX5FsizesX5Fget(instance,si0,si1);
si1=65535U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l2;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+12U);
l0=si0;
si0=!(si0);
if(si0){
si0=4856U;
l0=si0;
goto L103;
}
L105:;
si0=l0;
si1=1U;
si0+=si1;
l0=si0;
si0=!(si0);
if(si0){
goto L107;
}
si0=l2;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
si0=f29(instance,si0);
l1=si0;
si0=!(si0);
if(si0){
goto L107;
}
si0=l0;
si1=4U;
si0=f31(instance,si0,si1);
l0=si0;
if(si0){
goto L106;
}
si0=l1;
f30(instance,si0);
L107:;
si0=70U;
f35(instance,si0);
UNREACHABLE;
L106:;
si0=l0;
si1=l1;
si0=i22_wasiX5FsnapshotX5Fpreview111_environX5Fget(instance,si0,si1);
si1=65535U;
si0&=si1;
si0=!(si0);
if(si0){
goto L103;
}
si0=l1;
f30(instance,si0);
si0=l0;
f30(instance,si0);
}
L104:;
si0=71U;
f35(instance,si0);
UNREACHABLE;
L103:;
si0=3952U;
si1=l0;
i32_store(i->m0,si0,si1);
si0=l2;
si1=16U;
si0+=si1;
i->g0=si0;
}
L102:;
si0=0U;
si1=1226U;
si2=61U;
si1=f81(instance,si1,si2);
l0=si1;
si2=1226U;
si1=si1 == si2;
if(si1){
goto L101;
}
si0=l0;
si1=1226U;
si0-=si1;
l1=si0;
si1=1226U;
si0+=si1;
si0=i32_load8_u(i->m0,si0);
if(si0){
goto L108;
}
si0=3952U;
si0=i32_load(i->m0,si0);
l2=si0;
si0=!(si0);
if(si0){
goto L108;
}
si0=l2;
si0=i32_load(i->m0,si0);
l0=si0;
si0=!(si0);
if(si0){
goto L108;
}
si0=l2;
si1=4U;
si0+=si1;
l2=si0;
L109:;
W2C2_LOOP_START
{
si0=l0;
l11=si0;
si0=0U;
si1=l1;
l4=si1;
si1=!(si1);
if(si1){
goto L111;
}
si0=1226U;
si0=i32_load8_u(i->m0,si0);
l7=si0;
si0=!(si0);
if(si0){
si0=0U;
l7=si0;
goto L112;
}
L113:;
si0=1227U;
l10=si0;
si0=l4;
si1=1U;
si0-=si1;
l4=si0;
L115:;
W2C2_LOOP_START
{
si0=l7;
si1=l11;
si1=i32_load8_u(i->m0,si1);
l12=si1;
si0=si0 != si1;
if(si0){
goto L114;
}
si0=l12;
si0=!(si0);
if(si0){
goto L114;
}
si0=l4;
si0=!(si0);
if(si0){
goto L114;
}
si0=l4;
si1=1U;
si0-=si1;
l4=si0;
si0=l11;
si1=1U;
si0+=si1;
l11=si0;
si0=l10;
si0=i32_load8_u(i->m0,si0);
l7=si0;
si0=l10;
si1=1U;
si0+=si1;
l10=si0;
si0=l7;
if(si0){
goto L115;
}
}
si0=0U;
l7=si0;
L114:;
L112:;
si0=l7;
si1=l11;
si1=i32_load8_u(i->m0,si1);
si0-=si1;
L111:;
si0=!(si0);
if(si0){
si0=l0;
si1=l1;
si0+=si1;
l0=si0;
si0=i32_load8_u(i->m0,si0);
si1=61U;
si0=si0 == si1;
if(si0){
goto L110;
}
}
L116:;
si0=l2;
si0=i32_load(i->m0,si0);
l0=si0;
si0=l2;
si1=4U;
si0+=si1;
l2=si0;
si0=l0;
if(si0){
goto L109;
}
goto L108;
L110:;
}
si0=l0;
si1=1U;
si0+=si1;
l5=si0;
L108:;
si0=l5;
L101:;
l0=si0;
si0=l3;
si1=l18;
si2=2U;
si1=(U32)((I32)si1>=(I32)si2);
if(si1){
si1=l13;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
}else{
si1=1261U;
}
L117:;
i32_store(i->m0,(WasmMemoryAddress)si0+228U,si1);
si0=l3;
si1=l18;
i32_store(i->m0,(WasmMemoryAddress)si0+224U,si1);
si0=l3;
si1=l0;
si2=1261U;
si3=l0;
si1=si3?si1:si2;
i32_store(i->m0,(WasmMemoryAddress)si0+232U,si1);
si0=1296U;
si1=l3;
si2=224U;
si1+=si2;
f67(instance,si0,si1);
goto L15;
}
L100:;
si0=l3;
si1=304U;
si0+=si1;
si1=1256U;
si2=3U;
si0=f77(instance,si0,si1,si2);
si0=!(si0);
if(si0){
sj0=i33_wasiX3AclocksX2FmonotonicX2DclockX400X2E2X2E03_now(instance);
l6=sj0;
sj0=W2C2_LL(50000000U);
si0=i33_wasiX3AclocksX2FmonotonicX2DclockX400X2E2X2E018_subscribeX2Dduration(instance,sj0);
l0=si0;
sj0=W2C2_LL(10000000U);
si0=i33_wasiX3AclocksX2FmonotonicX2DclockX400X2E2X2E018_subscribeX2Dduration(instance,sj0);
l1=si0;
sj0=W2C2_LL(0U);
si0=i33_wasiX3AclocksX2FmonotonicX2DclockX400X2E2X2E018_subscribeX2Dduration(instance,sj0);
l5=si0;
si0=i18_wasiX3AioX2FpollX400X2E2X2E022_X5BmethodX5DpollableX2Eready(instance,si0);
l10=si0;
si0=l0;
si0=i18_wasiX3AioX2FpollX400X2E2X2E022_X5BmethodX5DpollableX2Eready(instance,si0);
l7=si0;
si0=l3;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+564U,si1);
si0=l3;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+560U,si1);
si0=l3;
si1=560U;
si0+=si1;
si1=2U;
si2=l3;
si3=704U;
si2+=si3;
i18_wasiX3AioX2FpollX400X2E2X2E04_poll(instance,si0,si1,si2);
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+708U);
l4=si0;
si1=0U;
si0=(U32)((I32)si0<=(I32)si1);
if(si0){
si0=-1U;
}else{
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+704U);
si0=i32_load(i->m0,si0);
}
L119:;
l2=si0;
si0=l3;
si1=256U;
si0+=si1;
sj1=i33_wasiX3AclocksX2FmonotonicX2DclockX400X2E2X2E03_now(instance);
sj2=l6;
sj1-=sj2;
sj2=W2C2_LL(9999999U);
si1=sj1 > sj2;
i32_store(i->m0,si0,si1);
si0=l3;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+252U,si1);
si0=l3;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+248U,si1);
si0=l3;
si1=l7;
i32_store(i->m0,(WasmMemoryAddress)si0+244U,si1);
si0=l3;
si1=l10;
i32_store(i->m0,(WasmMemoryAddress)si0+240U,si1);
si0=1460U;
si1=l3;
si2=240U;
si1+=si2;
f67(instance,si0,si1);
si0=l0;
i18_wasiX3AioX2FpollX400X2E2X2E023_X5BresourceX2DdropX5Dpollable(instance,si0);
si0=l1;
i18_wasiX3AioX2FpollX400X2E2X2E023_X5BresourceX2DdropX5Dpollable(instance,si0);
si0=l5;
i18_wasiX3AioX2FpollX400X2E2X2E023_X5BresourceX2DdropX5Dpollable(instance,si0);
goto L15;
}
L118:;
si0=l3;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+304U);
sj1=W2C2_LL(29113321788762477U);
si0=sj0 == sj1;
if(si0){
si0=l17;
si0=f50(instance,si0);
si0=l3;
si1=1U;
si1=i10_spireX5Ftest7_missing(instance,si1);
i32_store(i->m0,(WasmMemoryAddress)si0+272U,si1);
si0=1528U;
si1=l3;
si2=272U;
si1+=si2;
f67(instance,si0,si1);
goto L15;
}
L120:;
si0=l3;
si1=304U;
si0+=si1;
si1=1250U;
si2=6U;
si0=f77(instance,si0,si1,si2);
si0=!(si0);
if(si0){
goto L39;
}
si0=l3;
si1=304U;
si0+=si1;
si1=1089U;
si2=5U;
si0=f77(instance,si0,si1,si2);
si0=!(si0);
if(si0){
goto L38;
}
si0=l3;
si1=l3;
si2=304U;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+288U,si1);
si0=1319U;
si1=l3;
si2=288U;
si1+=si2;
f67(instance,si0,si1);
goto L15;
L40:;
si0=1139U;
f69(instance,si0);
goto L15;
L39:;
si0=l17;
si0=f50(instance,si0);
f51(instance);
si0=3U;
f35(instance,si0);
UNREACHABLE;
L38:;
si0=l17;
si0=f50(instance,si0);
UNREACHABLE;
L15:;
si0=l3;
si1=304U;
si0+=si1;
si1=l22;
si0=f53(instance,si0,si1);
if(si0){
goto L11;
}
}
}
L10:;
si0=1112U;
f69(instance,si0);
si0=l3;
si1=768U;
si0+=si1;
i->g0=si0;
si0=l14;
si1=16U;
si0+=si1;
i->g0=si0;
si0=0U;
goto L3;
}
L8:;
si0=71U;
f35(instance,si0);
UNREACHABLE;
L7:;
si0=70U;
f35(instance,si0);
UNREACHABLE;
L6:;
si0=70U;
f35(instance,si0);
UNREACHABLE;
L5:;
si0=l0;
f30(instance,si0);
si0=70U;
f35(instance,si0);
UNREACHABLE;
L4:;
si0=l0;
f30(instance,si0);
si0=l13;
f30(instance,si0);
si0=71U;
f35(instance,si0);
UNREACHABLE;
L3:;
l13=si0;
f51(instance);
si0=l13;
if(si0){
goto L1;
}
goto L0;
}
L2:;
UNREACHABLE;
L1:;
si0=l13;
f42(instance,si0);
UNREACHABLE;
L0:;
}

U32 f29(wasmModuleInstance* instance,U32 l0) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l1=0;
U32 l2=0;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 l6=0;
U32 l7=0;
U32 l8=0;
U32 l9=0;
U32 l10=0;
U32 l11=0;
U32 si0,si1,si2,si3,si4,si5;
U64 sj1,sj3;
si0=i->g0;
si1=16U;
si0-=si1;
l11=si0;
i->g0=si0;
si0=4348U;
si0=i32_load(i->m0,si0);
l9=si0;
si0=!(si0);
if(si0){
si0=4796U;
si0=i32_load(i->m0,si0);
l2=si0;
si0=!(si0);
if(si0){
si0=4808U;
sj1=W2C2_LL(-1U);
i64_store(i->m0,si0,sj1);
si0=4800U;
sj1=W2C2_LL(281474976776192U);
i64_store(i->m0,si0,sj1);
si0=4796U;
si1=l11;
si2=8U;
si1+=si2;
si2=-16U;
si1&=si2;
si2=1431655768U;
si1^=si2;
l2=si1;
i32_store(i->m0,si0,si1);
si0=4816U;
si1=0U;
i32_store(i->m0,si0,si1);
si0=4768U;
si1=0U;
i32_store(i->m0,si0,si1);
}
L2:;
si0=4772U;
si1=72528U;
i32_store(i->m0,si0,si1);
si0=4340U;
si1=72528U;
i32_store(i->m0,si0,si1);
si0=4360U;
si1=l2;
i32_store(i->m0,si0,si1);
si0=4356U;
si1=-1U;
i32_store(i->m0,si0,si1);
si0=4776U;
si1=58544U;
i32_store(i->m0,si0,si1);
si0=4760U;
si1=58544U;
i32_store(i->m0,si0,si1);
si0=4756U;
si1=58544U;
i32_store(i->m0,si0,si1);
L3:;
W2C2_LOOP_START
{
si0=l1;
si1=4384U;
si0+=si1;
si1=l1;
si2=4372U;
si1+=si2;
l2=si1;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l1;
si2=4364U;
si1+=si2;
l4=si1;
i32_store(i->m0,si0,si1);
si0=l1;
si1=4376U;
si0+=si1;
si1=l4;
i32_store(i->m0,si0,si1);
si0=l1;
si1=4392U;
si0+=si1;
si1=l1;
si2=4380U;
si1+=si2;
l4=si1;
i32_store(i->m0,si0,si1);
si0=l4;
si1=l2;
i32_store(i->m0,si0,si1);
si0=l1;
si1=4400U;
si0+=si1;
si1=l1;
si2=4388U;
si1+=si2;
l2=si1;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l4;
i32_store(i->m0,si0,si1);
si0=l1;
si1=4396U;
si0+=si1;
si1=l2;
i32_store(i->m0,si0,si1);
si0=l1;
si1=32U;
si0+=si1;
l1=si0;
si1=256U;
si0=si0 != si1;
if(si0){
goto L3;
}
}
si0=131020U;
si1=56U;
i32_store(i->m0,si0,si1);
si0=4352U;
si1=4812U;
si1=i32_load(i->m0,si1);
i32_store(i->m0,si0,si1);
si0=4348U;
si1=72536U;
l9=si1;
i32_store(i->m0,si0,si1);
si0=4336U;
si1=58480U;
i32_store(i->m0,si0,si1);
si0=72540U;
si1=58481U;
i32_store(i->m0,si0,si1);
}
L1:;
si0=l0;
si1=236U;
si0=si0 <= si1;
if(si0){
si0=4324U;
si0=i32_load(i->m0,si0);
l6=si0;
si1=16U;
si2=l0;
si3=19U;
si2+=si3;
si3=496U;
si2&=si3;
si3=l0;
si4=11U;
si3=si3 < si4;
si1=si3?si1:si2;
l4=si1;
si2=3U;
si1>>=(si2&31);
l2=si1;
si0>>=(si1&31);
l1=si0;
si1=3U;
si0&=si1;
if(si0){
si0=l1;
si1=1U;
si0&=si1;
si1=l2;
si0|=si1;
si1=1U;
si0^=si1;
l4=si0;
si1=3U;
si0<<=(si1&31);
l2=si0;
si1=4364U;
si0+=si1;
l1=si0;
si1=l2;
si2=4372U;
si1+=si2;
si1=i32_load(i->m0,si1);
l2=si1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l0=si1;
si0=si0 == si1;
if(si0){
si0=4324U;
si1=l6;
si2=-2U;
si3=l4;
si2=I32_ROTL(si2,si3);
si1&=si2;
i32_store(i->m0,si0,si1);
goto L18;
}
L19:;
si0=l1;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l0;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
L18:;
si0=l2;
si1=8U;
si0+=si1;
l1=si0;
si0=l2;
si1=l4;
si2=3U;
si1<<=(si2&31);
l4=si1;
si2=3U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l2;
si1=l4;
si0+=si1;
l2=si0;
si1=l2;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
goto L4;
}
L17:;
si0=l4;
si1=4332U;
si1=i32_load(i->m0,si1);
l8=si1;
si0=si0 <= si1;
if(si0){
goto L15;
}
si0=l1;
if(si0){
si0=l1;
si1=l2;
si0<<=(si1&31);
si1=2U;
si2=l2;
si1<<=(si2&31);
l1=si1;
si2=0U;
si3=l1;
si2-=si3;
si1|=si2;
si0&=si1;
si0=I32_CTZ(si0);
l2=si0;
si1=3U;
si0<<=(si1&31);
l1=si0;
si1=4364U;
si0+=si1;
l0=si0;
si1=l1;
si2=4372U;
si1+=si2;
si1=i32_load(i->m0,si1);
l1=si1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l3=si1;
si0=si0 == si1;
if(si0){
si0=4324U;
si1=l6;
si2=-2U;
si3=l2;
si2=I32_ROTL(si2,si3);
si1&=si2;
l6=si1;
i32_store(i->m0,si0,si1);
goto L21;
}
L22:;
si0=l0;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l3;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
L21:;
si0=l1;
si1=l4;
si2=3U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l1;
si1=l2;
si2=3U;
si1<<=(si2&31);
l2=si1;
si0+=si1;
si1=l2;
si2=l4;
si1-=si2;
l0=si1;
i32_store(i->m0,si0,si1);
si0=l1;
si1=l4;
si0+=si1;
l3=si0;
si1=l0;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l8;
if(si0){
si0=l8;
si1=-8U;
si0&=si1;
si1=4364U;
si0+=si1;
l4=si0;
si0=4344U;
si0=i32_load(i->m0,si0);
l2=si0;
si0=l6;
si1=1U;
si2=l8;
si3=3U;
si2>>=(si3&31);
si1<<=(si2&31);
l5=si1;
si0&=si1;
si0=!(si0);
if(si0){
si0=4324U;
si1=l5;
si2=l6;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=l4;
goto L24;
}
L25:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
L24:;
l5=si0;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l4;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l2;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l2;
si1=l5;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
}
L23:;
si0=l1;
si1=8U;
si0+=si1;
l1=si0;
si0=4344U;
si1=l3;
i32_store(i->m0,si0,si1);
si0=4332U;
si1=l0;
i32_store(i->m0,si0,si1);
goto L4;
}
L20:;
si0=4328U;
si0=i32_load(i->m0,si0);
l10=si0;
si0=!(si0);
if(si0){
goto L15;
}
si0=l10;
si0=I32_CTZ(si0);
si1=2U;
si0<<=(si1&31);
si1=4628U;
si0+=si1;
si0=i32_load(i->m0,si0);
l3=si0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
si1=-8U;
si0&=si1;
si1=l4;
si0-=si1;
l2=si0;
si0=l3;
l0=si0;
L26:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l1=si0;
si0=!(si0);
if(si0){
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l1=si0;
si0=!(si0);
if(si0){
goto L27;
}
}
L28:;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
si1=-8U;
si0&=si1;
si1=l4;
si0-=si1;
l0=si0;
si1=l2;
si2=l0;
si3=l2;
si2=si2 < si3;
l0=si2;
si0=si2?si0:si1;
l2=si0;
si0=l1;
si1=l3;
si2=l0;
si0=si2?si0:si1;
l3=si0;
si0=l1;
l0=si0;
goto L26;
L27:;
}
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+24U);
l9=si0;
si0=l3;
si1=l3;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+12U);
l1=si1;
si0=si0 != si1;
if(si0){
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l0=si0;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l1;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
goto L5;
}
L29:;
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l0=si0;
if(si0){
si0=l3;
si1=20U;
si0+=si1;
}else{
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l0=si0;
si0=!(si0);
if(si0){
goto L14;
}
si0=l3;
si1=16U;
si0+=si1;
}
L30:;
l5=si0;
L31:;
W2C2_LOOP_START
{
si0=l5;
l7=si0;
si0=l0;
l1=si0;
si1=20U;
si0+=si1;
l5=si0;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l0=si0;
if(si0){
goto L31;
}
si0=l1;
si1=16U;
si0+=si1;
l5=si0;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l0=si0;
if(si0){
goto L31;
}
}
si0=l7;
si1=0U;
i32_store(i->m0,si0,si1);
goto L5;
}
L16:;
si0=-1U;
l4=si0;
si0=l0;
si1=-65U;
si0=si0 > si1;
if(si0){
goto L15;
}
si0=l0;
si1=19U;
si0+=si1;
l1=si0;
si1=-16U;
si0&=si1;
l4=si0;
si0=4328U;
si0=i32_load(i->m0,si0);
l10=si0;
si0=!(si0);
if(si0){
goto L15;
}
si0=31U;
l8=si0;
si0=0U;
si1=l4;
si0-=si1;
l2=si0;
si0=l0;
si1=16777196U;
si0=si0 <= si1;
if(si0){
si0=l4;
si1=38U;
si2=l1;
si3=8U;
si2>>=(si3&31);
si2=I32_CLZ(si2);
l1=si2;
si1-=si2;
si0>>=(si1&31);
si1=1U;
si0&=si1;
si1=l1;
si2=1U;
si1<<=(si2&31);
si0-=si1;
si1=62U;
si0+=si1;
l8=si0;
}
L32:;
si0=l8;
si1=2U;
si0<<=(si1&31);
si1=4628U;
si0+=si1;
si0=i32_load(i->m0,si0);
l0=si0;
si0=!(si0);
if(si0){
si0=0U;
l1=si0;
goto L35;
}
L36:;
si0=0U;
l1=si0;
si0=l4;
si1=0U;
si2=25U;
si3=l8;
si4=1U;
si3>>=(si4&31);
si2-=si3;
si3=l8;
si4=31U;
si3=si3 == si4;
si1=si3?si1:si2;
si0<<=(si1&31);
l3=si0;
L37:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
si1=-8U;
si0&=si1;
si1=l4;
si0-=si1;
l6=si0;
si1=l2;
si0=si0 >= si1;
if(si0){
goto L38;
}
si0=l0;
l5=si0;
si0=l6;
l2=si0;
if(si0){
goto L38;
}
si0=0U;
l2=si0;
si0=l0;
l1=si0;
goto L34;
L38:;
si0=l1;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+20U);
l6=si1;
si2=l6;
si3=l0;
si4=l3;
si5=29U;
si4>>=(si5&31);
si5=4U;
si4&=si5;
si3+=si4;
si4=16U;
si3+=si4;
si3=i32_load(i->m0,si3);
l7=si3;
si2=si2 == si3;
si0=si2?si0:si1;
si1=l1;
si2=l6;
si0=si2?si0:si1;
l1=si0;
si0=l3;
si1=1U;
si0<<=(si1&31);
l3=si0;
si0=l7;
l0=si0;
if(si0){
goto L37;
}
}
L35:;
si0=l1;
si1=l5;
si0|=si1;
si0=!(si0);
if(si0){
si0=0U;
l5=si0;
si0=2U;
si1=l8;
si0<<=(si1&31);
l1=si0;
si1=0U;
si2=l1;
si1-=si2;
si0|=si1;
si1=l10;
si0&=si1;
l1=si0;
si0=!(si0);
if(si0){
goto L15;
}
si0=l1;
si0=I32_CTZ(si0);
si1=2U;
si0<<=(si1&31);
si1=4628U;
si0+=si1;
si0=i32_load(i->m0,si0);
l1=si0;
}
L39:;
si0=l1;
si0=!(si0);
if(si0){
goto L33;
}
L34:;
L40:;
W2C2_LOOP_START
{
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
si1=-8U;
si0&=si1;
si1=l4;
si0-=si1;
l6=si0;
si1=l2;
si0=si0 < si1;
l3=si0;
si0=l6;
si1=l2;
si2=l3;
si0=si2?si0:si1;
l2=si0;
si0=l1;
si1=l5;
si2=l3;
si0=si2?si0:si1;
l5=si0;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l0=si0;
if(si0){
si0=l0;
}else{
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
}
L41:;
l1=si0;
if(si0){
goto L40;
}
}
L33:;
si0=l5;
si0=!(si0);
if(si0){
goto L15;
}
si0=l2;
si1=4332U;
si1=i32_load(i->m0,si1);
si2=l4;
si1-=si2;
si0=si0 >= si1;
if(si0){
goto L15;
}
si0=l5;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+24U);
l7=si0;
si0=l5;
si1=l5;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+12U);
l1=si1;
si0=si0 != si1;
if(si0){
si0=l5;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l0=si0;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l1;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
goto L6;
}
L42:;
si0=l5;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l0=si0;
if(si0){
si0=l5;
si1=20U;
si0+=si1;
}else{
si0=l5;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l0=si0;
si0=!(si0);
if(si0){
goto L13;
}
si0=l5;
si1=16U;
si0+=si1;
}
L43:;
l3=si0;
L44:;
W2C2_LOOP_START
{
si0=l3;
l6=si0;
si0=l0;
l1=si0;
si1=20U;
si0+=si1;
l3=si0;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l0=si0;
if(si0){
goto L44;
}
si0=l1;
si1=16U;
si0+=si1;
l3=si0;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l0=si0;
if(si0){
goto L44;
}
}
si0=l6;
si1=0U;
i32_store(i->m0,si0,si1);
goto L6;
L15:;
si0=l4;
si1=4332U;
si1=i32_load(i->m0,si1);
l1=si1;
si0=si0 <= si1;
if(si0){
si0=4344U;
si0=i32_load(i->m0,si0);
l2=si0;
si0=l1;
si1=l4;
si0-=si1;
l0=si0;
si1=16U;
si0=si0 >= si1;
if(si0){
si0=l2;
si1=l4;
si0+=si1;
l3=si0;
si1=l0;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l1;
si1=l2;
si0+=si1;
si1=l0;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l4;
si2=3U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
goto L46;
}
L47:;
si0=l2;
si1=l1;
si2=3U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l1;
si1=l2;
si0+=si1;
l1=si0;
si1=l1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=0U;
l3=si0;
si0=0U;
l0=si0;
L46:;
si0=4332U;
si1=l0;
i32_store(i->m0,si0,si1);
si0=4344U;
si1=l3;
i32_store(i->m0,si0,si1);
si0=l2;
si1=8U;
si0+=si1;
l1=si0;
goto L4;
}
L45:;
si0=l4;
si1=4336U;
si1=i32_load(i->m0,si1);
l0=si1;
si0=si0 < si1;
if(si0){
si0=l4;
si1=l9;
si0+=si1;
l1=si0;
si1=l0;
si2=l4;
si1-=si2;
l2=si1;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=4348U;
si1=l1;
i32_store(i->m0,si0,si1);
si0=4336U;
si1=l2;
i32_store(i->m0,si0,si1);
si0=l9;
si1=l4;
si2=3U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l9;
si1=8U;
si0+=si1;
l1=si0;
goto L4;
}
L48:;
si0=0U;
l1=si0;
si0=l4;
si1=l4;
si2=71U;
si1+=si2;
l7=si1;
si2=4796U;
si2=i32_load(i->m0,si2);
if(si2){
si2=4804U;
si2=i32_load(i->m0,si2);
goto L49;
}
L50:;
si2=4808U;
sj3=W2C2_LL(-1U);
i64_store(i->m0,si2,sj3);
si2=4800U;
sj3=W2C2_LL(281474976776192U);
i64_store(i->m0,si2,sj3);
si2=4796U;
si3=l11;
si4=12U;
si3+=si4;
si4=-16U;
si3&=si4;
si4=1431655768U;
si3^=si4;
i32_store(i->m0,si2,si3);
si2=4816U;
si3=0U;
i32_store(i->m0,si2,si3);
si2=4768U;
si3=0U;
i32_store(i->m0,si2,si3);
si2=65536U;
L49:;
l2=si2;
si1+=si2;
l3=si1;
si2=0U;
si3=l2;
si2-=si3;
l6=si2;
si1&=si2;
l5=si1;
si0=si0 >= si1;
if(si0){
si0=4820U;
si1=48U;
i32_store(i->m0,si0,si1);
goto L4;
}
L51:;
si0=4764U;
si0=i32_load(i->m0,si0);
l1=si0;
si0=!(si0);
if(si0){
goto L52;
}
si0=4756U;
si0=i32_load(i->m0,si0);
l2=si0;
si1=l5;
si0+=si1;
l8=si0;
si1=l2;
si0=si0 > si1;
si1=l1;
si2=l8;
si1=si1 >= si2;
si0&=si1;
if(si0){
goto L52;
}
si0=0U;
l1=si0;
si0=4820U;
si1=48U;
i32_store(i->m0,si0,si1);
goto L4;
L52:;
si0=4768U;
si0=i32_load8_u(i->m0,si0);
si1=4U;
si0&=si1;
if(si0){
goto L10;
}
si0=l9;
if(si0){
si0=4772U;
l1=si0;
L56:;
W2C2_LOOP_START
{
si0=l9;
si1=l1;
si1=i32_load(i->m0,si1);
l2=si1;
si0=si0 >= si1;
if(si0){
si0=l2;
si1=l1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si0+=si1;
si1=l9;
si0=si0 > si1;
if(si0){
goto L54;
}
}
L57:;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l1=si0;
if(si0){
goto L56;
}
}
}
L55:;
si0=0U;
si0=f48(instance,si0);
l3=si0;
si1=-1U;
si0=si0 == si1;
if(si0){
goto L11;
}
si0=l5;
l6=si0;
si0=4800U;
si0=i32_load(i->m0,si0);
l1=si0;
si1=1U;
si0-=si1;
l2=si0;
si1=l3;
si0&=si1;
if(si0){
si0=l5;
si1=l3;
si0-=si1;
si1=l2;
si2=l3;
si1+=si2;
si2=0U;
si3=l1;
si2-=si3;
si1&=si2;
si0+=si1;
l6=si0;
}
L58:;
si0=l4;
si1=l6;
si0=si0 >= si1;
if(si0){
goto L11;
}
si0=l6;
si1=2147483646U;
si0=si0 > si1;
if(si0){
goto L11;
}
si0=4764U;
si0=i32_load(i->m0,si0);
l1=si0;
if(si0){
si0=4756U;
si0=i32_load(i->m0,si0);
l2=si0;
si1=l6;
si0+=si1;
l0=si0;
si1=l2;
si0=si0 <= si1;
if(si0){
goto L11;
}
si0=l0;
si1=l1;
si0=si0 > si1;
if(si0){
goto L11;
}
}
L59:;
si0=l6;
si0=f48(instance,si0);
l1=si0;
si1=l3;
si0=si0 != si1;
if(si0){
goto L53;
}
goto L9;
L54:;
si0=l3;
si1=l0;
si0-=si1;
si1=l6;
si0&=si1;
l6=si0;
si1=2147483646U;
si0=si0 > si1;
if(si0){
goto L11;
}
si0=l6;
si0=f48(instance,si0);
l3=si0;
si1=l1;
si1=i32_load(i->m0,si1);
si2=l1;
si2=i32_load(i->m0,(WasmMemoryAddress)si2+4U);
si1+=si2;
si0=si0 == si1;
if(si0){
goto L12;
}
si0=l3;
l1=si0;
L53:;
si0=l6;
si1=l4;
si2=72U;
si1+=si2;
si0=si0 >= si1;
if(si0){
goto L60;
}
si0=l1;
si1=-1U;
si0=si0 == si1;
if(si0){
goto L60;
}
si0=4804U;
si0=i32_load(i->m0,si0);
l2=si0;
si1=l7;
si2=l6;
si1-=si2;
si0+=si1;
si1=0U;
si2=l2;
si1-=si2;
si0&=si1;
l2=si0;
si1=2147483646U;
si0=si0 > si1;
if(si0){
si0=l1;
l3=si0;
goto L9;
}
L61:;
si0=l2;
si0=f48(instance,si0);
si1=-1U;
si0=si0 != si1;
if(si0){
si0=l2;
si1=l6;
si0+=si1;
l6=si0;
si0=l1;
l3=si0;
goto L9;
}
L62:;
si0=0U;
si1=l6;
si0-=si1;
si0=f48(instance,si0);
goto L11;
L60:;
si0=l1;
l3=si0;
si0=l1;
si1=-1U;
si0=si0 != si1;
if(si0){
goto L9;
}
goto L11;
L14:;
si0=0U;
l1=si0;
goto L5;
L13:;
si0=0U;
l1=si0;
goto L6;
L12:;
si0=l3;
si1=-1U;
si0=si0 != si1;
if(si0){
goto L9;
}
L11:;
si0=4768U;
si1=4768U;
si1=i32_load(i->m0,si1);
si2=4U;
si1|=si2;
i32_store(i->m0,si0,si1);
L10:;
si0=l5;
si1=2147483646U;
si0=si0 > si1;
if(si0){
goto L8;
}
si0=l5;
si0=f48(instance,si0);
l3=si0;
si0=0U;
si0=f48(instance,si0);
l1=si0;
si0=l3;
si1=-1U;
si0=si0 == si1;
if(si0){
goto L8;
}
si0=l1;
si1=-1U;
si0=si0 == si1;
if(si0){
goto L8;
}
si0=l1;
si1=l3;
si0=si0 <= si1;
if(si0){
goto L8;
}
si0=l1;
si1=l3;
si0-=si1;
l6=si0;
si1=l4;
si2=56U;
si1+=si2;
si0=si0 <= si1;
if(si0){
goto L8;
}
L9:;
si0=4756U;
si1=4756U;
si1=i32_load(i->m0,si1);
si2=l6;
si1+=si2;
l1=si1;
i32_store(i->m0,si0,si1);
si0=4760U;
si0=i32_load(i->m0,si0);
si1=l1;
si0=si0 < si1;
if(si0){
si0=4760U;
si1=l1;
i32_store(i->m0,si0,si1);
}
L63:;
si0=4348U;
si0=i32_load(i->m0,si0);
l2=si0;
if(si0){
si0=4772U;
l1=si0;
L68:;
W2C2_LOOP_START
{
si0=l3;
si1=l1;
si1=i32_load(i->m0,si1);
l0=si1;
si2=l1;
si2=i32_load(i->m0,(WasmMemoryAddress)si2+4U);
l5=si2;
si1+=si2;
si0=si0 == si1;
if(si0){
goto L66;
}
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l1=si0;
if(si0){
goto L68;
}
}
goto L65;
}
L67:;
si0=4340U;
si0=i32_load(i->m0,si0);
l1=si0;
si1=0U;
si2=l1;
si3=l3;
si2=si2 <= si3;
si0=si2?si0:si1;
si0=!(si0);
if(si0){
si0=4340U;
si1=l3;
i32_store(i->m0,si0,si1);
}
L69:;
si0=0U;
l1=si0;
si0=4776U;
si1=l6;
i32_store(i->m0,si0,si1);
si0=4772U;
si1=l3;
i32_store(i->m0,si0,si1);
si0=4356U;
si1=-1U;
i32_store(i->m0,si0,si1);
si0=4360U;
si1=4796U;
si1=i32_load(i->m0,si1);
i32_store(i->m0,si0,si1);
si0=4784U;
si1=0U;
i32_store(i->m0,si0,si1);
L70:;
W2C2_LOOP_START
{
si0=l1;
si1=4384U;
si0+=si1;
si1=l1;
si2=4372U;
si1+=si2;
l2=si1;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l1;
si2=4364U;
si1+=si2;
l0=si1;
i32_store(i->m0,si0,si1);
si0=l1;
si1=4376U;
si0+=si1;
si1=l0;
i32_store(i->m0,si0,si1);
si0=l1;
si1=4392U;
si0+=si1;
si1=l1;
si2=4380U;
si1+=si2;
l0=si1;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l2;
i32_store(i->m0,si0,si1);
si0=l1;
si1=4400U;
si0+=si1;
si1=l1;
si2=4388U;
si1+=si2;
l2=si1;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l0;
i32_store(i->m0,si0,si1);
si0=l1;
si1=4396U;
si0+=si1;
si1=l2;
i32_store(i->m0,si0,si1);
si0=l1;
si1=32U;
si0+=si1;
l1=si0;
si1=256U;
si0=si0 != si1;
if(si0){
goto L70;
}
}
si0=l3;
si1=-8U;
si2=l3;
si1-=si2;
si2=15U;
si1&=si2;
l1=si1;
si0+=si1;
l2=si0;
si1=l6;
si2=56U;
si1-=si2;
l0=si1;
si2=l1;
si1-=si2;
l1=si1;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=4352U;
si1=4812U;
si1=i32_load(i->m0,si1);
i32_store(i->m0,si0,si1);
si0=4336U;
si1=l1;
i32_store(i->m0,si0,si1);
si0=4348U;
si1=l2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l3;
si0+=si1;
si1=56U;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
goto L64;
L66:;
si0=l2;
si1=l3;
si0=si0 >= si1;
if(si0){
goto L65;
}
si0=l0;
si1=l2;
si0=si0 > si1;
if(si0){
goto L65;
}
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+12U);
si1=8U;
si0&=si1;
if(si0){
goto L65;
}
si0=l2;
si1=-8U;
si2=l2;
si1-=si2;
si2=15U;
si1&=si2;
l0=si1;
si0+=si1;
l3=si0;
si1=4336U;
si1=i32_load(i->m0,si1);
si2=l6;
si1+=si2;
l7=si1;
si2=l0;
si1-=si2;
l0=si1;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l1;
si1=l5;
si2=l6;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=4352U;
si1=4812U;
si1=i32_load(i->m0,si1);
i32_store(i->m0,si0,si1);
si0=4336U;
si1=l0;
i32_store(i->m0,si0,si1);
si0=4348U;
si1=l3;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l7;
si0+=si1;
si1=56U;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
goto L64;
L65:;
si0=4340U;
si0=i32_load(i->m0,si0);
si1=l3;
si0=si0 > si1;
if(si0){
si0=4340U;
si1=l3;
i32_store(i->m0,si0,si1);
}
L71:;
si0=l3;
si1=l6;
si0+=si1;
l0=si0;
si0=4772U;
l1=si0;
L73:;
W2C2_LOOP_START
{
si0=l0;
si1=l1;
si1=i32_load(i->m0,si1);
l5=si1;
si0=si0 != si1;
if(si0){
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l1=si0;
if(si0){
goto L73;
}
goto L72;
}
L74:;
}
si0=l1;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+12U);
si1=8U;
si0&=si1;
si0=!(si0);
if(si0){
goto L7;
}
L72:;
si0=4772U;
l1=si0;
L75:;
W2C2_LOOP_START
{
si0=l2;
si1=l1;
si1=i32_load(i->m0,si1);
l0=si1;
si0=si0 >= si1;
if(si0){
si0=l0;
si1=l1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si0+=si1;
l0=si0;
si1=l2;
si0=si0 > si1;
if(si0){
goto L76;
}
}
L77:;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l1=si0;
goto L75;
L76:;
}
si0=l3;
si1=-8U;
si2=l3;
si1-=si2;
si2=15U;
si1&=si2;
l1=si1;
si0+=si1;
l7=si0;
si1=l6;
si2=56U;
si1-=si2;
l5=si1;
si2=l1;
si1-=si2;
l1=si1;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l3;
si1=l5;
si0+=si1;
si1=56U;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l2;
si1=l0;
si2=55U;
si3=l0;
si2-=si3;
si3=15U;
si2&=si3;
si1+=si2;
si2=63U;
si1-=si2;
l5=si1;
si2=l5;
si3=l2;
si4=16U;
si3+=si4;
si2=si2 < si3;
si0=si2?si0:si1;
l5=si0;
si1=35U;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=4352U;
si1=4812U;
si1=i32_load(i->m0,si1);
i32_store(i->m0,si0,si1);
si0=4336U;
si1=l1;
i32_store(i->m0,si0,si1);
si0=4348U;
si1=l7;
i32_store(i->m0,si0,si1);
si0=l5;
si1=16U;
si0+=si1;
si1=4780U;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,si0,sj1);
si0=l5;
si1=4772U;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,(WasmMemoryAddress)si0+8U,sj1);
si0=4780U;
si1=l5;
si2=8U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=4776U;
si1=l6;
i32_store(i->m0,si0,si1);
si0=4772U;
si1=l3;
i32_store(i->m0,si0,si1);
si0=4784U;
si1=0U;
i32_store(i->m0,si0,si1);
si0=l5;
si1=36U;
si0+=si1;
l1=si0;
L78:;
W2C2_LOOP_START
{
si0=l1;
si1=7U;
i32_store(i->m0,si0,si1);
si0=l1;
si1=4U;
si0+=si1;
l1=si0;
si1=l0;
si0=si0 < si1;
if(si0){
goto L78;
}
}
si0=l2;
si1=l5;
si0=si0 == si1;
if(si0){
goto L64;
}
si0=l5;
si1=l5;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=-2U;
si1&=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l5;
si1=l5;
si2=l2;
si1-=si2;
l3=si1;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l3;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l3;
si1=255U;
si0=si0 <= si1;
if(si0){
si0=l3;
si1=-8U;
si0&=si1;
si1=4364U;
si0+=si1;
l1=si0;
si0=4324U;
si0=i32_load(i->m0,si0);
l0=si0;
si1=1U;
si2=l3;
si3=3U;
si2>>=(si3&31);
si1<<=(si2&31);
l3=si1;
si0&=si1;
si0=!(si0);
if(si0){
si0=4324U;
si1=l0;
si2=l3;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=l1;
goto L81;
}
L82:;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
L81:;
l0=si0;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l1;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=12U;
l3=si0;
si0=8U;
goto L79;
}
L80:;
si0=31U;
l1=si0;
si0=l3;
si1=16777215U;
si0=si0 <= si1;
if(si0){
si0=l3;
si1=38U;
si2=l3;
si3=8U;
si2>>=(si3&31);
si2=I32_CLZ(si2);
l1=si2;
si1-=si2;
si0>>=(si1&31);
si1=1U;
si0&=si1;
si1=l1;
si2=1U;
si1<<=(si2&31);
si0-=si1;
si1=62U;
si0+=si1;
l1=si0;
}
L83:;
si0=l2;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+28U,si1);
si0=l2;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+16U,sj1);
si0=l1;
si1=2U;
si0<<=(si1&31);
si1=4628U;
si0+=si1;
l0=si0;
si0=4328U;
si0=i32_load(i->m0,si0);
l5=si0;
si1=1U;
si2=l1;
si1<<=(si2&31);
l6=si1;
si0&=si1;
si0=!(si0);
if(si0){
si0=l0;
si1=l2;
i32_store(i->m0,si0,si1);
si0=4328U;
si1=l5;
si2=l6;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
goto L85;
}
L86:;
si0=l3;
si1=0U;
si2=25U;
si3=l1;
si4=1U;
si3>>=(si4&31);
si2-=si3;
si3=l1;
si4=31U;
si3=si3 == si4;
si1=si3?si1:si2;
si0<<=(si1&31);
l1=si0;
si0=l0;
si0=i32_load(i->m0,si0);
l5=si0;
L87:;
W2C2_LOOP_START
{
si0=l5;
l0=si0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
si1=-8U;
si0&=si1;
si1=l3;
si0=si0 == si1;
if(si0){
goto L84;
}
si0=l1;
si1=29U;
si0>>=(si1&31);
l5=si0;
si0=l1;
si1=1U;
si0<<=(si1&31);
l1=si0;
si0=l0;
si1=l5;
si2=4U;
si1&=si2;
si0+=si1;
si1=16U;
si0+=si1;
l6=si0;
si0=i32_load(i->m0,si0);
l5=si0;
if(si0){
goto L87;
}
}
si0=l6;
si1=l2;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
L85:;
si0=8U;
l3=si0;
si0=l2;
l0=si0;
si0=l2;
l1=si0;
si0=12U;
goto L79;
L84:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l1=si0;
si0=l0;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l1;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l2;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=0U;
l1=si0;
si0=24U;
l3=si0;
si0=12U;
L79:;
si1=l2;
si0+=si1;
si1=l0;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l3;
si0+=si1;
si1=l1;
i32_store(i->m0,si0,si1);
L64:;
si0=4336U;
si0=i32_load(i->m0,si0);
l1=si0;
si1=l4;
si0=si0 <= si1;
if(si0){
goto L8;
}
si0=4348U;
si0=i32_load(i->m0,si0);
l2=si0;
si1=l4;
si0+=si1;
l0=si0;
si1=l1;
si2=l4;
si1-=si2;
l1=si1;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=4336U;
si1=l1;
i32_store(i->m0,si0,si1);
si0=4348U;
si1=l0;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l4;
si2=3U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l2;
si1=8U;
si0+=si1;
l1=si0;
goto L4;
L8:;
si0=0U;
l1=si0;
si0=4820U;
si1=48U;
i32_store(i->m0,si0,si1);
goto L4;
L7:;
si0=l1;
si1=l3;
i32_store(i->m0,si0,si1);
si0=l1;
si1=l1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=l6;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l3;
si1=-8U;
si2=l3;
si1-=si2;
si2=15U;
si1&=si2;
si0+=si1;
l9=si0;
si1=l4;
si2=3U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l5;
si1=-8U;
si2=l5;
si1-=si2;
si2=15U;
si1&=si2;
si0+=si1;
l6=si0;
si1=l4;
si2=l9;
si1+=si2;
l2=si1;
si0-=si1;
l3=si0;
si0=4348U;
si0=i32_load(i->m0,si0);
si1=l6;
si0=si0 == si1;
if(si0){
si0=4348U;
si1=l2;
i32_store(i->m0,si0,si1);
si0=4336U;
si1=4336U;
si1=i32_load(i->m0,si1);
si2=l3;
si1+=si2;
l4=si1;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l4;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
goto L88;
}
L89:;
si0=4344U;
si0=i32_load(i->m0,si0);
si1=l6;
si0=si0 == si1;
if(si0){
si0=4344U;
si1=l2;
i32_store(i->m0,si0,si1);
si0=4332U;
si1=4332U;
si1=i32_load(i->m0,si1);
si2=l3;
si1+=si2;
l4=si1;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l4;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l2;
si1=l4;
si0+=si1;
si1=l4;
i32_store(i->m0,si0,si1);
goto L88;
}
L90:;
si0=l6;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l5=si0;
si1=3U;
si0&=si1;
si1=1U;
si0=si0 == si1;
if(si0){
si0=l5;
si1=-8U;
si0&=si1;
l8=si0;
si0=l6;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+12U);
l4=si0;
si0=l5;
si1=255U;
si0=si0 <= si1;
if(si0){
si0=l6;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l0=si0;
si1=l4;
si0=si0 == si1;
if(si0){
si0=4324U;
si1=4324U;
si1=i32_load(i->m0,si1);
si2=-2U;
si3=l5;
si4=3U;
si3>>=(si4&31);
si2=I32_ROTL(si2,si3);
si1&=si2;
i32_store(i->m0,si0,si1);
goto L92;
}
L94:;
si0=l4;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l0;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
goto L92;
}
L93:;
si0=l6;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+24U);
l7=si0;
si0=l4;
si1=l6;
si0=si0 != si1;
if(si0){
si0=l6;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l5=si0;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l4;
si1=l5;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
goto L95;
}
L96:;
si0=l6;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l5=si0;
if(si0){
si0=l6;
si1=20U;
si0+=si1;
}else{
si0=l6;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l5=si0;
si0=!(si0);
if(si0){
goto L97;
}
si0=l6;
si1=16U;
si0+=si1;
}
L98:;
l0=si0;
L99:;
W2C2_LOOP_START
{
si0=l0;
l1=si0;
si0=l5;
l4=si0;
si1=20U;
si0+=si1;
l0=si0;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l5=si0;
if(si0){
goto L99;
}
si0=l4;
si1=16U;
si0+=si1;
l0=si0;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l5=si0;
if(si0){
goto L99;
}
}
si0=l1;
si1=0U;
i32_store(i->m0,si0,si1);
goto L95;
L97:;
si0=0U;
l4=si0;
L95:;
si0=l7;
si0=!(si0);
if(si0){
goto L92;
}
si0=l6;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+28U);
l0=si0;
si1=2U;
si0<<=(si1&31);
si1=4628U;
si0+=si1;
l5=si0;
si0=i32_load(i->m0,si0);
si1=l6;
si0=si0 == si1;
if(si0){
si0=l5;
si1=l4;
i32_store(i->m0,si0,si1);
si0=l4;
if(si0){
goto L100;
}
si0=4328U;
si1=4328U;
si1=i32_load(i->m0,si1);
si2=-2U;
si3=l0;
si2=I32_ROTL(si2,si3);
si1&=si2;
i32_store(i->m0,si0,si1);
goto L92;
}
L101:;
si0=l7;
si1=16U;
si2=20U;
si3=l7;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+16U);
si4=l6;
si3=si3 == si4;
si1=si3?si1:si2;
si0+=si1;
si1=l4;
i32_store(i->m0,si0,si1);
si0=l4;
si0=!(si0);
if(si0){
goto L92;
}
L100:;
si0=l4;
si1=l7;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l6;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l5=si0;
if(si0){
si0=l4;
si1=l5;
i32_store(i->m0,(WasmMemoryAddress)si0+16U,si1);
si0=l5;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
}
L102:;
si0=l6;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l5=si0;
si0=!(si0);
if(si0){
goto L92;
}
si0=l4;
si1=l5;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=l5;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
L92:;
si0=l6;
si1=l8;
si0+=si1;
l6=si0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l5=si0;
si0=l3;
si1=l8;
si0+=si1;
l3=si0;
}
L91:;
si0=l6;
si1=l5;
si2=-2U;
si1&=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l2;
si1=l3;
si0+=si1;
si1=l3;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l3;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l3;
si1=255U;
si0=si0 <= si1;
if(si0){
si0=l3;
si1=-8U;
si0&=si1;
si1=4364U;
si0+=si1;
l4=si0;
si0=4324U;
si0=i32_load(i->m0,si0);
l5=si0;
si1=1U;
si2=l3;
si3=3U;
si2>>=(si3&31);
si1<<=(si2&31);
l3=si1;
si0&=si1;
si0=!(si0);
if(si0){
si0=4324U;
si1=l3;
si2=l5;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=l4;
goto L104;
}
L105:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
L104:;
l3=si0;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l4;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l2;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l2;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
goto L88;
}
L103:;
si0=31U;
l4=si0;
si0=l3;
si1=16777215U;
si0=si0 <= si1;
if(si0){
si0=l3;
si1=38U;
si2=l3;
si3=8U;
si2>>=(si3&31);
si2=I32_CLZ(si2);
l4=si2;
si1-=si2;
si0>>=(si1&31);
si1=1U;
si0&=si1;
si1=l4;
si2=1U;
si1<<=(si2&31);
si0-=si1;
si1=62U;
si0+=si1;
l4=si0;
}
L106:;
si0=l2;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+28U,si1);
si0=l2;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+16U,sj1);
si0=l4;
si1=2U;
si0<<=(si1&31);
si1=4628U;
si0+=si1;
l5=si0;
si0=4328U;
si0=i32_load(i->m0,si0);
l0=si0;
si1=1U;
si2=l4;
si1<<=(si2&31);
l6=si1;
si0&=si1;
si0=!(si0);
if(si0){
si0=l5;
si1=l2;
i32_store(i->m0,si0,si1);
si0=4328U;
si1=l0;
si2=l6;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l5;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l2;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l2;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
goto L88;
}
L107:;
si0=l3;
si1=0U;
si2=25U;
si3=l4;
si4=1U;
si3>>=(si4&31);
si2-=si3;
si3=l4;
si4=31U;
si3=si3 == si4;
si1=si3?si1:si2;
si0<<=(si1&31);
l4=si0;
si0=l5;
si0=i32_load(i->m0,si0);
l0=si0;
L109:;
W2C2_LOOP_START
{
si0=l0;
l5=si0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
si1=-8U;
si0&=si1;
si1=l3;
si0=si0 == si1;
if(si0){
goto L108;
}
si0=l4;
si1=29U;
si0>>=(si1&31);
l0=si0;
si0=l4;
si1=1U;
si0<<=(si1&31);
l4=si0;
si0=l5;
si1=l0;
si2=4U;
si1&=si2;
si0+=si1;
si1=16U;
si0+=si1;
l6=si0;
si0=i32_load(i->m0,si0);
l0=si0;
if(si0){
goto L109;
}
}
si0=l6;
si1=l2;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l5;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l2;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l2;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
goto L88;
L108:;
si0=l5;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l4=si0;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l5;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l2;
si1=0U;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l2;
si1=l5;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l2;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
L88:;
si0=l9;
si1=8U;
si0+=si1;
l1=si0;
goto L4;
L6:;
si0=l7;
si0=!(si0);
if(si0){
goto L110;
}
si0=l5;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+28U);
l3=si0;
si1=2U;
si0<<=(si1&31);
si1=4628U;
si0+=si1;
l0=si0;
si0=i32_load(i->m0,si0);
si1=l5;
si0=si0 == si1;
if(si0){
si0=l0;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l1;
if(si0){
goto L111;
}
si0=4328U;
si1=l10;
si2=-2U;
si3=l3;
si2=I32_ROTL(si2,si3);
si1&=si2;
l10=si1;
i32_store(i->m0,si0,si1);
goto L110;
}
L112:;
si0=l7;
si1=16U;
si2=20U;
si3=l7;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+16U);
si4=l5;
si3=si3 == si4;
si1=si3?si1:si2;
si0+=si1;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l1;
si0=!(si0);
if(si0){
goto L110;
}
L111:;
si0=l1;
si1=l7;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l5;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l0=si0;
if(si0){
si0=l1;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+16U,si1);
si0=l0;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
}
L113:;
si0=l5;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l0=si0;
si0=!(si0);
if(si0){
goto L110;
}
si0=l1;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=l0;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
L110:;
si0=l2;
si1=15U;
si0=si0 <= si1;
if(si0){
si0=l5;
si1=l2;
si2=l4;
si1|=si2;
l1=si1;
si2=3U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l1;
si1=l5;
si0+=si1;
l1=si0;
si1=l1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
goto L114;
}
L115:;
si0=l4;
si1=l5;
si0+=si1;
l3=si0;
si1=l2;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l5;
si1=l4;
si2=3U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l2;
si1=l3;
si0+=si1;
si1=l2;
i32_store(i->m0,si0,si1);
si0=l2;
si1=255U;
si0=si0 <= si1;
if(si0){
si0=l2;
si1=-8U;
si0&=si1;
si1=4364U;
si0+=si1;
l1=si0;
si0=4324U;
si0=i32_load(i->m0,si0);
l4=si0;
si1=1U;
si2=l2;
si3=3U;
si2>>=(si3&31);
si1<<=(si2&31);
l2=si1;
si0&=si1;
si0=!(si0);
if(si0){
si0=4324U;
si1=l2;
si2=l4;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=l1;
goto L117;
}
L118:;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
L117:;
l2=si0;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l1;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l3;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l3;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
goto L114;
}
L116:;
si0=31U;
l1=si0;
si0=l2;
si1=16777215U;
si0=si0 <= si1;
if(si0){
si0=l2;
si1=38U;
si2=l2;
si3=8U;
si2>>=(si3&31);
si2=I32_CLZ(si2);
l1=si2;
si1-=si2;
si0>>=(si1&31);
si1=1U;
si0&=si1;
si1=l1;
si2=1U;
si1<<=(si2&31);
si0-=si1;
si1=62U;
si0+=si1;
l1=si0;
}
L119:;
si0=l3;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+28U,si1);
si0=l3;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+16U,sj1);
si0=l1;
si1=2U;
si0<<=(si1&31);
si1=4628U;
si0+=si1;
l4=si0;
si0=l10;
si1=1U;
si2=l1;
si1<<=(si2&31);
l0=si1;
si0&=si1;
si0=!(si0);
if(si0){
si0=l4;
si1=l3;
i32_store(i->m0,si0,si1);
si0=4328U;
si1=l0;
si2=l10;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=l3;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l3;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l3;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
goto L114;
}
L120:;
si0=l2;
si1=0U;
si2=25U;
si3=l1;
si4=1U;
si3>>=(si4&31);
si2-=si3;
si3=l1;
si4=31U;
si3=si3 == si4;
si1=si3?si1:si2;
si0<<=(si1&31);
l1=si0;
si0=l4;
si0=i32_load(i->m0,si0);
l0=si0;
L122:;
W2C2_LOOP_START
{
si0=l0;
l4=si0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
si1=-8U;
si0&=si1;
si1=l2;
si0=si0 == si1;
if(si0){
goto L121;
}
si0=l1;
si1=29U;
si0>>=(si1&31);
l0=si0;
si0=l1;
si1=1U;
si0<<=(si1&31);
l1=si0;
si0=l4;
si1=l0;
si2=4U;
si1&=si2;
si0+=si1;
si1=16U;
si0+=si1;
l6=si0;
si0=i32_load(i->m0,si0);
l0=si0;
if(si0){
goto L122;
}
}
si0=l6;
si1=l3;
i32_store(i->m0,si0,si1);
si0=l3;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l3;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l3;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
goto L114;
L121:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l1=si0;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l4;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l3;
si1=0U;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l3;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l3;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
L114:;
si0=l5;
si1=8U;
si0+=si1;
l1=si0;
goto L4;
L5:;
si0=l9;
si0=!(si0);
if(si0){
goto L123;
}
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+28U);
l5=si0;
si1=2U;
si0<<=(si1&31);
si1=4628U;
si0+=si1;
l0=si0;
si0=i32_load(i->m0,si0);
si1=l3;
si0=si0 == si1;
if(si0){
si0=l0;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l1;
if(si0){
goto L124;
}
si0=4328U;
si1=l10;
si2=-2U;
si3=l5;
si2=I32_ROTL(si2,si3);
si1&=si2;
i32_store(i->m0,si0,si1);
goto L123;
}
L125:;
si0=l9;
si1=16U;
si2=20U;
si3=l9;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+16U);
si4=l3;
si3=si3 == si4;
si1=si3?si1:si2;
si0+=si1;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l1;
si0=!(si0);
if(si0){
goto L123;
}
L124:;
si0=l1;
si1=l9;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l0=si0;
if(si0){
si0=l1;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+16U,si1);
si0=l0;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
}
L126:;
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l0=si0;
si0=!(si0);
if(si0){
goto L123;
}
si0=l1;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=l0;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
L123:;
si0=l2;
si1=15U;
si0=si0 <= si1;
if(si0){
si0=l3;
si1=l2;
si2=l4;
si1|=si2;
l1=si1;
si2=3U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l1;
si1=l3;
si0+=si1;
l1=si0;
si1=l1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
goto L127;
}
L128:;
si0=l3;
si1=l4;
si0+=si1;
l0=si0;
si1=l2;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l3;
si1=l4;
si2=3U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l0;
si1=l2;
si0+=si1;
si1=l2;
i32_store(i->m0,si0,si1);
si0=l8;
if(si0){
si0=l8;
si1=-8U;
si0&=si1;
si1=4364U;
si0+=si1;
l4=si0;
si0=4344U;
si0=i32_load(i->m0,si0);
l1=si0;
si0=1U;
si1=l8;
si2=3U;
si1>>=(si2&31);
si0<<=(si1&31);
l5=si0;
si1=l6;
si0&=si1;
si0=!(si0);
if(si0){
si0=4324U;
si1=l5;
si2=l6;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=l4;
goto L130;
}
L131:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
L130:;
l5=si0;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l4;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l1;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l1;
si1=l5;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
}
L129:;
si0=4344U;
si1=l0;
i32_store(i->m0,si0,si1);
si0=4332U;
si1=l2;
i32_store(i->m0,si0,si1);
L127:;
si0=l3;
si1=8U;
si0+=si1;
l1=si0;
L4:;
si0=l11;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l1;
L0:;
return si0;
}

void f30(wasmModuleInstance* instance,U32 l0) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l1=0;
U32 l2=0;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 l6=0;
U32 l7=0;
U32 si0,si1,si2,si3,si4;
U64 sj1;
si0=l0;
si0=!(si0);
if(si0){
goto L1;
}
si0=l0;
si1=8U;
si0-=si1;
l3=si0;
si1=l0;
si2=4U;
si1-=si2;
si1=i32_load(i->m0,si1);
l1=si1;
si2=-8U;
si1&=si2;
l0=si1;
si0+=si1;
l4=si0;
si0=l1;
si1=1U;
si0&=si1;
if(si0){
goto L2;
}
si0=l1;
si1=2U;
si0&=si1;
si0=!(si0);
if(si0){
goto L1;
}
si0=l3;
si1=l3;
si1=i32_load(i->m0,si1);
l2=si1;
si0-=si1;
l3=si0;
si1=4340U;
si1=i32_load(i->m0,si1);
si0=si0 < si1;
if(si0){
goto L1;
}
si0=l0;
si1=l2;
si0+=si1;
l0=si0;
si0=4344U;
si0=i32_load(i->m0,si0);
si1=l3;
si0=si0 != si1;
if(si0){
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+12U);
l1=si0;
si0=l2;
si1=255U;
si0=si0 <= si1;
if(si0){
si0=l1;
si1=l3;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l5=si1;
si0=si0 != si1;
if(si0){
goto L5;
}
si0=4324U;
si1=4324U;
si1=i32_load(i->m0,si1);
si2=-2U;
si3=l2;
si4=3U;
si3>>=(si4&31);
si2=I32_ROTL(si2,si3);
si1&=si2;
i32_store(i->m0,si0,si1);
goto L2;
}
L7:;
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+24U);
l6=si0;
si0=l1;
si1=l3;
si0=si0 != si1;
if(si0){
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l2=si0;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l1;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
goto L3;
}
L8:;
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l2=si0;
if(si0){
si0=l3;
si1=20U;
si0+=si1;
}else{
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l2=si0;
si0=!(si0);
if(si0){
goto L4;
}
si0=l3;
si1=16U;
si0+=si1;
}
L9:;
l5=si0;
L10:;
W2C2_LOOP_START
{
si0=l5;
l7=si0;
si0=l2;
l1=si0;
si1=20U;
si0+=si1;
l5=si0;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l2=si0;
if(si0){
goto L10;
}
si0=l1;
si1=16U;
si0+=si1;
l5=si0;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l2=si0;
if(si0){
goto L10;
}
}
si0=l7;
si1=0U;
i32_store(i->m0,si0,si1);
goto L3;
}
L6:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l1=si0;
si1=3U;
si0&=si1;
si1=3U;
si0=si0 != si1;
if(si0){
goto L2;
}
si0=l4;
si1=l1;
si2=-2U;
si1&=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=4332U;
si1=l0;
i32_store(i->m0,si0,si1);
si0=l4;
si1=l0;
i32_store(i->m0,si0,si1);
si0=l3;
si1=l0;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
goto L0;
L5:;
si0=l1;
si1=l5;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l5;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
goto L2;
L4:;
si0=0U;
l1=si0;
L3:;
si0=l6;
si0=!(si0);
if(si0){
goto L2;
}
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+28U);
l5=si0;
si1=2U;
si0<<=(si1&31);
si1=4628U;
si0+=si1;
l2=si0;
si0=i32_load(i->m0,si0);
si1=l3;
si0=si0 == si1;
if(si0){
si0=l2;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l1;
if(si0){
goto L11;
}
si0=4328U;
si1=4328U;
si1=i32_load(i->m0,si1);
si2=-2U;
si3=l5;
si2=I32_ROTL(si2,si3);
si1&=si2;
i32_store(i->m0,si0,si1);
goto L2;
}
L12:;
si0=l6;
si1=16U;
si2=20U;
si3=l6;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+16U);
si4=l3;
si3=si3 == si4;
si1=si3?si1:si2;
si0+=si1;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l1;
si0=!(si0);
if(si0){
goto L2;
}
L11:;
si0=l1;
si1=l6;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l2=si0;
if(si0){
si0=l1;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+16U,si1);
si0=l2;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
}
L13:;
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l2=si0;
si0=!(si0);
if(si0){
goto L2;
}
si0=l1;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=l2;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
L2:;
si0=l3;
si1=l4;
si0=si0 >= si1;
if(si0){
goto L1;
}
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l2=si0;
si1=1U;
si0&=si1;
si0=!(si0);
if(si0){
goto L1;
}
si0=l2;
si1=2U;
si0&=si1;
si0=!(si0);
if(si0){
si0=4348U;
si0=i32_load(i->m0,si0);
si1=l4;
si0=si0 == si1;
if(si0){
si0=4348U;
si1=l3;
i32_store(i->m0,si0,si1);
si0=4336U;
si1=4336U;
si1=i32_load(i->m0,si1);
si2=l0;
si1+=si2;
l0=si1;
i32_store(i->m0,si0,si1);
si0=l3;
si1=l0;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l3;
si1=4344U;
si1=i32_load(i->m0,si1);
si0=si0 != si1;
if(si0){
goto L1;
}
si0=4332U;
si1=0U;
i32_store(i->m0,si0,si1);
si0=4344U;
si1=0U;
i32_store(i->m0,si0,si1);
goto L0;
}
L19:;
si0=4344U;
si0=i32_load(i->m0,si0);
si1=l4;
si0=si0 == si1;
if(si0){
si0=4344U;
si1=l3;
i32_store(i->m0,si0,si1);
si0=4332U;
si1=4332U;
si1=i32_load(i->m0,si1);
si2=l0;
si1+=si2;
l0=si1;
i32_store(i->m0,si0,si1);
si0=l3;
si1=l0;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l0;
si1=l3;
si0+=si1;
si1=l0;
i32_store(i->m0,si0,si1);
goto L0;
}
L20:;
si0=l2;
si1=-8U;
si0&=si1;
si1=l0;
si0+=si1;
l0=si0;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+12U);
l1=si0;
si0=l2;
si1=255U;
si0=si0 <= si1;
if(si0){
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l5=si0;
si1=l1;
si0=si0 == si1;
if(si0){
si0=4324U;
si1=4324U;
si1=i32_load(i->m0,si1);
si2=-2U;
si3=l2;
si4=3U;
si3>>=(si4&31);
si2=I32_ROTL(si2,si3);
si1&=si2;
i32_store(i->m0,si0,si1);
goto L15;
}
L22:;
si0=l1;
si1=l5;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l5;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
goto L15;
}
L21:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+24U);
l6=si0;
si0=l1;
si1=l4;
si0=si0 != si1;
if(si0){
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l2=si0;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l1;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
goto L16;
}
L23:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l2=si0;
if(si0){
si0=l4;
si1=20U;
si0+=si1;
}else{
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l2=si0;
si0=!(si0);
if(si0){
goto L17;
}
si0=l4;
si1=16U;
si0+=si1;
}
L24:;
l5=si0;
L25:;
W2C2_LOOP_START
{
si0=l5;
l7=si0;
si0=l2;
l1=si0;
si1=20U;
si0+=si1;
l5=si0;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l2=si0;
if(si0){
goto L25;
}
si0=l1;
si1=16U;
si0+=si1;
l5=si0;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l2=si0;
if(si0){
goto L25;
}
}
si0=l7;
si1=0U;
i32_store(i->m0,si0,si1);
goto L16;
}
L18:;
si0=l4;
si1=l2;
si2=-2U;
si1&=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l0;
si1=l3;
si0+=si1;
si1=l0;
i32_store(i->m0,si0,si1);
si0=l3;
si1=l0;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
goto L14;
L17:;
si0=0U;
l1=si0;
L16:;
si0=l6;
si0=!(si0);
if(si0){
goto L15;
}
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+28U);
l5=si0;
si1=2U;
si0<<=(si1&31);
si1=4628U;
si0+=si1;
l2=si0;
si0=i32_load(i->m0,si0);
si1=l4;
si0=si0 == si1;
if(si0){
si0=l2;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l1;
if(si0){
goto L26;
}
si0=4328U;
si1=4328U;
si1=i32_load(i->m0,si1);
si2=-2U;
si3=l5;
si2=I32_ROTL(si2,si3);
si1&=si2;
i32_store(i->m0,si0,si1);
goto L15;
}
L27:;
si0=l6;
si1=16U;
si2=20U;
si3=l6;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+16U);
si4=l4;
si3=si3 == si4;
si1=si3?si1:si2;
si0+=si1;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l1;
si0=!(si0);
if(si0){
goto L15;
}
L26:;
si0=l1;
si1=l6;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l2=si0;
if(si0){
si0=l1;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+16U,si1);
si0=l2;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
}
L28:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l2=si0;
si0=!(si0);
if(si0){
goto L15;
}
si0=l1;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=l2;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
L15:;
si0=l0;
si1=l3;
si0+=si1;
si1=l0;
i32_store(i->m0,si0,si1);
si0=l3;
si1=l0;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l3;
si1=4344U;
si1=i32_load(i->m0,si1);
si0=si0 != si1;
if(si0){
goto L14;
}
si0=4332U;
si1=l0;
i32_store(i->m0,si0,si1);
goto L0;
L14:;
si0=l0;
si1=255U;
si0=si0 <= si1;
if(si0){
si0=l0;
si1=-8U;
si0&=si1;
si1=4364U;
si0+=si1;
l1=si0;
si0=4324U;
si0=i32_load(i->m0,si0);
l2=si0;
si1=1U;
si2=l0;
si3=3U;
si2>>=(si3&31);
si1<<=(si2&31);
l0=si1;
si0&=si1;
si0=!(si0);
if(si0){
si0=4324U;
si1=l0;
si2=l2;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=l1;
goto L30;
}
L31:;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
L30:;
l0=si0;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l1;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l3;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l3;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
goto L0;
}
L29:;
si0=31U;
l1=si0;
si0=l0;
si1=16777215U;
si0=si0 <= si1;
if(si0){
si0=l0;
si1=38U;
si2=l0;
si3=8U;
si2>>=(si3&31);
si2=I32_CLZ(si2);
l1=si2;
si1-=si2;
si0>>=(si1&31);
si1=1U;
si0&=si1;
si1=l1;
si2=1U;
si1<<=(si2&31);
si0-=si1;
si1=62U;
si0+=si1;
l1=si0;
}
L32:;
si0=l3;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+28U,si1);
si0=l3;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+16U,sj1);
si0=l1;
si1=2U;
si0<<=(si1&31);
si1=4628U;
si0+=si1;
l4=si0;
si0=4328U;
si0=i32_load(i->m0,si0);
l2=si0;
si1=1U;
si2=l1;
si1<<=(si2&31);
l5=si1;
si0&=si1;
si0=!(si0);
if(si0){
si0=4328U;
si1=l2;
si2=l5;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=24U;
l1=si0;
si0=l4;
l5=si0;
si0=8U;
goto L35;
}
L36:;
si0=l0;
si1=0U;
si2=25U;
si3=l1;
si4=1U;
si3>>=(si4&31);
si2-=si3;
si3=l1;
si4=31U;
si3=si3 == si4;
si1=si3?si1:si2;
si0<<=(si1&31);
l1=si0;
si0=l4;
si0=i32_load(i->m0,si0);
l5=si0;
L37:;
W2C2_LOOP_START
{
si0=l5;
l2=si0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
si1=-8U;
si0&=si1;
si1=l0;
si0=si0 == si1;
if(si0){
goto L34;
}
si0=l1;
si1=29U;
si0>>=(si1&31);
l5=si0;
si0=l1;
si1=1U;
si0<<=(si1&31);
l1=si0;
si0=l2;
si1=l5;
si2=4U;
si1&=si2;
si0+=si1;
si1=16U;
si0+=si1;
l4=si0;
si0=i32_load(i->m0,si0);
l5=si0;
if(si0){
goto L37;
}
}
si0=24U;
l1=si0;
si0=l2;
l5=si0;
si0=8U;
L35:;
l0=si0;
si0=l3;
l2=si0;
si0=l3;
goto L33;
L34:;
si0=l2;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l5=si0;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=8U;
l1=si0;
si0=l2;
si1=8U;
si0+=si1;
l4=si0;
si0=24U;
l0=si0;
si0=0U;
L33:;
l7=si0;
si0=l4;
si1=l3;
i32_store(i->m0,si0,si1);
si0=l1;
si1=l3;
si0+=si1;
si1=l5;
i32_store(i->m0,si0,si1);
si0=l3;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l0;
si1=l3;
si0+=si1;
si1=l7;
i32_store(i->m0,si0,si1);
si0=4356U;
si1=4356U;
si1=i32_load(i->m0,si1);
si2=1U;
si1-=si2;
l3=si1;
si2=-1U;
si3=l3;
si1=si3?si1:si2;
i32_store(i->m0,si0,si1);
L1:;
L0:;
}

U32 f31(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l2=0;
U64 l3=0;
U32 si0,si1,si2;
U64 sj0,sj1,sj2,sj3;
si0=0U;
si1=l0;
si1=!(si1);
if(si1){
goto L2;
}
si0=l0;
sj0=(U64)(si0);
si1=l1;
sj1=(U64)(si1);
sj0*=sj1;
l3=sj0;
si0=(U32)(sj0);
l2=si0;
si1=l0;
si2=l1;
si1|=si2;
si2=65536U;
si1=si1 < si2;
if(si1){
goto L2;
}
si0=-1U;
si1=l2;
sj2=l3;
sj3=W2C2_LL(32U);
sj2>>=(sj3&63);
si2=(U32)(sj2);
si0=si2?si0:si1;
L2:;
l2=si0;
si0=f29(instance,si0);
l0=si0;
si0=!(si0);
if(si0){
goto L1;
}
si0=l0;
si1=4U;
si0-=si1;
si0=i32_load8_u(i->m0,si0);
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L1;
}
si0=l0;
si1=0U;
si2=l2;
si0=f79(instance,si0,si1,si2);
L1:;
si0=l0;
L0:;
return si0;
}

U32 f32(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l2=0;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 l6=0;
U32 l7=0;
U32 l8=0;
U32 l9=0;
U32 l10=0;
U32 l11=0;
U32 l12=0;
U32 si0,si1,si2,si3,si4,si5;
si0=l0;
si0=!(si0);
if(si0){
si0=l1;
si0=f29(instance,si0);
goto L0;
}
L1:;
si0=l1;
si1=-64U;
si0=si0 >= si1;
if(si0){
si0=4820U;
si1=48U;
i32_store(i->m0,si0,si1);
si0=0U;
goto L0;
}
L2:;
si0=16U;
si1=l1;
si2=19U;
si1+=si2;
si2=-16U;
si1&=si2;
si2=l1;
si3=11U;
si2=si2 < si3;
si0=si2?si0:si1;
l3=si0;
si0=l0;
si1=4U;
si0-=si1;
l7=si0;
si0=i32_load(i->m0,si0);
l8=si0;
si1=-8U;
si0&=si1;
l2=si0;
si0=l8;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l3;
si1=256U;
si0=si0 < si1;
if(si0){
goto L4;
}
si0=l2;
si1=l3;
si2=4U;
si1|=si2;
si0=si0 < si1;
if(si0){
goto L4;
}
si0=l2;
si1=l3;
si0-=si1;
si1=4804U;
si1=i32_load(i->m0,si1);
si2=1U;
si1<<=(si2&31);
si0=si0 <= si1;
if(si0){
goto L3;
}
goto L4;
}
L5:;
si0=l0;
si1=8U;
si0-=si1;
l6=si0;
si1=l2;
si0+=si1;
l4=si0;
si0=l2;
si1=l3;
si0=si0 >= si1;
if(si0){
si0=l2;
si1=l3;
si0-=si1;
l1=si0;
si1=16U;
si0=si0 < si1;
if(si0){
goto L3;
}
si0=l7;
si1=l3;
si2=l8;
si3=1U;
si2&=si3;
si1|=si2;
si2=2U;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=l3;
si1=l6;
si0+=si1;
l3=si0;
si1=l1;
si2=3U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l4;
si1=l4;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l3;
si1=l1;
f33(instance,si0,si1);
si0=l0;
goto L0;
}
L6:;
si0=4348U;
si0=i32_load(i->m0,si0);
si1=l4;
si0=si0 == si1;
if(si0){
si0=4336U;
si0=i32_load(i->m0,si0);
si1=l2;
si0+=si1;
l2=si0;
si1=l3;
si0=si0 <= si1;
if(si0){
goto L4;
}
si0=l7;
si1=l3;
si2=l8;
si3=1U;
si2&=si3;
si1|=si2;
si2=2U;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=4348U;
si1=l3;
si2=l6;
si1+=si2;
l1=si1;
i32_store(i->m0,si0,si1);
si0=4336U;
si1=l2;
si2=l3;
si1-=si2;
l3=si1;
i32_store(i->m0,si0,si1);
si0=l1;
si1=l3;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l0;
goto L0;
}
L7:;
si0=4344U;
si0=i32_load(i->m0,si0);
si1=l4;
si0=si0 == si1;
if(si0){
si0=4332U;
si0=i32_load(i->m0,si0);
si1=l2;
si0+=si1;
l2=si0;
si1=l3;
si0=si0 < si1;
if(si0){
goto L4;
}
si0=l2;
si1=l3;
si0-=si1;
l1=si0;
si1=16U;
si0=si0 >= si1;
if(si0){
si0=l7;
si1=l3;
si2=l8;
si3=1U;
si2&=si3;
si1|=si2;
si2=2U;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=l3;
si1=l6;
si0+=si1;
l3=si0;
si1=l1;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l2;
si1=l6;
si0+=si1;
l2=si0;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l2;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=-2U;
si1&=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
goto L9;
}
L10:;
si0=l7;
si1=l8;
si2=1U;
si1&=si2;
si2=l2;
si1|=si2;
si2=2U;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l6;
si0+=si1;
l1=si0;
si1=l1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=0U;
l1=si0;
si0=0U;
l3=si0;
L9:;
si0=4344U;
si1=l3;
i32_store(i->m0,si0,si1);
si0=4332U;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l0;
goto L0;
}
L8:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l5=si0;
si1=2U;
si0&=si1;
if(si0){
goto L4;
}
si0=l5;
si1=-8U;
si0&=si1;
si1=l2;
si0+=si1;
l9=si0;
si1=l3;
si0=si0 < si1;
if(si0){
goto L4;
}
si0=l9;
si1=l3;
si0-=si1;
l11=si0;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+12U);
l1=si0;
si0=l5;
si1=255U;
si0=si0 <= si1;
if(si0){
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l2=si0;
si1=l1;
si0=si0 == si1;
if(si0){
si0=4324U;
si1=4324U;
si1=i32_load(i->m0,si1);
si2=-2U;
si3=l5;
si4=3U;
si3>>=(si4&31);
si2=I32_ROTL(si2,si3);
si1&=si2;
i32_store(i->m0,si0,si1);
goto L11;
}
L13:;
si0=l1;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l2;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
goto L11;
}
L12:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+24U);
l10=si0;
si0=l1;
si1=l4;
si0=si0 != si1;
if(si0){
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l2=si0;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l1;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
goto L14;
}
L15:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l2=si0;
if(si0){
si0=l4;
si1=20U;
si0+=si1;
}else{
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l2=si0;
si0=!(si0);
if(si0){
goto L16;
}
si0=l4;
si1=16U;
si0+=si1;
}
L17:;
l5=si0;
L18:;
W2C2_LOOP_START
{
si0=l5;
l12=si0;
si0=l2;
l1=si0;
si1=20U;
si0+=si1;
l5=si0;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l2=si0;
if(si0){
goto L18;
}
si0=l1;
si1=16U;
si0+=si1;
l5=si0;
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l2=si0;
if(si0){
goto L18;
}
}
si0=l12;
si1=0U;
i32_store(i->m0,si0,si1);
goto L14;
L16:;
si0=0U;
l1=si0;
L14:;
si0=l10;
si0=!(si0);
if(si0){
goto L11;
}
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+28U);
l5=si0;
si1=2U;
si0<<=(si1&31);
si1=4628U;
si0+=si1;
l2=si0;
si0=i32_load(i->m0,si0);
si1=l4;
si0=si0 == si1;
if(si0){
si0=l2;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l1;
if(si0){
goto L19;
}
si0=4328U;
si1=4328U;
si1=i32_load(i->m0,si1);
si2=-2U;
si3=l5;
si2=I32_ROTL(si2,si3);
si1&=si2;
i32_store(i->m0,si0,si1);
goto L11;
}
L20:;
si0=l10;
si1=16U;
si2=20U;
si3=l10;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+16U);
si4=l4;
si3=si3 == si4;
si1=si3?si1:si2;
si0+=si1;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l1;
si0=!(si0);
if(si0){
goto L11;
}
L19:;
si0=l1;
si1=l10;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l2=si0;
if(si0){
si0=l1;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+16U,si1);
si0=l2;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
}
L21:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l2=si0;
si0=!(si0);
if(si0){
goto L11;
}
si0=l1;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=l2;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
L11:;
si0=l11;
si1=15U;
si0=si0 <= si1;
if(si0){
si0=l7;
si1=l8;
si2=1U;
si1&=si2;
si2=l9;
si1|=si2;
si2=2U;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=l6;
si1=l9;
si0+=si1;
l1=si0;
si1=l1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l0;
goto L0;
}
L22:;
si0=l7;
si1=l3;
si2=l8;
si3=1U;
si2&=si3;
si1|=si2;
si2=2U;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=l3;
si1=l6;
si0+=si1;
l1=si0;
si1=l11;
si2=3U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l6;
si1=l9;
si0+=si1;
l3=si0;
si1=l3;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l1;
si1=l11;
f33(instance,si0,si1);
si0=l0;
goto L0;
L4:;
si0=l1;
si0=f29(instance,si0);
l3=si0;
si0=!(si0);
if(si0){
si0=0U;
goto L0;
}
L23:;
si0=l3;
si1=l0;
si2=-4U;
si3=-8U;
si4=l7;
si4=i32_load(i->m0,si4);
l2=si4;
si5=3U;
si4&=si5;
si2=si4?si2:si3;
si3=l2;
si4=-8U;
si3&=si4;
si2+=si3;
l2=si2;
si3=l1;
si4=l1;
si5=l2;
si4=si4 > si5;
si2=si4?si2:si3;
si0=f78(instance,si0,si1,si2);
l1=si0;
si0=l0;
f30(instance,si0);
si0=l1;
l0=si0;
L3:;
si0=l0;
L0:;
return si0;
}

void f33(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l2=0;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 l6=0;
U32 l7=0;
U32 si0,si1,si2,si3,si4;
U64 sj1;
si0=l0;
si1=l1;
si0+=si1;
l4=si0;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l2=si0;
si1=1U;
si0&=si1;
if(si0){
goto L2;
}
si0=l2;
si1=2U;
si0&=si1;
si0=!(si0);
if(si0){
goto L1;
}
si0=l0;
si0=i32_load(i->m0,si0);
l3=si0;
si1=l1;
si0+=si1;
l1=si0;
si0=l0;
si1=l3;
si0-=si1;
l0=si0;
si1=4344U;
si1=i32_load(i->m0,si1);
si0=si0 != si1;
if(si0){
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+12U);
l2=si0;
si0=l3;
si1=255U;
si0=si0 <= si1;
if(si0){
si0=l2;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l5=si1;
si0=si0 != si1;
if(si0){
goto L5;
}
si0=4324U;
si1=4324U;
si1=i32_load(i->m0,si1);
si2=-2U;
si3=l3;
si4=3U;
si3>>=(si4&31);
si2=I32_ROTL(si2,si3);
si1&=si2;
i32_store(i->m0,si0,si1);
goto L2;
}
L7:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+24U);
l6=si0;
si0=l0;
si1=l2;
si0=si0 != si1;
if(si0){
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l3=si0;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l2;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
goto L3;
}
L8:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l3=si0;
if(si0){
si0=l0;
si1=20U;
si0+=si1;
}else{
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l3=si0;
si0=!(si0);
if(si0){
goto L4;
}
si0=l0;
si1=16U;
si0+=si1;
}
L9:;
l5=si0;
L10:;
W2C2_LOOP_START
{
si0=l5;
l7=si0;
si0=l3;
l2=si0;
si1=20U;
si0+=si1;
l5=si0;
si0=l2;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l3=si0;
if(si0){
goto L10;
}
si0=l2;
si1=16U;
si0+=si1;
l5=si0;
si0=l2;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l3=si0;
if(si0){
goto L10;
}
}
si0=l7;
si1=0U;
i32_store(i->m0,si0,si1);
goto L3;
}
L6:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l2=si0;
si1=3U;
si0&=si1;
si1=3U;
si0=si0 != si1;
if(si0){
goto L2;
}
si0=l4;
si1=l2;
si2=-2U;
si1&=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=4332U;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l4;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
goto L0;
L5:;
si0=l2;
si1=l5;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l5;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
goto L2;
L4:;
si0=0U;
l2=si0;
L3:;
si0=l6;
si0=!(si0);
if(si0){
goto L2;
}
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+28U);
l5=si0;
si1=2U;
si0<<=(si1&31);
si1=4628U;
si0+=si1;
l3=si0;
si0=i32_load(i->m0,si0);
si1=l0;
si0=si0 == si1;
if(si0){
si0=l3;
si1=l2;
i32_store(i->m0,si0,si1);
si0=l2;
if(si0){
goto L11;
}
si0=4328U;
si1=4328U;
si1=i32_load(i->m0,si1);
si2=-2U;
si3=l5;
si2=I32_ROTL(si2,si3);
si1&=si2;
i32_store(i->m0,si0,si1);
goto L2;
}
L12:;
si0=l6;
si1=16U;
si2=20U;
si3=l6;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+16U);
si4=l0;
si3=si3 == si4;
si1=si3?si1:si2;
si0+=si1;
si1=l2;
i32_store(i->m0,si0,si1);
si0=l2;
si0=!(si0);
if(si0){
goto L2;
}
L11:;
si0=l2;
si1=l6;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l3=si0;
if(si0){
si0=l2;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+16U,si1);
si0=l3;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
}
L13:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l3=si0;
si0=!(si0);
if(si0){
goto L2;
}
si0=l2;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=l3;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
L2:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l3=si0;
si1=2U;
si0&=si1;
si0=!(si0);
if(si0){
si0=4348U;
si0=i32_load(i->m0,si0);
si1=l4;
si0=si0 == si1;
if(si0){
si0=4348U;
si1=l0;
i32_store(i->m0,si0,si1);
si0=4336U;
si1=4336U;
si1=i32_load(i->m0,si1);
si2=l1;
si1+=si2;
l1=si1;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l0;
si1=4344U;
si1=i32_load(i->m0,si1);
si0=si0 != si1;
if(si0){
goto L1;
}
si0=4332U;
si1=0U;
i32_store(i->m0,si0,si1);
si0=4344U;
si1=0U;
i32_store(i->m0,si0,si1);
goto L0;
}
L19:;
si0=4344U;
si0=i32_load(i->m0,si0);
si1=l4;
si0=si0 == si1;
if(si0){
si0=4344U;
si1=l0;
i32_store(i->m0,si0,si1);
si0=4332U;
si1=4332U;
si1=i32_load(i->m0,si1);
si2=l1;
si1+=si2;
l1=si1;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l0;
si1=l1;
si0+=si1;
si1=l1;
i32_store(i->m0,si0,si1);
goto L0;
}
L20:;
si0=l3;
si1=-8U;
si0&=si1;
si1=l1;
si0+=si1;
l1=si0;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+12U);
l2=si0;
si0=l3;
si1=255U;
si0=si0 <= si1;
if(si0){
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l5=si0;
si1=l2;
si0=si0 == si1;
if(si0){
si0=4324U;
si1=4324U;
si1=i32_load(i->m0,si1);
si2=-2U;
si3=l3;
si4=3U;
si3>>=(si4&31);
si2=I32_ROTL(si2,si3);
si1&=si2;
i32_store(i->m0,si0,si1);
goto L15;
}
L22:;
si0=l2;
si1=l5;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l5;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
goto L15;
}
L21:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+24U);
l6=si0;
si0=l2;
si1=l4;
si0=si0 != si1;
if(si0){
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l3=si0;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l2;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
goto L16;
}
L23:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l3=si0;
if(si0){
si0=l4;
si1=20U;
si0+=si1;
}else{
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l3=si0;
si0=!(si0);
if(si0){
goto L17;
}
si0=l4;
si1=16U;
si0+=si1;
}
L24:;
l5=si0;
L25:;
W2C2_LOOP_START
{
si0=l5;
l7=si0;
si0=l3;
l2=si0;
si1=20U;
si0+=si1;
l5=si0;
si0=l2;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l3=si0;
if(si0){
goto L25;
}
si0=l2;
si1=16U;
si0+=si1;
l5=si0;
si0=l2;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l3=si0;
if(si0){
goto L25;
}
}
si0=l7;
si1=0U;
i32_store(i->m0,si0,si1);
goto L16;
}
L18:;
si0=l4;
si1=l3;
si2=-2U;
si1&=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l0;
si1=l1;
si0+=si1;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
goto L14;
L17:;
si0=0U;
l2=si0;
L16:;
si0=l6;
si0=!(si0);
if(si0){
goto L15;
}
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+28U);
l5=si0;
si1=2U;
si0<<=(si1&31);
si1=4628U;
si0+=si1;
l3=si0;
si0=i32_load(i->m0,si0);
si1=l4;
si0=si0 == si1;
if(si0){
si0=l3;
si1=l2;
i32_store(i->m0,si0,si1);
si0=l2;
if(si0){
goto L26;
}
si0=4328U;
si1=4328U;
si1=i32_load(i->m0,si1);
si2=-2U;
si3=l5;
si2=I32_ROTL(si2,si3);
si1&=si2;
i32_store(i->m0,si0,si1);
goto L15;
}
L27:;
si0=l6;
si1=16U;
si2=20U;
si3=l6;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+16U);
si4=l4;
si3=si3 == si4;
si1=si3?si1:si2;
si0+=si1;
si1=l2;
i32_store(i->m0,si0,si1);
si0=l2;
si0=!(si0);
if(si0){
goto L15;
}
L26:;
si0=l2;
si1=l6;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l3=si0;
if(si0){
si0=l2;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+16U,si1);
si0=l3;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
}
L28:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l3=si0;
si0=!(si0);
if(si0){
goto L15;
}
si0=l2;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=l3;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
L15:;
si0=l0;
si1=l1;
si0+=si1;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
si2=1U;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l0;
si1=4344U;
si1=i32_load(i->m0,si1);
si0=si0 != si1;
if(si0){
goto L14;
}
si0=4332U;
si1=l1;
i32_store(i->m0,si0,si1);
goto L0;
L14:;
si0=l1;
si1=255U;
si0=si0 <= si1;
if(si0){
si0=l1;
si1=-8U;
si0&=si1;
si1=4364U;
si0+=si1;
l2=si0;
si0=4324U;
si0=i32_load(i->m0,si0);
l3=si0;
si1=1U;
si2=l1;
si3=3U;
si2>>=(si3&31);
si1<<=(si2&31);
l1=si1;
si0&=si1;
si0=!(si0);
if(si0){
si0=4324U;
si1=l1;
si2=l3;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=l2;
goto L30;
}
L31:;
si0=l2;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
L30:;
l1=si0;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l2;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l0;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l0;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
goto L0;
}
L29:;
si0=31U;
l2=si0;
si0=l1;
si1=16777215U;
si0=si0 <= si1;
if(si0){
si0=l1;
si1=38U;
si2=l1;
si3=8U;
si2>>=(si3&31);
si2=I32_CLZ(si2);
l2=si2;
si1-=si2;
si0>>=(si1&31);
si1=1U;
si0&=si1;
si1=l2;
si2=1U;
si1<<=(si2&31);
si0-=si1;
si1=62U;
si0+=si1;
l2=si0;
}
L32:;
si0=l0;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+28U,si1);
si0=l0;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+16U,sj1);
si0=l2;
si1=2U;
si0<<=(si1&31);
si1=4628U;
si0+=si1;
l3=si0;
si0=4328U;
si0=i32_load(i->m0,si0);
l5=si0;
si1=1U;
si2=l2;
si1<<=(si2&31);
l4=si1;
si0&=si1;
si0=!(si0);
if(si0){
si0=l3;
si1=l0;
i32_store(i->m0,si0,si1);
si0=4328U;
si1=l4;
si2=l5;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l0;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l0;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
goto L0;
}
L33:;
si0=l1;
si1=0U;
si2=25U;
si3=l2;
si4=1U;
si3>>=(si4&31);
si2-=si3;
si3=l2;
si4=31U;
si3=si3 == si4;
si1=si3?si1:si2;
si0<<=(si1&31);
l2=si0;
si0=l3;
si0=i32_load(i->m0,si0);
l5=si0;
L35:;
W2C2_LOOP_START
{
si0=l5;
l3=si0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
si1=-8U;
si0&=si1;
si1=l1;
si0=si0 == si1;
if(si0){
goto L34;
}
si0=l2;
si1=29U;
si0>>=(si1&31);
l5=si0;
si0=l2;
si1=1U;
si0<<=(si1&31);
l2=si0;
si0=l3;
si1=l5;
si2=4U;
si1&=si2;
si0+=si1;
si1=16U;
si0+=si1;
l4=si0;
si0=i32_load(i->m0,si0);
l5=si0;
if(si0){
goto L35;
}
}
si0=l4;
si1=l0;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l0;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l0;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
goto L0;
L34:;
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l1=si0;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l3;
si1=l0;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l0;
si1=0U;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l0;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l0;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
L1:;
L0:;
}

void f34(wasmModuleInstance* instance,U32 l0) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l1=0;
U32 l2=0;
U64 l3=0;
U64 l4=0;
U32 si0,si1,si2,si3;
U64 sj1,sj2,sj3;
si0=i->g0;
si1=16U;
si0-=si1;
l1=si0;
i->g0=si0;
si0=1664U;
si0=i32_load(i->m0,si0);
sj1=W2C2_LL(1U);
si2=l1;
si3=8U;
si2+=si3;
si0=f37(instance,si0,sj1,si2);
l2=si0;
if(si0){
si0=4820U;
si1=l2;
i32_store(i->m0,si0,si1);
goto L1;
}
L2:;
si0=l0;
si1=l1;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+8U);
l3=sj1;
sj2=W2C2_LL(1000000000U);
sj1=DIV_U(sj1,sj2);
l4=sj1;
i64_store(i->m0,si0,sj1);
si0=l0;
sj1=l3;
sj2=l4;
sj3=W2C2_LL(1000000000U);
sj2*=sj3;
sj1-=sj2;
i64_store32(i->m0,(WasmMemoryAddress)si0+8U,sj1);
L1:;
si0=l1;
si1=16U;
si0+=si1;
i->g0=si0;
L0:;
}

void f35(wasmModuleInstance* instance,U32 l0) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 si0;
si0=l0;
f42(instance,si0);
UNREACHABLE;
L0:;
}

U32 f36(wasmModuleInstance* instance,U32 l0) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 si0,si1;
f46(instance);
si0=l0;
si0=i22_wasiX5FsnapshotX5Fpreview18_fdX5Fclose(instance,si0);
si1=65535U;
si0&=si1;
l0=si0;
si0=!(si0);
if(si0){
si0=0U;
goto L0;
}
L1:;
si0=4820U;
si1=l0;
i32_store(i->m0,si0,si1);
si0=-1U;
L0:;
return si0;
}

U32 f37(wasmModuleInstance* instance,U32 l0,U64 l1,U32 l2) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 si0,si1,si2;
U64 sj1;
si0=l0;
sj1=l1;
si2=l2;
si0=i22_wasiX5FsnapshotX5Fpreview114_clockX5FtimeX5Fget(instance,si0,sj1,si2);
si1=65535U;
si0&=si1;
L0:;
return si0;
}

U32 f38(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 si0,si1;
si0=l0;
si1=l1;
si0=i22_wasiX5FsnapshotX5Fpreview113_fdX5FfdstatX5Fget(instance,si0,si1);
si1=65535U;
si0&=si1;
L0:;
return si0;
}

U32 f39(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2,U32 l3) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 si0,si1,si2,si3;
si0=l0;
si1=l1;
si2=l2;
si3=l3;
si0=i22_wasiX5FsnapshotX5Fpreview17_fdX5Fread(instance,si0,si1,si2,si3);
si1=65535U;
si0&=si1;
L0:;
return si0;
}

U32 f40(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2,U64 l3,U32 l4) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 si0,si1,si2,si4;
U64 sj3;
si0=l0;
si1=l1;
si2=l2;
sj3=l3;
si4=l4;
si0=i22_wasiX5FsnapshotX5Fpreview110_fdX5Freaddir(instance,si0,si1,si2,sj3,si4);
si1=65535U;
si0&=si1;
L0:;
return si0;
}

U32 f41(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2,U32 l3) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 si0,si1,si2,si3,si4;
si0=l0;
si1=l1;
si2=l2;
si3=l2;
si3=f82(instance,si3);
si4=l3;
si0=i22_wasiX5FsnapshotX5Fpreview117_pathX5FfilestatX5Fget(instance,si0,si1,si2,si3,si4);
si1=65535U;
si0&=si1;
L0:;
return si0;
}

void f42(wasmModuleInstance* instance,U32 l0) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 si0;
si0=l0;
i22_wasiX5FsnapshotX5Fpreview19_procX5Fexit(instance,si0);
UNREACHABLE;
L0:;
}

U32 f43(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l3=0;
U32 l4=0;
U64 l5=0;
U64 l6=0;
U32 si0,si1,si2,si3,si4,si5,si7,si8,si9;
U64 sj0,sj1,sj5,sj6;
si0=i->g0;
si1=32U;
si0-=si1;
l4=si0;
i->g0=si0;
si0=l2;
si1=503316480U;
si0&=si1;
si1=33554432U;
si0-=si1;
si1=25U;
si0>>=(si1&31);
l3=si0;
si1=9U;
si0=si0 <= si1;
if(si0){
si0=1U;
si1=l3;
si0<<=(si1&31);
l3=si0;
si1=642U;
si0&=si1;
if(si0){
goto L3;
}
sj0=W2C2_LL(-4211012U);
si1=l3;
si2=9U;
si1&=si2;
if(si1){
goto L2;
}
}
L4:;
si0=4820U;
si1=28U;
i32_store(i->m0,si0,si1);
si0=-1U;
l3=si0;
goto L1;
L3:;
sj0=W2C2_LL(-4194626U);
sj1=W2C2_LL(-4211012U);
si2=l2;
si3=67108864U;
si2&=si3;
sj0=si2?sj0:sj1;
l5=sj0;
sj1=W2C2_LL(4194625U);
sj0|=sj1;
sj1=l5;
si2=l2;
si3=268435456U;
si2&=si3;
sj0=si2?sj0:sj1;
L2:;
l5=sj0;
si0=l0;
si1=l4;
si2=8U;
si1+=si2;
si0=f38(instance,si0,si1);
l3=si0;
if(si0){
si0=4820U;
si1=l3;
i32_store(i->m0,si0,si1);
si0=-1U;
l3=si0;
goto L1;
}
L5:;
si0=-1U;
l3=si0;
sj0=l5;
si1=l4;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+24U);
l5=sj1;
sj0&=sj1;
l6=sj0;
si0=l0;
si1=l2;
si2=-1U;
si1^=si2;
si2=24U;
si1>>=(si2&31);
si2=1U;
si1&=si2;
si2=l1;
si3=l1;
si3=f82(instance,si3);
si4=l2;
si5=12U;
si4>>=(si5&31);
si5=4095U;
si4&=si5;
sj5=l6;
sj6=l5;
si7=l2;
si8=4095U;
si7&=si8;
si8=l4;
si9=4U;
si8+=si9;
si0=i22_wasiX5FsnapshotX5Fpreview19_pathX5Fopen(instance,si0,si1,si2,si3,si4,sj5,sj6,si7,si8);
si1=65535U;
si0&=si1;
l2=si0;
if(si0){
si0=4820U;
si1=l2;
i32_store(i->m0,si0,si1);
goto L1;
}
L6:;
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l3=si0;
L1:;
si0=l4;
si1=32U;
si0+=si1;
i->g0=si0;
si0=l3;
L0:;
return si0;
}

U32 f44(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2,U32 l3) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l4=0;
U32 l5=0;
U64 l6=0;
U64 l7=0;
U64 l8=0;
U64 l9=0;
U64 l10=0;
U64 l11=0;
U32 si0,si1,si2,si3;
U64 sj1,sj2,sj3;
si0=i->g0;
si1=96U;
si0-=si1;
l4=si0;
i->g0=si0;
si0=-1U;
l5=si0;
si0=l0;
si1=l3;
si2=-1U;
si1^=si2;
si2=1U;
si1&=si2;
si2=l1;
si3=l4;
si0=f41(instance,si0,si1,si2,si3);
l3=si0;
if(si0){
si0=4820U;
si1=l3;
i32_store(i->m0,si0,si1);
goto L1;
}
L2:;
si0=l2;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+24U,sj1);
si0=l2;
si1=l4;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+24U);
i64_store(i->m0,(WasmMemoryAddress)si0+16U,sj1);
si0=l2;
si1=l4;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+8U);
i64_store(i->m0,(WasmMemoryAddress)si0+8U,sj1);
si0=l2;
si1=l4;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,si0,sj1);
si0=l2;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+64U,sj1);
si0=0U;
l5=si0;
si0=l2;
si1=0U;
i32_store(i->m0,(WasmMemoryAddress)si0+56U,si1);
si0=l2;
si1=l4;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+32U);
i64_store(i->m0,(WasmMemoryAddress)si0+48U,sj1);
si0=l4;
si1=80U;
si0+=si1;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=l4;
si1=88U;
si0+=si1;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=l2;
si1=32U;
si0+=si1;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=l2;
si1=40U;
si0+=si1;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=l2;
si1=l4;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+56U);
l6=sj1;
sj2=W2C2_LL(1000000000U);
sj1=DIV_U(sj1,sj2);
l7=sj1;
i64_store(i->m0,(WasmMemoryAddress)si0+104U,sj1);
si0=l2;
si1=l4;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+48U);
l8=sj1;
sj2=W2C2_LL(1000000000U);
sj1=DIV_U(sj1,sj2);
l9=sj1;
i64_store(i->m0,(WasmMemoryAddress)si0+88U,sj1);
si0=l2;
si1=l4;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+40U);
l10=sj1;
sj2=W2C2_LL(1000000000U);
sj1=DIV_U(sj1,sj2);
l11=sj1;
i64_store(i->m0,(WasmMemoryAddress)si0+72U,sj1);
si0=l2;
sj1=l6;
sj2=l7;
sj3=W2C2_LL(1000000000U);
sj2*=sj3;
sj1-=sj2;
i64_store32(i->m0,(WasmMemoryAddress)si0+112U,sj1);
si0=l2;
sj1=l8;
sj2=l9;
sj3=W2C2_LL(1000000000U);
sj2*=sj3;
sj1-=sj2;
i64_store32(i->m0,(WasmMemoryAddress)si0+96U,sj1);
si0=l2;
sj1=l10;
sj2=l11;
sj3=W2C2_LL(1000000000U);
sj2*=sj3;
sj1-=sj2;
i64_store32(i->m0,(WasmMemoryAddress)si0+80U,sj1);
si0=l4;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+72U,sj1);
si0=l2;
si1=140U;
si0+=si1;
si1=l4;
si2=92U;
si1+=si2;
si1=i32_load(i->m0,si1);
i32_store(i->m0,si0,si1);
si0=l2;
si1=132U;
si0+=si1;
si1=l4;
si2=84U;
si1+=si2;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,si0,sj1);
si0=l2;
si1=124U;
si0+=si1;
si1=l4;
si2=76U;
si1+=si2;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,si0,sj1);
si0=l2;
si1=l4;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+68U);
i64_store(i->m0,(WasmMemoryAddress)si0+116U,sj1);
si0=l4;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+16U);
si1=1U;
si0-=si1;
si1=255U;
si0&=si1;
l3=si0;
si1=6U;
si0=si0 > si1;
if(si0){
goto L1;
}
si0=l2;
si1=24U;
si0+=si1;
si1=l3;
si2=2U;
si1<<=(si2&31);
si2=1720U;
si1+=si2;
si1=i32_load(i->m0,si1);
i32_store(i->m0,si0,si1);
L1:;
si0=l4;
si1=96U;
si0+=si1;
i->g0=si0;
si0=l5;
L0:;
return si0;
}

U32 f45(wasmModuleInstance* instance,U32 l0) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l1=0;
U32 l2=0;
U32 l3=0;
U32 si0,si1,si2,si3,si4;
si0=i->g0;
si1=16U;
si0-=si1;
l2=si0;
i->g0=si0;
si0=-1U;
l1=si0;
si0=l0;
si1=l2;
si2=12U;
si1+=si2;
si2=4824U;
si3=4828U;
si3=i32_load(i->m0,si3);
si0=f47(instance,si0,si1,si2,si3);
l0=si0;
si1=-1U;
si0=si0 == si1;
if(si0){
si0=4820U;
si1=44U;
i32_store(i->m0,si0,si1);
goto L1;
}
L2:;
si0=4824U;
si0=i32_load(i->m0,si0);
l1=si0;
si0=i->g0;
si1=96U;
si0-=si1;
l3=si0;
i->g0=si0;
si0=l0;
si1=1U;
si2=l1;
si3=l3;
si4=32U;
si3+=si4;
si0=f41(instance,si0,si1,si2,si3);
l1=si0;
if(si0){
si0=4820U;
si1=l1;
i32_store(i->m0,si0,si1);
goto L4;
}
L5:;
si0=0U;
goto L3;
L4:;
si0=-1U;
L3:;
l1=si0;
si0=l3;
si1=96U;
si0+=si1;
i->g0=si0;
L1:;
si0=l2;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l1;
L0:;
return si0;
}

void f46(wasmModuleInstance* instance) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l0=0;
U32 l1=0;
U32 l2=0;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 l6=0;
U32 l7=0;
U32 si0,si1,si2,si3,si4;
si0=i->g0;
si1=16U;
si0-=si1;
l5=si0;
i->g0=si0;
si0=4848U;
si0=i32_load8_u(i->m0,si0);
si1=1U;
si0&=si1;
if(si0){
goto L1;
}
si0=4848U;
si0=i32_load8_u(i->m0,si0);
si1=1U;
si0&=si1;
if(si0){
goto L1;
}
si0=3U;
l2=si0;
L3:;
W2C2_LOOP_START
{
si0=l2;
si1=l5;
si2=8U;
si1+=si2;
si0=i22_wasiX5FsnapshotX5Fpreview114_fdX5FprestatX5Fget(instance,si0,si1);
si1=65535U;
si0&=si1;
l3=si0;
if(si0){
si0=l3;
si1=8U;
si0=si0 != si1;
if(si0){
goto L4;
}
si0=4848U;
si1=1U;
i32_store8(i->m0,si0,si1);
goto L1;
}
L5:;
si0=l5;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+8U);
si0=!(si0);
if(si0){
si0=l5;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+12U);
l0=si0;
si1=1U;
si0+=si1;
si0=f29(instance,si0);
l3=si0;
si0=!(si0);
if(si0){
goto L2;
}
si0=l2;
si1=l3;
si2=l0;
si0=i22_wasiX5FsnapshotX5Fpreview119_fdX5FprestatX5FdirX5Fname(instance,si0,si1,si2);
si1=65535U;
si0&=si1;
if(si0){
goto L4;
}
si0=l3;
si1=l5;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+12U);
si0+=si1;
si1=0U;
i32_store8(i->m0,si0,si1);
si0=l3;
l0=si0;
si0=l2;
si1=2U;
si0+=si1;
switch(si0){
case 0:
goto L8;
case 1:
goto L8;
default:
goto L9;
}
L9:;
si0=l0;
si0=!(si0);
if(si0){
goto L8;
}
si0=4840U;
si0=i32_load(i->m0,si0);
l6=si0;
si1=4852U;
si1=i32_load(i->m0,si1);
si0=si0 == si1;
if(si0){
si0=4844U;
si0=i32_load(i->m0,si0);
l1=si0;
si0=-1U;
si1=8U;
si2=l6;
si3=1U;
si2<<=(si3&31);
si3=4U;
si4=l6;
si2=si4?si2:si3;
l7=si2;
si1=f31(instance,si1,si2);
l4=si1;
si1=!(si1);
if(si1){
goto L7;
}
si0=l4;
si1=l1;
si2=l6;
si3=3U;
si2<<=(si3&31);
si0=f78(instance,si0,si1,si2);
l4=si0;
si0=4852U;
si1=l7;
i32_store(i->m0,si0,si1);
si0=4844U;
si1=l4;
i32_store(i->m0,si0,si1);
si0=l1;
f30(instance,si0);
}
L10:;
L11:;
W2C2_LOOP_START
{
si0=l0;
l1=si0;
si0=i32_load8_u(i->m0,si0);
si1=46U;
si0-=si1;
switch(si0){
case 0:
goto L13;
case 1:
goto L14;
default:
goto L12;
}
L14:;
si0=l1;
si1=1U;
si0+=si1;
l0=si0;
goto L11;
L13:;
si0=l1;
si1=1U;
si0+=si1;
l0=si0;
si0=l1;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+1U);
l7=si0;
si0=!(si0);
if(si0){
goto L11;
}
si0=l7;
si1=47U;
si0=si0 != si1;
if(si0){
goto L12;
}
si0=l1;
si1=2U;
si0+=si1;
l0=si0;
goto L11;
L12:;
}
si0=l1;
si0=f82(instance,si0);
si1=1U;
si0+=si1;
l0=si0;
si0=f29(instance,si0);
l4=si0;
if(si0){
si0=l4;
si1=l1;
si2=l0;
si0=f78(instance,si0,si1,si2);
}
L15:;
si0=-1U;
si1=l4;
l1=si1;
si1=!(si1);
if(si1){
goto L7;
}
si0=4840U;
si1=l6;
si2=1U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=4844U;
si0=i32_load(i->m0,si0);
si1=l6;
si2=3U;
si1<<=(si2&31);
si0+=si1;
l0=si0;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l0;
si1=l1;
i32_store(i->m0,si0,si1);
si0=0U;
goto L7;
L8:;
UNREACHABLE;
L7:;
if(si0){
goto L2;
}
si0=l3;
f30(instance,si0);
}
L6:;
si0=l2;
si1=1U;
si0+=si1;
l2=si0;
goto L3;
L4:;
}
si0=71U;
f35(instance,si0);
UNREACHABLE;
L2:;
si0=70U;
f35(instance,si0);
UNREACHABLE;
L1:;
si0=l5;
si1=16U;
si0+=si1;
i->g0=si0;
L0:;
}

U32 f47(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2,U32 l3) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l4=0;
U32 l5=0;
U32 l6=0;
U32 l7=0;
U32 l8=0;
U32 l9=0;
U32 l10=0;
U32 l11=0;
U32 l12=0;
U32 si0,si1,si2,si3;
si0=i->g0;
si1=16U;
si0-=si1;
l9=si0;
i->g0=si0;
si0=l9;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=0U;
l3=si0;
si0=l0;
si1=1U;
si0-=si1;
l4=si0;
f46(instance);
L2:;
W2C2_LOOP_START
{
si0=l4;
si1=1U;
si0+=si1;
l4=si0;
si0=i32_load8_u(i->m0,si0);
si1=47U;
si0=si0 == si1;
if(si0){
goto L2;
}
}
si0=4840U;
si0=i32_load(i->m0,si0);
l6=si0;
if(si0){
si0=4844U;
si0=i32_load(i->m0,si0);
l10=si0;
si0=-1U;
l7=si0;
L5:;
W2C2_LOOP_START
{
si0=l7;
si1=-1U;
si0=si0 != si1;
si1=l10;
si2=l6;
si3=3U;
si2<<=(si3&31);
si1+=si2;
l11=si1;
si2=8U;
si1-=si2;
si1=i32_load(i->m0,si1);
l8=si1;
si1=f82(instance,si1);
l0=si1;
si2=l3;
si1=si1 <= si2;
si0&=si1;
if(si0){
goto L6;
}
si0=l0;
si0=!(si0);
si1=l4;
si1=i32_load8_u(i->m0,si1);
l5=si1;
si2=47U;
si1=si1 != si2;
si0&=si1;
if(si0){
goto L7;
}
si0=l4;
si1=l8;
si2=l0;
si0=f77(instance,si0,si1,si2);
if(si0){
goto L6;
}
si0=l0;
if(si0){
si0=l8;
si1=1U;
si0-=si1;
l12=si0;
si0=l0;
l5=si0;
L10:;
W2C2_LOOP_START
{
si0=l5;
si1=l12;
si0+=si1;
si0=i32_load8_u(i->m0,si0);
si1=47U;
si0=si0 != si1;
if(si0){
goto L9;
}
si0=l5;
si1=1U;
si0-=si1;
l5=si0;
if(si0){
goto L10;
}
}
si0=0U;
l5=si0;
L9:;
si0=l4;
si1=l5;
si0+=si1;
si0=i32_load8_u(i->m0,si0);
}else{
si0=l5;
}
L8:;
si1=255U;
si0&=si1;
l5=si0;
si1=47U;
si0=si0 == si1;
if(si0){
goto L7;
}
si0=l5;
if(si0){
goto L6;
}
L7:;
si0=l1;
si1=l8;
i32_store(i->m0,si0,si1);
si0=l11;
si1=4U;
si0-=si1;
si0=i32_load(i->m0,si0);
l7=si0;
si0=l0;
l3=si0;
L6:;
si0=l6;
si1=1U;
si0-=si1;
l6=si0;
if(si0){
goto L5;
}
}
si0=l7;
si1=-1U;
si0=si0 != si1;
if(si0){
goto L3;
}
}
L4:;
si0=4820U;
si1=44U;
i32_store(i->m0,si0,si1);
si0=-1U;
goto L1;
L3:;
si0=l3;
si1=l4;
si0+=si1;
si1=1U;
si0-=si1;
l4=si0;
L11:;
W2C2_LOOP_START
{
si0=l4;
si1=1U;
si0+=si1;
l4=si0;
si0=i32_load8_u(i->m0,si0);
l6=si0;
si1=47U;
si0=si0 == si1;
if(si0){
goto L11;
}
}
si0=l2;
si1=l4;
si2=1259U;
si3=l6;
si1=si3?si1:si2;
i32_store(i->m0,si0,si1);
si0=l7;
L1:;
l3=si0;
si0=l9;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l3;
L0:;
return si0;
}

U32 f48(wasmModuleInstance* instance,U32 l0) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 si0,si1;
si0=l0;
si0=!(si0);
if(si0){
si0=wasmMemorySize(i->m0);
si1=16U;
si0<<=(si1&31);
goto L0;
}
L1:;
si0=l0;
si1=65535U;
si0&=si1;
if(si0){
goto L2;
}
si0=l0;
si1=0U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
goto L2;
}
si0=l0;
si1=16U;
si0>>=(si1&31);
si0=wasmMemoryGrow(i->m0,si0);
l0=si0;
si1=-1U;
si0=si0 == si1;
if(si0){
si0=4820U;
si1=48U;
i32_store(i->m0,si0,si1);
si0=-1U;
goto L0;
}
L3:;
si0=l0;
si1=16U;
si0<<=(si1&31);
goto L0;
L2:;
UNREACHABLE;
L0:;
return si0;
}

void f49(wasmModuleInstance* instance,U32 l0) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l1=0;
U32 l2=0;
U32 si0,si1;
si0=l0;
si0=f50(instance,si0);
si0=l0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+12U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si1,&m5_guestFuncType2);
si0=((U32 (*)(wasmModuleInstance*,U32))entry.func)(entry.instance,si0);
}
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=1U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+52U);
l1=si0;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+48U);
l2=si0;
if(si0){
si0=l2;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+52U,si1);
}
L2:;
si0=l1;
if(si0){
si0=l1;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+48U,si1);
}
L3:;
si0=l0;
si1=4860U;
si1=i32_load(i->m0,si1);
si0=si0 == si1;
if(si0){
si0=4860U;
si1=l1;
i32_store(i->m0,si0,si1);
}
L4:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+80U);
f30(instance,si0);
si0=l0;
f30(instance,si0);
}
L1:;
L0:;
}

U32 f50(wasmModuleInstance* instance,U32 l0) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l1=0;
U32 l2=0;
U32 si0,si1,si2,si3,si4;
U64 sj0,sj1,sj2;
si0=l0;
si0=!(si0);
if(si0){
si0=4312U;
si0=i32_load(i->m0,si0);
if(si0){
si0=4312U;
si0=i32_load(i->m0,si0);
si0=f50(instance,si0);
l1=si0;
}
L2:;
si0=4072U;
si0=i32_load(i->m0,si0);
if(si0){
si0=4072U;
si0=i32_load(i->m0,si0);
si0=f50(instance,si0);
si1=l1;
si0|=si1;
l1=si0;
}
L3:;
si0=4860U;
si0=i32_load(i->m0,si0);
l0=si0;
if(si0){
L5:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+24U);
si0=si0 != si1;
if(si0){
si0=l0;
si1=0U;
si2=0U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+32U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
si0=l1;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+20U);
if(si1){
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
l2=si1;
si2=l0;
si2=i32_load(i->m0,(WasmMemoryAddress)si2+8U);
l1=si2;
si1=si1 != si2;
if(si1){
si1=l0;
si2=l2;
si3=l1;
si2-=si3;
sj2=(U64)(I64)(I32)(si2);
si3=1U;
si4=l0;
si4=i32_load(i->m0,(WasmMemoryAddress)si4+36U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si4,&m5_guestFuncType5);
sj1=((U64 (*)(wasmModuleInstance*,U32,U64,U32))entry.func)(entry.instance,si1,sj2,si3);
}
}
L8:;
si1=l0;
si2=0U;
i32_store(i->m0,(WasmMemoryAddress)si1+24U,si2);
si1=l0;
sj2=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si1+16U,sj2);
si1=l0;
sj2=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si1+4U,sj2);
si1=0U;
}else{
si1=-1U;
}
L7:;
si0|=si1;
l1=si0;
}
L6:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+52U);
l0=si0;
if(si0){
goto L5;
}
}
}
L4:;
si0=l1;
goto L0;
}
L1:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+24U);
si0=si0 == si1;
if(si0){
goto L9;
}
si0=l0;
si1=0U;
si2=0U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+32U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
if(si0){
goto L9;
}
si0=-1U;
goto L0;
L9:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l1=si0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l2=si1;
si0=si0 != si1;
if(si0){
si0=l0;
si1=l1;
si2=l2;
si1-=si2;
sj1=(U64)(I64)(I32)(si1);
si2=1U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+36U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType5);
sj0=((U64 (*)(wasmModuleInstance*,U32,U64,U32))entry.func)(entry.instance,si0,sj1,si2);
}
}
L10:;
si0=l0;
si1=0U;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l0;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+16U,sj1);
si0=l0;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+4U,sj1);
si0=0U;
L0:;
return si0;
}

void f51(wasmModuleInstance* instance) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l0=0;
U32 l1=0;
U32 l2=0;
U32 si0,si1,si2,si3;
U64 sj0,sj1;
si0=4860U;
si0=i32_load(i->m0,si0);
l0=si0;
if(si0){
L2:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+24U);
si0=si0 != si1;
if(si0){
si0=l0;
si1=0U;
si2=0U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+32U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
}
L3:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l1=si0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l2=si1;
si0=si0 != si1;
if(si0){
si0=l0;
si1=l1;
si2=l2;
si1-=si2;
sj1=(U64)(I64)(I32)(si1);
si2=1U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+36U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType5);
sj0=((U64 (*)(wasmModuleInstance*,U32,U64,U32))entry.func)(entry.instance,si0,sj1,si2);
}
}
L4:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+52U);
l0=si0;
if(si0){
goto L2;
}
}
}
L1:;
si0=4192U;
si0=i32_load(i->m0,si0);
l0=si0;
si0=!(si0);
if(si0){
goto L5;
}
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+24U);
si0=si0 != si1;
if(si0){
si0=l0;
si1=0U;
si2=0U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+32U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
}
L6:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l1=si0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l2=si1;
si0=si0 == si1;
if(si0){
goto L5;
}
si0=l0;
si1=l1;
si2=l2;
si1-=si2;
sj1=(U64)(I64)(I32)(si1);
si2=1U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+36U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType5);
sj0=((U64 (*)(wasmModuleInstance*,U32,U64,U32))entry.func)(entry.instance,si0,sj1,si2);
}
L5:;
si0=4312U;
si0=i32_load(i->m0,si0);
l0=si0;
si0=!(si0);
if(si0){
goto L7;
}
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+24U);
si0=si0 != si1;
if(si0){
si0=l0;
si1=0U;
si2=0U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+32U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
}
L8:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l1=si0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l2=si1;
si0=si0 == si1;
if(si0){
goto L7;
}
si0=l0;
si1=l1;
si2=l2;
si1-=si2;
sj1=(U64)(I64)(I32)(si1);
si2=1U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+36U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType5);
sj0=((U64 (*)(wasmModuleInstance*,U32,U64,U32))entry.func)(entry.instance,si0,sj1,si2);
}
L7:;
si0=4072U;
si0=i32_load(i->m0,si0);
l0=si0;
si0=!(si0);
if(si0){
goto L9;
}
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+24U);
si0=si0 != si1;
if(si0){
si0=l0;
si1=0U;
si2=0U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+32U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
}
L10:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l1=si0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l2=si1;
si0=si0 == si1;
if(si0){
goto L9;
}
si0=l0;
si1=l1;
si2=l2;
si1-=si2;
sj1=(U64)(I64)(I32)(si1);
si2=1U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+36U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType5);
sj0=((U64 (*)(wasmModuleInstance*,U32,U64,U32))entry.func)(entry.instance,si0,sj1,si2);
}
L9:;
L0:;
}

U32 f52(wasmModuleInstance* instance,U32 l0) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l1=0;
U32 l2=0;
U32 si0,si1,si2,si3;
U64 sj1;
si0=l0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+60U);
l1=si1;
si2=1U;
si1-=si2;
si2=l1;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+60U,si1);
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+24U);
si0=si0 != si1;
if(si0){
si0=l0;
si1=0U;
si2=0U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+32U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
}
L1:;
si0=l0;
si1=0U;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l0;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+16U,sj1);
si0=l0;
si0=i32_load(i->m0,si0);
l1=si0;
si1=4U;
si0&=si1;
if(si0){
si0=l0;
si1=l1;
si2=32U;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=-1U;
goto L0;
}
L2:;
si0=l0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+40U);
si2=l0;
si2=i32_load(i->m0,(WasmMemoryAddress)si2+44U);
si1+=si2;
l2=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l0;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l1;
si1=27U;
si0<<=(si1&31);
si1=31U;
si0=(U32)((I32)si0>>(si1&31));
L0:;
return si0;
}

U32 f53(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l2=0;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 l6=0;
U32 si0,si1,si2,si3,si4,si5;
si0=255U;
l4=si0;
si0=l0;
l5=si0;
L2:;
W2C2_LOOP_START
{
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l2=si0;
si1=l1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l3=si1;
si0=si0 == si1;
if(si0){
goto L5;
}
si0=l5;
si1=l2;
si2=l2;
si3=10U;
si4=l3;
si5=l2;
si4-=si5;
l3=si4;
si2=f76(instance,si2,si3,si4);
l6=si2;
si3=l2;
si2-=si3;
si3=1U;
si2+=si3;
si3=l3;
si4=l6;
si2=si4?si2:si3;
l3=si2;
si3=l4;
si4=l3;
si5=l4;
si4=si4 < si5;
si2=si4?si2:si3;
l3=si2;
si0=f78(instance,si0,si1,si2);
l5=si0;
si0=l1;
si1=l1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=l3;
si1+=si2;
l2=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l3;
si1=l5;
si0+=si1;
l5=si0;
si0=l6;
if(si0){
goto L3;
}
si0=l4;
si1=l3;
si0-=si1;
l4=si0;
si0=!(si0);
if(si0){
goto L3;
}
si0=l2;
si1=l1;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
si0=si0 == si1;
if(si0){
goto L5;
}
si0=l1;
si1=l2;
si2=1U;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l2;
si0=i32_load8_u(i->m0,si0);
l2=si0;
goto L4;
L5:;
si0=i->g0;
si1=16U;
si0-=si1;
l2=si0;
i->g0=si0;
si0=-1U;
l3=si0;
si0=l1;
si0=f52(instance,si0);
if(si0){
goto L6;
}
si0=l1;
si1=l2;
si2=15U;
si1+=si2;
si2=1U;
si3=l1;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+28U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
si1=1U;
si0=si0 != si1;
if(si0){
goto L6;
}
si0=l2;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+15U);
l3=si0;
L6:;
si0=l2;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l3;
l2=si0;
si1=0U;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L4;
}
si0=0U;
l4=si0;
si0=l0;
si1=l5;
si0=si0 == si1;
if(si0){
goto L1;
}
si0=l1;
si0=i32_load8_u(i->m0,si0);
si1=16U;
si0&=si1;
si0=!(si0);
if(si0){
goto L1;
}
goto L3;
L4:;
si0=l5;
si1=l2;
i32_store8(i->m0,si0,si1);
si0=l5;
si1=1U;
si0+=si1;
l5=si0;
si0=l2;
si1=255U;
si0&=si1;
si1=10U;
si0=si0 == si1;
if(si0){
goto L3;
}
si0=l4;
si1=1U;
si0-=si1;
l4=si0;
if(si0){
goto L2;
}
L3:;
}
si0=l0;
si0=!(si0);
if(si0){
si0=0U;
goto L0;
}
L7:;
si0=l5;
si1=0U;
i32_store8(i->m0,si0,si1);
si0=l0;
l4=si0;
L1:;
si0=l4;
L0:;
return si0;
}

U32 f54(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l3=0;
U64 l4=0;
U64 l5=0;
U32 si0,si1,si2;
U64 sj0,sj1;
si0=i->g0;
si1=32U;
si0-=si1;
l3=si0;
i->g0=si0;
si0=l1;
si1=1U;
si0-=si1;
switch(si0){
case 0:
goto L1;
case 1:
goto L6;
case 2:
goto L5;
case 3:
goto L4;
default:
goto L3;
}
L6:;
si0=0U;
l1=si0;
goto L1;
L5:;
si0=l0;
si1=l3;
si2=8U;
si1+=si2;
si0=f38(instance,si0,si1);
l1=si0;
if(si0){
si0=4820U;
si1=l1;
i32_store(i->m0,si0,si1);
goto L2;
}
L7:;
si0=l3;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+16U);
l5=sj0;
sj1=W2C2_LL(64U);
sj0&=sj1;
l4=sj0;
si0=l3;
si0=i32_load16_u(i->m0,(WasmMemoryAddress)si0+10U);
l1=si0;
sj0=l5;
sj1=W2C2_LL(16386U);
sj0&=sj1;
si0=!(sj0);
si0=!(si0);
if(si0){
sj0=l4;
si0=!(sj0);
si0=!(si0);
if(si0){
si0=l1;
si1=335544320U;
si0|=si1;
l1=si0;
goto L1;
}
L9:;
si0=l1;
si1=67108864U;
si0|=si1;
l1=si0;
goto L1;
}
L8:;
sj0=l4;
si0=!(sj0);
si0=!(si0);
if(si0){
si0=l1;
si1=268435456U;
si0|=si1;
l1=si0;
goto L1;
}
L10:;
si0=l1;
si1=134217728U;
si0|=si1;
l1=si0;
goto L1;
L4:;
si0=l3;
si1=l2;
si2=4U;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l0;
si1=l2;
si1=i32_load(i->m0,si1);
si2=4095U;
si1&=si2;
si0=i22_wasiX5FsnapshotX5Fpreview119_fdX5FfdstatX5FsetX5Fflags(instance,si0,si1);
si1=65535U;
si0&=si1;
l1=si0;
si0=!(si0);
if(si0){
si0=0U;
l1=si0;
goto L1;
}
L11:;
si0=4820U;
si1=l1;
i32_store(i->m0,si0,si1);
goto L2;
L3:;
si0=4820U;
si1=28U;
i32_store(i->m0,si0,si1);
L2:;
si0=-1U;
l1=si0;
L1:;
si0=l3;
si1=32U;
si0+=si1;
i->g0=si0;
si0=l1;
L0:;
return si0;
}

U32 f55(wasmModuleInstance* instance,U32 l0) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l1=0;
U32 si0,si1,si2;
si0=i->g0;
si1=32U;
si0-=si1;
l1=si0;
i->g0=si0;
si0=l0;
si1=l1;
si2=8U;
si1+=si2;
si0=f38(instance,si0,si1);
l0=si0;
if(si0){
goto L2;
}
si0=59U;
l0=si0;
si0=l1;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+8U);
si1=2U;
si0=si0 != si1;
if(si0){
goto L2;
}
si0=l1;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+16U);
si1=36U;
si0&=si1;
if(si0){
goto L2;
}
si0=1U;
goto L1;
L2:;
si0=4820U;
si1=l0;
i32_store(i->m0,si0,si1);
si0=0U;
L1:;
l0=si0;
si0=l1;
si1=32U;
si0+=si1;
i->g0=si0;
si0=l0;
L0:;
return si0;
}

U64 f56(wasmModuleInstance* instance,U32 l0,U64 l1,U32 l2) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l3=0;
U32 si0,si1,si2,si3,si4;
U64 sj0,sj1;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
l3=si0;
si0=i->g0;
si1=16U;
si0-=si1;
l0=si0;
i->g0=si0;
si0=l3;
sj1=l1;
si2=l2;
si3=255U;
si2&=si3;
si3=l0;
si4=8U;
si3+=si4;
si0=i22_wasiX5FsnapshotX5Fpreview17_fdX5Fseek(instance,si0,sj1,si2,si3);
si1=65535U;
si0&=si1;
l2=si0;
if(si0){
si0=4820U;
si1=70U;
si2=l2;
si3=l2;
si4=76U;
si3=si3 == si4;
si1=si3?si1:si2;
i32_store(i->m0,si0,si1);
sj0=W2C2_LL(-1U);
goto L1;
}
L2:;
si0=l0;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+8U);
L1:;
l1=sj0;
si0=l0;
si1=16U;
si0+=si1;
i->g0=si0;
sj0=l1;
L0:;
return sj0;
}

U32 f57(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l3=0;
U32 l4=0;
U32 si0,si1,si2,si3,si4;
si0=i->g0;
si1=16U;
si0-=si1;
l3=si0;
i->g0=si0;
si0=-1U;
l4=si0;
si0=l2;
si1=0U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
si0=4820U;
si1=28U;
i32_store(i->m0,si0,si1);
goto L1;
}
L2:;
si0=l0;
si1=l1;
si2=l2;
si3=l3;
si4=12U;
si3+=si4;
si0=i22_wasiX5FsnapshotX5Fpreview18_fdX5Fwrite(instance,si0,si1,si2,si3);
si1=65535U;
si0&=si1;
l2=si0;
if(si0){
si0=4820U;
si1=l2;
i32_store(i->m0,si0,si1);
goto L1;
}
L3:;
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+12U);
l4=si0;
L1:;
si0=l3;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l4;
L0:;
return si0;
}

U32 f58(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 l6=0;
U32 l7=0;
U32 l8=0;
U32 l9=0;
U32 si0,si1,si2,si3,si4;
U64 sj1;
si0=i->g0;
si1=16U;
si0-=si1;
l4=si0;
i->g0=si0;
si0=l4;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l4;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l4;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+24U);
l1=si1;
i32_store(i->m0,si0,si1);
si0=l4;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+20U);
si2=l1;
si1-=si2;
l3=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=2U;
l5=si0;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
si1=l4;
si2=2U;
si0=f57(instance,si0,si1,si2);
l1=si0;
si1=l2;
si2=l3;
si1+=si2;
l8=si1;
si0=si0 != si1;
if(si0){
si0=l4;
l3=si0;
L3:;
W2C2_LOOP_START
{
si0=l1;
si1=0U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
si0=l0;
si1=0U;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l0;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+16U,sj1);
si0=l0;
si1=l0;
si1=i32_load(i->m0,si1);
si2=32U;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=0U;
si1=l5;
si2=2U;
si1=si1 == si2;
if(si1){
goto L1;
}
si0=l2;
si1=l3;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si0-=si1;
goto L1;
}
L4:;
si0=l3;
si1=l1;
si2=l3;
si2=i32_load(i->m0,(WasmMemoryAddress)si2+4U);
l6=si2;
si1=si1 > si2;
l7=si1;
si2=3U;
si1<<=(si2&31);
si0+=si1;
l9=si0;
si1=l1;
si2=l6;
si3=0U;
si4=l7;
si2=si4?si2:si3;
si1-=si2;
l6=si1;
si2=l9;
si2=i32_load(i->m0,si2);
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l3;
si1=12U;
si2=4U;
si3=l7;
si1=si3?si1:si2;
si0+=si1;
l3=si0;
si1=l3;
si1=i32_load(i->m0,si1);
si2=l6;
si1-=si2;
i32_store(i->m0,si0,si1);
si0=l8;
si1=l1;
si0-=si1;
l8=si0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+56U);
si2=l9;
l3=si2;
si3=l5;
si4=l7;
si3-=si4;
l5=si3;
si1=f57(instance,si1,si2,si3);
l1=si1;
si0=si0 != si1;
if(si0){
goto L3;
}
}
}
L2:;
si0=l0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+40U);
l1=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l0;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=l0;
si1=l1;
si2=l0;
si2=i32_load(i->m0,(WasmMemoryAddress)si2+44U);
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+16U,si1);
si0=l2;
L1:;
l1=si0;
si0=l4;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l1;
L0:;
return si0;
}

U32 f59(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 l6=0;
U32 l7=0;
U32 si0,si1,si2,si3,si4;
si0=i->g0;
si1=16U;
si0-=si1;
l5=si0;
i->g0=si0;
si0=l5;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l5;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+44U);
l4=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l5;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+40U);
l6=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l5;
si1=l2;
si2=l4;
si3=0U;
si2=si2 != si3;
si1-=si2;
l3=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
l7=si0;
si0=l3;
if(si0){
si0=i->g0;
si1=16U;
si0-=si1;
l6=si0;
i->g0=si0;
si0=-1U;
l3=si0;
si0=l7;
si1=l5;
si2=2U;
si3=l6;
si4=12U;
si3+=si4;
si0=f39(instance,si0,si1,si2,si3);
l4=si0;
if(si0){
si0=4820U;
si1=l4;
i32_store(i->m0,si0,si1);
goto L3;
}
L4:;
si0=l6;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+12U);
l3=si0;
L3:;
si0=l6;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l3;
goto L1;
}
L2:;
si0=i->g0;
si1=16U;
si0-=si1;
l3=si0;
i->g0=si0;
si0=l3;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l3;
si1=l6;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l7;
si1=l3;
si2=8U;
si1+=si2;
si2=1U;
si3=l3;
si4=4U;
si3+=si4;
si0=f39(instance,si0,si1,si2,si3);
l4=si0;
if(si0){
si0=4820U;
si1=8U;
si2=l4;
si3=l4;
si4=76U;
si3=si3 == si4;
si1=si3?si1:si2;
i32_store(i->m0,si0,si1);
si0=-1U;
goto L5;
}
L6:;
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
L5:;
l4=si0;
si0=l3;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l4;
L1:;
l4=si0;
si0=0U;
l3=si0;
si0=l4;
si1=0U;
si0=(U32)((I32)si0<=(I32)si1);
if(si0){
si0=l0;
si1=l0;
si1=i32_load(i->m0,si1);
si2=32U;
si3=16U;
si4=l4;
si2=si4?si2:si3;
si1|=si2;
i32_store(i->m0,si0,si1);
goto L7;
}
L8:;
si0=l5;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l7=si0;
si1=l4;
si0=si0 >= si1;
if(si0){
si0=l4;
l3=si0;
goto L7;
}
L9:;
si0=l0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+40U);
l3=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l0;
si1=l3;
si2=l4;
si3=l7;
si2-=si3;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+44U);
if(si0){
si0=l0;
si1=l3;
si2=1U;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l1;
si1=l2;
si0+=si1;
si1=1U;
si0-=si1;
si1=l3;
si1=i32_load8_u(i->m0,si1);
i32_store8(i->m0,si0,si1);
}
L10:;
si0=l2;
l3=si0;
L7:;
si0=l5;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l3;
L0:;
return si0;
}

U32 f60(wasmModuleInstance* instance,U32 l0) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 si0;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
si0=f36(instance,si0);
L0:;
return si0;
}

U32 f61(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l2=0;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 l6=0;
U32 si0,si1,si2,si3,si4;
si0=1216U;
si1=l1;
si1=i32_load8_s(i->m0,si1);
si2=4U;
si0=f76(instance,si0,si1,si2);
si0=!(si0);
if(si0){
si0=4820U;
si1=28U;
i32_store(i->m0,si0,si1);
si0=0U;
goto L0;
}
L1:;
si0=335544320U;
si1=67108864U;
si2=268435456U;
si3=l1;
l3=si3;
si3=i32_load8_u(i->m0,si3);
l2=si3;
si4=114U;
si3=si3 == si4;
l4=si3;
si1=si3?si1:si2;
si2=l3;
si3=43U;
si2=f80(instance,si2,si3);
si0=si2?si0:si1;
l6=si0;
si1=16384U;
si0|=si1;
si1=l6;
si2=l3;
si3=120U;
si2=f80(instance,si2,si3);
si0=si2?si0:si1;
l3=si0;
si1=l3;
si2=4096U;
si1|=si2;
si2=l4;
si0=si2?si0:si1;
l3=si0;
si1=32768U;
si0|=si1;
si1=l3;
si2=l2;
si3=119U;
si2=si2 == si3;
si0=si2?si0:si1;
si1=l2;
si2=97U;
si1=si1 == si2;
si0|=si1;
l4=si0;
si0=i->g0;
si1=16U;
si0-=si1;
l3=si0;
i->g0=si0;
si0=-1U;
l2=si0;
si0=l0;
si1=l3;
si2=12U;
si1+=si2;
si2=4824U;
si3=4828U;
si3=i32_load(i->m0,si3);
si0=f47(instance,si0,si1,si2,si3);
l0=si0;
si1=-1U;
si0=si0 == si1;
if(si0){
si0=4820U;
si1=44U;
i32_store(i->m0,si0,si1);
goto L2;
}
L3:;
si0=l0;
si1=4824U;
si1=i32_load(i->m0,si1);
si2=l4;
si0=f43(instance,si0,si1,si2);
l2=si0;
L2:;
si0=l3;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l2;
l0=si0;
si1=0U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
goto L4;
}
si0=l0;
l3=si0;
si0=i->g0;
si1=32U;
si0-=si1;
l4=si0;
i->g0=si0;
si0=1216U;
si1=l1;
si1=i32_load8_s(i->m0,si1);
l6=si1;
si2=4U;
si0=f76(instance,si0,si1,si2);
si0=!(si0);
if(si0){
si0=4820U;
si1=28U;
i32_store(i->m0,si0,si1);
si0=0U;
goto L5;
}
L6:;
si0=0U;
si1=1144U;
si1=f29(instance,si1);
l2=si1;
si1=!(si1);
if(si1){
goto L5;
}
si0=l2;
si1=0U;
si2=112U;
si0=f79(instance,si0,si1,si2);
l2=si0;
si0=l1;
si1=43U;
si0=f80(instance,si0,si1);
si0=!(si0);
if(si0){
si0=l2;
si1=8U;
si2=4U;
si3=l6;
si4=114U;
si3=si3 == si4;
si1=si3?si1:si2;
l5=si1;
i32_store(i->m0,si0,si1);
}
L7:;
si0=l1;
si1=101U;
si0=f80(instance,si0,si1);
if(si0){
si0=l4;
si1=1U;
i32_store(i->m0,(WasmMemoryAddress)si0+16U,si1);
si0=l3;
si1=2U;
si2=l4;
si3=16U;
si2+=si3;
si0=f54(instance,si0,si1,si2);
si0=l1;
si0=i32_load8_u(i->m0,si0);
}else{
si0=l6;
}
L8:;
si1=255U;
si0&=si1;
si1=97U;
si0=si0 == si1;
if(si0){
si0=l3;
si1=3U;
si2=0U;
si0=f54(instance,si0,si1,si2);
l1=si0;
si1=1U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l4;
si1=l1;
si2=1U;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=l3;
si1=4U;
si2=l4;
si0=f54(instance,si0,si1,si2);
}
L10:;
si0=l2;
si1=l5;
si2=128U;
si1|=si2;
l5=si1;
i32_store(i->m0,si0,si1);
}
L9:;
si0=l2;
si1=-1U;
i32_store(i->m0,(WasmMemoryAddress)si0+64U,si1);
si0=l2;
si1=1024U;
i32_store(i->m0,(WasmMemoryAddress)si0+44U,si1);
si0=l2;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+56U,si1);
si0=l2;
si1=l2;
si2=120U;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+40U,si1);
si0=l5;
si1=8U;
si0&=si1;
if(si0){
goto L11;
}
si0=l3;
si0=f55(instance,si0);
si0=!(si0);
if(si0){
goto L11;
}
si0=l2;
si1=10U;
i32_store(i->m0,(WasmMemoryAddress)si0+64U,si1);
L11:;
si0=l2;
si1=1U;
i32_store(i->m0,(WasmMemoryAddress)si0+36U,si1);
si0=l2;
si1=2U;
i32_store(i->m0,(WasmMemoryAddress)si0+32U,si1);
si0=l2;
si1=3U;
i32_store(i->m0,(WasmMemoryAddress)si0+28U,si1);
si0=l2;
si1=4U;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l2;
si1=4860U;
si1=i32_load(i->m0,si1);
l3=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+52U,si1);
si0=l3;
if(si0){
si0=l3;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+48U,si1);
}
L12:;
si0=4860U;
si1=l2;
i32_store(i->m0,si0,si1);
si0=l2;
L5:;
l2=si0;
si0=l4;
si1=32U;
si0+=si1;
i->g0=si0;
si0=l2;
l5=si0;
if(si0){
goto L4;
}
si0=l0;
si0=f36(instance,si0);
si0=0U;
l5=si0;
L4:;
si0=l5;
L0:;
return si0;
}

U32 f62(wasmModuleInstance* instance,U32 l0) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l1=0;
U32 si0,si1,si2;
U64 sj1;
si0=l0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+60U);
l1=si1;
si2=1U;
si1-=si2;
si2=l1;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+60U,si1);
si0=l0;
si0=i32_load(i->m0,si0);
l1=si0;
si1=8U;
si0&=si1;
if(si0){
si0=l0;
si1=l1;
si2=32U;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=-1U;
goto L0;
}
L1:;
si0=l0;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+4U,sj1);
si0=l0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+40U);
l1=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l0;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=l0;
si1=l1;
si2=l0;
si2=i32_load(i->m0,(WasmMemoryAddress)si2+44U);
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+16U,si1);
si0=0U;
L0:;
return si0;
}

void f63(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l2=0;
U32 l3=0;
U32 si0,si1,si2,si3;
si0=i->g0;
si1=16U;
si0-=si1;
l2=si0;
i->g0=si0;
si0=l2;
si1=l1;
i32_store8(i->m0,(WasmMemoryAddress)si0+15U,si1);
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l3=si0;
if(si0){
si0=l3;
}else{
si0=l0;
si0=f62(instance,si0);
if(si0){
goto L1;
}
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
}
L3:;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+20U);
l3=si1;
si0=si0 == si1;
if(si0){
goto L2;
}
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+64U);
si1=l1;
si2=255U;
si1&=si2;
si0=si0 == si1;
if(si0){
goto L2;
}
si0=l0;
si1=l3;
si2=1U;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=l3;
si1=l1;
i32_store8(i->m0,si0,si1);
goto L1;
L2:;
si0=l0;
si1=l2;
si2=15U;
si1+=si2;
si2=1U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+32U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
si1=1U;
si0=si0 != si1;
if(si0){
goto L1;
}
si0=l2;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+15U);
L1:;
si0=l2;
si1=16U;
si0+=si1;
i->g0=si0;
L0:;
}

U32 f64(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 si0,si1,si2,si3,si4,si5;
si0=l2;
si1=l2;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+60U);
l3=si1;
si2=1U;
si1-=si2;
si2=l3;
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+60U,si1);
si0=l1;
l5=si0;
si0=l2;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l3=si0;
si1=l2;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l4=si1;
si0=si0 == si1;
if(si0){
si0=l5;
}else{
si0=l0;
si1=l3;
si2=l4;
si3=l3;
si2-=si3;
l4=si2;
si3=l5;
si4=l4;
si5=l5;
si4=si4 < si5;
si2=si4?si2:si3;
l4=si2;
si0=f78(instance,si0,si1,si2);
l0=si0;
si0=l2;
si1=l3;
si2=l4;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l0;
si1=l4;
si0+=si1;
l0=si0;
si0=l5;
si1=l4;
si0-=si1;
}
L1:;
l3=si0;
si0=l1;
l4=si0;
si0=l3;
if(si0){
L3:;
W2C2_LOOP_START
{
si0=l2;
si0=f52(instance,si0);
si0=!(si0);
if(si0){
si0=l2;
si1=l0;
si2=l3;
si3=l2;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+28U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
l1=si0;
if(si0){
goto L4;
}
}
L5:;
si0=l5;
si1=l3;
si0-=si1;
goto L0;
L4:;
si0=l0;
si1=l1;
si0+=si1;
l0=si0;
si0=l3;
si1=l1;
si0-=si1;
l3=si0;
if(si0){
goto L3;
}
}
}
L2:;
si0=l4;
L0:;
return si0;
}

void f65(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 si0,si1,si2,si3;
si0=l1;
si1=l2;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+16U);
l3=si1;
if(si1){
si1=l3;
}else{
si1=l2;
si1=f62(instance,si1);
if(si1){
goto L1;
}
si1=l2;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+16U);
}
L2:;
si2=l2;
si2=i32_load(i->m0,(WasmMemoryAddress)si2+20U);
l4=si2;
si1-=si2;
si0=si0 > si1;
if(si0){
si0=l2;
si1=l0;
si2=l1;
si3=l2;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+32U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
goto L0;
}
L3:;
si0=l2;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+64U);
si1=0U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
goto L4;
}
si0=l1;
si0=!(si0);
if(si0){
goto L4;
}
si0=l0;
si1=l1;
si0+=si1;
l5=si0;
si0=0U;
l3=si0;
L6:;
W2C2_LOOP_START
{
si0=l3;
si1=l5;
si0+=si1;
si1=1U;
si0-=si1;
si0=i32_load8_u(i->m0,si0);
si1=10U;
si0=si0 == si1;
if(si0){
goto L5;
}
si0=l1;
si1=l3;
si2=1U;
si1-=si2;
l3=si1;
si0+=si1;
if(si0){
goto L6;
}
}
goto L4;
L5:;
si0=l2;
si1=l0;
si2=l1;
si3=l3;
si2+=si3;
l1=si2;
si3=l2;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+32U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
si1=l1;
si0=si0 < si1;
if(si0){
goto L1;
}
si0=l0;
si1=l1;
si0+=si1;
l0=si0;
si0=0U;
si1=l3;
si0-=si1;
l1=si0;
si0=l2;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l4=si0;
L4:;
si0=l4;
si1=l0;
si2=l1;
si0=f78(instance,si0,si1,si2);
si0=l2;
si1=l2;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+20U);
si2=l1;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
L1:;
L0:;
}

U32 f66(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2,U32 l3) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l4=0;
U32 l5=0;
U32 l6=0;
U32 l7=0;
U32 l8=0;
U32 si0,si1,si2,si3;
si0=l1;
si1=l2;
si0*=si1;
l5=si0;
si0=l5;
si1=l3;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+16U);
l4=si1;
if(si1){
si1=l4;
}else{
si1=l3;
si1=f62(instance,si1);
if(si1){
goto L1;
}
si1=l3;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+16U);
}
L2:;
si2=l3;
si2=i32_load(i->m0,(WasmMemoryAddress)si2+20U);
l8=si2;
si1-=si2;
si0=si0 > si1;
if(si0){
si0=l3;
si1=l0;
si2=l5;
si3=l3;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+32U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
l6=si0;
goto L1;
}
L3:;
si0=l5;
si1=l5;
si1=!(si1);
if(si1){
goto L4;
}
si0=0U;
l4=si0;
si0=l5;
si1=l3;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+64U);
si2=0U;
si1=(U32)((I32)si1<(I32)si2);
if(si1){
goto L4;
}
si0=l0;
si1=l5;
si0+=si1;
l6=si0;
L6:;
W2C2_LOOP_START
{
si0=l4;
si1=l6;
si0+=si1;
si1=1U;
si0-=si1;
si0=i32_load8_u(i->m0,si0);
si1=10U;
si0=si0 == si1;
if(si0){
goto L5;
}
si0=l5;
si1=l4;
si2=1U;
si1-=si2;
l4=si1;
si0+=si1;
if(si0){
goto L6;
}
}
si0=l5;
goto L4;
L5:;
si0=l3;
si1=l0;
si2=l4;
si3=l5;
si2+=si3;
l7=si2;
si3=l3;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+32U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
l6=si0;
si1=l7;
si0=si0 < si1;
if(si0){
goto L1;
}
si0=l0;
si1=l7;
si0+=si1;
l0=si0;
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l8=si0;
si0=0U;
si1=l4;
si0-=si1;
L4:;
l4=si0;
si0=l8;
si1=l0;
si2=l4;
si0=f78(instance,si0,si1,si2);
si0=l3;
si1=l3;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+20U);
si2=l4;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=l4;
si1=l7;
si0+=si1;
l6=si0;
L1:;
si0=l5;
si1=l6;
si0=si0 == si1;
if(si0){
si0=l2;
si1=0U;
si2=l1;
si0=si2?si0:si1;
goto L0;
}
L7:;
si0=l6;
si1=l1;
si0=DIV_U(si0,si1);
L0:;
return si0;
}

void f67(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l2=0;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 si0,si1,si2,si3,si4,si5;
U64 sj1;
si0=i->g0;
si1=16U;
si0-=si1;
l4=si0;
i->g0=si0;
si0=l4;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=i->g0;
si1=208U;
si0-=si1;
l2=si0;
i->g0=si0;
si0=l2;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+204U,si1);
si0=l2;
si1=192U;
si0+=si1;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=l2;
si1=184U;
si0+=si1;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=l2;
si1=176U;
si0+=si1;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=l2;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+168U,sj1);
si0=l2;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+160U,sj1);
si0=l2;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+200U,si1);
si0=0U;
si1=l0;
si2=l2;
si3=200U;
si2+=si3;
si3=l2;
si4=80U;
si3+=si4;
si4=l2;
si5=160U;
si4+=si5;
si0=f73(instance,si0,si1,si2,si3,si4);
si1=0U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
si0=-1U;
}else{
si0=4200U;
si0=i32_load(i->m0,si0);
l5=si0;
si0=4260U;
si0=i32_load(i->m0,si0);
si1=0U;
si0=(U32)((I32)si0<=(I32)si1);
if(si0){
si0=4200U;
si1=l5;
si2=-33U;
si1&=si2;
i32_store(i->m0,si0,si1);
}
L2:;
si0=4244U;
si0=i32_load(i->m0,si0);
si0=!(si0);
if(si0){
si0=4244U;
si1=80U;
i32_store(i->m0,si0,si1);
si0=4224U;
si1=0U;
i32_store(i->m0,si0,si1);
si0=4216U;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=4240U;
si0=i32_load(i->m0,si0);
l3=si0;
si0=4240U;
si1=l2;
i32_store(i->m0,si0,si1);
goto L5;
}
L6:;
si0=4216U;
si0=i32_load(i->m0,si0);
if(si0){
goto L4;
}
L5:;
si0=-1U;
si1=4200U;
si1=f62(instance,si1);
if(si1){
goto L3;
}
L4:;
si0=4200U;
si1=l0;
si2=l2;
si3=200U;
si2+=si3;
si3=l2;
si4=80U;
si3+=si4;
si4=l2;
si5=160U;
si4+=si5;
si0=f73(instance,si0,si1,si2,si3,si4);
L3:;
l1=si0;
si0=l5;
si1=32U;
si0&=si1;
l0=si0;
si0=l3;
if(si0){
si0=4200U;
si1=0U;
si2=0U;
si3=4232U;
si3=i32_load(i->m0,si3);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_guestFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
si0=4244U;
si1=0U;
i32_store(i->m0,si0,si1);
si0=4240U;
si1=l3;
i32_store(i->m0,si0,si1);
si0=4224U;
si1=0U;
i32_store(i->m0,si0,si1);
si0=4220U;
si0=i32_load(i->m0,si0);
si0=4216U;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=0U;
}else{
si0=l1;
}
L7:;
si0=4200U;
si1=4200U;
si1=i32_load(i->m0,si1);
si2=l0;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=0U;
}
L1:;
si0=l2;
si1=208U;
si0+=si1;
i->g0=si0;
si0=l4;
si1=16U;
si0+=si1;
i->g0=si0;
L0:;
}

U32 f68(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 si0,si1,si2,si3,si4,si5;
si0=-1U;
si1=0U;
si2=l0;
si3=1U;
si4=l0;
si4=f82(instance,si4);
l0=si4;
si5=l1;
si2=f66(instance,si2,si3,si4,si5);
si3=l0;
si2=si2 != si3;
si0=si2?si0:si1;
L0:;
return si0;
}

void f69(wasmModuleInstance* instance,U32 l0) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 si0,si1,si2;
si0=l0;
si1=4200U;
si0=f68(instance,si0,si1);
si1=0U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
goto L0;
}
L1:;
si0=4264U;
si0=i32_load(i->m0,si0);
si1=10U;
si0=si0 == si1;
if(si0){
goto L2;
}
si0=4220U;
si0=i32_load(i->m0,si0);
l0=si0;
si1=4216U;
si1=i32_load(i->m0,si1);
si0=si0 == si1;
if(si0){
goto L2;
}
si0=4220U;
si1=l0;
si2=1U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=10U;
i32_store8(i->m0,si0,si1);
goto L0;
L2:;
si0=4200U;
si1=10U;
f63(instance,si0,si1);
L0:;
}

U32 f70(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 si0,si1,si2;
si0=l0;
si1=2U;
i32_store(i->m0,(WasmMemoryAddress)si0+32U,si1);
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=64U;
si0&=si1;
if(si0){
goto L1;
}
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
si0=f55(instance,si0);
if(si0){
goto L1;
}
si0=l0;
si1=-1U;
i32_store(i->m0,(WasmMemoryAddress)si0+64U,si1);
L1:;
si0=l0;
si1=l1;
si2=l2;
si0=f58(instance,si0,si1,si2);
L0:;
return si0;
}

U32 f71(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l2=0;
U32 si0,si1,si2;
si0=l0;
si0=!(si0);
if(si0){
si0=0U;
goto L0;
}
L1:;
si0=l0;
if(si0){
si0=l1;
si1=127U;
si0=si0 <= si1;
if(si0){
si0=l0;
si1=l1;
i32_store8(i->m0,si0,si1);
si0=1U;
goto L3;
}
L4:;
si0=6976U;
si0=i32_load(i->m0,si0);
l2=si0;
if(si0){
si0=l2;
}else{
si0=6976U;
si1=6952U;
i32_store(i->m0,si0,si1);
si0=6952U;
}
L6:;
si0=i32_load(i->m0,si0);
si0=!(si0);
if(si0){
si0=l1;
si1=-128U;
si0&=si1;
si1=57216U;
si0=si0 != si1;
if(si0){
si0=4820U;
si1=25U;
i32_store(i->m0,si0,si1);
goto L5;
}
L8:;
si0=l0;
si1=l1;
i32_store8(i->m0,si0,si1);
si0=1U;
goto L3;
}
L7:;
si0=l1;
si1=2047U;
si0=si0 <= si1;
if(si0){
si0=l0;
si1=l1;
si2=63U;
si1&=si2;
si2=128U;
si1|=si2;
i32_store8(i->m0,(WasmMemoryAddress)si0+1U,si1);
si0=l0;
si1=l1;
si2=6U;
si1>>=(si2&31);
si2=192U;
si1|=si2;
i32_store8(i->m0,si0,si1);
si0=2U;
goto L3;
}
L9:;
si0=l1;
si1=-8192U;
si0&=si1;
si1=57344U;
si0=si0 != si1;
si1=l1;
si2=55296U;
si1=si1 >= si2;
si0&=si1;
si0=!(si0);
if(si0){
si0=l0;
si1=l1;
si2=63U;
si1&=si2;
si2=128U;
si1|=si2;
i32_store8(i->m0,(WasmMemoryAddress)si0+2U,si1);
si0=l0;
si1=l1;
si2=12U;
si1>>=(si2&31);
si2=224U;
si1|=si2;
i32_store8(i->m0,si0,si1);
si0=l0;
si1=l1;
si2=6U;
si1>>=(si2&31);
si2=63U;
si1&=si2;
si2=128U;
si1|=si2;
i32_store8(i->m0,(WasmMemoryAddress)si0+1U,si1);
si0=3U;
goto L3;
}
L10:;
si0=l1;
si1=65536U;
si0-=si1;
si1=1048575U;
si0=si0 <= si1;
if(si0){
si0=l0;
si1=l1;
si2=63U;
si1&=si2;
si2=128U;
si1|=si2;
i32_store8(i->m0,(WasmMemoryAddress)si0+3U,si1);
si0=l0;
si1=l1;
si2=18U;
si1>>=(si2&31);
si2=240U;
si1|=si2;
i32_store8(i->m0,si0,si1);
si0=l0;
si1=l1;
si2=6U;
si1>>=(si2&31);
si2=63U;
si1&=si2;
si2=128U;
si1|=si2;
i32_store8(i->m0,(WasmMemoryAddress)si0+2U,si1);
si0=l0;
si1=l1;
si2=12U;
si1>>=(si2&31);
si2=63U;
si1&=si2;
si2=128U;
si1|=si2;
i32_store8(i->m0,(WasmMemoryAddress)si0+1U,si1);
si0=4U;
goto L3;
}
L11:;
si0=4820U;
si1=25U;
i32_store(i->m0,si0,si1);
L5:;
si0=-1U;
L3:;
}else{
si0=1U;
}
L2:;
L0:;
return si0;
}

F64 f72(wasmModuleInstance* instance,F64 l0,U32 l1) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l2=0;
U64 l3=0;
U32 si0,si1,si2;
U64 sj0,sj1;
F64 sd0,sd1;
sd0=l0;
sj0=i64_reinterpret_f64(sd0);
l3=sj0;
sj1=W2C2_LL(52U);
sj0>>=(sj1&63);
si0=(U32)(sj0);
si1=2047U;
si0&=si1;
l2=si0;
si1=2047U;
si0=si0 != si1;
if(si0){
si0=l2;
si0=!(si0);
if(si0){
sd0=l0;
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
si0=sd0 == sd1;
if(si0){
si0=l1;
si1=0U;
i32_store(i->m0,si0,si1);
sd0=l0;
goto L0;
}
L3:;
sd0=l0;
sd1=f64_reinterpret_i64(W2C2_LL(0x43F0000000000000U));
sd0*=sd1;
si1=l1;
sd0=f72(instance,sd0,si1);
l0=sd0;
si0=l1;
si1=l1;
si1=i32_load(i->m0,si1);
si2=-64U;
si1+=si2;
i32_store(i->m0,si0,si1);
sd0=l0;
goto L0;
}
L2:;
si0=l1;
si1=l2;
si2=1022U;
si1-=si2;
i32_store(i->m0,si0,si1);
sj0=l3;
sj1=W2C2_LL(-9218868437227405313U);
sj0&=sj1;
sj1=W2C2_LL(4602678819172646912U);
sj0|=sj1;
sd0=f64_reinterpret_i64(sj0);
}else{
sd0=l0;
}
L1:;
L0:;
return sd0;
}

U32 f73(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2,U32 l3,U32 l4) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l5=0;
U32 l6=0;
U32 l7=0;
U32 l8=0;
U32 l9=0;
U32 l10=0;
U32 l11=0;
U32 l12=0;
U32 l13=0;
U32 l14=0;
U32 l15=0;
F64 l16=0;
U32 l17=0;
U64 l18=0;
U32 l19=0;
U32 l20=0;
U32 l21=0;
U32 l22=0;
F64 l23=0;
U32 l24=0;
U32 l25=0;
U32 l26=0;
U32 l27=0;
U64 l28=0;
U32 l29=0;
U32 l30=0;
U32 l31=0;
U32 l32=0;
U32 l33=0;
U32 l34=0;
U32 l35=0;
U32 l36=0;
U32 l37=0;
U32 l38=0;
U32 l39=0;
U32 l40=0;
U32 l41=0;
U64 l42=0;
U32 si0,si1,si2,si3,si4,si5;
U64 sj0,sj1,sj2,sj3;
F64 sd0,sd1,sd2,sd3;
si0=i->g0;
si1=880U;
si0-=si1;
l7=si0;
i->g0=si0;
si0=l3;
si1=384U;
si0-=si1;
l35=si0;
si0=l7;
si1=80U;
si0+=si1;
l27=si0;
si0=l7;
si1=108U;
si0+=si1;
l37=si0;
si0=l7;
si1=55U;
si0+=si1;
l38=si0;
si0=l7;
si1=79U;
si0+=si1;
l39=si0;
si0=l7;
si1=79U;
si0+=si1;
l40=si0;
si0=l7;
si1=80U;
si0+=si1;
si1=8U;
si0|=si1;
l36=si0;
si0=l7;
si1=80U;
si0+=si1;
si1=9U;
si0|=si1;
l32=si0;
si0=l7;
si1=78U;
si0+=si1;
l41=si0;
si0=l7;
si1=56U;
si0+=si1;
l24=si0;
L3:;
W2C2_LOOP_START
{
si0=l1;
l10=si0;
si0=l5;
si1=l20;
si2=2147483647U;
si1^=si2;
si0=(U32)((I32)si0>(I32)si1);
if(si0){
goto L4;
}
si0=l5;
si1=l20;
si0+=si1;
l20=si0;
si0=l10;
si0=i32_load8_u(i->m0,si0);
l5=si0;
if(si0){
L14:;
W2C2_LOOP_START
{
si0=l5;
si1=255U;
si0&=si1;
l5=si0;
if(si0){
si0=l5;
si1=37U;
si0=si0 != si1;
if(si0){
goto L15;
}
si0=l1;
l6=si0;
l5=si0;
L18:;
W2C2_LOOP_START
{
si0=l5;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+1U);
si1=37U;
si0=si0 != si1;
if(si0){
si0=l5;
l1=si0;
goto L16;
}
L19:;
si0=l6;
si1=1U;
si0+=si1;
l6=si0;
si0=l5;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+2U);
l8=si0;
si0=l5;
si1=2U;
si0+=si1;
l1=si0;
l5=si0;
si0=l8;
si1=37U;
si0=si0 == si1;
if(si0){
goto L18;
}
}
goto L16;
}
L17:;
si0=l1;
l6=si0;
L16:;
si0=l6;
si1=l10;
si0-=si1;
l5=si0;
si1=l20;
si2=2147483647U;
si1^=si2;
l6=si1;
si0=(U32)((I32)si0>(I32)si1);
if(si0){
goto L4;
}
si0=l0;
si0=!(si0);
if(si0){
goto L20;
}
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L20;
}
si0=l10;
si1=l5;
si2=l0;
f65(instance,si0,si1,si2);
L20:;
si0=l5;
if(si0){
goto L3;
}
si0=l1;
si1=1U;
si0+=si1;
l5=si0;
si0=-1U;
l17=si0;
si0=l1;
si0=i32_load8_s(i->m0,(WasmMemoryAddress)si0+1U);
l9=si0;
si1=48U;
si0-=si1;
l8=si0;
si1=9U;
si0=si0 > si1;
if(si0){
goto L21;
}
si0=l1;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+2U);
si1=36U;
si0=si0 != si1;
if(si0){
goto L21;
}
si0=l1;
si1=3U;
si0+=si1;
l5=si0;
si0=l1;
si0=i32_load8_s(i->m0,(WasmMemoryAddress)si0+3U);
l9=si0;
si0=1U;
l30=si0;
si0=l8;
l17=si0;
L21:;
si0=0U;
l11=si0;
si0=l9;
si1=32U;
si0-=si1;
l1=si0;
si1=31U;
si0=si0 > si1;
if(si0){
goto L22;
}
si0=1U;
si1=l1;
si0<<=(si1&31);
l1=si0;
si1=75913U;
si0&=si1;
si0=!(si0);
if(si0){
goto L22;
}
si0=l5;
si1=1U;
si0+=si1;
l8=si0;
L23:;
W2C2_LOOP_START
{
si0=l1;
si1=l11;
si0|=si1;
l11=si0;
si0=l8;
l5=si0;
si0=i32_load8_s(i->m0,si0);
l9=si0;
si1=32U;
si0-=si1;
l1=si0;
si1=32U;
si0=si0 >= si1;
if(si0){
goto L22;
}
si0=l5;
si1=1U;
si0+=si1;
l8=si0;
si0=1U;
si1=l1;
si0<<=(si1&31);
l1=si0;
si1=75913U;
si0&=si1;
if(si0){
goto L23;
}
}
L22:;
si0=l9;
si1=42U;
si0=si0 == si1;
if(si0){
si0=l5;
si0=i32_load8_s(i->m0,(WasmMemoryAddress)si0+1U);
si1=48U;
si0-=si1;
l1=si0;
si1=9U;
si0=si0 > si1;
if(si0){
goto L26;
}
si0=l5;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+2U);
si1=36U;
si0=si0 != si1;
if(si0){
goto L26;
}
si0=l4;
si1=l1;
si2=2U;
si1<<=(si2&31);
si0+=si1;
si1=10U;
i32_store(i->m0,si0,si1);
si0=l5;
si1=3U;
si0+=si1;
l1=si0;
si0=1U;
l30=si0;
si0=l35;
si1=l5;
si1=i32_load8_s(i->m0,(WasmMemoryAddress)si1+1U);
si2=3U;
si1<<=(si2&31);
si0+=si1;
si0=i32_load(i->m0,si0);
goto L25;
L26:;
si0=l30;
if(si0){
goto L11;
}
si0=l5;
si1=1U;
si0+=si1;
l1=si0;
si0=l0;
si0=!(si0);
if(si0){
si0=0U;
l30=si0;
si0=0U;
l14=si0;
goto L12;
}
L27:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
l5=si1;
si2=4U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=0U;
l30=si0;
si0=l5;
si0=i32_load(i->m0,si0);
L25:;
l14=si0;
si1=0U;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L12;
}
si0=0U;
si1=l14;
si0-=si1;
l14=si0;
si0=l11;
si1=8192U;
si0|=si1;
l11=si0;
goto L12;
}
L24:;
si0=0U;
l14=si0;
si0=l9;
si1=48U;
si0-=si1;
l8=si0;
si1=9U;
si0=si0 > si1;
if(si0){
si0=l5;
l1=si0;
goto L12;
}
L28:;
L29:;
W2C2_LOOP_START
{
si0=l14;
si1=214748364U;
si0=si0 <= si1;
if(si0){
si0=-1U;
si1=l14;
si2=10U;
si1*=si2;
l1=si1;
si2=l8;
si1+=si2;
si2=l8;
si3=l1;
si4=2147483647U;
si3^=si4;
si2=si2 > si3;
l9=si2;
si0=si2?si0:si1;
l14=si0;
si0=l5;
si0=i32_load8_s(i->m0,(WasmMemoryAddress)si0+1U);
l8=si0;
si0=l5;
si1=1U;
si0+=si1;
l1=si0;
l5=si0;
si0=l8;
si1=48U;
si0-=si1;
l8=si0;
si1=10U;
si0=si0 < si1;
if(si0){
goto L29;
}
si0=l9;
if(si0){
goto L4;
}
goto L12;
}
L30:;
si0=l5;
si0=i32_load8_s(i->m0,(WasmMemoryAddress)si0+1U);
l1=si0;
si0=-1U;
l14=si0;
si0=l5;
si1=1U;
si0+=si1;
l5=si0;
si0=l1;
si1=48U;
si0-=si1;
l8=si0;
si1=10U;
si0=si0 < si1;
if(si0){
goto L29;
}
}
goto L4;
L15:;
si0=l1;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+1U);
l5=si0;
si0=l1;
si1=1U;
si0+=si1;
l1=si0;
goto L14;
}
UNREACHABLE;
}
L13:;
si0=l0;
if(si0){
goto L1;
}
si0=l30;
si0=!(si0);
if(si0){
si0=0U;
l20=si0;
goto L1;
}
L31:;
si0=1U;
si1=l4;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
l1=si1;
si1=!(si1);
if(si1){
goto L33;
}
si0=l3;
si1=8U;
si0+=si1;
si1=l1;
si2=l2;
f74(instance,si0,si1,si2);
si0=2U;
si1=l4;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l1=si1;
si1=!(si1);
if(si1){
goto L33;
}
si0=l3;
si1=16U;
si0+=si1;
si1=l1;
si2=l2;
f74(instance,si0,si1,si2);
si0=3U;
si1=l4;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+12U);
l1=si1;
si1=!(si1);
if(si1){
goto L33;
}
si0=l3;
si1=24U;
si0+=si1;
si1=l1;
si2=l2;
f74(instance,si0,si1,si2);
si0=4U;
si1=l4;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+16U);
l1=si1;
si1=!(si1);
if(si1){
goto L33;
}
si0=l3;
si1=32U;
si0+=si1;
si1=l1;
si2=l2;
f74(instance,si0,si1,si2);
si0=5U;
si1=l4;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+20U);
l1=si1;
si1=!(si1);
if(si1){
goto L33;
}
si0=l3;
si1=40U;
si0+=si1;
si1=l1;
si2=l2;
f74(instance,si0,si1,si2);
si0=6U;
si1=l4;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+24U);
l1=si1;
si1=!(si1);
if(si1){
goto L33;
}
si0=l3;
si1=48U;
si0+=si1;
si1=l1;
si2=l2;
f74(instance,si0,si1,si2);
si0=7U;
si1=l4;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+28U);
l1=si1;
si1=!(si1);
if(si1){
goto L33;
}
si0=l3;
si1=56U;
si0+=si1;
si1=l1;
si2=l2;
f74(instance,si0,si1,si2);
si0=8U;
si1=l4;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+32U);
l1=si1;
si1=!(si1);
if(si1){
goto L33;
}
si0=l3;
si1=-64U;
si0-=si1;
si1=l1;
si2=l2;
f74(instance,si0,si1,si2);
si0=l4;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+36U);
l1=si0;
if(si0){
goto L32;
}
si0=9U;
L33:;
si1=2U;
si0<<=(si1&31);
l1=si0;
L34:;
W2C2_LOOP_START
{
si0=l1;
si1=l4;
si0+=si1;
si0=i32_load(i->m0,si0);
if(si0){
goto L11;
}
si0=l1;
si1=4U;
si0+=si1;
l1=si0;
si1=40U;
si0=si0 != si1;
if(si0){
goto L34;
}
}
si0=1U;
l20=si0;
goto L1;
L32:;
si0=l3;
si1=72U;
si0+=si1;
si1=l1;
si2=l2;
f74(instance,si0,si1,si2);
si0=1U;
l20=si0;
goto L1;
L12:;
si0=0U;
l5=si0;
si0=l1;
si0=i32_load8_u(i->m0,si0);
si1=46U;
si0=si0 != si1;
if(si0){
si0=-1U;
l9=si0;
si0=0U;
l12=si0;
goto L35;
}
L36:;
si0=l1;
si0=i32_load8_s(i->m0,(WasmMemoryAddress)si0+1U);
l8=si0;
si1=42U;
si0=si0 == si1;
if(si0){
si0=l1;
si0=i32_load8_s(i->m0,(WasmMemoryAddress)si0+2U);
si1=48U;
si0-=si1;
l8=si0;
si1=9U;
si0=si0 > si1;
if(si0){
goto L38;
}
si0=l1;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+3U);
si1=36U;
si0=si0 != si1;
if(si0){
goto L38;
}
si0=l4;
si1=l8;
si2=2U;
si1<<=(si2&31);
si0+=si1;
si1=10U;
i32_store(i->m0,si0,si1);
si0=l1;
si0=i32_load8_s(i->m0,(WasmMemoryAddress)si0+2U);
l8=si0;
si0=l1;
si1=4U;
si0+=si1;
l1=si0;
si0=l35;
si1=l8;
si2=3U;
si1<<=(si2&31);
si0+=si1;
si0=i32_load(i->m0,si0);
l9=si0;
si1=0U;
si0=(U32)((I32)si0>=(I32)si1);
l12=si0;
goto L35;
L38:;
si0=l30;
if(si0){
goto L11;
}
si0=l1;
si1=2U;
si0+=si1;
l1=si0;
si0=l0;
si0=!(si0);
if(si0){
si0=0U;
l9=si0;
si0=1U;
l12=si0;
goto L35;
}
L39:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
l8=si1;
si2=4U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l8;
si0=i32_load(i->m0,si0);
l9=si0;
si1=0U;
si0=(U32)((I32)si0>=(I32)si1);
l12=si0;
goto L35;
}
L37:;
si0=l1;
si1=1U;
si0+=si1;
l1=si0;
si0=l8;
si1=48U;
si0-=si1;
l13=si0;
si1=9U;
si0=si0 > si1;
if(si0){
si0=1U;
l12=si0;
si0=0U;
l9=si0;
goto L35;
}
L40:;
si0=0U;
l15=si0;
si0=l1;
l8=si0;
L41:;
W2C2_LOOP_START
{
si0=-1U;
l9=si0;
si0=l15;
si1=214748364U;
si0=si0 <= si1;
if(si0){
si0=-1U;
si1=l15;
si2=10U;
si1*=si2;
l1=si1;
si2=l13;
si1+=si2;
si2=l13;
si3=l1;
si4=2147483647U;
si3^=si4;
si2=si2 > si3;
si0=si2?si0:si1;
l9=si0;
}
L42:;
si0=1U;
l12=si0;
si0=l8;
si0=i32_load8_s(i->m0,(WasmMemoryAddress)si0+1U);
l13=si0;
si0=l9;
l15=si0;
si0=l8;
si1=1U;
si0+=si1;
l1=si0;
l8=si0;
si0=l13;
si1=48U;
si0-=si1;
l13=si0;
si1=10U;
si0=si0 < si1;
if(si0){
goto L41;
}
}
L35:;
L43:;
W2C2_LOOP_START
{
si0=l5;
l8=si0;
si0=l1;
si0=i32_load8_s(i->m0,si0);
l5=si0;
si1=123U;
si0-=si1;
si1=-58U;
si0=si0 < si1;
if(si0){
goto L11;
}
si0=l1;
si1=1U;
si0+=si1;
l1=si0;
si0=l5;
si1=l8;
si2=58U;
si1*=si2;
si0+=si1;
si1=3407U;
si0+=si1;
si0=i32_load8_u(i->m0,si0);
l5=si0;
si1=1U;
si0-=si1;
si1=8U;
si0=si0 < si1;
if(si0){
goto L43;
}
}
si0=l5;
si1=27U;
si0=si0 != si1;
if(si0){
si0=l5;
si0=!(si0);
if(si0){
goto L11;
}
si0=l17;
si1=0U;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
si0=l4;
si1=l17;
si2=2U;
si1<<=(si2&31);
si0+=si1;
si1=l5;
i32_store(i->m0,si0,si1);
si0=l7;
si1=l3;
si2=l17;
si3=3U;
si2<<=(si3&31);
si1+=si2;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,(WasmMemoryAddress)si0+56U,sj1);
goto L45;
}
L47:;
si0=l0;
si0=!(si0);
if(si0){
si0=0U;
l20=si0;
goto L1;
}
L48:;
si0=l7;
si1=56U;
si0+=si1;
si1=l5;
si2=l2;
f74(instance,si0,si1,si2);
goto L44;
}
L46:;
si0=l17;
si1=0U;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L11;
}
L45:;
si0=0U;
l5=si0;
si0=l0;
si0=!(si0);
if(si0){
goto L3;
}
L44:;
si0=l11;
si1=-65537U;
si0&=si1;
l15=si0;
si1=l11;
si2=l11;
si3=8192U;
si2&=si3;
si0=si2?si0:si1;
l19=si0;
si0=l1;
si1=1U;
si0-=si1;
si0=i32_load8_s(i->m0,si0);
l5=si0;
si1=-45U;
si0&=si1;
si1=l5;
si2=l5;
si3=15U;
si2&=si3;
si3=3U;
si2=si2 == si3;
si0=si2?si0:si1;
si1=l5;
si2=l8;
si0=si2?si0:si1;
l21=si0;
si1=65U;
si0-=si1;
switch(si0){
case 0:
goto L49;
case 1:
goto L10;
case 2:
goto L52;
case 3:
goto L10;
case 4:
goto L49;
case 5:
goto L49;
case 6:
goto L49;
case 7:
goto L10;
case 8:
goto L10;
case 9:
goto L10;
case 10:
goto L10;
case 11:
goto L10;
case 12:
goto L10;
case 13:
goto L10;
case 14:
goto L10;
case 15:
goto L10;
case 16:
goto L10;
case 17:
goto L10;
case 18:
goto L53;
case 19:
goto L10;
case 20:
goto L10;
case 21:
goto L10;
case 22:
goto L10;
case 23:
goto L62;
case 24:
goto L10;
case 25:
goto L10;
case 26:
goto L10;
case 27:
goto L10;
case 28:
goto L10;
case 29:
goto L10;
case 30:
goto L10;
case 31:
goto L10;
case 32:
goto L49;
case 33:
goto L10;
case 34:
goto L57;
case 35:
goto L60;
case 36:
goto L49;
case 37:
goto L49;
case 38:
goto L49;
case 39:
goto L10;
case 40:
goto L60;
case 41:
goto L10;
case 42:
goto L10;
case 43:
goto L10;
case 44:
goto L56;
case 45:
goto L64;
case 46:
goto L61;
case 47:
goto L63;
case 48:
goto L10;
case 49:
goto L10;
case 50:
goto L55;
case 51:
goto L10;
case 52:
goto L65;
case 53:
goto L10;
case 54:
goto L10;
case 55:
goto L62;
default:
goto L10;
}
L65:;
si0=0U;
l13=si0;
si0=l7;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+56U);
l18=sj0;
si0=1030U;
goto L59;
L64:;
si0=0U;
l5=si0;
si0=l8;
si1=255U;
si0&=si1;
switch(si0){
case 0:
goto L72;
case 1:
goto L71;
case 2:
goto L70;
case 3:
goto L69;
case 4:
goto L68;
case 5:
goto L3;
case 6:
goto L67;
case 7:
goto L66;
default:
goto L3;
}
L72:;
si0=l7;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
si1=l20;
i32_store(i->m0,si0,si1);
goto L3;
L71:;
si0=l7;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
si1=l20;
i32_store(i->m0,si0,si1);
goto L3;
L70:;
si0=l7;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
si1=l20;
sj1=(U64)(I64)(I32)(si1);
i64_store(i->m0,si0,sj1);
goto L3;
L69:;
si0=l7;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
si1=l20;
i32_store16(i->m0,si0,si1);
goto L3;
L68:;
si0=l7;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
si1=l20;
i32_store8(i->m0,si0,si1);
goto L3;
L67:;
si0=l7;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
si1=l20;
i32_store(i->m0,si0,si1);
goto L3;
L66:;
si0=l7;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
si1=l20;
sj1=(U64)(I64)(I32)(si1);
i64_store(i->m0,si0,sj1);
goto L3;
L63:;
si0=l9;
si1=8U;
si2=l9;
si3=8U;
si2=si2 > si3;
si0=si2?si0:si1;
l9=si0;
si0=l19;
si1=8U;
si0|=si1;
l19=si0;
si0=120U;
l21=si0;
L62:;
si0=0U;
l13=si0;
si0=1030U;
l17=si0;
si0=l7;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+56U);
l18=sj0;
si0=!(sj0);
if(si0){
si0=l24;
l10=si0;
goto L58;
}
L73:;
si0=l21;
si1=32U;
si0&=si1;
l8=si0;
si0=l24;
l10=si0;
L74:;
W2C2_LOOP_START
{
si0=l10;
si1=1U;
si0-=si1;
l10=si0;
sj1=l18;
si1=(U32)(sj1);
si2=15U;
si1&=si2;
si2=3936U;
si1+=si2;
si1=i32_load8_u(i->m0,si1);
si2=l8;
si1|=si2;
i32_store8(i->m0,si0,si1);
sj0=l18;
sj1=W2C2_LL(15U);
si0=sj0 > sj1;
l5=si0;
sj0=l18;
sj1=W2C2_LL(4U);
sj0>>=(sj1&63);
l18=sj0;
si0=l5;
if(si0){
goto L74;
}
}
si0=l19;
si1=8U;
si0&=si1;
si0=!(si0);
if(si0){
goto L58;
}
si0=l21;
si1=4U;
si0=(U32)((I32)si0>>(si1&31));
si1=1030U;
si0+=si1;
l17=si0;
si0=2U;
l13=si0;
goto L58;
L61:;
si0=l24;
l10=si0;
si0=l7;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+56U);
l18=sj0;
si0=!(sj0);
si0=!(si0);
if(si0){
L76:;
W2C2_LOOP_START
{
si0=l10;
si1=1U;
si0-=si1;
l10=si0;
sj1=l18;
si1=(U32)(sj1);
si2=7U;
si1&=si2;
si2=48U;
si1|=si2;
i32_store8(i->m0,si0,si1);
sj0=l18;
sj1=W2C2_LL(7U);
si0=sj0 > sj1;
l5=si0;
sj0=l18;
sj1=W2C2_LL(3U);
sj0>>=(sj1&63);
l18=sj0;
si0=l5;
if(si0){
goto L76;
}
}
}
L75:;
si0=0U;
l13=si0;
si0=1030U;
l17=si0;
si0=l19;
si1=8U;
si0&=si1;
si0=!(si0);
if(si0){
goto L58;
}
si0=l9;
si1=l24;
si2=l10;
si1-=si2;
l5=si1;
si2=1U;
si1+=si2;
si2=l5;
si3=l9;
si2=(U32)((I32)si2<(I32)si3);
si0=si2?si0:si1;
l9=si0;
goto L58;
L60:;
si0=l7;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+56U);
l18=sj0;
sj1=W2C2_LL(0U);
si0=(U64)((I64)sj0<(I64)sj1);
if(si0){
si0=l7;
sj1=W2C2_LL(0U);
sj2=l18;
sj1-=sj2;
l18=sj1;
i64_store(i->m0,(WasmMemoryAddress)si0+56U,sj1);
si0=1U;
l13=si0;
si0=1030U;
goto L59;
}
L77:;
si0=l19;
si1=2048U;
si0&=si1;
if(si0){
si0=1U;
l13=si0;
si0=1031U;
goto L59;
}
L78:;
si0=1032U;
si1=1030U;
si2=l19;
si3=1U;
si2&=si3;
l13=si2;
si0=si2?si0:si1;
L59:;
l17=si0;
sj0=l18;
sj1=W2C2_LL(4294967296U);
si0=sj0 < sj1;
if(si0){
sj0=l18;
l28=sj0;
si0=l24;
l10=si0;
goto L79;
}
L80:;
si0=l24;
l10=si0;
L81:;
W2C2_LOOP_START
{
si0=l10;
si1=1U;
si0-=si1;
l10=si0;
sj1=l18;
sj2=l18;
sj3=W2C2_LL(10U);
sj2=DIV_U(sj2,sj3);
l28=sj2;
sj3=W2C2_LL(10U);
sj2*=sj3;
sj1-=sj2;
si1=(U32)(sj1);
si2=48U;
si1|=si2;
i32_store8(i->m0,si0,si1);
sj0=l18;
sj1=W2C2_LL(42949672959U);
si0=sj0 > sj1;
l5=si0;
sj0=l28;
l18=sj0;
si0=l5;
if(si0){
goto L81;
}
}
L79:;
sj0=l28;
si0=!(sj0);
if(si0){
goto L58;
}
sj0=l28;
si0=(U32)(sj0);
l5=si0;
L82:;
W2C2_LOOP_START
{
si0=l10;
si1=1U;
si0-=si1;
l10=si0;
si1=l5;
si2=l5;
si3=10U;
si2=DIV_U(si2,si3);
l8=si2;
si3=10U;
si2*=si3;
si1-=si2;
si2=48U;
si1|=si2;
i32_store8(i->m0,si0,si1);
si0=l5;
si1=9U;
si0=si0 > si1;
l11=si0;
si0=l8;
l5=si0;
si0=l11;
if(si0){
goto L82;
}
}
L58:;
si0=l12;
si1=l9;
si2=0U;
si1=(U32)((I32)si1<(I32)si2);
si0&=si1;
if(si0){
goto L4;
}
si0=l19;
si1=-65537U;
si0&=si1;
si1=l19;
si2=l12;
si0=si2?si0:si1;
l15=si0;
si0=l7;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+56U);
l18=sj0;
sj1=W2C2_LL(0U);
si0=sj0 != sj1;
if(si0){
goto L83;
}
si0=0U;
l11=si0;
si0=l9;
if(si0){
goto L83;
}
si0=l24;
l10=si0;
l5=si0;
goto L9;
L83:;
si0=l9;
sj1=l18;
si1=!(sj1);
si2=l24;
si3=l10;
si2-=si3;
si1+=si2;
l5=si1;
si2=l5;
si3=l9;
si2=(U32)((I32)si2<(I32)si3);
si0=si2?si0:si1;
l11=si0;
si0=l24;
l5=si0;
goto L9;
L57:;
si0=l7;
si1=l7;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+56U);
i64_store8(i->m0,(WasmMemoryAddress)si0+55U,sj1);
si0=0U;
l13=si0;
si0=1030U;
l17=si0;
si0=1U;
l11=si0;
si0=l38;
l10=si0;
si0=l24;
l5=si0;
goto L9;
L56:;
si0=4820U;
si0=i32_load(i->m0,si0);
l10=si0;
si0=6976U;
si0=i32_load(i->m0,si0);
l5=si0;
if(si0){
si0=l5;
}else{
si0=6976U;
si1=6952U;
i32_store(i->m0,si0,si1);
si0=6952U;
}
L84:;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
si0=0U;
si1=l10;
si2=l10;
si3=76U;
si2=si2 > si3;
si0=si2?si0:si1;
si1=1U;
si0<<=(si1&31);
si1=3312U;
si0+=si1;
si0=i32_load16_u(i->m0,si0);
si1=1760U;
si0+=si1;
goto L54;
L55:;
si0=l7;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
l5=si0;
si1=1263U;
si2=l5;
si0=si2?si0:si1;
L54:;
l10=si0;
si1=0U;
si2=l9;
si3=2147483647U;
si4=l9;
si5=2147483647U;
si4=si4 < si5;
si2=si4?si2:si3;
l8=si2;
si0=f76(instance,si0,si1,si2);
l5=si0;
si1=l10;
si0-=si1;
si1=l8;
si2=l5;
si0=si2?si0:si1;
l11=si0;
si1=l10;
si0+=si1;
l5=si0;
si0=0U;
l13=si0;
si0=1030U;
l17=si0;
si0=l9;
si1=0U;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L9;
}
si0=l5;
si0=i32_load8_u(i->m0,si0);
si0=!(si0);
if(si0){
goto L9;
}
goto L4;
L53:;
si0=l7;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
l10=si0;
si1=l9;
if(si1){
goto L51;
}
si0=0U;
l5=si0;
goto L50;
L52:;
si0=l7;
si1=0U;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l7;
si1=l7;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+56U);
i64_store32(i->m0,(WasmMemoryAddress)si0+8U,sj1);
si0=l7;
si1=l7;
si2=8U;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+56U,si1);
si0=-1U;
l9=si0;
si0=l7;
si1=8U;
si0+=si1;
L51:;
l10=si0;
si0=0U;
l5=si0;
si0=l10;
l6=si0;
L85:;
W2C2_LOOP_START
{
si0=l6;
si0=i32_load(i->m0,si0);
l8=si0;
si0=!(si0);
if(si0){
goto L86;
}
si0=l7;
si1=4U;
si0+=si1;
si1=l8;
si0=f71(instance,si0,si1);
l8=si0;
si1=0U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
goto L2;
}
si0=l8;
si1=l9;
si2=l5;
si1-=si2;
si0=si0 > si1;
if(si0){
goto L86;
}
si0=l6;
si1=4U;
si0+=si1;
l6=si0;
si0=l5;
si1=l8;
si0+=si1;
l5=si0;
si1=l9;
si0=si0 < si1;
if(si0){
goto L85;
}
L86:;
}
si0=l5;
si1=0U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
goto L4;
}
L50:;
si0=l19;
si1=73728U;
si0&=si1;
l11=si0;
if(si0){
goto L87;
}
si0=l5;
si1=l14;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L87;
}
si0=l7;
si1=112U;
si0+=si1;
si1=32U;
si2=l14;
si3=l5;
si2-=si3;
l6=si2;
si3=256U;
si4=l6;
si5=256U;
si4=si4 < si5;
l8=si4;
si2=si4?si2:si3;
si0=f79(instance,si0,si1,si2);
si0=l8;
si0=!(si0);
if(si0){
L89:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l7;
si1=112U;
si0+=si1;
si1=256U;
si2=l0;
f65(instance,si0,si1,si2);
}
L90:;
si0=l6;
si1=256U;
si0-=si1;
l6=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L89;
}
}
}
L88:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L87;
}
si0=l7;
si1=112U;
si0+=si1;
si1=l6;
si2=l0;
f65(instance,si0,si1,si2);
L87:;
si0=l5;
si0=!(si0);
if(si0){
goto L91;
}
si0=0U;
l6=si0;
L92:;
W2C2_LOOP_START
{
si0=l10;
si0=i32_load(i->m0,si0);
l8=si0;
si0=!(si0);
if(si0){
goto L91;
}
si0=l7;
si1=4U;
si0+=si1;
si1=l8;
si0=f71(instance,si0,si1);
l8=si0;
si1=l6;
si0+=si1;
l6=si0;
si1=l5;
si0=si0 > si1;
if(si0){
goto L91;
}
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l7;
si1=4U;
si0+=si1;
si1=l8;
si2=l0;
f65(instance,si0,si1,si2);
}
L93:;
si0=l10;
si1=4U;
si0+=si1;
l10=si0;
si0=l5;
si1=l6;
si0=si0 > si1;
if(si0){
goto L92;
}
}
L91:;
si0=l11;
si1=8192U;
si0=si0 != si1;
if(si0){
goto L94;
}
si0=l5;
si1=l14;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L94;
}
si0=l7;
si1=112U;
si0+=si1;
si1=32U;
si2=l14;
si3=l5;
si2-=si3;
l6=si2;
si3=256U;
si4=l6;
si5=256U;
si4=si4 < si5;
l8=si4;
si2=si4?si2:si3;
si0=f79(instance,si0,si1,si2);
si0=l8;
si0=!(si0);
if(si0){
L96:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l7;
si1=112U;
si0+=si1;
si1=256U;
si2=l0;
f65(instance,si0,si1,si2);
}
L97:;
si0=l6;
si1=256U;
si0-=si1;
l6=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L96;
}
}
}
L95:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L94;
}
si0=l7;
si1=112U;
si0+=si1;
si1=l6;
si2=l0;
f65(instance,si0,si1,si2);
L94:;
si0=l14;
si1=l5;
si2=l5;
si3=l14;
si2=(U32)((I32)si2<(I32)si3);
si0=si2?si0:si1;
l5=si0;
goto L3;
L49:;
si0=l12;
si1=l9;
si2=0U;
si1=(U32)((I32)si1<(I32)si2);
l5=si1;
si0&=si1;
if(si0){
goto L4;
}
si0=l7;
sd0=f64_load(i->m0,(WasmMemoryAddress)si0+56U);
l16=sd0;
si0=l7;
si1=0U;
i32_store(i->m0,(WasmMemoryAddress)si0+108U,si1);
sd0=l16;
sj0=i64_reinterpret_f64(sd0);
sj1=W2C2_LL(0U);
si0=(U64)((I64)sj0<(I64)sj1);
if(si0){
sd0=l16;
sd0=-(sd0);
l16=sd0;
si0=1U;
l25=si0;
si0=1040U;
l31=si0;
si0=0U;
goto L98;
}
L99:;
si0=l19;
si1=2048U;
si0&=si1;
if(si0){
si0=1U;
l25=si0;
si0=1043U;
l31=si0;
si0=0U;
goto L98;
}
L100:;
si0=1046U;
si1=1041U;
si2=l19;
si3=1U;
si2&=si3;
l25=si2;
si0=si2?si0:si1;
l31=si0;
si0=l25;
si0=!(si0);
L98:;
l33=si0;
sd0=l16;
sd1=f64_reinterpret_i64(W2C2_LL(0x7FF0000000000000U));
si0=sd0 < sd1;
si0=!(si0);
if(si0){
si0=l25;
si1=3U;
si0+=si1;
l6=si0;
si0=l19;
si1=8192U;
si0&=si1;
if(si0){
goto L102;
}
si0=l6;
si1=l14;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L102;
}
si0=l7;
si1=624U;
si0+=si1;
si1=32U;
si2=l14;
si3=l6;
si2-=si3;
l5=si2;
si3=256U;
si4=l5;
si5=256U;
si4=si4 < si5;
l8=si4;
si2=si4?si2:si3;
si0=f79(instance,si0,si1,si2);
si0=l8;
si0=!(si0);
if(si0){
L104:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l7;
si1=624U;
si0+=si1;
si1=256U;
si2=l0;
f65(instance,si0,si1,si2);
}
L105:;
si0=l5;
si1=256U;
si0-=si1;
l5=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L104;
}
}
}
L103:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L102;
}
si0=l7;
si1=624U;
si0+=si1;
si1=l5;
si2=l0;
f65(instance,si0,si1,si2);
L102:;
si0=l0;
si0=i32_load(i->m0,si0);
l5=si0;
si1=32U;
si0&=si1;
if(si0){
si0=l5;
}else{
si0=l31;
si1=l25;
si2=l0;
f65(instance,si0,si1,si2);
si0=l0;
si0=i32_load(i->m0,si0);
}
L106:;
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=1094U;
si1=1242U;
si2=l21;
si3=32U;
si2&=si3;
l5=si2;
si0=si2?si0:si1;
si1=1116U;
si2=1246U;
si3=l5;
si1=si3?si1:si2;
sd2=l16;
sd3=l16;
si2=sd2 != sd3;
si0=si2?si0:si1;
si1=3U;
si2=l0;
f65(instance,si0,si1,si2);
}
L107:;
si0=l19;
si1=73728U;
si0&=si1;
si1=8192U;
si0=si0 != si1;
if(si0){
goto L108;
}
si0=l6;
si1=l14;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L108;
}
si0=l7;
si1=624U;
si0+=si1;
si1=32U;
si2=l14;
si3=l6;
si2-=si3;
l5=si2;
si3=256U;
si4=l5;
si5=256U;
si4=si4 < si5;
l8=si4;
si2=si4?si2:si3;
si0=f79(instance,si0,si1,si2);
si0=l8;
si0=!(si0);
if(si0){
L110:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l7;
si1=624U;
si0+=si1;
si1=256U;
si2=l0;
f65(instance,si0,si1,si2);
}
L111:;
si0=l5;
si1=256U;
si0-=si1;
l5=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L110;
}
}
}
L109:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L108;
}
si0=l7;
si1=624U;
si0+=si1;
si1=l5;
si2=l0;
f65(instance,si0,si1,si2);
L108:;
si0=l6;
si1=l14;
si2=l6;
si3=l14;
si2=(U32)((I32)si2>(I32)si3);
si0=si2?si0:si1;
l5=si0;
goto L3;
}
L101:;
sd0=l16;
si1=l7;
si2=108U;
si1+=si2;
sd0=f72(instance,sd0,si1);
l16=sd0;
sd1=l16;
sd0+=sd1;
l16=sd0;
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
si0=sd0 != sd1;
if(si0){
si0=l7;
si1=l7;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+108U);
l6=si1;
si2=1U;
si1-=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+108U,si1);
si0=l21;
si1=32U;
si0|=si1;
l26=si0;
si1=97U;
si0=si0 != si1;
if(si0){
goto L113;
}
goto L6;
}
L114:;
si0=l21;
si1=32U;
si0|=si1;
l26=si0;
si1=97U;
si0=si0 == si1;
if(si0){
goto L6;
}
si0=l7;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+108U);
l10=si0;
si0=6U;
si1=l9;
si2=l5;
si0=si2?si0:si1;
goto L112;
L113:;
si0=l7;
si1=l6;
si2=29U;
si1-=si2;
l10=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+108U,si1);
sd0=l16;
sd1=f64_reinterpret_i64(W2C2_LL(0x41B0000000000000U));
sd0*=sd1;
l16=sd0;
si0=6U;
si1=l9;
si2=l5;
si0=si2?si0:si1;
L112:;
l12=si0;
si0=l7;
si1=112U;
si0+=si1;
si1=0U;
si2=288U;
si3=l10;
si4=0U;
si3=(U32)((I32)si3<(I32)si4);
l34=si3;
si1=si3?si1:si2;
si0+=si1;
l17=si0;
l6=si0;
L115:;
W2C2_LOOP_START
{
si0=l6;
sd1=l16;
sd2=f64_reinterpret_i64(W2C2_LL(0x41F0000000000000U));
si1=sd1 < sd2;
sd2=l16;
sd3=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
si2=sd2 >= sd3;
si1&=si2;
if(si1){
sd1=l16;
si1=I32_TRUNC_U_F64(sd1);
goto L116;
}
L117:;
si1=0U;
L116:;
l5=si1;
i32_store(i->m0,si0,si1);
si0=l6;
si1=4U;
si0+=si1;
l6=si0;
sd0=l16;
si1=l5;
sd1=(F64)(si1);
sd0-=sd1;
sd1=f64_reinterpret_i64(W2C2_LL(0x41CDCD6500000000U));
sd0*=sd1;
l16=sd0;
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
si0=sd0 != sd1;
if(si0){
goto L115;
}
}
si0=l10;
si1=0U;
si0=(U32)((I32)si0<=(I32)si1);
if(si0){
si0=l6;
l5=si0;
si0=l17;
l8=si0;
goto L118;
}
L119:;
si0=l17;
l8=si0;
L120:;
W2C2_LOOP_START
{
si0=l10;
si1=29U;
si2=l10;
si3=29U;
si2=si2 < si3;
si0=si2?si0:si1;
l10=si0;
si0=l6;
si1=4U;
si0-=si1;
l5=si0;
si1=l8;
si0=si0 < si1;
if(si0){
goto L121;
}
si0=l10;
sj0=(U64)(si0);
l42=sj0;
sj0=W2C2_LL(0U);
l18=sj0;
L122:;
W2C2_LOOP_START
{
si0=l5;
sj1=l18;
sj2=W2C2_LL(4294967295U);
sj1&=sj2;
si2=l5;
sj2=i64_load32_u(i->m0,si2);
sj3=l42;
sj2<<=(sj3&63);
sj1+=sj2;
l28=sj1;
sj2=l28;
sj3=W2C2_LL(1000000000U);
sj2=DIV_U(sj2,sj3);
l18=sj2;
sj3=W2C2_LL(1000000000U);
sj2*=sj3;
sj1-=sj2;
i64_store32(i->m0,si0,sj1);
si0=l5;
si1=4U;
si0-=si1;
l5=si0;
si1=l8;
si0=si0 >= si1;
if(si0){
goto L122;
}
}
sj0=l28;
sj1=W2C2_LL(1000000000U);
si0=sj0 < sj1;
if(si0){
goto L121;
}
si0=l8;
si1=4U;
si0-=si1;
l8=si0;
sj1=l18;
i64_store32(i->m0,si0,sj1);
L121:;
L123:;
W2C2_LOOP_START
{
si0=l8;
si1=l6;
l5=si1;
si0=si0 < si1;
if(si0){
si0=l5;
si1=4U;
si0-=si1;
l6=si0;
si0=i32_load(i->m0,si0);
si0=!(si0);
if(si0){
goto L123;
}
}
L124:;
}
si0=l7;
si1=l7;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+108U);
si2=l10;
si1-=si2;
l10=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+108U,si1);
si0=l5;
l6=si0;
si0=l10;
si1=0U;
si0=(U32)((I32)si0>(I32)si1);
if(si0){
goto L120;
}
}
L118:;
si0=l10;
si1=0U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
si0=l12;
si1=25U;
si0+=si1;
si1=9U;
si0=DIV_U(si0,si1);
si1=1U;
si0+=si1;
l22=si0;
si0=l26;
si1=102U;
si0=si0 == si1;
l29=si0;
L126:;
W2C2_LOOP_START
{
si0=0U;
si1=l10;
si0-=si1;
l6=si0;
si1=9U;
si2=l6;
si3=9U;
si2=si2 < si3;
si0=si2?si0:si1;
l9=si0;
si0=l5;
si1=l8;
si0=si0 <= si1;
if(si0){
si0=l8;
si0=i32_load(i->m0,si0);
si0=!(si0);
si1=2U;
si0<<=(si1&31);
l6=si0;
goto L127;
}
L128:;
si0=1000000000U;
si1=l9;
si0>>=(si1&31);
l15=si0;
si0=-1U;
si1=l9;
si0<<=(si1&31);
si1=-1U;
si0^=si1;
l13=si0;
si0=0U;
l10=si0;
si0=l8;
l6=si0;
L129:;
W2C2_LOOP_START
{
si0=l6;
si1=l6;
si1=i32_load(i->m0,si1);
l11=si1;
si2=l9;
si1>>=(si2&31);
si2=l10;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l11;
si1=l13;
si0&=si1;
si1=l15;
si0*=si1;
l10=si0;
si0=l6;
si1=4U;
si0+=si1;
l6=si0;
si1=l5;
si0=si0 < si1;
if(si0){
goto L129;
}
}
si0=l8;
si0=i32_load(i->m0,si0);
si0=!(si0);
si1=2U;
si0<<=(si1&31);
l6=si0;
si0=l10;
si0=!(si0);
if(si0){
goto L127;
}
si0=l5;
si1=l10;
i32_store(i->m0,si0,si1);
si0=l5;
si1=4U;
si0+=si1;
l5=si0;
L127:;
si0=l7;
si1=l7;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+108U);
si2=l9;
si1+=si2;
l10=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+108U,si1);
si0=l17;
si1=l6;
si2=l8;
si1+=si2;
l8=si1;
si2=l29;
si0=si2?si0:si1;
l6=si0;
si1=l22;
si2=2U;
si1<<=(si2&31);
si0+=si1;
si1=l5;
si2=l5;
si3=l6;
si2-=si3;
si3=2U;
si2=(U32)((I32)si2>>(si3&31));
si3=l22;
si2=(U32)((I32)si2>(I32)si3);
si0=si2?si0:si1;
l5=si0;
si0=l10;
si1=0U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
goto L126;
}
}
}
L125:;
si0=0U;
l11=si0;
si0=l5;
si1=l8;
si0=si0 <= si1;
if(si0){
goto L130;
}
si0=l17;
si1=l8;
si0-=si1;
si1=2U;
si0=(U32)((I32)si0>>(si1&31));
si1=9U;
si0*=si1;
l11=si0;
si0=l8;
si0=i32_load(i->m0,si0);
l10=si0;
si1=10U;
si0=si0 < si1;
if(si0){
goto L130;
}
si0=10U;
l6=si0;
L131:;
W2C2_LOOP_START
{
si0=l11;
si1=1U;
si0+=si1;
l11=si0;
si0=l10;
si1=l6;
si2=10U;
si1*=si2;
l6=si1;
si0=si0 >= si1;
if(si0){
goto L131;
}
}
L130:;
si0=l12;
si1=0U;
si2=l11;
si3=l26;
si4=102U;
si3=si3 == si4;
si1=si3?si1:si2;
si0-=si1;
si1=l26;
si2=103U;
si1=si1 == si2;
l13=si1;
si2=l12;
si3=0U;
si2=si2 != si3;
si1&=si2;
si0-=si1;
l6=si0;
si1=l5;
si2=l17;
si1-=si2;
si2=2U;
si1=(U32)((I32)si1>>(si2&31));
si2=9U;
si1*=si2;
si2=9U;
si1-=si2;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
si0=-4092U;
si1=-3804U;
si2=l34;
si0=si2?si0:si1;
l26=si0;
si1=l7;
si2=112U;
si1+=si2;
si0+=si1;
si1=l6;
si2=9216U;
si1+=si2;
l10=si1;
si2=9U;
si1=I32_DIV_S(si1,si2);
l9=si1;
si2=2U;
si1<<=(si2&31);
l34=si1;
si0+=si1;
l15=si0;
si0=10U;
l6=si0;
si0=l10;
si1=l9;
si2=9U;
si1*=si2;
si0-=si1;
l9=si0;
si1=7U;
si0=(U32)((I32)si0>(I32)si1);
if(si0){
goto L133;
}
si0=8U;
si1=l9;
si0-=si1;
l22=si0;
si1=7U;
si0&=si1;
l10=si0;
si0=l9;
si1=1U;
si0-=si1;
si1=7U;
si0=si0 >= si1;
if(si0){
si0=l22;
si1=-8U;
si0&=si1;
l9=si0;
L135:;
W2C2_LOOP_START
{
si0=l6;
si1=100000000U;
si0*=si1;
l6=si0;
si0=l9;
si1=8U;
si0-=si1;
l9=si0;
if(si0){
goto L135;
}
}
}
L134:;
si0=l10;
si0=!(si0);
if(si0){
goto L133;
}
L136:;
W2C2_LOOP_START
{
si0=l6;
si1=10U;
si0*=si1;
l6=si0;
si0=l10;
si1=1U;
si0-=si1;
l10=si0;
if(si0){
goto L136;
}
}
L133:;
si0=l15;
si0=i32_load(i->m0,si0);
l10=si0;
si1=l10;
si2=l6;
si1=DIV_U(si1,si2);
l29=si1;
si2=l6;
si1*=si2;
si0-=si1;
l9=si0;
si0=!(si0);
si1=l15;
si2=4U;
si1+=si2;
l22=si1;
si2=l5;
si1=si1 == si2;
si0&=si1;
if(si0){
goto L137;
}
si0=l29;
si1=1U;
si0&=si1;
si0=!(si0);
if(si0){
sd0=f64_reinterpret_i64(W2C2_LL(0x4340000000000000U));
l16=sd0;
si0=l6;
si1=1000000000U;
si0=si0 != si1;
if(si0){
goto L138;
}
si0=l8;
si1=l15;
si0=si0 >= si1;
if(si0){
goto L138;
}
si0=l15;
si1=4U;
si0-=si1;
si0=i32_load8_u(i->m0,si0);
si1=1U;
si0&=si1;
si0=!(si0);
if(si0){
goto L138;
}
}
L139:;
sd0=f64_reinterpret_i64(W2C2_LL(0x4340000000000001U));
l16=sd0;
L138:;
sd0=f64_reinterpret_i64(W2C2_LL(0x3FE0000000000000U));
sd1=f64_reinterpret_i64(W2C2_LL(0x3FF0000000000000U));
sd2=f64_reinterpret_i64(W2C2_LL(0x3FF8000000000000U));
si3=l5;
si4=l22;
si3=si3 == si4;
sd1=si3?sd1:sd2;
sd2=f64_reinterpret_i64(W2C2_LL(0x3FF8000000000000U));
si3=l9;
si4=l6;
si5=1U;
si4>>=(si5&31);
l22=si4;
si3=si3 == si4;
sd1=si3?sd1:sd2;
si2=l9;
si3=l22;
si2=si2 < si3;
sd0=si2?sd0:sd1;
l23=sd0;
si0=l33;
if(si0){
goto L140;
}
si0=l31;
si0=i32_load8_u(i->m0,si0);
si1=45U;
si0=si0 != si1;
if(si0){
goto L140;
}
sd0=l23;
sd0=-(sd0);
l23=sd0;
sd0=l16;
sd0=-(sd0);
l16=sd0;
L140:;
si0=l15;
si1=l10;
si2=l9;
si1-=si2;
l10=si1;
i32_store(i->m0,si0,si1);
sd0=l16;
sd1=l23;
sd0+=sd1;
sd1=l16;
si0=sd0 == sd1;
if(si0){
goto L137;
}
si0=l15;
si1=l6;
si2=l10;
si1+=si2;
l6=si1;
i32_store(i->m0,si0,si1);
si0=l6;
si1=1000000000U;
si0=si0 >= si1;
if(si0){
si0=l37;
si1=l26;
si2=l34;
si1+=si2;
si0+=si1;
l6=si0;
L142:;
W2C2_LOOP_START
{
si0=l6;
si1=4U;
si0+=si1;
si1=0U;
i32_store(i->m0,si0,si1);
si0=l6;
si1=l8;
si0=si0 < si1;
if(si0){
si0=l8;
si1=4U;
si0-=si1;
l8=si0;
si1=0U;
i32_store(i->m0,si0,si1);
}
L143:;
si0=l6;
si1=l6;
si1=i32_load(i->m0,si1);
si2=1U;
si1+=si2;
l10=si1;
i32_store(i->m0,si0,si1);
si0=l6;
si1=4U;
si0-=si1;
l6=si0;
si0=l10;
si1=999999999U;
si0=si0 > si1;
if(si0){
goto L142;
}
}
si0=l6;
si1=4U;
si0+=si1;
l15=si0;
}
L141:;
si0=l17;
si1=l8;
si0-=si1;
si1=2U;
si0=(U32)((I32)si0>>(si1&31));
si1=9U;
si0*=si1;
l11=si0;
si0=l8;
si0=i32_load(i->m0,si0);
l10=si0;
si1=10U;
si0=si0 < si1;
if(si0){
goto L137;
}
si0=10U;
l6=si0;
L144:;
W2C2_LOOP_START
{
si0=l11;
si1=1U;
si0+=si1;
l11=si0;
si0=l10;
si1=l6;
si2=10U;
si1*=si2;
l6=si1;
si0=si0 >= si1;
if(si0){
goto L144;
}
}
L137:;
si0=l15;
si1=4U;
si0+=si1;
l6=si0;
si1=l5;
si2=l5;
si3=l6;
si2=si2 > si3;
si0=si2?si0:si1;
l5=si0;
}
L132:;
si0=l5;
si1=l17;
si0-=si1;
l6=si0;
L145:;
W2C2_LOOP_START
{
si0=l6;
l10=si0;
si0=l5;
l15=si0;
si1=l8;
si0=si0 <= si1;
l9=si0;
if(si0){
goto L146;
}
si0=l10;
si1=4U;
si0-=si1;
l6=si0;
si0=l15;
si1=4U;
si0-=si1;
l5=si0;
si0=i32_load(i->m0,si0);
si0=!(si0);
if(si0){
goto L145;
}
L146:;
}
si0=l13;
si0=!(si0);
if(si0){
si0=l19;
si1=8U;
si0&=si1;
l22=si0;
goto L147;
}
L148:;
si0=l11;
si1=-1U;
si0^=si1;
si1=-1U;
si2=l12;
si3=1U;
si4=l12;
si2=si4?si2:si3;
l5=si2;
si3=l11;
si2=(U32)((I32)si2>(I32)si3);
si3=l11;
si4=-5U;
si3=(U32)((I32)si3>(I32)si4);
si2&=si3;
l6=si2;
si0=si2?si0:si1;
si1=l5;
si0+=si1;
l12=si0;
si0=-1U;
si1=-2U;
si2=l6;
si0=si2?si0:si1;
si1=l21;
si0+=si1;
l21=si0;
si0=l19;
si1=8U;
si0&=si1;
l22=si0;
if(si0){
goto L147;
}
si0=-9U;
l5=si0;
si0=l9;
if(si0){
goto L149;
}
si0=l15;
si1=4U;
si0-=si1;
si0=i32_load(i->m0,si0);
l9=si0;
si0=!(si0);
if(si0){
goto L149;
}
si0=0U;
l5=si0;
si0=l9;
si1=10U;
si0=REM_U(si0,si1);
if(si0){
goto L149;
}
si0=10U;
l6=si0;
L150:;
W2C2_LOOP_START
{
si0=l5;
si1=1U;
si0-=si1;
l5=si0;
si0=l9;
si1=l6;
si2=10U;
si1*=si2;
l6=si1;
si0=REM_U(si0,si1);
si0=!(si0);
if(si0){
goto L150;
}
}
L149:;
si0=l10;
si1=2U;
si0=(U32)((I32)si0>>(si1&31));
si1=9U;
si0*=si1;
l6=si0;
si0=l21;
si1=-33U;
si0&=si1;
si1=70U;
si0=si0 == si1;
if(si0){
si0=0U;
l22=si0;
si0=l12;
si1=l5;
si2=l6;
si1+=si2;
si2=9U;
si1-=si2;
l5=si1;
si2=0U;
si3=l5;
si4=0U;
si3=(U32)((I32)si3>(I32)si4);
si1=si3?si1:si2;
l5=si1;
si2=l5;
si3=l12;
si2=(U32)((I32)si2>(I32)si3);
si0=si2?si0:si1;
l12=si0;
goto L147;
}
L151:;
si0=0U;
l22=si0;
si0=l12;
si1=l6;
si2=l11;
si1+=si2;
si2=l5;
si1+=si2;
si2=9U;
si1-=si2;
l5=si1;
si2=0U;
si3=l5;
si4=0U;
si3=(U32)((I32)si3>(I32)si4);
si1=si3?si1:si2;
l5=si1;
si2=l5;
si3=l12;
si2=(U32)((I32)si2>(I32)si3);
si0=si2?si0:si1;
l12=si0;
L147:;
si0=l12;
si1=2147483645U;
si2=2147483646U;
si3=l12;
si4=l22;
si3|=si4;
l26=si3;
si1=si3?si1:si2;
si0=(U32)((I32)si0>(I32)si1);
if(si0){
goto L4;
}
si0=l12;
si1=l26;
si2=0U;
si1=si1 != si2;
si0+=si1;
si1=1U;
si0+=si1;
l29=si0;
si0=l21;
si1=-33U;
si0&=si1;
si1=70U;
si0=si0 != si1;
l34=si0;
si0=!(si0);
if(si0){
si0=l11;
si1=l29;
si2=2147483647U;
si1^=si2;
si0=(U32)((I32)si0>(I32)si1);
if(si0){
goto L4;
}
si0=l11;
si1=0U;
si2=l11;
si3=0U;
si2=(U32)((I32)si2>(I32)si3);
si0=si2?si0:si1;
l5=si0;
goto L152;
}
L153:;
si0=l11;
si0=!(si0);
if(si0){
si0=l27;
l10=si0;
l6=si0;
goto L154;
}
L155:;
si0=l11;
si1=l11;
si2=31U;
si1=(U32)((I32)si1>>(si2&31));
l5=si1;
si0^=si1;
si1=l5;
si0-=si1;
l5=si0;
si0=l27;
l10=si0;
l6=si0;
L156:;
W2C2_LOOP_START
{
si0=l6;
si1=1U;
si0-=si1;
l6=si0;
si1=l5;
si2=l5;
si3=10U;
si2=DIV_U(si2,si3);
l9=si2;
si3=10U;
si2*=si3;
si1-=si2;
si2=48U;
si1|=si2;
i32_store8(i->m0,si0,si1);
si0=l10;
si1=1U;
si0-=si1;
l10=si0;
si0=l5;
si1=9U;
si0=si0 > si1;
l13=si0;
si0=l9;
l5=si0;
si0=l13;
if(si0){
goto L156;
}
}
L154:;
si0=l27;
si1=l10;
si0-=si1;
si1=1U;
si0=(U32)((I32)si0<=(I32)si1);
if(si0){
si0=l6;
si1=l41;
si2=l10;
si1-=si2;
si0+=si1;
l6=si0;
si1=48U;
si2=l10;
si3=l7;
si2-=si3;
si3=78U;
si2-=si3;
si0=f79(instance,si0,si1,si2);
}
L157:;
si0=l6;
si1=2U;
si0-=si1;
l33=si0;
si1=l21;
i32_store8(i->m0,si0,si1);
si0=l6;
si1=1U;
si0-=si1;
si1=45U;
si2=43U;
si3=l11;
si4=0U;
si3=(U32)((I32)si3<(I32)si4);
si1=si3?si1:si2;
i32_store8(i->m0,si0,si1);
si0=l27;
si1=l33;
si0-=si1;
l5=si0;
si1=l29;
si2=2147483647U;
si1^=si2;
si0=(U32)((I32)si0>(I32)si1);
if(si0){
goto L4;
}
L152:;
si0=l5;
si1=l29;
si0+=si1;
l5=si0;
si1=l25;
si2=2147483647U;
si1^=si2;
si0=(U32)((I32)si0>(I32)si1);
if(si0){
goto L4;
}
si0=l5;
si1=l25;
si0+=si1;
l13=si0;
si0=l19;
si1=73728U;
si0&=si1;
l19=si0;
if(si0){
goto L158;
}
si0=l13;
si1=l14;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L158;
}
si0=l7;
si1=624U;
si0+=si1;
si1=32U;
si2=l14;
si3=l13;
si2-=si3;
l5=si2;
si3=256U;
si4=l5;
si5=256U;
si4=si4 < si5;
l6=si4;
si2=si4?si2:si3;
si0=f79(instance,si0,si1,si2);
si0=l6;
si0=!(si0);
if(si0){
L160:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l7;
si1=624U;
si0+=si1;
si1=256U;
si2=l0;
f65(instance,si0,si1,si2);
}
L161:;
si0=l5;
si1=256U;
si0-=si1;
l5=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L160;
}
}
}
L159:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L158;
}
si0=l7;
si1=624U;
si0+=si1;
si1=l5;
si2=l0;
f65(instance,si0,si1,si2);
L158:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l31;
si1=l25;
si2=l0;
f65(instance,si0,si1,si2);
}
L162:;
si0=l19;
si1=65536U;
si0=si0 != si1;
if(si0){
goto L163;
}
si0=l13;
si1=l14;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L163;
}
si0=l7;
si1=624U;
si0+=si1;
si1=48U;
si2=l14;
si3=l13;
si2-=si3;
l5=si2;
si3=256U;
si4=l5;
si5=256U;
si4=si4 < si5;
l6=si4;
si2=si4?si2:si3;
si0=f79(instance,si0,si1,si2);
si0=l6;
si0=!(si0);
if(si0){
L165:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l7;
si1=624U;
si0+=si1;
si1=256U;
si2=l0;
f65(instance,si0,si1,si2);
}
L166:;
si0=l5;
si1=256U;
si0-=si1;
l5=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L165;
}
}
}
L164:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L163;
}
si0=l7;
si1=624U;
si0+=si1;
si1=l5;
si2=l0;
f65(instance,si0,si1,si2);
L163:;
si0=l34;
if(si0){
goto L8;
}
si0=l17;
si1=l8;
si2=l8;
si3=l17;
si2=si2 > si3;
si0=si2?si0:si1;
l11=si0;
l9=si0;
L167:;
W2C2_LOOP_START
{
si0=l9;
si0=i32_load(i->m0,si0);
l5=si0;
if(si0){
si0=8U;
l6=si0;
L172:;
W2C2_LOOP_START
{
si0=l7;
si1=80U;
si0+=si1;
si1=l6;
si0+=si1;
si1=l5;
si2=l5;
si3=10U;
si2=DIV_U(si2,si3);
l8=si2;
si3=10U;
si2*=si3;
si1-=si2;
si2=48U;
si1|=si2;
i32_store8(i->m0,si0,si1);
si0=l6;
si1=1U;
si0-=si1;
l6=si0;
si0=l5;
si1=9U;
si0=si0 > si1;
l10=si0;
si0=l8;
l5=si0;
si0=l10;
if(si0){
goto L172;
}
}
si0=l6;
si1=1U;
si0+=si1;
l8=si0;
si1=l7;
si2=80U;
si1+=si2;
si0+=si1;
l5=si0;
si0=l9;
si1=l11;
si0=si0 != si1;
if(si0){
si0=l6;
si1=2U;
si0+=si1;
si1=2U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
goto L168;
}
goto L169;
}
L173:;
si0=l6;
si1=8U;
si0=si0 != si1;
if(si0){
goto L168;
}
goto L170;
}
L171:;
si0=9U;
l8=si0;
si0=l9;
si1=l11;
si0=si0 != si1;
if(si0){
goto L169;
}
L170:;
si0=l7;
si1=48U;
i32_store8(i->m0,(WasmMemoryAddress)si0+88U,si1);
si0=l36;
l5=si0;
goto L168;
L169:;
si0=l7;
si1=80U;
si0+=si1;
si1=l8;
si2=l40;
si1+=si2;
l5=si1;
si2=l7;
si3=80U;
si2+=si3;
si3=l5;
si2=si2 < si3;
si0=si2?si0:si1;
l5=si0;
si1=48U;
si2=l8;
si3=l7;
si4=80U;
si3+=si4;
si2+=si3;
si3=l5;
si2-=si3;
si0=f79(instance,si0,si1,si2);
L168:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l5;
si1=l32;
si2=l5;
si1-=si2;
si2=l0;
f65(instance,si0,si1,si2);
}
L174:;
si0=l9;
si1=4U;
si0+=si1;
l9=si0;
si1=l17;
si0=si0 <= si1;
if(si0){
goto L167;
}
}
si0=l26;
si0=!(si0);
if(si0){
goto L175;
}
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L175;
}
si0=1259U;
si1=1U;
si2=l0;
f65(instance,si0,si1,si2);
L175:;
si0=l12;
si1=0U;
si0=(U32)((I32)si0<=(I32)si1);
if(si0){
si0=l12;
l5=si0;
goto L176;
}
L177:;
si0=l9;
si1=l15;
si0=si0 >= si1;
if(si0){
si0=l12;
l5=si0;
goto L176;
}
L178:;
L179:;
W2C2_LOOP_START
{
si0=l9;
si0=i32_load(i->m0,si0);
l5=si0;
si0=!(si0);
if(si0){
si0=l32;
l6=si0;
l8=si0;
goto L181;
}
L182:;
si0=l32;
l8=si0;
l6=si0;
L183:;
W2C2_LOOP_START
{
si0=l6;
si1=1U;
si0-=si1;
l6=si0;
si1=l5;
si2=l5;
si3=10U;
si2=DIV_U(si2,si3);
l10=si2;
si3=10U;
si2*=si3;
si1-=si2;
si2=48U;
si1|=si2;
i32_store8(i->m0,si0,si1);
si0=l8;
si1=1U;
si0-=si1;
l8=si0;
si0=l5;
si1=9U;
si0=si0 > si1;
l11=si0;
si0=l10;
l5=si0;
si0=l11;
if(si0){
goto L183;
}
}
si0=l6;
si1=l7;
si2=80U;
si1+=si2;
si0=si0 <= si1;
if(si0){
goto L180;
}
L181:;
si0=l6;
si1=l7;
si2=80U;
si1+=si2;
si0+=si1;
si1=l8;
si0-=si1;
l6=si0;
si1=48U;
si2=l8;
si3=l7;
si4=80U;
si3+=si4;
si2-=si3;
si0=f79(instance,si0,si1,si2);
L180:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l6;
si1=l12;
si2=9U;
si3=l12;
si4=9U;
si3=(U32)((I32)si3<(I32)si4);
si1=si3?si1:si2;
si2=l0;
f65(instance,si0,si1,si2);
}
L184:;
si0=l12;
si1=9U;
si0-=si1;
l5=si0;
si0=l9;
si1=4U;
si0+=si1;
l9=si0;
si1=l15;
si0=si0 >= si1;
if(si0){
goto L176;
}
si0=l12;
si1=9U;
si0=(U32)((I32)si0>(I32)si1);
l6=si0;
si0=l5;
l12=si0;
si0=l6;
if(si0){
goto L179;
}
}
L176:;
si0=l0;
si1=l5;
si2=9U;
si1+=si2;
si2=9U;
f75(instance,si0,si1,si2);
goto L7;
L11:;
si0=4820U;
si1=28U;
i32_store(i->m0,si0,si1);
goto L2;
L10:;
si0=0U;
l13=si0;
si0=1030U;
l17=si0;
si0=l24;
l5=si0;
si0=l19;
l15=si0;
si0=l9;
l11=si0;
L9:;
si0=l11;
si1=l5;
si2=l10;
si1-=si2;
l9=si1;
si2=l9;
si3=l11;
si2=(U32)((I32)si2<(I32)si3);
si0=si2?si0:si1;
l12=si0;
si1=l13;
si2=2147483647U;
si1^=si2;
si0=(U32)((I32)si0>(I32)si1);
if(si0){
goto L4;
}
si0=l14;
si1=l12;
si2=l13;
si1+=si2;
l8=si1;
si2=l8;
si3=l14;
si2=(U32)((I32)si2<(I32)si3);
si0=si2?si0:si1;
l5=si0;
si1=l6;
si0=(U32)((I32)si0>(I32)si1);
if(si0){
goto L4;
}
si0=l15;
si1=73728U;
si0&=si1;
l15=si0;
if(si0){
goto L185;
}
si0=l8;
si1=l14;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L185;
}
si0=l7;
si1=112U;
si0+=si1;
si1=32U;
si2=l5;
si3=l8;
si2-=si3;
l6=si2;
si3=256U;
si4=l6;
si5=256U;
si4=si4 < si5;
l19=si4;
si2=si4?si2:si3;
si0=f79(instance,si0,si1,si2);
si0=l19;
si0=!(si0);
if(si0){
L187:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l7;
si1=112U;
si0+=si1;
si1=256U;
si2=l0;
f65(instance,si0,si1,si2);
}
L188:;
si0=l6;
si1=256U;
si0-=si1;
l6=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L187;
}
}
}
L186:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L185;
}
si0=l7;
si1=112U;
si0+=si1;
si1=l6;
si2=l0;
f65(instance,si0,si1,si2);
L185:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l17;
si1=l13;
si2=l0;
f65(instance,si0,si1,si2);
}
L189:;
si0=l15;
si1=65536U;
si0=si0 != si1;
if(si0){
goto L190;
}
si0=l8;
si1=l14;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L190;
}
si0=l7;
si1=112U;
si0+=si1;
si1=48U;
si2=l5;
si3=l8;
si2-=si3;
l6=si2;
si3=256U;
si4=l6;
si5=256U;
si4=si4 < si5;
l13=si4;
si2=si4?si2:si3;
si0=f79(instance,si0,si1,si2);
si0=l13;
si0=!(si0);
if(si0){
L192:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l7;
si1=112U;
si0+=si1;
si1=256U;
si2=l0;
f65(instance,si0,si1,si2);
}
L193:;
si0=l6;
si1=256U;
si0-=si1;
l6=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L192;
}
}
}
L191:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L190;
}
si0=l7;
si1=112U;
si0+=si1;
si1=l6;
si2=l0;
f65(instance,si0,si1,si2);
L190:;
si0=l9;
si1=l11;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L194;
}
si0=l7;
si1=112U;
si0+=si1;
si1=48U;
si2=l12;
si3=l9;
si2-=si3;
l6=si2;
si3=256U;
si4=l6;
si5=256U;
si4=si4 < si5;
l11=si4;
si2=si4?si2:si3;
si0=f79(instance,si0,si1,si2);
si0=l11;
si0=!(si0);
if(si0){
L196:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l7;
si1=112U;
si0+=si1;
si1=256U;
si2=l0;
f65(instance,si0,si1,si2);
}
L197:;
si0=l6;
si1=256U;
si0-=si1;
l6=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L196;
}
}
}
L195:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L194;
}
si0=l7;
si1=112U;
si0+=si1;
si1=l6;
si2=l0;
f65(instance,si0,si1,si2);
L194:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l10;
si1=l9;
si2=l0;
f65(instance,si0,si1,si2);
}
L198:;
si0=l15;
si1=8192U;
si0=si0 != si1;
if(si0){
goto L3;
}
si0=l8;
si1=l14;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L3;
}
si0=l7;
si1=112U;
si0+=si1;
si1=32U;
si2=l5;
si3=l8;
si2-=si3;
l6=si2;
si3=256U;
si4=l6;
si5=256U;
si4=si4 < si5;
l8=si4;
si2=si4?si2:si3;
si0=f79(instance,si0,si1,si2);
si0=l8;
si0=!(si0);
if(si0){
L200:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l7;
si1=112U;
si0+=si1;
si1=256U;
si2=l0;
f65(instance,si0,si1,si2);
}
L201:;
si0=l6;
si1=256U;
si0-=si1;
l6=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L200;
}
}
}
L199:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L3;
}
si0=l7;
si1=112U;
si0+=si1;
si1=l6;
si2=l0;
f65(instance,si0,si1,si2);
goto L3;
L8:;
si0=l12;
si1=0U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
goto L202;
}
si0=l15;
si1=l8;
si2=4U;
si1+=si2;
si2=l8;
si3=l15;
si2=si2 < si3;
si0=si2?si0:si1;
l15=si0;
si0=l8;
l9=si0;
L203:;
W2C2_LOOP_START
{
si0=l9;
si0=i32_load(i->m0,si0);
l5=si0;
if(si0){
si0=l32;
l6=si0;
L206:;
W2C2_LOOP_START
{
si0=l6;
si1=1U;
si0-=si1;
l6=si0;
si1=l5;
si2=l5;
si3=10U;
si2=DIV_U(si2,si3);
l10=si2;
si3=10U;
si2*=si3;
si1-=si2;
si2=48U;
si1|=si2;
i32_store8(i->m0,si0,si1);
si0=l5;
si1=10U;
si0=si0 < si1;
l11=si0;
si0=l10;
l5=si0;
si0=l11;
si0=!(si0);
if(si0){
goto L206;
}
}
goto L204;
}
L205:;
si0=l7;
si1=48U;
i32_store8(i->m0,(WasmMemoryAddress)si0+88U,si1);
si0=l36;
l6=si0;
L204:;
si0=l8;
si1=l9;
si0=si0 != si1;
if(si0){
si0=l6;
si1=l7;
si2=80U;
si1+=si2;
si0=si0 <= si1;
if(si0){
goto L207;
}
si0=l7;
si1=80U;
si0+=si1;
si1=48U;
si2=l6;
si3=l7;
si4=80U;
si3+=si4;
si2-=si3;
si0=f79(instance,si0,si1,si2);
si0=l7;
si1=80U;
si0+=si1;
l6=si0;
goto L207;
}
L208:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l6;
si1=1U;
si2=l0;
f65(instance,si0,si1,si2);
}
L209:;
si0=l6;
si1=1U;
si0+=si1;
l6=si0;
si0=l22;
si0=!(si0);
si1=l12;
si2=0U;
si1=(U32)((I32)si1<=(I32)si2);
si0&=si1;
if(si0){
goto L207;
}
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L207;
}
si0=1259U;
si1=1U;
si2=l0;
f65(instance,si0,si1,si2);
L207:;
si0=l32;
si1=l6;
si0-=si1;
l5=si0;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l6;
si1=l5;
si2=l12;
si3=l5;
si4=l12;
si3=(U32)((I32)si3<(I32)si4);
si1=si3?si1:si2;
si2=l0;
f65(instance,si0,si1,si2);
}
L210:;
si0=l12;
si1=l5;
si0-=si1;
l12=si0;
si0=l9;
si1=4U;
si0+=si1;
l9=si0;
si1=l15;
si0=si0 >= si1;
if(si0){
goto L202;
}
si0=l12;
si1=0U;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L203;
}
}
L202:;
si0=l0;
si1=l12;
si2=18U;
si1+=si2;
si2=18U;
f75(instance,si0,si1,si2);
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L7;
}
si0=l33;
si1=l27;
si2=l33;
si1-=si2;
si2=l0;
f65(instance,si0,si1,si2);
L7:;
si0=l19;
si1=8192U;
si0=si0 != si1;
if(si0){
goto L5;
}
si0=l13;
si1=l14;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L5;
}
si0=l7;
si1=624U;
si0+=si1;
si1=32U;
si2=l14;
si3=l13;
si2-=si3;
l5=si2;
si3=256U;
si4=l5;
si5=256U;
si4=si4 < si5;
l6=si4;
si2=si4?si2:si3;
si0=f79(instance,si0,si1,si2);
si0=l6;
si0=!(si0);
if(si0){
L212:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l7;
si1=624U;
si0+=si1;
si1=256U;
si2=l0;
f65(instance,si0,si1,si2);
}
L213:;
si0=l5;
si1=256U;
si0-=si1;
l5=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L212;
}
}
}
L211:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L5;
}
si0=l7;
si1=624U;
si0+=si1;
si1=l5;
si2=l0;
f65(instance,si0,si1,si2);
goto L5;
L6:;
si0=l31;
si1=l21;
si2=26U;
si1<<=(si2&31);
si2=31U;
si1=(U32)((I32)si1>>(si2&31));
si2=9U;
si1&=si2;
si0+=si1;
l17=si0;
si0=l9;
si1=11U;
si0=si0 > si1;
if(si0){
goto L214;
}
si0=12U;
si1=l9;
si0-=si1;
l5=si0;
si1=7U;
si0&=si1;
l6=si0;
si0=!(si0);
if(si0){
sd0=f64_reinterpret_i64(W2C2_LL(0x4030000000000000U));
l23=sd0;
goto L215;
}
L216:;
si0=l9;
si1=12U;
si0-=si1;
l5=si0;
sd0=f64_reinterpret_i64(W2C2_LL(0x4030000000000000U));
l23=sd0;
L217:;
W2C2_LOOP_START
{
si0=l5;
si1=1U;
si0+=si1;
l5=si0;
sd0=l23;
sd1=f64_reinterpret_i64(W2C2_LL(0x4030000000000000U));
sd0*=sd1;
l23=sd0;
si0=l6;
si1=1U;
si0-=si1;
l6=si0;
if(si0){
goto L217;
}
}
si0=0U;
si1=l5;
si0-=si1;
l5=si0;
L215:;
si0=l9;
si1=5U;
si0-=si1;
si1=7U;
si0=si0 >= si1;
if(si0){
L219:;
W2C2_LOOP_START
{
sd0=l23;
sd1=f64_reinterpret_i64(W2C2_LL(0x4030000000000000U));
sd0*=sd1;
sd1=f64_reinterpret_i64(W2C2_LL(0x4030000000000000U));
sd0*=sd1;
sd1=f64_reinterpret_i64(W2C2_LL(0x4030000000000000U));
sd0*=sd1;
sd1=f64_reinterpret_i64(W2C2_LL(0x4030000000000000U));
sd0*=sd1;
sd1=f64_reinterpret_i64(W2C2_LL(0x4030000000000000U));
sd0*=sd1;
sd1=f64_reinterpret_i64(W2C2_LL(0x4030000000000000U));
sd0*=sd1;
sd1=f64_reinterpret_i64(W2C2_LL(0x4030000000000000U));
sd0*=sd1;
sd1=f64_reinterpret_i64(W2C2_LL(0x4030000000000000U));
sd0*=sd1;
l23=sd0;
si0=l5;
si1=8U;
si0-=si1;
l5=si0;
if(si0){
goto L219;
}
}
}
L218:;
si0=l17;
si0=i32_load8_u(i->m0,si0);
si1=45U;
si0=si0 == si1;
if(si0){
sd0=l23;
sd1=l16;
sd1=-(sd1);
sd2=l23;
sd1-=sd2;
sd0+=sd1;
sd0=-(sd0);
l16=sd0;
goto L214;
}
L220:;
sd0=l16;
sd1=l23;
sd0+=sd1;
sd1=l23;
sd0-=sd1;
l16=sd0;
L214:;
si0=l7;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+108U);
l11=si0;
if(si0){
si0=l11;
si1=l11;
si2=31U;
si1=(U32)((I32)si1>>(si2&31));
l5=si1;
si0^=si1;
si1=l5;
si0-=si1;
l5=si0;
si0=l27;
l6=si0;
L223:;
W2C2_LOOP_START
{
si0=l6;
si1=1U;
si0-=si1;
l6=si0;
si1=l5;
si2=l5;
si3=10U;
si2=DIV_U(si2,si3);
l8=si2;
si3=10U;
si2*=si3;
si1-=si2;
si2=48U;
si1|=si2;
i32_store8(i->m0,si0,si1);
si0=l5;
si1=10U;
si0=si0 < si1;
l10=si0;
si0=l8;
l5=si0;
si0=l10;
si0=!(si0);
if(si0){
goto L223;
}
}
goto L221;
}
L222:;
si0=l7;
si1=48U;
i32_store8(i->m0,(WasmMemoryAddress)si0+79U,si1);
si0=l39;
l6=si0;
L221:;
si0=l25;
si1=2U;
si0|=si1;
l15=si0;
si0=l21;
si1=32U;
si0&=si1;
l8=si0;
si0=l6;
si1=2U;
si0-=si1;
l12=si0;
si1=l21;
si2=15U;
si1+=si2;
i32_store8(i->m0,si0,si1);
si0=l6;
si1=1U;
si0-=si1;
si1=45U;
si2=43U;
si3=l11;
si4=0U;
si3=(U32)((I32)si3<(I32)si4);
si1=si3?si1:si2;
i32_store8(i->m0,si0,si1);
si0=l19;
si1=8U;
si0&=si1;
l10=si0;
si0=l7;
si1=80U;
si0+=si1;
l6=si0;
L224:;
W2C2_LOOP_START
{
si0=l6;
l5=si0;
sd1=l16;
sd1=fabs(sd1);
sd2=f64_reinterpret_i64(W2C2_LL(0x41E0000000000000U));
si1=sd1 < sd2;
if(si1){
sd1=l16;
si1=I32_TRUNC_S_F64(sd1);
goto L225;
}
L226:;
si1=-2147483648U;
L225:;
l6=si1;
si2=3936U;
si1+=si2;
si1=i32_load8_u(i->m0,si1);
si2=l8;
si1|=si2;
i32_store8(i->m0,si0,si1);
sd0=l16;
si1=l6;
sd1=(F64)(I32)(si1);
sd0-=sd1;
sd1=f64_reinterpret_i64(W2C2_LL(0x4030000000000000U));
sd0*=sd1;
l16=sd0;
si0=l5;
si1=1U;
si0+=si1;
l6=si0;
si1=l7;
si2=80U;
si1+=si2;
si0-=si1;
si1=1U;
si0=si0 != si1;
if(si0){
goto L227;
}
si0=l10;
if(si0){
goto L228;
}
si0=l9;
si1=0U;
si0=(U32)((I32)si0>(I32)si1);
if(si0){
goto L228;
}
sd0=l16;
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
si0=sd0 == sd1;
if(si0){
goto L227;
}
L228:;
si0=l5;
si1=46U;
i32_store8(i->m0,(WasmMemoryAddress)si0+1U,si1);
si0=l5;
si1=2U;
si0+=si1;
l6=si0;
L227:;
sd0=l16;
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
si0=sd0 != sd1;
if(si0){
goto L224;
}
}
si0=2147483645U;
si1=l27;
si2=l12;
si1-=si2;
l11=si1;
si2=l15;
si1+=si2;
l5=si1;
si0-=si1;
si1=l9;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
goto L4;
}
si0=l9;
si1=2U;
si0+=si1;
si1=l6;
si2=l7;
si3=80U;
si2+=si3;
si1-=si2;
l6=si1;
si2=l6;
si3=2U;
si2-=si3;
si3=l9;
si2=(U32)((I32)si2<(I32)si3);
si0=si2?si0:si1;
si1=l6;
si2=l9;
si0=si2?si0:si1;
l10=si0;
si1=l5;
si0+=si1;
l13=si0;
si0=l19;
si1=73728U;
si0&=si1;
l8=si0;
if(si0){
goto L229;
}
si0=l13;
si1=l14;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L229;
}
si0=l7;
si1=624U;
si0+=si1;
si1=32U;
si2=l14;
si3=l13;
si2-=si3;
l5=si2;
si3=256U;
si4=l5;
si5=256U;
si4=si4 < si5;
l9=si4;
si2=si4?si2:si3;
si0=f79(instance,si0,si1,si2);
si0=l9;
si0=!(si0);
if(si0){
L231:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l7;
si1=624U;
si0+=si1;
si1=256U;
si2=l0;
f65(instance,si0,si1,si2);
}
L232:;
si0=l5;
si1=256U;
si0-=si1;
l5=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L231;
}
}
}
L230:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L229;
}
si0=l7;
si1=624U;
si0+=si1;
si1=l5;
si2=l0;
f65(instance,si0,si1,si2);
L229:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l17;
si1=l15;
si2=l0;
f65(instance,si0,si1,si2);
}
L233:;
si0=l8;
si1=65536U;
si0=si0 != si1;
if(si0){
goto L234;
}
si0=l13;
si1=l14;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L234;
}
si0=l7;
si1=624U;
si0+=si1;
si1=48U;
si2=l14;
si3=l13;
si2-=si3;
l5=si2;
si3=256U;
si4=l5;
si5=256U;
si4=si4 < si5;
l9=si4;
si2=si4?si2:si3;
si0=f79(instance,si0,si1,si2);
si0=l9;
si0=!(si0);
if(si0){
L236:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l7;
si1=624U;
si0+=si1;
si1=256U;
si2=l0;
f65(instance,si0,si1,si2);
}
L237:;
si0=l5;
si1=256U;
si0-=si1;
l5=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L236;
}
}
}
L235:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L234;
}
si0=l7;
si1=624U;
si0+=si1;
si1=l5;
si2=l0;
f65(instance,si0,si1,si2);
L234:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l7;
si1=80U;
si0+=si1;
si1=l6;
si2=l0;
f65(instance,si0,si1,si2);
}
L238:;
si0=l10;
si1=l6;
si0-=si1;
l5=si0;
si1=0U;
si0=(U32)((I32)si0<=(I32)si1);
if(si0){
goto L239;
}
si0=l7;
si1=624U;
si0+=si1;
si1=48U;
si2=l5;
si3=256U;
si4=l5;
si5=256U;
si4=si4 < si5;
l6=si4;
si2=si4?si2:si3;
si0=f79(instance,si0,si1,si2);
si0=l6;
si0=!(si0);
if(si0){
L241:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l7;
si1=624U;
si0+=si1;
si1=256U;
si2=l0;
f65(instance,si0,si1,si2);
}
L242:;
si0=l5;
si1=256U;
si0-=si1;
l5=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L241;
}
}
}
L240:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L239;
}
si0=l7;
si1=624U;
si0+=si1;
si1=l5;
si2=l0;
f65(instance,si0,si1,si2);
L239:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l12;
si1=l11;
si2=l0;
f65(instance,si0,si1,si2);
}
L243:;
si0=l8;
si1=8192U;
si0=si0 != si1;
if(si0){
goto L5;
}
si0=l13;
si1=l14;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L5;
}
si0=l7;
si1=624U;
si0+=si1;
si1=32U;
si2=l14;
si3=l13;
si2-=si3;
l5=si2;
si3=256U;
si4=l5;
si5=256U;
si4=si4 < si5;
l6=si4;
si2=si4?si2:si3;
si0=f79(instance,si0,si1,si2);
si0=l6;
si0=!(si0);
if(si0){
L245:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l7;
si1=624U;
si0+=si1;
si1=256U;
si2=l0;
f65(instance,si0,si1,si2);
}
L246:;
si0=l5;
si1=256U;
si0-=si1;
l5=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L245;
}
}
}
L244:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L5;
}
si0=l7;
si1=624U;
si0+=si1;
si1=l5;
si2=l0;
f65(instance,si0,si1,si2);
L5:;
si0=l13;
si1=l14;
si2=l13;
si3=l14;
si2=(U32)((I32)si2>(I32)si3);
si0=si2?si0:si1;
l5=si0;
si1=0U;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L3;
}
L4:;
}
si0=4820U;
si1=61U;
i32_store(i->m0,si0,si1);
L2:;
si0=-1U;
l20=si0;
L1:;
si0=l7;
si1=880U;
si0+=si1;
i->g0=si0;
si0=l20;
L0:;
return si0;
}

void f74(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 si0,si1,si2;
U64 sj1;
F64 sd1;
si0=l1;
si1=9U;
si0-=si1;
switch(si0){
case 0:
goto L2;
case 1:
goto L19;
case 2:
goto L18;
case 3:
goto L15;
case 4:
goto L17;
case 5:
goto L16;
case 6:
goto L14;
case 7:
goto L13;
case 8:
goto L12;
case 9:
goto L11;
case 10:
goto L10;
case 11:
goto L9;
case 12:
goto L8;
case 13:
goto L7;
case 14:
goto L6;
case 15:
goto L5;
case 16:
goto L4;
case 17:
goto L3;
default:
goto L1;
}
L19:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
l1=si1;
si2=4U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
sj1=i64_load32_s(i->m0,si1);
i64_store(i->m0,si0,sj1);
goto L0;
L18:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
l1=si1;
si2=4U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
sj1=i64_load32_u(i->m0,si1);
i64_store(i->m0,si0,sj1);
goto L0;
L17:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
l1=si1;
si2=4U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
sj1=i64_load32_s(i->m0,si1);
i64_store(i->m0,si0,sj1);
goto L0;
L16:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
l1=si1;
si2=4U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
sj1=i64_load32_u(i->m0,si1);
i64_store(i->m0,si0,sj1);
goto L0;
L15:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
si2=7U;
si1+=si2;
si2=-8U;
si1&=si2;
l1=si1;
si2=8U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,si0,sj1);
goto L0;
L14:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
l1=si1;
si2=4U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
sj1=i64_load16_s(i->m0,si1);
i64_store(i->m0,si0,sj1);
goto L0;
L13:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
l1=si1;
si2=4U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
sj1=i64_load16_u(i->m0,si1);
i64_store(i->m0,si0,sj1);
goto L0;
L12:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
l1=si1;
si2=4U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
sj1=i64_load8_s(i->m0,si1);
i64_store(i->m0,si0,sj1);
goto L0;
L11:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
l1=si1;
si2=4U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
sj1=i64_load8_u(i->m0,si1);
i64_store(i->m0,si0,sj1);
goto L0;
L10:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
si2=7U;
si1+=si2;
si2=-8U;
si1&=si2;
l1=si1;
si2=8U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,si0,sj1);
goto L0;
L9:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
l1=si1;
si2=4U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
sj1=i64_load32_u(i->m0,si1);
i64_store(i->m0,si0,sj1);
goto L0;
L8:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
si2=7U;
si1+=si2;
si2=-8U;
si1&=si2;
l1=si1;
si2=8U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,si0,sj1);
goto L0;
L7:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
si2=7U;
si1+=si2;
si2=-8U;
si1&=si2;
l1=si1;
si2=8U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,si0,sj1);
goto L0;
L6:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
l1=si1;
si2=4U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
sj1=i64_load32_s(i->m0,si1);
i64_store(i->m0,si0,sj1);
goto L0;
L5:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
l1=si1;
si2=4U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
sj1=i64_load32_u(i->m0,si1);
i64_store(i->m0,si0,sj1);
goto L0;
L4:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
si2=7U;
si1+=si2;
si2=-8U;
si1&=si2;
l1=si1;
si2=8U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
sd1=f64_load(i->m0,si1);
f64_store(i->m0,si0,sd1);
goto L0;
L3:;
si0=1532U;
si1=3960U;
si0=f68(instance,si0,si1);
UNREACHABLE;
L2:;
si0=l2;
si1=l2;
si1=i32_load(i->m0,si1);
l1=si1;
si2=4U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=l1;
si1=i32_load(i->m0,si1);
i32_store(i->m0,si0,si1);
L1:;
L0:;
}

void f75(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l3=0;
U32 l4=0;
U32 si0,si1,si2,si3,si4,si5;
si0=i->g0;
si1=256U;
si0-=si1;
l3=si0;
i->g0=si0;
si0=l1;
si1=l2;
si0=(U32)((I32)si0<=(I32)si1);
if(si0){
goto L1;
}
si0=l3;
si1=48U;
si2=l1;
si3=l2;
si2-=si3;
l2=si2;
si3=256U;
si4=l2;
si5=256U;
si4=si4 < si5;
l4=si4;
si2=si4?si2:si3;
si0=f79(instance,si0,si1,si2);
l1=si0;
si0=l4;
si0=!(si0);
if(si0){
L3:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l1;
si1=256U;
si2=l0;
f65(instance,si0,si1,si2);
}
L4:;
si0=l2;
si1=256U;
si0-=si1;
l2=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L3;
}
}
}
L2:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L1;
}
si0=l1;
si1=l2;
si2=l0;
f65(instance,si0,si1,si2);
L1:;
si0=l3;
si1=256U;
si0+=si1;
i->g0=si0;
L0:;
}

U32 f76(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 si0,si1,si2;
si0=l2;
si1=0U;
si0=si0 != si1;
l5=si0;
si0=l0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L4;
}
si0=l2;
si0=!(si0);
if(si0){
goto L4;
}
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=l1;
si2=255U;
si1&=si2;
si0=si0 == si1;
if(si0){
si0=l0;
l3=si0;
si0=l2;
l4=si0;
goto L2;
}
L5:;
si0=l2;
si1=1U;
si0-=si1;
l4=si0;
si1=0U;
si0=si0 != si1;
l5=si0;
si0=l0;
si1=1U;
si0+=si1;
l3=si0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L3;
}
si0=l4;
si0=!(si0);
if(si0){
goto L3;
}
si0=l3;
si0=i32_load8_u(i->m0,si0);
si1=l1;
si2=255U;
si1&=si2;
si0=si0 == si1;
if(si0){
goto L2;
}
si0=l2;
si1=2U;
si0-=si1;
l4=si0;
si1=0U;
si0=si0 != si1;
l5=si0;
si0=l0;
si1=2U;
si0+=si1;
l3=si0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L3;
}
si0=l4;
si0=!(si0);
if(si0){
goto L3;
}
si0=l3;
si0=i32_load8_u(i->m0,si0);
si1=l1;
si2=255U;
si1&=si2;
si0=si0 == si1;
if(si0){
goto L2;
}
si0=l2;
si1=3U;
si0-=si1;
l4=si0;
si1=0U;
si0=si0 != si1;
l5=si0;
si0=l0;
si1=3U;
si0+=si1;
l3=si0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L3;
}
si0=l4;
si0=!(si0);
if(si0){
goto L3;
}
si0=l3;
si0=i32_load8_u(i->m0,si0);
si1=l1;
si2=255U;
si1&=si2;
si0=si0 == si1;
if(si0){
goto L2;
}
si0=l0;
si1=4U;
si0+=si1;
l3=si0;
si0=l2;
si1=4U;
si0-=si1;
l4=si0;
si1=0U;
si0=si0 != si1;
l5=si0;
goto L3;
L4:;
si0=l2;
l4=si0;
si0=l0;
l3=si0;
L3:;
si0=l5;
si0=!(si0);
if(si0){
goto L1;
}
si0=l3;
si0=i32_load8_u(i->m0,si0);
si1=l1;
si2=255U;
si1&=si2;
si0=si0 == si1;
if(si0){
goto L6;
}
si0=l4;
si1=4U;
si0=si0 < si1;
if(si0){
goto L6;
}
si0=l1;
si1=255U;
si0&=si1;
si1=16843009U;
si0*=si1;
l0=si0;
L7:;
W2C2_LOOP_START
{
si0=16843008U;
si1=l3;
si1=i32_load(i->m0,si1);
si2=l0;
si1^=si2;
l2=si1;
si0-=si1;
si1=l2;
si0|=si1;
si1=-2139062144U;
si0&=si1;
si1=-2139062144U;
si0=si0 != si1;
if(si0){
goto L2;
}
si0=l3;
si1=4U;
si0+=si1;
l3=si0;
si0=l4;
si1=4U;
si0-=si1;
l4=si0;
si1=3U;
si0=si0 > si1;
if(si0){
goto L7;
}
}
L6:;
si0=l4;
si0=!(si0);
if(si0){
goto L1;
}
L2:;
si0=l1;
si1=255U;
si0&=si1;
l2=si0;
L8:;
W2C2_LOOP_START
{
si0=l2;
si1=l3;
si1=i32_load8_u(i->m0,si1);
si0=si0 == si1;
if(si0){
si0=l3;
goto L0;
}
L9:;
si0=l3;
si1=1U;
si0+=si1;
l3=si0;
si0=l4;
si1=1U;
si0-=si1;
l4=si0;
if(si0){
goto L8;
}
}
L1:;
si0=0U;
L0:;
return si0;
}

U32 f77(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 si0,si1;
si0=l2;
si0=!(si0);
if(si0){
goto L1;
}
L2:;
W2C2_LOOP_START
{
si0=l0;
si0=i32_load8_u(i->m0,si0);
l4=si0;
si1=l1;
si1=i32_load8_u(i->m0,si1);
l5=si1;
si0=si0 == si1;
if(si0){
si0=l1;
si1=1U;
si0+=si1;
l1=si0;
si0=l0;
si1=1U;
si0+=si1;
l0=si0;
si0=l2;
si1=1U;
si0-=si1;
l2=si0;
if(si0){
goto L2;
}
goto L1;
}
L3:;
}
si0=l4;
si1=l5;
si0-=si1;
l3=si0;
L1:;
si0=l3;
L0:;
return si0;
}

U32 f78(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 l6=0;
U32 si0,si1,si2,si3;
U64 sj1;
si0=l2;
si1=32U;
si0=si0 <= si1;
if(si0){
si0=l1;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L2;
}
si0=l2;
si0=!(si0);
if(si0){
goto L2;
}
si0=l0;
si1=l1;
si1=i32_load8_u(i->m0,si1);
i32_store8(i->m0,si0,si1);
si0=l2;
si1=1U;
si0-=si1;
l5=si0;
si0=l0;
si1=1U;
si0+=si1;
l4=si0;
si0=l1;
si1=1U;
si0+=si1;
l3=si0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L1;
}
si0=l5;
si0=!(si0);
if(si0){
goto L1;
}
si0=l0;
si1=l1;
si1=i32_load8_u(i->m0,(WasmMemoryAddress)si1+1U);
i32_store8(i->m0,(WasmMemoryAddress)si0+1U,si1);
si0=l2;
si1=2U;
si0-=si1;
l5=si0;
si0=l0;
si1=2U;
si0+=si1;
l4=si0;
si0=l1;
si1=2U;
si0+=si1;
l3=si0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L1;
}
si0=l5;
si0=!(si0);
if(si0){
goto L1;
}
si0=l0;
si1=l1;
si1=i32_load8_u(i->m0,(WasmMemoryAddress)si1+2U);
i32_store8(i->m0,(WasmMemoryAddress)si0+2U,si1);
si0=l2;
si1=3U;
si0-=si1;
l5=si0;
si0=l0;
si1=3U;
si0+=si1;
l4=si0;
si0=l1;
si1=3U;
si0+=si1;
l3=si0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L1;
}
si0=l5;
si0=!(si0);
if(si0){
goto L1;
}
si0=l0;
si1=l1;
si1=i32_load8_u(i->m0,(WasmMemoryAddress)si1+3U);
i32_store8(i->m0,(WasmMemoryAddress)si0+3U,si1);
si0=l2;
si1=4U;
si0-=si1;
l5=si0;
si0=l0;
si1=4U;
si0+=si1;
l4=si0;
si0=l1;
si1=4U;
si0+=si1;
l3=si0;
goto L1;
}
L3:;
si0=l0;
si1=l1;
si2=l2;
wasmMemoryCopy(i->m0,i->m0,si0,si1,si2);
si0=l0;
goto L0;
L2:;
si0=l2;
l5=si0;
si0=l0;
l4=si0;
si0=l1;
l3=si0;
L1:;
si0=l4;
si1=3U;
si0&=si1;
l2=si0;
si0=!(si0);
if(si0){
si0=l5;
si1=16U;
si0=si0 < si1;
if(si0){
si0=l5;
l2=si0;
goto L6;
}
L7:;
si0=l5;
si1=16U;
si0-=si1;
l2=si0;
si1=16U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l4;
si1=l3;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,si0,sj1);
si0=l4;
si1=l3;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+8U);
i64_store(i->m0,(WasmMemoryAddress)si0+8U,sj1);
si0=l4;
si1=16U;
si0+=si1;
l4=si0;
si0=l3;
si1=16U;
si0+=si1;
l3=si0;
si0=l2;
l5=si0;
}
L8:;
si0=l2;
si1=16U;
si0=si0 < si1;
if(si0){
goto L6;
}
si0=l5;
l2=si0;
L9:;
W2C2_LOOP_START
{
si0=l4;
si1=l3;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,si0,sj1);
si0=l4;
si1=l3;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+8U);
i64_store(i->m0,(WasmMemoryAddress)si0+8U,sj1);
si0=l4;
si1=l3;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+16U);
i64_store(i->m0,(WasmMemoryAddress)si0+16U,sj1);
si0=l4;
si1=l3;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+24U);
i64_store(i->m0,(WasmMemoryAddress)si0+24U,sj1);
si0=l4;
si1=32U;
si0+=si1;
l4=si0;
si0=l3;
si1=32U;
si0+=si1;
l3=si0;
si0=l2;
si1=32U;
si0-=si1;
l2=si0;
si1=15U;
si0=si0 > si1;
if(si0){
goto L9;
}
}
L6:;
si0=l2;
si1=8U;
si0=si0 >= si1;
if(si0){
si0=l4;
si1=l3;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,si0,sj1);
si0=l4;
si1=8U;
si0+=si1;
l4=si0;
si0=l3;
si1=8U;
si0+=si1;
l3=si0;
}
L10:;
si0=l2;
si1=4U;
si0&=si1;
if(si0){
si0=l4;
si1=l3;
si1=i32_load(i->m0,si1);
i32_store(i->m0,si0,si1);
si0=l4;
si1=4U;
si0+=si1;
l4=si0;
si0=l3;
si1=4U;
si0+=si1;
l3=si0;
}
L11:;
si0=l2;
si1=2U;
si0&=si1;
if(si0){
si0=l4;
si1=l3;
si1=i32_load16_u(i->m0,si1);
i32_store16(i->m0,si0,si1);
si0=l4;
si1=2U;
si0+=si1;
l4=si0;
si0=l3;
si1=2U;
si0+=si1;
l3=si0;
}
L12:;
si0=l2;
si1=1U;
si0&=si1;
si0=!(si0);
if(si0){
goto L4;
}
si0=l4;
si1=l3;
si1=i32_load8_u(i->m0,si1);
i32_store8(i->m0,si0,si1);
si0=l0;
goto L0;
}
L5:;
si0=l5;
si1=32U;
si0=si0 >= si1;
if(si0){
si0=l4;
si1=l3;
si1=i32_load(i->m0,si1);
l5=si1;
i32_store8(i->m0,si0,si1);
si0=l2;
si1=2U;
si0-=si1;
switch(si0){
case 0:
goto L19;
case 1:
goto L18;
default:
goto L16;
}
L19:;
si0=l4;
si1=l5;
si2=8U;
si1>>=(si2&31);
i32_store8(i->m0,(WasmMemoryAddress)si0+1U,si1);
si0=l4;
si1=l3;
si2=6U;
si1+=si2;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,(WasmMemoryAddress)si0+6U,sj1);
si0=l4;
si1=l3;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=16U;
si1<<=(si2&31);
si2=l5;
si3=16U;
si2>>=(si3&31);
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+2U,si1);
si0=l3;
si1=18U;
si0+=si1;
l1=si0;
si0=14U;
l6=si0;
si0=l3;
si1=14U;
si0+=si1;
si0=i32_load(i->m0,si0);
l3=si0;
si0=14U;
l5=si0;
si0=l4;
si1=18U;
si0+=si1;
goto L15;
L18:;
si0=l4;
si1=l3;
si2=5U;
si1+=si2;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,(WasmMemoryAddress)si0+5U,sj1);
si0=l4;
si1=l3;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=24U;
si1<<=(si2&31);
si2=l5;
si3=8U;
si2>>=(si3&31);
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+1U,si1);
si0=l3;
si1=17U;
si0+=si1;
l1=si0;
si0=13U;
l6=si0;
si0=l3;
si1=13U;
si0+=si1;
si0=i32_load(i->m0,si0);
l3=si0;
si0=15U;
l5=si0;
si0=l4;
si1=17U;
si0+=si1;
goto L15;
}
L17:;
si0=l5;
si1=16U;
si0=si0 < si1;
if(si0){
si0=l4;
l2=si0;
si0=l3;
goto L20;
}
L21:;
si0=l4;
si1=l3;
si1=i32_load8_u(i->m0,si1);
i32_store8(i->m0,si0,si1);
si0=l4;
si1=l3;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+1U);
i32_store(i->m0,(WasmMemoryAddress)si0+1U,si1);
si0=l4;
si1=l3;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+5U);
i64_store(i->m0,(WasmMemoryAddress)si0+5U,sj1);
si0=l4;
si1=l3;
si1=i32_load16_u(i->m0,(WasmMemoryAddress)si1+13U);
i32_store16(i->m0,(WasmMemoryAddress)si0+13U,si1);
si0=l4;
si1=l3;
si1=i32_load8_u(i->m0,(WasmMemoryAddress)si1+15U);
i32_store8(i->m0,(WasmMemoryAddress)si0+15U,si1);
si0=l4;
si1=16U;
si0+=si1;
l2=si0;
si0=l3;
si1=16U;
si0+=si1;
L20:;
l1=si0;
si0=l5;
si1=8U;
si0&=si1;
if(si0){
goto L14;
}
goto L13;
L16:;
si0=l4;
si1=l5;
si2=16U;
si1>>=(si2&31);
i32_store8(i->m0,(WasmMemoryAddress)si0+2U,si1);
si0=l4;
si1=l5;
si2=8U;
si1>>=(si2&31);
i32_store8(i->m0,(WasmMemoryAddress)si0+1U,si1);
si0=l4;
si1=l3;
si2=7U;
si1+=si2;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,(WasmMemoryAddress)si0+7U,sj1);
si0=l4;
si1=l3;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=8U;
si1<<=(si2&31);
si2=l5;
si3=24U;
si2>>=(si3&31);
si1|=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+3U,si1);
si0=l3;
si1=19U;
si0+=si1;
l1=si0;
si0=15U;
l6=si0;
si0=l3;
si1=15U;
si0+=si1;
si0=i32_load(i->m0,si0);
l3=si0;
si0=13U;
l5=si0;
si0=l4;
si1=19U;
si0+=si1;
L15:;
l2=si0;
si0=l4;
si1=l6;
si0+=si1;
si1=l3;
i32_store(i->m0,si0,si1);
L14:;
si0=l2;
si1=l1;
sj1=i64_load(i->m0,si1);
i64_store(i->m0,si0,sj1);
si0=l2;
si1=8U;
si0+=si1;
l2=si0;
si0=l1;
si1=8U;
si0+=si1;
l1=si0;
L13:;
si0=l5;
si1=4U;
si0&=si1;
if(si0){
si0=l2;
si1=l1;
si1=i32_load(i->m0,si1);
i32_store(i->m0,si0,si1);
si0=l2;
si1=4U;
si0+=si1;
l2=si0;
si0=l1;
si1=4U;
si0+=si1;
l1=si0;
}
L22:;
si0=l5;
si1=2U;
si0&=si1;
if(si0){
si0=l2;
si1=l1;
si1=i32_load16_u(i->m0,si1);
i32_store16(i->m0,si0,si1);
si0=l2;
si1=2U;
si0+=si1;
l2=si0;
si0=l1;
si1=2U;
si0+=si1;
l1=si0;
}
L23:;
si0=l5;
si1=1U;
si0&=si1;
si0=!(si0);
if(si0){
goto L4;
}
si0=l2;
si1=l1;
si1=i32_load8_u(i->m0,si1);
i32_store8(i->m0,si0,si1);
L4:;
si0=l0;
L0:;
return si0;
}

U32 f79(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l3=0;
U32 l4=0;
U64 l5=0;
U32 l6=0;
U32 si0,si1,si2;
U64 sj0,sj1;
si0=l2;
si1=33U;
si0=si0 >= si1;
if(si0){
si0=l0;
si1=l1;
si2=l2;
wasmMemoryFill(i->m0,si0,si1,si2);
si0=l0;
goto L0;
}
L1:;
si0=l2;
si0=!(si0);
if(si0){
goto L2;
}
si0=l0;
si1=l1;
i32_store8(i->m0,si0,si1);
si0=l0;
si1=l2;
si0+=si1;
l3=si0;
si1=1U;
si0-=si1;
si1=l1;
i32_store8(i->m0,si0,si1);
si0=l2;
si1=3U;
si0=si0 < si1;
if(si0){
goto L2;
}
si0=l0;
si1=l1;
i32_store8(i->m0,(WasmMemoryAddress)si0+2U,si1);
si0=l0;
si1=l1;
i32_store8(i->m0,(WasmMemoryAddress)si0+1U,si1);
si0=l3;
si1=3U;
si0-=si1;
si1=l1;
i32_store8(i->m0,si0,si1);
si0=l3;
si1=2U;
si0-=si1;
si1=l1;
i32_store8(i->m0,si0,si1);
si0=l2;
si1=7U;
si0=si0 < si1;
if(si0){
goto L2;
}
si0=l0;
si1=l1;
i32_store8(i->m0,(WasmMemoryAddress)si0+3U,si1);
si0=l3;
si1=4U;
si0-=si1;
si1=l1;
i32_store8(i->m0,si0,si1);
si0=l2;
si1=9U;
si0=si0 < si1;
if(si0){
goto L2;
}
si0=l0;
si1=0U;
si2=l0;
si1-=si2;
si2=3U;
si1&=si2;
l6=si1;
si0+=si1;
l4=si0;
si1=l1;
si2=255U;
si1&=si2;
si2=16843009U;
si1*=si2;
l3=si1;
i32_store(i->m0,si0,si1);
si0=l4;
si1=l2;
si2=l6;
si1-=si2;
si2=60U;
si1&=si2;
l1=si1;
si0+=si1;
l2=si0;
si1=4U;
si0-=si1;
si1=l3;
i32_store(i->m0,si0,si1);
si0=l1;
si1=9U;
si0=si0 < si1;
if(si0){
goto L2;
}
si0=l4;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l4;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l2;
si1=8U;
si0-=si1;
si1=l3;
i32_store(i->m0,si0,si1);
si0=l2;
si1=12U;
si0-=si1;
si1=l3;
i32_store(i->m0,si0,si1);
si0=l1;
si1=25U;
si0=si0 < si1;
if(si0){
goto L2;
}
si0=l4;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l4;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=l4;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+16U,si1);
si0=l4;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l2;
si1=16U;
si0-=si1;
si1=l3;
i32_store(i->m0,si0,si1);
si0=l2;
si1=20U;
si0-=si1;
si1=l3;
i32_store(i->m0,si0,si1);
si0=l2;
si1=24U;
si0-=si1;
si1=l3;
i32_store(i->m0,si0,si1);
si0=l2;
si1=28U;
si0-=si1;
si1=l3;
i32_store(i->m0,si0,si1);
si0=l1;
si1=l4;
si2=4U;
si1&=si2;
si2=24U;
si1|=si2;
l2=si1;
si0-=si1;
l1=si0;
si1=32U;
si0=si0 < si1;
if(si0){
goto L2;
}
si0=l3;
sj0=(U64)(si0);
sj1=W2C2_LL(4294967297U);
sj0*=sj1;
l5=sj0;
si0=l2;
si1=l4;
si0+=si1;
l2=si0;
L3:;
W2C2_LOOP_START
{
si0=l2;
sj1=l5;
i64_store(i->m0,(WasmMemoryAddress)si0+24U,sj1);
si0=l2;
sj1=l5;
i64_store(i->m0,(WasmMemoryAddress)si0+16U,sj1);
si0=l2;
sj1=l5;
i64_store(i->m0,(WasmMemoryAddress)si0+8U,sj1);
si0=l2;
sj1=l5;
i64_store(i->m0,si0,sj1);
si0=l2;
si1=32U;
si0+=si1;
l2=si0;
si0=l1;
si1=32U;
si0-=si1;
l1=si0;
si1=31U;
si0=si0 > si1;
if(si0){
goto L3;
}
}
L2:;
si0=l0;
L0:;
return si0;
}

U32 f80(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 si0,si1,si2,si3,si4;
si0=l0;
si1=l1;
si0=f81(instance,si0,si1);
l0=si0;
si1=0U;
si2=l0;
si2=i32_load8_u(i->m0,si2);
si3=l1;
si4=255U;
si3&=si4;
si2=si2 == si3;
si0=si2?si0:si1;
L0:;
return si0;
}

U32 f81(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l2=0;
U32 l3=0;
U32 l4=0;
U32 si0,si1,si2;
si0=l1;
si1=255U;
si0&=si1;
l4=si0;
if(si0){
si0=l0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L2;
}
si0=l0;
si0=i32_load8_u(i->m0,si0);
l2=si0;
si0=!(si0);
if(si0){
si0=l0;
goto L0;
}
L5:;
si0=l2;
si1=l1;
si2=255U;
si1&=si2;
si0=si0 != si1;
if(si0){
goto L3;
}
si0=l0;
goto L0;
}
L4:;
si0=l0;
si0=f82(instance,si0);
si1=l0;
si0+=si1;
goto L0;
L3:;
si0=l0;
si1=1U;
si0+=si1;
l2=si0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l2;
l0=si0;
goto L2;
}
L6:;
si0=l2;
si0=i32_load8_u(i->m0,si0);
l3=si0;
si0=!(si0);
if(si0){
goto L1;
}
si0=l3;
si1=l1;
si2=255U;
si1&=si2;
si0=si0 == si1;
if(si0){
goto L1;
}
si0=l0;
si1=2U;
si0+=si1;
l2=si0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l2;
l0=si0;
goto L2;
}
L7:;
si0=l2;
si0=i32_load8_u(i->m0,si0);
l3=si0;
si0=!(si0);
if(si0){
goto L1;
}
si0=l3;
si1=l1;
si2=255U;
si1&=si2;
si0=si0 == si1;
if(si0){
goto L1;
}
si0=l0;
si1=3U;
si0+=si1;
l2=si0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l2;
l0=si0;
goto L2;
}
L8:;
si0=l2;
si0=i32_load8_u(i->m0,si0);
l3=si0;
si0=!(si0);
if(si0){
goto L1;
}
si0=l3;
si1=l1;
si2=255U;
si1&=si2;
si0=si0 == si1;
if(si0){
goto L1;
}
si0=l0;
si1=4U;
si0+=si1;
l0=si0;
L2:;
si0=16843008U;
si1=l0;
si1=i32_load(i->m0,si1);
l2=si1;
si0-=si1;
si1=l2;
si0|=si1;
si1=-2139062144U;
si0&=si1;
si1=-2139062144U;
si0=si0 != si1;
if(si0){
si0=l0;
l4=si0;
goto L9;
}
L10:;
si0=l4;
si1=16843009U;
si0*=si1;
l3=si0;
L11:;
W2C2_LOOP_START
{
si0=16843008U;
si1=l2;
si2=l3;
si1^=si2;
l2=si1;
si0-=si1;
si1=l2;
si0|=si1;
si1=-2139062144U;
si0&=si1;
si1=-2139062144U;
si0=si0 != si1;
if(si0){
si0=l0;
l4=si0;
goto L9;
}
L12:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l2=si0;
si0=l0;
si1=4U;
si0+=si1;
l4=si0;
l0=si0;
si0=l2;
si1=16843008U;
si2=l2;
si1-=si2;
si0|=si1;
si1=-2139062144U;
si0&=si1;
si1=-2139062144U;
si0=si0 == si1;
if(si0){
goto L11;
}
}
L9:;
si0=l4;
si1=1U;
si0-=si1;
l2=si0;
L13:;
W2C2_LOOP_START
{
si0=l2;
si1=1U;
si0+=si1;
l2=si0;
si0=i32_load8_u(i->m0,si0);
l0=si0;
si0=!(si0);
if(si0){
goto L1;
}
si0=l0;
si1=l1;
si2=255U;
si1&=si2;
si0=si0 != si1;
if(si0){
goto L13;
}
}
L1:;
si0=l2;
L0:;
return si0;
}

U32 f82(wasmModuleInstance* instance,U32 l0) {
m5_guestInstance* i = (m5_guestInstance*)instance;
U32 l1=0;
U32 l2=0;
U32 l3=0;
U32 si0,si1,si2;
si0=l0;
l1=si0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L2;
}
si0=l0;
si0=i32_load8_u(i->m0,si0);
si0=!(si0);
if(si0){
si0=0U;
goto L0;
}
L3:;
si0=l0;
si1=1U;
si0+=si1;
l1=si0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L2;
}
si0=l1;
si0=i32_load8_u(i->m0,si0);
si0=!(si0);
if(si0){
goto L1;
}
si0=l0;
si1=2U;
si0+=si1;
l1=si0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L2;
}
si0=l1;
si0=i32_load8_u(i->m0,si0);
si0=!(si0);
if(si0){
goto L1;
}
si0=l0;
si1=3U;
si0+=si1;
l1=si0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L2;
}
si0=l1;
si0=i32_load8_u(i->m0,si0);
si0=!(si0);
if(si0){
goto L1;
}
si0=l0;
si1=4U;
si0+=si1;
l1=si0;
si1=3U;
si0&=si1;
if(si0){
goto L1;
}
L2:;
si0=l1;
si1=4U;
si0-=si1;
l2=si0;
si0=l1;
si1=5U;
si0-=si1;
l1=si0;
L4:;
W2C2_LOOP_START
{
si0=l1;
si1=4U;
si0+=si1;
l1=si0;
si0=16843008U;
si1=l2;
si2=4U;
si1+=si2;
l2=si1;
si1=i32_load(i->m0,si1);
l3=si1;
si0-=si1;
si1=l3;
si0|=si1;
si1=-2139062144U;
si0&=si1;
si1=-2139062144U;
si0=si0 == si1;
if(si0){
goto L4;
}
}
L5:;
W2C2_LOOP_START
{
si0=l1;
si1=1U;
si0+=si1;
l1=si0;
si0=l2;
si0=i32_load8_u(i->m0,si0);
l3=si0;
si0=l2;
si1=1U;
si0+=si1;
l2=si0;
si0=l3;
if(si0){
goto L5;
}
}
L1:;
si0=l1;
si1=l0;
si0-=si1;
L0:;
return si0;
}

#if W2C2_RUNTIME_CHECKS
const wasmFuncType m5_guestFuncType0={3,"iii",1,"i"};
const wasmFuncType m5_guestFuncType1={2,"ii",1,"i"};
const wasmFuncType m5_guestFuncType2={1,"i",1,"i"};
const wasmFuncType m5_guestFuncType3={1,"i",0,""};
const wasmFuncType m5_guestFuncType4={4,"iiii",1,"i"};
const wasmFuncType m5_guestFuncType5={3,"iji",1,"j"};
const wasmFuncType m5_guestFuncType6={3,"iii",0,""};
const wasmFuncType m5_guestFuncType7={0,"",0,""};
const wasmFuncType m5_guestFuncType8={2,"ii",0,""};
const wasmFuncType m5_guestFuncType9={3,"iji",1,"i"};
const wasmFuncType m5_guestFuncType10={5,"iiiji",1,"i"};
const wasmFuncType m5_guestFuncType11={5,"iiiii",1,"i"};
const wasmFuncType m5_guestFuncType12={0,"",1,"j"};
const wasmFuncType m5_guestFuncType13={1,"j",1,"i"};
const wasmFuncType m5_guestFuncType14={4,"ijii",1,"i"};
const wasmFuncType m5_guestFuncType15={9,"iiiiijjii",1,"i"};
const wasmFuncType m5_guestFuncType16={6,"iiiiii",1,"i"};
const wasmFuncType m5_guestFuncType17={2,"di",1,"d"};
#endif

wasmFuncExport m5_guestFuncExports[2] = {
{(wasmFunc)f28,{"_start",6}},
{NULL,{NULL,0}}
};

static void m5_guestInitMemories(m5_guestInstance* i, m5_guestInstance* parent) {
i->m0 = wasmMemoryAllocate(2, 65536, false);
}

static void m5_guestInitDataSegments(m5_guestInstance* i) {
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[0]=0;
#endif
LOAD_DATA((*i->m0), 1024U, m5_guestData0, 2513);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[1]=0;
#endif
LOAD_DATA((*i->m0), 3553U, m5_guestData1, 33);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[2]=0;
#endif
LOAD_DATA((*i->m0), 3611U, m5_guestData2, 1);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[3]=0;
#endif
LOAD_DATA((*i->m0), 3623U, m5_guestData3, 21);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[4]=0;
#endif
LOAD_DATA((*i->m0), 3669U, m5_guestData4, 1);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[5]=0;
#endif
LOAD_DATA((*i->m0), 3681U, m5_guestData5, 21);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[6]=0;
#endif
LOAD_DATA((*i->m0), 3727U, m5_guestData6, 1);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[7]=0;
#endif
LOAD_DATA((*i->m0), 3739U, m5_guestData7, 30);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[8]=0;
#endif
LOAD_DATA((*i->m0), 3794U, m5_guestData8, 14);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[9]=0;
#endif
LOAD_DATA((*i->m0), 3843U, m5_guestData9, 1);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[10]=0;
#endif
LOAD_DATA((*i->m0), 3855U, m5_guestData10, 21);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[11]=0;
#endif
LOAD_DATA((*i->m0), 3901U, m5_guestData11, 1);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[12]=0;
#endif
LOAD_DATA((*i->m0), 3913U, m5_guestData12, 39);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[13]=0;
#endif
LOAD_DATA((*i->m0), 3952U, m5_guestData13, 9);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[14]=0;
#endif
LOAD_DATA((*i->m0), 3972U, m5_guestData14, 1);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[15]=0;
#endif
LOAD_DATA((*i->m0), 3992U, m5_guestData15, 10);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[16]=0;
#endif
LOAD_DATA((*i->m0), 4016U, m5_guestData16, 12);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[17]=0;
#endif
LOAD_DATA((*i->m0), 4072U, m5_guestData17, 9);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[18]=0;
#endif
LOAD_DATA((*i->m0), 4092U, m5_guestData18, 1);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[19]=0;
#endif
LOAD_DATA((*i->m0), 4108U, m5_guestData19, 18);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[20]=0;
#endif
LOAD_DATA((*i->m0), 4192U, m5_guestData20, 9);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[21]=0;
#endif
LOAD_DATA((*i->m0), 4212U, m5_guestData21, 1);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[22]=0;
#endif
LOAD_DATA((*i->m0), 4232U, m5_guestData22, 14);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[23]=0;
#endif
LOAD_DATA((*i->m0), 4256U, m5_guestData23, 9);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[24]=0;
#endif
LOAD_DATA((*i->m0), 4312U, m5_guestData24, 2);
}

static void m5_guestInitTables(m5_guestInstance* i) {
U32 offset;
wasmTableAllocate(&i->t0, 6, 6);
offset=1U;
wasmTableCheckRange(&i->t0,offset,5);
wasmTableSetTyped(&i->t0,offset+0,(wasmFunc)&f56,&i->common,&m5_guestFuncType5);
wasmTableSetTyped(&i->t0,offset+1,(wasmFunc)&f58,&i->common,&m5_guestFuncType0);
wasmTableSetTyped(&i->t0,offset+2,(wasmFunc)&f59,&i->common,&m5_guestFuncType0);
wasmTableSetTyped(&i->t0,offset+3,(wasmFunc)&f60,&i->common,&m5_guestFuncType2);
wasmTableSetTyped(&i->t0,offset+4,(wasmFunc)&f70,&i->common,&m5_guestFuncType0);
}

static void m5_guestInitGlobals(m5_guestInstance* i) {
i->g0=72528U;
}

static void m5_guestInitImports(m5_guestInstance* i, wasmImportResolver resolve) {
if (resolve == NULL) { return; }
}

wasmMemory*m5_guestExport6_memory(m5_guestInstance* i){
return i->m0;
}

void m5_guestExport6_X5Fstart(m5_guestInstance*i){
f28(&i->common);
}

static void m5_guestFreeChild(wasmModuleInstance* child) {
m5_guestInstance* i = (m5_guestInstance*)child;
wasmMemoryFree(i->m0);
i->m0=NULL;
wasmTableFree(&i->t0);
free(i);
}

static wasmModuleInstance* m5_guestNewChild(wasmModuleInstance* instance) {
m5_guestInstance* self = (m5_guestInstance*)instance;
m5_guestInstance* child = (m5_guestInstance*)calloc(1, sizeof(m5_guestInstance));
child->common.funcExports = self->common.funcExports;
child->common.resolveImports = self->common.resolveImports;
child->common.newChild = self->common.newChild;
child->common.freeChild = self->common.freeChild;
m5_guestInitImports(child, self->common.resolveImports);
m5_guestInitMemories(child, self);
m5_guestInitDataSegments(child);
m5_guestInitTables(child);
m5_guestInitGlobals(child);
return &child->common;
}

void m5_guestInstantiate(m5_guestInstance* i, wasmImportResolver resolveImports) {
i->common.funcExports = m5_guestFuncExports;
i->common.resolveImports = resolveImports;
i->common.newChild = m5_guestNewChild;
i->common.freeChild = m5_guestFreeChild;
m5_guestInitImports(i, resolveImports);
m5_guestInitMemories(i, NULL);
m5_guestInitDataSegments(i);
m5_guestInitTables(i);
m5_guestInitGlobals(i);
}

void m5_guestFreeInstance(m5_guestInstance* i) {
wasmMemoryFree(i->m0);
i->m0=NULL;
wasmTableFree(&i->t0);
}

