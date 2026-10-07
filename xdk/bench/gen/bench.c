#include "w2c2_base.h"

#include "bench.h"

const U8 m5_benchData0[]={
0x2d,0x2b,0x20,0x20,0x20,0x30,0x58,0x30,0x78,0,0x2d,0x30,0x58,0x2b,0x30,0x58,0x20,0x30,
0x58,0x2d,0x30,0x78,0x2b,0x30,0x78,0x20,0x30,0x78,0,0x43,0x41,0x52,0x44,0x2e,0x49,0x54,
0x45,0x4d,0x5f,0x25,0x64,0x5f,0x25,0x75,0,0x62,0x65,0x6e,0x63,0x68,0x20,0x73,0x74,0x61,
0x72,0x74,0,0x6e,0x61,0x6e,0,0x69,0x6e,0x66,0,0x62,0x65,0x6e,0x63,0x68,0x20,0x64,
0x6f,0x6e,0x65,0,0x4e,0x41,0x4e,0,0x49,0x4e,0x46,0,0x2e,0,0x28,0x6e,0x75,0x6c,
0x6c,0x29,0,0x6d,0x61,0x74,0x68,0x20,0x20,0x20,0x20,0x20,0x25,0x38,0x6c,0x75,0x20,0x75,
0x73,0x20,0x20,0x63,0x68,0x65,0x63,0x6b,0x20,0x25,0x30,0x38,0x78,0x25,0x30,0x38,0x78,0xa,
0,0x6f,0x62,0x6a,0x65,0x63,0x74,0x73,0x20,0x20,0x25,0x38,0x6c,0x75,0x20,0x75,0x73,0x20,
0x20,0x63,0x68,0x65,0x63,0x6b,0x20,0x25,0x30,0x38,0x78,0xa,0,0x73,0x74,0x72,0x69,0x6e,
0x67,0x73,0x20,0x20,0x25,0x38,0x6c,0x75,0x20,0x75,0x73,0x20,0x20,0x63,0x68,0x65,0x63,0x6b,
0x20,0x25,0x30,0x38,0x78,0xa,0,0x73,0x6f,0x72,0x74,0x20,0x20,0x20,0x20,0x20,0x25,0x38,
0x6c,0x75,0x20,0x75,0x73,0x20,0x20,0x63,0x68,0x65,0x63,0x6b,0x20,0x25,0x30,0x38,0x78,0xa,
0,0x74,0x6f,0x74,0x61,0x6c,0x20,0x20,0x20,0x20,0x25,0x38,0x6c,0x75,0x20,0x75,0x73,0xa,
0,0x53,0x75,0x70,0x70,0x6f,0x72,0x74,0x20,0x66,0x6f,0x72,0x20,0x66,0x6f,0x72,0x6d,0x61,
0x74,0x74,0x69,0x6e,0x67,0x20,0x6c,0x6f,0x6e,0x67,0x20,0x64,0x6f,0x75,0x62,0x6c,0x65,0x20,
0x76,0x61,0x6c,0x75,0x65,0x73,0x20,0x69,0x73,0x20,0x63,0x75,0x72,0x72,0x65,0x6e,0x74,0x6c,
0x79,0x20,0x64,0x69,0x73,0x61,0x62,0x6c,0x65,0x64,0x2e,0xa,0x54,0x6f,0x20,0x65,0x6e,0x61,
0x62,0x6c,0x65,0x20,0x69,0x74,0x2c,0x20,0x61,0x64,0x64,0x20,0x2d,0x6c,0x63,0x2d,0x70,0x72,
0x69,0x6e,0x74,0x73,0x63,0x61,0x6e,0x2d,0x6c,0x6f,0x6e,0x67,0x2d,0x64,0x6f,0x75,0x62,0x6c,
0x65,0x20,0x74,0x6f,0x20,0x74,0x68,0x65,0x20,0x6c,0x69,0x6e,0x6b,0x20,0x63,0x6f,0x6d,0x6d,
0x61,0x6e,0x64,0x2e,0xa,0,0,0,2,0,0,0,3,0,0,0,4,0,
0,0,5,0,0,0,1
};

const U8 m5_benchData1[]={
3,0,0,0,4,0,0,0,4,0,0,0,6,0,0,0,0x83,0xf9,
0xa2,0,0x44,0x4e,0x6e,0,0xfc,0x29,0x15,0,0xd1,0x57,0x27,0,0xdd,0x34,0xf5,0,
0x62,0xdb,0xc0,0,0x3c,0x99,0x95,0,0x41,0x90,0x43,0,0x63,0x51,0xfe,0,0xbb,0xde,
0xab,0,0xb7,0x61,0xc5,0,0x3a,0x6e,0x24,0,0xd2,0x4d,0x42,0,0x49,6,0xe0,0,
9,0xea,0x2e,0,0x1c,0x92,0xd1,0,0xeb,0x1d,0xfe,0,0x29,0xb1,0x1c,0,0xe8,0x3e,
0xa7,0,0xf5,0x35,0x82,0,0x44,0xbb,0x2e,0,0x9c,0xe9,0x84,0,0xb4,0x26,0x70,0,
0x41,0x7e,0x5f,0,0xd6,0x91,0x39,0,0x53,0x83,0x39,0,0x9c,0xf4,0x39,0,0x8b,0x5f,
0x84,0,0x28,0xf9,0xbd,0,0xf8,0x1f,0x3b,0,0xde,0xff,0x97,0,0xf,0x98,5,0,
0x11,0x2f,0xef,0,0xa,0x5a,0x8b,0,0x6d,0x1f,0x6d,0,0xcf,0x7e,0x36,0,9,0xcb,
0x27,0,0x46,0x4f,0xb7,0,0x9e,0x66,0x3f,0,0x2d,0xea,0x5f,0,0xba,0x27,0x75,0,
0xe5,0xeb,0xc7,0,0x3d,0x7b,0xf1,0,0xf7,0x39,7,0,0x92,0x52,0x8a,0,0xfb,0x6b,
0xea,0,0x1f,0xb1,0x5f,0,8,0x5d,0x8d,0,0x30,3,0x56,0,0x7b,0xfc,0x46,0,
0xf0,0xab,0x6b,0,0x20,0xbc,0xcf,0,0x36,0xf4,0x9a,0,0xe3,0xa9,0x1d,0,0x5e,0x61,
0x91,0,8,0x1b,0xe6,0,0x85,0x99,0x65,0,0xa0,0x14,0x5f,0,0x8d,0x40,0x68,0,
0x80,0xd8,0xff,0,0x27,0x73,0x4d,0,6,6,0x31,0,0xca,0x56,0x15,0,0xc9,0xa8,
0x73,0,0x7b,0xe2,0x60,0,0x6b,0x8c,0xc0,0,0x19,0xc4,0x47,0,0xcd,0x67,0xc3,0,
9,0xe8,0xdc,0,0x59,0x83,0x2a,0,0x8b,0x76,0xc4,0,0xa6,0x1c,0x96,0,0x44,0xaf,
0xdd,0,0x19,0x57,0xd1,0,0xa5,0x3e,5,0,5,7,0xff,0,0x33,0x7e,0x3f,0,
0xc2,0x32,0xe8,0,0x98,0x4f,0xde,0,0xbb,0x7d,0x32,0,0x26,0x3d,0xc3,0,0x1e,0x6b,
0xef,0,0x9f,0xf8,0x5e,0,0x35,0x1f,0x3a,0,0x7f,0xf2,0xca,0,0xf1,0x87,0x1d,0,
0x7c,0x90,0x21,0,0x6a,0x24,0x7c,0,0xd5,0x6e,0xfa,0,0x30,0x2d,0x77,0,0x15,0x3b,
0x43,0,0xb5,0x14,0xc6,0,0xc3,0x19,0x9d,0,0xad,0xc4,0xc2,0,0x2c,0x4d,0x41,0,
0xc,0,0x5d,0,0x86,0x7d,0x46,0,0xe3,0x71,0x2d,0,0x9b,0xc6,0x9a,0,0x33,0x62,
0,0,0xb4,0xd2,0x7c,0,0xb4,0xa7,0x97,0,0x37,0x55,0xd5,0,0xd7,0x3e,0xf6,0,
0xa3,0x10,0x18,0,0x4d,0x76,0xfc,0,0x64,0x9d,0x2a,0,0x70,0xd7,0xab,0,0x63,0x7c,
0xf8,0,0x7a,0xb0,0x57,0,0x17,0x15,0xe7,0,0xc0,0x49,0x56,0,0x3b,0xd6,0xd9,0,
0xa7,0x84,0x38,0,0x24,0x23,0xcb,0,0xd6,0x8a,0x77,0,0x5a,0x54,0x23,0,0,0x1f,
0xb9,0,0xf1,0xa,0x1b,0,0x19,0xce,0xdf,0,0x9f,0x31,0xff,0,0x66,0x1e,0x6a,0,
0x99,0x57,0x61,0,0xac,0xfb,0x47,0,0x7e,0x7f,0xd8,0,0x22,0x65,0xb7,0,0x32,0xe8,
0x89,0,0xe6,0xbf,0x60,0,0xef,0xc4,0xcd,0,0x6c,0x36,9,0,0x5d,0x3f,0xd4,0,
0x16,0xde,0xd7,0,0x58,0x3b,0xde,0,0xde,0x9b,0x92,0,0xd2,0x22,0x28,0,0x28,0x86,
0xe8,0,0xe2,0x58,0x4d,0,0xc6,0xca,0x32,0,8,0xe3,0x16,0,0xe0,0x7d,0xcb,0,
0x17,0xc0,0x50,0,0xf3,0x1d,0xa7,0,0x18,0xe0,0x5b,0,0x2e,0x13,0x34,0,0x83,0x12,
0x62,0,0x83,0x48,1,0,0xf5,0x8e,0x5b,0,0xad,0xb0,0x7f,0,0x1e,0xe9,0xf2,0,
0x48,0x4a,0x43,0,0x10,0x67,0xd3,0,0xaa,0xdd,0xd8,0,0xae,0x5f,0x42,0,0x6a,0x61,
0xce,0,0xa,0x28,0xa4,0,0xd3,0x99,0xb4,0,6,0xa6,0xf2,0,0x5c,0x77,0x7f,0,
0xa3,0xc2,0x83,0,0x61,0x3c,0x88,0,0x8a,0x73,0x78,0,0xaf,0x8c,0x5a,0,0x6f,0xd7,
0xbd,0,0x2d,0xa6,0x63,0,0xf4,0xbf,0xcb,0,0x8d,0x81,0xef,0,0x26,0xc1,0x67,0,
0x55,0xca,0x45,0,0xca,0xd9,0x36,0,0x28,0xa8,0xd2,0,0xc2,0x61,0x8d,0,0x12,0xc9,
0x77,0,4,0x26,0x14,0,0x12,0x46,0x9b,0,0xc4,0x59,0xc4,0,0xc8,0xc5,0x44,0,
0x4d,0xb2,0x91,0,0,0x17,0xf3,0,0xd4,0x43,0xad,0,0x29,0x49,0xe5,0,0xfd,0xd5,
0x10,0,0,0xbe,0xfc,0,0x1e,0x94,0xcc,0,0x70,0xce,0xee,0,0x13,0x3e,0xf5,0,
0xec,0xf1,0x80,0,0xb3,0xe7,0xc3,0,0xc7,0xf8,0x28,0,0x93,5,0x94,0,0xc1,0x71,
0x3e,0,0x2e,9,0xb3,0,0xb,0x45,0xf3,0,0x88,0x12,0x9c,0,0xab,0x20,0x7b,0,
0x2e,0xb5,0x9f,0,0x47,0x92,0xc2,0,0x7b,0x32,0x2f,0,0xc,0x55,0x6d,0,0x72,0xa7,
0x90,0,0x6b,0xe7,0x1f,0,0x31,0xcb,0x96,0,0x79,0x16,0x4a,0,0x41,0x79,0xe2,0,
0xf4,0xdf,0x89,0,0xe8,0x94,0x97,0,0xe2,0xe6,0x84,0,0x99,0x31,0x97,0,0x88,0xed,
0x6b,0,0x5f,0x5f,0x36,0,0xbb,0xfd,0xe,0,0x48,0x9a,0xb4,0,0x67,0xa4,0x6c,0,
0x71,0x72,0x42,0,0x8d,0x5d,0x32,0,0x9f,0x15,0xb8,0,0xbc,0xe5,9,0,0x8d,0x31,
0x25,0,0xf7,0x74,0x39,0,0x30,5,0x1c,0,0xd,0xc,1,0,0x4b,8,0x68,0,
0x2c,0xee,0x58,0,0x47,0xaa,0x90,0,0x74,0xe7,2,0,0xbd,0xd6,0x24,0,0xf7,0x7d,
0xa6,0,0x6e,0x48,0x72,0,0x9f,0x16,0xef,0,0x8e,0x94,0xa6,0,0xb4,0x91,0xf6,0,
0xd1,0x53,0x51,0,0xcf,0xa,0xf2,0,0x20,0x98,0x33,0,0xf5,0x4b,0x7e,0,0xb2,0x63,
0x68,0,0xdd,0x3e,0x5f,0,0x40,0x5d,3,0,0x85,0x89,0x7f,0,0x55,0x52,0x29,0,
0x37,0x64,0xc0,0,0x6d,0xd8,0x10,0,0x32,0x48,0x32,0,0x5b,0x4c,0x75,0,0x4e,0x71,
0xd4,0,0x45,0x54,0x6e,0,0xb,9,0xc1,0,0x2a,0xf5,0x69,0,0x14,0x66,0xd5,0,
0x27,7,0x9d,0,0x5d,4,0x50,0,0xb4,0x3b,0xdb,0,0xea,0x76,0xc5,0,0x87,0xf9,
0x17,0,0x49,0x6b,0x7d,0,0x1d,0x27,0xba,0,0x96,0x69,0x29,0,0xc6,0xcc,0xac,0,
0xad,0x14,0x54,0,0x90,0xe2,0x6a,0,0x88,0xd9,0x89,0,0x2c,0x72,0x50,0,4,0xa4,
0xbe,0,0x77,7,0x94,0,0xf3,0x30,0x70,0,0,0xfc,0x27,0,0xea,0x71,0xa8,0,
0x66,0xc2,0x49,0,0x64,0xe0,0x3d,0,0x97,0xdd,0x83,0,0xa3,0x3f,0x97,0,0x43,0x94,
0xfd,0,0xd,0x86,0x8c,0,0x31,0x41,0xde,0,0x92,0x39,0x9d,0,0xdd,0x70,0x8c,0,
0x17,0xb7,0xe7,0,8,0xdf,0x3b,0,0x15,0x37,0x2b,0,0x5c,0x80,0xa0,0,0x5a,0x80,
0x93,0,0x10,0x11,0x92,0,0xf,0xe8,0xd8,0,0x6c,0x80,0xaf,0,0xdb,0xff,0x4b,0,
0x38,0x90,0xf,0,0x59,0x18,0x76,0,0x62,0xa5,0x15,0,0x61,0xcb,0xbb,0,0xc7,0x89,
0xb9,0,0x10,0x40,0xbd,0,0xd2,0xf2,4,0,0x49,0x75,0x27,0,0xeb,0xb6,0xf6,0,
0xdb,0x22,0xbb,0,0xa,0x14,0xaa,0,0x89,0x26,0x2f,0,0x64,0x83,0x76,0,9,0x3b,
0x33,0,0xe,0x94,0x1a,0,0x51,0x3a,0xaa,0,0x1d,0xa3,0xc2,0,0xaf,0xed,0xae,0,
0x5c,0x26,0x12,0,0x6d,0xc2,0x4d,0,0x2d,0x7a,0x9c,0,0xc0,0x56,0x97,0,3,0x3f,
0x83,0,9,0xf0,0xf6,0,0x2b,0x40,0x8c,0,0x6d,0x31,0x99,0,0x39,0xb4,7,0,
0xc,0x20,0x15,0,0xd8,0xc3,0x5b,0,0xf5,0x92,0xc4,0,0xc6,0xad,0x4b,0,0x4e,0xca,
0xa5,0,0xa7,0x37,0xcd,0,0xe6,0xa9,0x36,0,0xab,0x92,0x94,0,0xdd,0x42,0x68,0,
0x19,0x63,0xde,0,0x76,0x8c,0xef,0,0x68,0x8b,0x52,0,0xfc,0xdb,0x37,0,0xae,0xa1,
0xab,0,0xdf,0x15,0x31,0,0,0xae,0xa1,0,0xc,0xfb,0xda,0,0x64,0x4d,0x66,0,
0xed,5,0xb7,0,0x29,0x65,0x30,0,0x57,0x56,0xbf,0,0x47,0xff,0x3a,0,0x6a,0xf9,
0xb9,0,0x75,0xbe,0xf3,0,0x28,0x93,0xdf,0,0xab,0x80,0x30,0,0x66,0x8c,0xf6,0,
4,0xcb,0x15,0,0xfa,0x22,6,0,0xd9,0xe4,0x1d,0,0x3d,0xb3,0xa4,0,0x57,0x1b,
0x8f,0,0x36,0xcd,9,0,0x4e,0x42,0xe9,0,0x13,0xbe,0xa4,0,0x33,0x23,0xb5,0,
0xf0,0xaa,0x1a,0,0x4f,0x65,0xa8,0,0xd2,0xc1,0xa5,0,0xb,0x3f,0xf,0,0x5b,0x78,
0xcd,0,0x23,0xf9,0x76,0,0x7b,0x8b,4,0,0x89,0x17,0x72,0,0xc6,0xa6,0x53,0,
0x6f,0x6e,0xe2,0,0xef,0xeb,0,0,0x9b,0x4a,0x58,0,0xc4,0xda,0xb7,0,0xaa,0x66,
0xba,0,0x76,0xcf,0xcf,0,0xd1,2,0x1d,0,0xb1,0xf1,0x2d,0,0x8c,0x99,0xc1,0,
0xc3,0xad,0x77,0,0x86,0x48,0xda,0,0xf7,0x5d,0xa0,0,0xc6,0x80,0xf4,0,0xac,0xf0,
0x2f,0,0xdd,0xec,0x9a,0,0x3f,0x5c,0xbc,0,0xd0,0xde,0x6d,0,0x90,0xc7,0x1f,0,
0x2a,0xdb,0xb6,0,0xa3,0x25,0x3a,0,0,0xaf,0x9a,0,0xad,0x53,0x93,0,0xb6,0x57,
4,0,0x29,0x2d,0xb4,0,0x4b,0x80,0x7e,0,0xda,7,0xa7,0,0x76,0xaa,0xe,0,
0x7b,0x59,0xa1,0,0x16,0x12,0x2a,0,0xdc,0xb7,0x2d,0,0xfa,0xe5,0xfd,0,0x89,0xdb,
0xfe,0,0x89,0xbe,0xfd,0,0xe4,0x76,0x6c,0,6,0xa9,0xfc,0,0x3e,0x80,0x70,0,
0x85,0x6e,0x15,0,0xfd,0x87,0xff,0,0x28,0x3e,7,0,0x61,0x67,0x33,0,0x2a,0x18,
0x86,0,0x4d,0xbd,0xea,0,0xb3,0xe7,0xaf,0,0x8f,0x6d,0x6e,0,0x95,0x67,0x39,0,
0x31,0xbf,0x5b,0,0x84,0xd7,0x48,0,0x30,0xdf,0x16,0,0xc7,0x2d,0x43,0,0x25,0x61,
0x35,0,0xc9,0x70,0xce,0,0x30,0xcb,0xb8,0,0xbf,0x6c,0xfd,0,0xa4,0,0xa2,0,
5,0x6c,0xe4,0,0x5a,0xdd,0xa0,0,0x21,0x6f,0x47,0,0x62,0x12,0xd2,0,0xb9,0x5c,
0x84,0,0x70,0x61,0x49,0,0x6b,0x56,0xe0,0,0x99,0x52,1,0,0x50,0x55,0x37,0,
0x1e,0xd5,0xb7,0,0x33,0xf1,0xc4,0,0x13,0x6e,0x5f,0,0x5d,0x30,0xe4,0,0x85,0x2e,
0xa9,0,0x1d,0xb2,0xc3,0,0xa1,0x32,0x36,0,8,0xb7,0xa4,0,0xea,0xb1,0xd4,0,
0x16,0xf7,0x21,0,0x8f,0x69,0xe4,0,0x27,0xff,0x77,0,0xc,3,0x80,0,0x8d,0x40,
0x2d,0,0x4f,0xcd,0xa0,0,0x20,0xa5,0x99,0,0xb3,0xa2,0xd3,0,0x2f,0x5d,0xa,0,
0xb4,0xf9,0x42,0,0x11,0xda,0xcb,0,0x7d,0xbe,0xd0,0,0x9b,0xdb,0xc1,0,0xab,0x17,
0xbd,0,0xca,0xa2,0x81,0,8,0x6a,0x5c,0,0x2e,0x55,0x17,0,0x27,0,0x55,0,
0x7f,0x14,0xf0,0,0xe1,7,0x86,0,0x14,0xb,0x64,0,0x96,0x41,0x8d,0,0x87,0xbe,
0xde,0,0xda,0xfd,0x2a,0,0x6b,0x25,0xb6,0,0x7b,0x89,0x34,0,5,0xf3,0xfe,0,
0xb9,0xbf,0x9e,0,0x68,0x6a,0x4f,0,0x4a,0x2a,0xa8,0,0x4f,0xc4,0x5a,0,0x2d,0xf8,
0xbc,0,0xd7,0x5a,0x98,0,0xf4,0xc7,0x95,0,0xd,0x4d,0x8d,0,0x20,0x3a,0xa6,0,
0xa4,0x57,0x5f,0,0x14,0x3f,0xb1,0,0x80,0x38,0x95,0,0xcc,0x20,1,0,0x71,0xdd,
0x86,0,0xc9,0xde,0xb6,0,0xbf,0x60,0xf5,0,0x4d,0x65,0x11,0,1,7,0x6b,0,
0x8c,0xb0,0xac,0,0xb2,0xc0,0xd0,0,0x51,0x55,0x48,0,0x1e,0xfb,0xe,0,0x95,0x72,
0xc3,0,0xa3,6,0x3b,0,0xc0,0x40,0x35,0,6,0xdc,0x7b,0,0xe0,0x45,0xcc,0,
0x4e,0x29,0xfa,0,0xd6,0xca,0xc8,0,0xe8,0xf3,0x41,0,0x7c,0x64,0xde,0,0x9b,0x64,
0xd8,0,0xd9,0xbe,0x31,0,0xa4,0x97,0xc3,0,0x77,0x58,0xd4,0,0x69,0xe3,0xc5,0,
0xf0,0xda,0x13,0,0xba,0x3a,0x3c,0,0x46,0x18,0x46,0,0x55,0x75,0x5f,0,0xd2,0xbd,
0xf5,0,0x6e,0x92,0xc6,0,0xac,0x2e,0x5d,0,0xe,0x44,0xed,0,0x1c,0x3e,0x42,0,
0x61,0xc4,0x87,0,0x29,0xfd,0xe9,0,0xe7,0xd6,0xf3,0,0x22,0x7c,0xca,0,0x6f,0x91,
0x35,0,8,0xe0,0xc5,0,0xff,0xd7,0x8d,0,0x6e,0x6a,0xe2,0,0xb0,0xfd,0xc6,0,
0x93,8,0xc1,0,0x7c,0x5d,0x74,0,0x6b,0xad,0xb2,0,0xcd,0x6e,0x9d,0,0x3e,0x72,
0x7b,0,0xc6,0x11,0x6a,0,0xf7,0xcf,0xa9,0,0x29,0x73,0xdf,0,0xb5,0xc9,0xba,0,
0xb7,0,0x51,0,0xe2,0xb2,0xd,0,0x74,0xba,0x24,0,0xe5,0x7d,0x60,0,0x74,0xd8,
0x8a,0,0xd,0x15,0x2c,0,0x81,0x18,0xc,0,0x7e,0x66,0x94,0,1,0x29,0x16,0,
0x9f,0x7a,0x76,0,0xfd,0xfd,0xbe,0,0x56,0x45,0xef,0,0xd9,0x7e,0x36,0,0xec,0xd9,
0x13,0,0x8b,0xba,0xb9,0,0xc4,0x97,0xfc,0,0x31,0xa8,0x27,0,0xf1,0x6e,0xc3,0,
0x94,0xc5,0x36,0,0xd8,0xa8,0x56,0,0xb4,0xa8,0xb5,0,0xcf,0xcc,0xe,0,0x12,0x89,
0x2d,0,0x6f,0x57,0x34,0,0x2c,0x56,0x89,0,0x99,0xce,0xe3,0,0xd6,0x20,0xb9,0,
0x6b,0x5e,0xaa,0,0x3e,0x2a,0x9c,0,0x11,0x5f,0xcc,0,0xfd,0xb,0x4a,0,0xe1,0xf4,
0xfb,0,0x8e,0x3b,0x6d,0,0xe2,0x86,0x2c,0,0xe9,0xd4,0x84,0,0xfc,0xb4,0xa9,0,
0xef,0xee,0xd1,0,0x2e,0x35,0xc9,0,0x2f,0x39,0x61,0,0x38,0x21,0x44,0,0x1b,0xd9,
0xc8,0,0x81,0xfc,0xa,0,0xfb,0x4a,0x6a,0,0x2f,0x1c,0xd8,0,0x53,0xb4,0x84,0,
0x4e,0x99,0x8c,0,0x54,0x22,0xcc,0,0x2a,0x55,0xdc,0,0xc0,0xc6,0xd6,0,0xb,0x19,
0x96,0,0x1a,0x70,0xb8,0,0x69,0x95,0x64,0,0x26,0x5a,0x60,0,0x3f,0x52,0xee,0,
0x7f,0x11,0xf,0,0xf4,0xb5,0x11,0,0xfc,0xcb,0xf5,0,0x34,0xbc,0x2d,0,0x34,0xbc,
0xee,0,0xe8,0x5d,0xcc,0,0xdd,0x5e,0x60,0,0x67,0x8e,0x9b,0,0x92,0x33,0xef,0,
0xc9,0x17,0xb8,0,0x61,0x58,0x9b,0,0xe1,0x57,0xbc,0,0x51,0x83,0xc6,0,0xd8,0x3e,
0x10,0,0xdd,0x71,0x48,0,0x2d,0x1c,0xdd,0,0xaf,0x18,0xa1,0,0x21,0x2c,0x46,0,
0x59,0xf3,0xd7,0,0xd9,0x7a,0x98,0,0x9e,0x54,0xc0,0,0x4f,0x86,0xfa,0,0x56,6,
0xfc,0,0xe5,0x79,0xae,0,0x89,0x22,0x36,0,0x38,0xad,0x22,0,0x67,0x93,0xdc,0,
0x55,0xe8,0xaa,0,0x82,0x26,0x38,0,0xca,0xe7,0x9b,0,0x51,0xd,0xa4,0,0x99,0x33,
0xb1,0,0xa9,0xd7,0xe,0,0x69,5,0x48,0,0x65,0xb2,0xf0,0,0x7f,0x88,0xa7,0,
0x88,0x4c,0x97,0,0xf9,0xd1,0x36,0,0x21,0x92,0xb3,0,0x7b,0x82,0x4a,0,0x98,0xcf,
0x21,0,0x40,0x9f,0xdc,0,0xdc,0x47,0x55,0,0xe1,0x74,0x3a,0,0x67,0xeb,0x42,0,
0xfe,0x9d,0xdf,0,0x5e,0xd4,0x5f,0,0x7b,0x67,0xa4,0,0xba,0xac,0x7a,0,0x55,0xf6,
0xa2,0,0x2b,0x88,0x23,0,0x41,0xba,0x55,0,0x59,0x6e,8,0,0x21,0x2a,0x86,0,
0x39,0x47,0x83,0,0x89,0xe3,0xe6,0,0xe5,0x9e,0xd4,0,0x49,0xfb,0x40,0,0xff,0x56,
0xe9,0,0x1c,0xf,0xca,0,0xc5,0x59,0x8a,0,0x94,0xfa,0x2b,0,0xd3,0xc1,0xc5,0,
0xf,0xc5,0xcf,0,0xdb,0x5a,0xae,0,0x47,0xc5,0x86,0,0x85,0x43,0x62,0,0x21,0x86,
0x3b,0,0x2c,0x79,0x94,0,0x10,0x61,0x87,0,0x2a,0x4c,0x7b,0,0x80,0x2c,0x1a,0,
0x43,0xbf,0x12,0,0x88,0x26,0x90,0,0x78,0x3c,0x89,0,0xa8,0xc4,0xe4,0,0xe5,0xdb,
0x7b,0,0xc4,0x3a,0xc2,0,0x26,0xf4,0xea,0,0xf7,0x67,0x8a,0,0xd,0x92,0xbf,0,
0x65,0xa3,0x2b,0,0x3d,0x93,0xb1,0,0xbd,0x7c,0xb,0,0xa4,0x51,0xdc,0,0x27,0xdd,
0x63,0,0x69,0xe1,0xdd,0,0x9a,0x94,0x19,0,0xa8,0x29,0x95,0,0x68,0xce,0x28,0,
9,0xed,0xb4,0,0x44,0x9f,0x20,0,0x4e,0x98,0xca,0,0x70,0x82,0x63,0,0x7e,0x7c,
0x23,0,0xf,0xb9,0x32,0,0xa7,0xf5,0x8e,0,0x14,0x56,0xe7,0,0x21,0xf1,8,0,
0xb5,0x9d,0x2a,0,0x6f,0x7e,0x4d,0,0xa5,0x19,0x51,0,0xb5,0xf9,0xab,0,0x82,0xdf,
0xd6,0,0x96,0xdd,0x61,0,0x16,0x36,2,0,0xc4,0x3a,0x9f,0,0x83,0xa2,0xa1,0,
0x72,0xed,0x6d,0,0x39,0x8d,0x7a,0,0x82,0xb8,0xa9,0,0x6b,0x32,0x5c,0,0x46,0x27,
0x5b,0,0,0x34,0xed,0,0xd2,0,0x77,0,0xfc,0xf4,0x55,0,1,0x59,0x4d,0,
0xe0,0x71,0x80
};

const U8 m5_benchData2[]={
0x40,0xfb,0x21,0xf9,0x3f,0,0,0,0,0x2d,0x44,0x74,0x3e,0,0,0,0x80,0x98,
0x46,0xf8,0x3c,0,0,0,0x60,0x51,0xcc,0x78,0x3b,0,0,0,0x80,0x83,0x1b,0xf0,
0x39,0,0,0,0x40,0x20,0x25,0x7a,0x38,0,0,0,0x80,0x22,0x82,0xe3,0x36,0,
0,0,0,0x1d,0xf3,0x69,0x35,0x53,0x75,0x63,0x63,0x65,0x73,0x73,0,0x49,0x6c,0x6c,
0x65,0x67,0x61,0x6c,0x20,0x62,0x79,0x74,0x65,0x20,0x73,0x65,0x71,0x75,0x65,0x6e,0x63,0x65,
0,0x44,0x6f,0x6d,0x61,0x69,0x6e,0x20,0x65,0x72,0x72,0x6f,0x72,0,0x52,0x65,0x73,0x75,
0x6c,0x74,0x20,0x6e,0x6f,0x74,0x20,0x72,0x65,0x70,0x72,0x65,0x73,0x65,0x6e,0x74,0x61,0x62,
0x6c,0x65,0,0x4e,0x6f,0x74,0x20,0x61,0x20,0x74,0x74,0x79,0,0x50,0x65,0x72,0x6d,0x69,
0x73,0x73,0x69,0x6f,0x6e,0x20,0x64,0x65,0x6e,0x69,0x65,0x64,0,0x4f,0x70,0x65,0x72,0x61,
0x74,0x69,0x6f,0x6e,0x20,0x6e,0x6f,0x74,0x20,0x70,0x65,0x72,0x6d,0x69,0x74,0x74,0x65,0x64,
0,0x4e,0x6f,0x20,0x73,0x75,0x63,0x68,0x20,0x66,0x69,0x6c,0x65,0x20,0x6f,0x72,0x20,0x64,
0x69,0x72,0x65,0x63,0x74,0x6f,0x72,0x79,0,0x4e,0x6f,0x20,0x73,0x75,0x63,0x68,0x20,0x70,
0x72,0x6f,0x63,0x65,0x73,0x73,0,0x46,0x69,0x6c,0x65,0x20,0x65,0x78,0x69,0x73,0x74,0x73,
0,0x56,0x61,0x6c,0x75,0x65,0x20,0x74,0x6f,0x6f,0x20,0x6c,0x61,0x72,0x67,0x65,0x20,0x66,
0x6f,0x72,0x20,0x64,0x61,0x74,0x61,0x20,0x74,0x79,0x70,0x65,0,0x4e,0x6f,0x20,0x73,0x70,
0x61,0x63,0x65,0x20,0x6c,0x65,0x66,0x74,0x20,0x6f,0x6e,0x20,0x64,0x65,0x76,0x69,0x63,0x65,
0,0x4f,0x75,0x74,0x20,0x6f,0x66,0x20,0x6d,0x65,0x6d,0x6f,0x72,0x79,0,0x52,0x65,0x73,
0x6f,0x75,0x72,0x63,0x65,0x20,0x62,0x75,0x73,0x79,0,0x49,0x6e,0x74,0x65,0x72,0x72,0x75,
0x70,0x74,0x65,0x64,0x20,0x73,0x79,0x73,0x74,0x65,0x6d,0x20,0x63,0x61,0x6c,0x6c,0,0x52,
0x65,0x73,0x6f,0x75,0x72,0x63,0x65,0x20,0x74,0x65,0x6d,0x70,0x6f,0x72,0x61,0x72,0x69,0x6c,
0x79,0x20,0x75,0x6e,0x61,0x76,0x61,0x69,0x6c,0x61,0x62,0x6c,0x65,0,0x49,0x6e,0x76,0x61,
0x6c,0x69,0x64,0x20,0x73,0x65,0x65,0x6b,0,0x43,0x72,0x6f,0x73,0x73,0x2d,0x64,0x65,0x76,
0x69,0x63,0x65,0x20,0x6c,0x69,0x6e,0x6b,0,0x52,0x65,0x61,0x64,0x2d,0x6f,0x6e,0x6c,0x79,
0x20,0x66,0x69,0x6c,0x65,0x20,0x73,0x79,0x73,0x74,0x65,0x6d,0,0x44,0x69,0x72,0x65,0x63,
0x74,0x6f,0x72,0x79,0x20,0x6e,0x6f,0x74,0x20,0x65,0x6d,0x70,0x74,0x79,0,0x43,0x6f,0x6e,
0x6e,0x65,0x63,0x74,0x69,0x6f,0x6e,0x20,0x72,0x65,0x73,0x65,0x74,0x20,0x62,0x79,0x20,0x70,
0x65,0x65,0x72,0,0x4f,0x70,0x65,0x72,0x61,0x74,0x69,0x6f,0x6e,0x20,0x74,0x69,0x6d,0x65,
0x64,0x20,0x6f,0x75,0x74,0,0x43,0x6f,0x6e,0x6e,0x65,0x63,0x74,0x69,0x6f,0x6e,0x20,0x72,
0x65,0x66,0x75,0x73,0x65,0x64,0,0x48,0x6f,0x73,0x74,0x20,0x69,0x73,0x20,0x75,0x6e,0x72,
0x65,0x61,0x63,0x68,0x61,0x62,0x6c,0x65,0,0x41,0x64,0x64,0x72,0x65,0x73,0x73,0x20,0x69,
0x6e,0x20,0x75,0x73,0x65,0,0x42,0x72,0x6f,0x6b,0x65,0x6e,0x20,0x70,0x69,0x70,0x65,0,
0x49,0x2f,0x4f,0x20,0x65,0x72,0x72,0x6f,0x72,0,0x4e,0x6f,0x20,0x73,0x75,0x63,0x68,0x20,
0x64,0x65,0x76,0x69,0x63,0x65,0x20,0x6f,0x72,0x20,0x61,0x64,0x64,0x72,0x65,0x73,0x73,0,
0x4e,0x6f,0x20,0x73,0x75,0x63,0x68,0x20,0x64,0x65,0x76,0x69,0x63,0x65,0,0x4e,0x6f,0x74,
0x20,0x61,0x20,0x64,0x69,0x72,0x65,0x63,0x74,0x6f,0x72,0x79,0,0x49,0x73,0x20,0x61,0x20,
0x64,0x69,0x72,0x65,0x63,0x74,0x6f,0x72,0x79,0,0x54,0x65,0x78,0x74,0x20,0x66,0x69,0x6c,
0x65,0x20,0x62,0x75,0x73,0x79,0,0x45,0x78,0x65,0x63,0x20,0x66,0x6f,0x72,0x6d,0x61,0x74,
0x20,0x65,0x72,0x72,0x6f,0x72,0,0x49,0x6e,0x76,0x61,0x6c,0x69,0x64,0x20,0x61,0x72,0x67,
0x75,0x6d,0x65,0x6e,0x74,0,0x41,0x72,0x67,0x75,0x6d,0x65,0x6e,0x74,0x20,0x6c,0x69,0x73,
0x74,0x20,0x74,0x6f,0x6f,0x20,0x6c,0x6f,0x6e,0x67,0,0x53,0x79,0x6d,0x62,0x6f,0x6c,0x69,
0x63,0x20,0x6c,0x69,0x6e,0x6b,0x20,0x6c,0x6f,0x6f,0x70,0,0x46,0x69,0x6c,0x65,0x6e,0x61,
0x6d,0x65,0x20,0x74,0x6f,0x6f,0x20,0x6c,0x6f,0x6e,0x67,0,0x54,0x6f,0x6f,0x20,0x6d,0x61,
0x6e,0x79,0x20,0x6f,0x70,0x65,0x6e,0x20,0x66,0x69,0x6c,0x65,0x73,0x20,0x69,0x6e,0x20,0x73,
0x79,0x73,0x74,0x65,0x6d,0,0x4e,0x6f,0x20,0x66,0x69,0x6c,0x65,0x20,0x64,0x65,0x73,0x63,
0x72,0x69,0x70,0x74,0x6f,0x72,0x73,0x20,0x61,0x76,0x61,0x69,0x6c,0x61,0x62,0x6c,0x65,0,
0x42,0x61,0x64,0x20,0x66,0x69,0x6c,0x65,0x20,0x64,0x65,0x73,0x63,0x72,0x69,0x70,0x74,0x6f,
0x72,0,0x4e,0x6f,0x20,0x63,0x68,0x69,0x6c,0x64,0x20,0x70,0x72,0x6f,0x63,0x65,0x73,0x73,
0,0x42,0x61,0x64,0x20,0x61,0x64,0x64,0x72,0x65,0x73,0x73,0,0x46,0x69,0x6c,0x65,0x20,
0x74,0x6f,0x6f,0x20,0x6c,0x61,0x72,0x67,0x65,0,0x54,0x6f,0x6f,0x20,0x6d,0x61,0x6e,0x79,
0x20,0x6c,0x69,0x6e,0x6b,0x73,0,0x4e,0x6f,0x20,0x6c,0x6f,0x63,0x6b,0x73,0x20,0x61,0x76,
0x61,0x69,0x6c,0x61,0x62,0x6c,0x65,0,0x52,0x65,0x73,0x6f,0x75,0x72,0x63,0x65,0x20,0x64,
0x65,0x61,0x64,0x6c,0x6f,0x63,0x6b,0x20,0x77,0x6f,0x75,0x6c,0x64,0x20,0x6f,0x63,0x63,0x75,
0x72,0,0x53,0x74,0x61,0x74,0x65,0x20,0x6e,0x6f,0x74,0x20,0x72,0x65,0x63,0x6f,0x76,0x65,
0x72,0x61,0x62,0x6c,0x65,0,0x50,0x72,0x65,0x76,0x69,0x6f,0x75,0x73,0x20,0x6f,0x77,0x6e,
0x65,0x72,0x20,0x64,0x69,0x65,0x64,0,0x4f,0x70,0x65,0x72,0x61,0x74,0x69,0x6f,0x6e,0x20,
0x63,0x61,0x6e,0x63,0x65,0x6c,0x65,0x64,0,0x46,0x75,0x6e,0x63,0x74,0x69,0x6f,0x6e,0x20,
0x6e,0x6f,0x74,0x20,0x69,0x6d,0x70,0x6c,0x65,0x6d,0x65,0x6e,0x74,0x65,0x64,0,0x4e,0x6f,
0x20,0x6d,0x65,0x73,0x73,0x61,0x67,0x65,0x20,0x6f,0x66,0x20,0x64,0x65,0x73,0x69,0x72,0x65,
0x64,0x20,0x74,0x79,0x70,0x65,0,0x49,0x64,0x65,0x6e,0x74,0x69,0x66,0x69,0x65,0x72,0x20,
0x72,0x65,0x6d,0x6f,0x76,0x65,0x64,0,0x4c,0x69,0x6e,0x6b,0x20,0x68,0x61,0x73,0x20,0x62,
0x65,0x65,0x6e,0x20,0x73,0x65,0x76,0x65,0x72,0x65,0x64,0,0x50,0x72,0x6f,0x74,0x6f,0x63,
0x6f,0x6c,0x20,0x65,0x72,0x72,0x6f,0x72,0,0x42,0x61,0x64,0x20,0x6d,0x65,0x73,0x73,0x61,
0x67,0x65,0,0x4e,0x6f,0x74,0x20,0x61,0x20,0x73,0x6f,0x63,0x6b,0x65,0x74,0,0x44,0x65,
0x73,0x74,0x69,0x6e,0x61,0x74,0x69,0x6f,0x6e,0x20,0x61,0x64,0x64,0x72,0x65,0x73,0x73,0x20,
0x72,0x65,0x71,0x75,0x69,0x72,0x65,0x64,0,0x4d,0x65,0x73,0x73,0x61,0x67,0x65,0x20,0x74,
0x6f,0x6f,0x20,0x6c,0x61,0x72,0x67,0x65,0,0x50,0x72,0x6f,0x74,0x6f,0x63,0x6f,0x6c,0x20,
0x77,0x72,0x6f,0x6e,0x67,0x20,0x74,0x79,0x70,0x65,0x20,0x66,0x6f,0x72,0x20,0x73,0x6f,0x63,
0x6b,0x65,0x74,0,0x50,0x72,0x6f,0x74,0x6f,0x63,0x6f,0x6c,0x20,0x6e,0x6f,0x74,0x20,0x61,
0x76,0x61,0x69,0x6c,0x61,0x62,0x6c,0x65,0,0x50,0x72,0x6f,0x74,0x6f,0x63,0x6f,0x6c,0x20,
0x6e,0x6f,0x74,0x20,0x73,0x75,0x70,0x70,0x6f,0x72,0x74,0x65,0x64,0,0x4e,0x6f,0x74,0x20,
0x73,0x75,0x70,0x70,0x6f,0x72,0x74,0x65,0x64,0,0x41,0x64,0x64,0x72,0x65,0x73,0x73,0x20,
0x66,0x61,0x6d,0x69,0x6c,0x79,0x20,0x6e,0x6f,0x74,0x20,0x73,0x75,0x70,0x70,0x6f,0x72,0x74,
0x65,0x64,0x20,0x62,0x79,0x20,0x70,0x72,0x6f,0x74,0x6f,0x63,0x6f,0x6c,0,0x41,0x64,0x64,
0x72,0x65,0x73,0x73,0x20,0x6e,0x6f,0x74,0x20,0x61,0x76,0x61,0x69,0x6c,0x61,0x62,0x6c,0x65,
0,0x4e,0x65,0x74,0x77,0x6f,0x72,0x6b,0x20,0x69,0x73,0x20,0x64,0x6f,0x77,0x6e,0,0x4e,
0x65,0x74,0x77,0x6f,0x72,0x6b,0x20,0x75,0x6e,0x72,0x65,0x61,0x63,0x68,0x61,0x62,0x6c,0x65,
0,0x43,0x6f,0x6e,0x6e,0x65,0x63,0x74,0x69,0x6f,0x6e,0x20,0x72,0x65,0x73,0x65,0x74,0x20,
0x62,0x79,0x20,0x6e,0x65,0x74,0x77,0x6f,0x72,0x6b,0,0x43,0x6f,0x6e,0x6e,0x65,0x63,0x74,
0x69,0x6f,0x6e,0x20,0x61,0x62,0x6f,0x72,0x74,0x65,0x64,0,0x4e,0x6f,0x20,0x62,0x75,0x66,
0x66,0x65,0x72,0x20,0x73,0x70,0x61,0x63,0x65,0x20,0x61,0x76,0x61,0x69,0x6c,0x61,0x62,0x6c,
0x65,0,0x53,0x6f,0x63,0x6b,0x65,0x74,0x20,0x69,0x73,0x20,0x63,0x6f,0x6e,0x6e,0x65,0x63,
0x74,0x65,0x64,0,0x53,0x6f,0x63,0x6b,0x65,0x74,0x20,0x6e,0x6f,0x74,0x20,0x63,0x6f,0x6e,
0x6e,0x65,0x63,0x74,0x65,0x64,0,0x4f,0x70,0x65,0x72,0x61,0x74,0x69,0x6f,0x6e,0x20,0x61,
0x6c,0x72,0x65,0x61,0x64,0x79,0x20,0x69,0x6e,0x20,0x70,0x72,0x6f,0x67,0x72,0x65,0x73,0x73,
0,0x4f,0x70,0x65,0x72,0x61,0x74,0x69,0x6f,0x6e,0x20,0x69,0x6e,0x20,0x70,0x72,0x6f,0x67,
0x72,0x65,0x73,0x73,0,0x53,0x74,0x61,0x6c,0x65,0x20,0x66,0x69,0x6c,0x65,0x20,0x68,0x61,
0x6e,0x64,0x6c,0x65,0,0x51,0x75,0x6f,0x74,0x61,0x20,0x65,0x78,0x63,0x65,0x65,0x64,0x65,
0x64,0,0x4d,0x75,0x6c,0x74,0x69,0x68,0x6f,0x70,0x20,0x61,0x74,0x74,0x65,0x6d,0x70,0x74,
0x65,0x64,0,0x43,0x61,0x70,0x61,0x62,0x69,0x6c,0x69,0x74,0x69,0x65,0x73,0x20,0x69,0x6e,
0x73,0x75,0x66,0x66,0x69,0x63,0x69,0x65,0x6e,0x74,0,0,0,0x75,2,0x4e,0,0xd6,
1,0xe2,4,0xb9,4,0x18,1,0x8e,5,0xed,2,0x16,4,0xf2,0,0x97,3,1,
3,0x38,5,0xaf,1,0x82,1,0x4f,3,0x2f,4,0x1e,0,0xd4,5,0xa2,0,0x12,
3,0x1e,3,0xc2,1,0xde,3,8,0,0xac,5,0,1,0x64,2,0xf1,1,0x65,
5,0x34,2,0x8c,2,0xcf,2,0x2d,3,0x4c,4,0xe3,5,0x9f,2,0xf8,4,0x1c,
5,8,5,0xb1,2,0x4b,5,0x15,2,0x78,0,0x52,2,0x3c,3,0xf1,3,0xe4,
0,0xc3,3,0x7d,4,0xcc,0,0xaa,3,0x79,5,0x24,2,0x6e,1,0x6d,3,0x22,
4,0xab,4,0x44,0,0xfb,1,0xae,0,0x83,3,0x60,0,0xe5,1,7,4,0x94,
4,0x5e,4,0x2b,0,0x58,1,0x39,1,0x92,0,0xc2,5,0x9b,1,0x43,2,0x46,
1,0xf6,5,0,0,0,0,0,0,0x19,0,0xa,0,0x19,0x19,0x19,0,0,
0,0,5,0,0,0,0,0,0,9,0,0,0,0,0xb,0,0,0,
0,0,0,0,0,0x19,0,0x11,0xa,0x19,0x19,0x19,3,0xa,7,0,1,0x1b,
9,0xb,0x18,0,0,9,6,0xb,0,0,0xb,0,6,0x19,0,0,0,0x19,
0x19,0x19
};

const U8 m5_benchData3[]={
0xe,0,0,0,0,0,0,0,0,0x19,0,0xa,0xd,0x19,0x19,0x19,0,0xd,
0,0,2,0,9,0xe,0,0,0,9,0,0xe,0,0,0xe
};

const U8 m5_benchData4[]={0xc};

const U8 m5_benchData5[]={
0x13,0,0,0,0,0x13,0,0,0,0,9,0xc,0,0,0,0,0,0xc,
0,0,0xc
};

const U8 m5_benchData6[]={0x10};

const U8 m5_benchData7[]={
0xf,0,0,0,4,0xf,0,0,0,0,9,0x10,0,0,0,0,0,0x10,
0,0,0x10
};

const U8 m5_benchData8[]={0x12};

const U8 m5_benchData9[]={
0x11,0,0,0,0,0x11,0,0,0,0,9,0x12,0,0,0,0,0,0x12,
0,0,0x12,0,0,0x1a,0,0,0,0x1a,0x1a,0x1a
};

const U8 m5_benchData10[]={0x1a,0,0,0,0x1a,0x1a,0x1a,0,0,0,0,0,0,9};

const U8 m5_benchData11[]={0x14};

const U8 m5_benchData12[]={
0x17,0,0,0,0,0x17,0,0,0,0,9,0x14,0,0,0,0,0,0x14,
0,0,0x14
};

const U8 m5_benchData13[]={0x16};

const U8 m5_benchData14[]={
0x15,0,0,0,0,0x15,0,0,0,0,9,0x16,0,0,0,0,0,0x16,
0,0,0x16,0,0,0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37,0x38,0x39,0x41,0x42,0x43,
0x44,0x45,0x46
};

const U8 m5_benchData15[]={0x39,0x30,0,0,0,0,0,0,5};

const U8 m5_benchData16[]={7};

const U8 m5_benchData17[]={8,0,0,0,9,0,0,0,0x68,0xdb,0x2e,0,0,4};

const U8 m5_benchData18[]={1,0,0,0,0,0,0,0,0xa};

const U8 m5_benchData19[]={0x48,0x19,0,0,0,0,0,0,5};

const U8 m5_benchData20[]={7};

const U8 m5_benchData21[]={6,0,0,0,9,0,0,0,0x8c,0xdf,0x2e};

const U8 m5_benchData22[]={2,0,0,0,0,0,0,0,0xff,0xff,0xff,0xff};

const U8 m5_benchData23[]={0xc0,0x19};

void f5(wasmModuleInstance* instance) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 l0=0;
U32 l1=0;
F64 l2=0;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 l6=0;
U32 l7=0;
U32 l8=0;
U32 l9=0;
U32 l10=0;
F64 l11=0;
F64 l12=0;
U32 l13=0;
F64 l14=0;
U32 l15=0;
U32 l16=0;
U32 l17=0;
U64 l18=0;
U64 l19=0;
U32 l20=0;
U32 l21=0;
U32 l22=0;
U32 l23=0;
U32 l24=0;
U64 l25=0;
U32 l26=0;
U32 l27=0;
U32 l28=0;
F64 l29=0;
U32 l30=0;
U32 l31=0;
U64 l32=0;
U64 l33=0;
U64 l34=0;
U64 l35=0;
U64 l36=0;
U32 l37=0;
U32 l38=0;
U32 l39=0;
U32 l40=0;
U32 l41=0;
U32 l42=0;
U32 l43=0;
U32 l44=0;
U32 l45=0;
U32 si0,si1,si2,si3,si4,si5;
U64 sj0,sj1,sj2,sj3;
F64 sd0,sd1,sd2,sd3,sd4,sd5;
si0=6720U;
si0=i32_load(i->m0,si0);
si0=!(si0);
if(si0){
si0=6720U;
si1=1U;
i32_store(i->m0,si0,si1);
si0=i->g0;
si1=112U;
si0-=si1;
l7=si0;
i->g0=si0;
si0=1069U;
f19(instance,si0);
si0=l7;
si1=96U;
si0+=si1;
f11(instance,si0);
si0=l7;
sj0=i64_load32_s(i->m0,(WasmMemoryAddress)si0+104U);
l18=sj0;
si0=l7;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+96U);
l19=sj0;
si0=6737U;
l3=si0;
L2:;
W2C2_LOOP_START
{
si0=l7;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+80U,si1);
si0=l7;
si1=l4;
si2=-1640531535U;
si1*=si2;
si2=9973U;
si1=REM_U(si1,si2);
i32_store(i->m0,(WasmMemoryAddress)si0+84U,si1);
si0=i->g0;
si1=16U;
si0-=si1;
l17=si0;
i->g0=si0;
si0=l17;
si1=l7;
si2=80U;
si1+=si2;
l9=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=i->g0;
si1=128U;
si0-=si1;
l8=si0;
i->g0=si0;
si0=l8;
si1=l4;
si2=5U;
si1<<=(si2&31);
l5=si1;
si2=6736U;
si1+=si2;
l1=si1;
l0=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+116U,si1);
si0=l8;
si1=23U;
i32_store(i->m0,(WasmMemoryAddress)si0+120U,si1);
si0=l8;
si1=0U;
si2=112U;
si0=f37(instance,si0,si1,si2);
l8=si0;
si1=-1U;
i32_store(i->m0,(WasmMemoryAddress)si0+64U,si1);
si0=l8;
si1=10U;
i32_store(i->m0,(WasmMemoryAddress)si0+32U,si1);
si0=l8;
si1=l8;
si2=116U;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+68U,si1);
si0=l8;
si1=l8;
si2=127U;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+40U,si1);
si0=l0;
si1=0U;
i32_store8(i->m0,si0,si1);
si0=l8;
si1=1053U;
si2=l9;
f27(instance,si0,si1,si2);
si0=l8;
si1=128U;
si0+=si1;
i->g0=si0;
si0=l17;
si1=16U;
si0+=si1;
i->g0=si0;
si0=l5;
si1=6760U;
si0+=si1;
si1=l4;
i32_store(i->m0,si0,si1);
si0=l1;
si0=i32_load8_u(i->m0,si0);
l5=si0;
si0=!(si0);
if(si0){
si0=-2128831035U;
l8=si0;
goto L3;
}
L4:;
si0=-2128831035U;
l8=si0;
si0=l3;
l0=si0;
L5:;
W2C2_LOOP_START
{
si0=l8;
si1=l5;
si2=255U;
si1&=si2;
si0^=si1;
si1=16777619U;
si0*=si1;
l8=si0;
si0=l0;
si0=i32_load8_u(i->m0,si0);
l5=si0;
si0=l0;
si1=1U;
si0+=si1;
l0=si0;
si0=l5;
if(si0){
goto L5;
}
}
L3:;
si0=l1;
si1=l8;
si2=65535U;
si1&=si2;
si2=2U;
si1<<=(si2&31);
si2=1926736U;
si1+=si2;
l5=si1;
si1=i32_load(i->m0,si1);
i32_store(i->m0,(WasmMemoryAddress)si0+28U,si1);
si0=l5;
si1=l1;
i32_store(i->m0,si0,si1);
si0=l3;
si1=32U;
si0+=si1;
l3=si0;
si0=l4;
si1=1U;
si0+=si1;
l4=si0;
si1=60000U;
si0=si0 != si1;
if(si0){
goto L2;
}
}
L6:;
W2C2_LOOP_START
{
si0=6737U;
l3=si0;
si0=0U;
l5=si0;
L7:;
W2C2_LOOP_START
{
si0=l5;
l1=si0;
si1=5U;
si0<<=(si1&31);
si1=6736U;
si0+=si1;
l4=si0;
si0=i32_load8_u(i->m0,si0);
l5=si0;
si0=!(si0);
if(si0){
si0=-2128831035U;
l8=si0;
goto L8;
}
L9:;
si0=-2128831035U;
l8=si0;
si0=l3;
l0=si0;
L10:;
W2C2_LOOP_START
{
si0=l8;
si1=l5;
si2=255U;
si1&=si2;
si0^=si1;
si1=16777619U;
si0*=si1;
l8=si0;
si0=l0;
si0=i32_load8_u(i->m0,si0);
l5=si0;
si0=l0;
si1=1U;
si0+=si1;
l0=si0;
si0=l5;
if(si0){
goto L10;
}
}
L8:;
si0=l8;
si1=65535U;
si0&=si1;
si1=2U;
si0<<=(si1&31);
si1=1926736U;
si0+=si1;
si0=i32_load(i->m0,si0);
l5=si0;
si0=!(si0);
if(si0){
goto L11;
}
L12:;
W2C2_LOOP_START
{
si0=l4;
l9=si0;
si0=i32_load8_u(i->m0,si0);
l8=si0;
si0=l5;
l0=si0;
si0=i32_load8_u(i->m0,si0);
l17=si0;
si0=!(si0);
if(si0){
goto L13;
}
si0=l8;
si1=l17;
si0=si0 != si1;
if(si0){
goto L13;
}
si0=l0;
si1=1U;
si0+=si1;
l0=si0;
si0=l9;
si1=1U;
si0+=si1;
l9=si0;
L14:;
W2C2_LOOP_START
{
si0=l9;
si0=i32_load8_u(i->m0,si0);
l8=si0;
si0=l0;
si0=i32_load8_u(i->m0,si0);
l17=si0;
si0=!(si0);
if(si0){
goto L13;
}
si0=l0;
si1=1U;
si0+=si1;
l0=si0;
si0=l9;
si1=1U;
si0+=si1;
l9=si0;
si0=l8;
si1=l17;
si0=si0 == si1;
if(si0){
goto L14;
}
}
L13:;
si0=l17;
si1=l8;
si0-=si1;
if(si0){
si0=l5;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+28U);
l5=si0;
if(si0){
goto L12;
}
goto L11;
}
L15:;
}
si0=l5;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+24U);
si1=l24;
si0+=si1;
l24=si0;
L11:;
si0=l3;
si1=96U;
si0+=si1;
l3=si0;
si0=l1;
si1=3U;
si0+=si1;
l5=si0;
si0=l1;
si1=59997U;
si0=si0 < si1;
if(si0){
goto L7;
}
}
si0=l13;
si1=1U;
si0+=si1;
l13=si0;
si1=20U;
si0=si0 != si1;
if(si0){
goto L6;
}
}
si0=l7;
si1=96U;
si0+=si1;
f11(instance,si0);
si0=l7;
si1=l24;
i32_store(i->m0,(WasmMemoryAddress)si0+68U,si1);
si0=l7;
si1=l7;
sj1=i64_load32_s(i->m0,(WasmMemoryAddress)si1+104U);
sj2=W2C2_LL(1000U);
sj1=DIV_U(sj1,sj2);
sj2=l18;
sj3=W2C2_LL(1000U);
sj2=DIV_U(sj2,sj3);
sj1-=sj2;
si2=l7;
sj2=i64_load(i->m0,(WasmMemoryAddress)si2+96U);
sj3=l19;
sj2-=sj3;
sj3=W2C2_LL(1000000U);
sj2*=sj3;
sj1+=sj2;
l32=sj1;
i64_store32(i->m0,(WasmMemoryAddress)si0+64U,sj1);
si0=1181U;
si1=l7;
si2=-64U;
si1-=si2;
f15(instance,si0,si1);
si0=l7;
si1=96U;
si0+=si1;
f11(instance,si0);
si0=l7;
sj0=i64_load32_s(i->m0,(WasmMemoryAddress)si0+104U);
l18=sj0;
si0=l7;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+96U);
l19=sj0;
si0=6464U;
si0=i32_load(i->m0,si0);
l0=si0;
si0=-800000U;
l5=si0;
L16:;
W2C2_LOOP_START
{
si0=l5;
si1=2988880U;
si0+=si1;
si1=l0;
si2=13U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=17U;
si1>>=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=5U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
i32_store(i->m0,si0,si1);
si0=l5;
si1=2988884U;
si0+=si1;
si1=l0;
si2=13U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=17U;
si1>>=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=5U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
i32_store(i->m0,si0,si1);
si0=l5;
si1=8U;
si0+=si1;
l5=si0;
if(si0){
goto L16;
}
}
si0=6464U;
si1=l0;
i32_store(i->m0,si0,si1);
f34(instance);
si0=6464U;
si0=i32_load(i->m0,si0);
l0=si0;
si0=2588880U;
si0=i32_load(i->m0,si0);
l8=si0;
si0=2188880U;
si0=i32_load(i->m0,si0);
l4=si0;
si0=-800000U;
l5=si0;
L17:;
W2C2_LOOP_START
{
si0=l5;
si1=2988880U;
si0+=si1;
si1=l0;
si2=13U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=17U;
si1>>=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=5U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
i32_store(i->m0,si0,si1);
si0=l5;
si1=2988884U;
si0+=si1;
si1=l0;
si2=13U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=17U;
si1>>=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=5U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
i32_store(i->m0,si0,si1);
si0=l5;
si1=8U;
si0+=si1;
l5=si0;
if(si0){
goto L17;
}
}
si0=6464U;
si1=l0;
i32_store(i->m0,si0,si1);
f34(instance);
si0=6464U;
si0=i32_load(i->m0,si0);
l0=si0;
si0=2588880U;
si0=i32_load(i->m0,si0);
l1=si0;
si0=2188908U;
si0=i32_load(i->m0,si0);
l3=si0;
si0=-800000U;
l5=si0;
L18:;
W2C2_LOOP_START
{
si0=l5;
si1=2988880U;
si0+=si1;
si1=l0;
si2=13U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=17U;
si1>>=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=5U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
i32_store(i->m0,si0,si1);
si0=l5;
si1=2988884U;
si0+=si1;
si1=l0;
si2=13U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=17U;
si1>>=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=5U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
i32_store(i->m0,si0,si1);
si0=l5;
si1=8U;
si0+=si1;
l5=si0;
if(si0){
goto L18;
}
}
si0=6464U;
si1=l0;
i32_store(i->m0,si0,si1);
f34(instance);
si0=6464U;
si0=i32_load(i->m0,si0);
l0=si0;
si0=2588880U;
si0=i32_load(i->m0,si0);
l24=si0;
si0=2188936U;
si0=i32_load(i->m0,si0);
l13=si0;
si0=-800000U;
l5=si0;
L19:;
W2C2_LOOP_START
{
si0=l5;
si1=2988880U;
si0+=si1;
si1=l0;
si2=13U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=17U;
si1>>=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=5U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
i32_store(i->m0,si0,si1);
si0=l5;
si1=2988884U;
si0+=si1;
si1=l0;
si2=13U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=17U;
si1>>=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=5U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
i32_store(i->m0,si0,si1);
si0=l5;
si1=8U;
si0+=si1;
l5=si0;
if(si0){
goto L19;
}
}
si0=6464U;
si1=l0;
i32_store(i->m0,si0,si1);
f34(instance);
si0=6464U;
si0=i32_load(i->m0,si0);
l0=si0;
si0=2588880U;
si0=i32_load(i->m0,si0);
l17=si0;
si0=2188964U;
si0=i32_load(i->m0,si0);
l9=si0;
si0=-800000U;
l5=si0;
L20:;
W2C2_LOOP_START
{
si0=l5;
si1=2988880U;
si0+=si1;
si1=l0;
si2=13U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=17U;
si1>>=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=5U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
i32_store(i->m0,si0,si1);
si0=l5;
si1=2988884U;
si0+=si1;
si1=l0;
si2=13U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=17U;
si1>>=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=5U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
i32_store(i->m0,si0,si1);
si0=l5;
si1=8U;
si0+=si1;
l5=si0;
if(si0){
goto L20;
}
}
si0=6464U;
si1=l0;
i32_store(i->m0,si0,si1);
f34(instance);
si0=2588880U;
si0=i32_load(i->m0,si0);
l5=si0;
si0=2188992U;
si0=i32_load(i->m0,si0);
l0=si0;
si0=l7;
si1=96U;
si0+=si1;
f11(instance,si0);
si0=l7;
si1=l1;
si2=l3;
si1^=si2;
si2=l4;
si3=l8;
si2^=si3;
si1+=si2;
si2=l13;
si3=l24;
si2^=si3;
si1+=si2;
si2=l9;
si3=l17;
si2^=si3;
si1+=si2;
si2=l0;
si3=l5;
si2^=si3;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+52U,si1);
si0=l7;
si1=l7;
sj1=i64_load32_s(i->m0,(WasmMemoryAddress)si1+104U);
sj2=W2C2_LL(1000U);
sj1=DIV_U(sj1,sj2);
sj2=l18;
sj3=W2C2_LL(1000U);
sj2=DIV_U(sj2,sj3);
sj1-=sj2;
si2=l7;
sj2=i64_load(i->m0,(WasmMemoryAddress)si2+96U);
sj3=l19;
sj2-=sj3;
sj3=W2C2_LL(1000000U);
sj2*=sj3;
sj1+=sj2;
l33=sj1;
i64_store32(i->m0,(WasmMemoryAddress)si0+48U,sj1);
si0=1211U;
si1=l7;
si2=48U;
si1+=si2;
f15(instance,si0,si1);
si0=l7;
si1=96U;
si0+=si1;
f11(instance,si0);
si0=l7;
sj0=i64_load32_s(i->m0,(WasmMemoryAddress)si0+104U);
l18=sj0;
si0=l7;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+96U);
l19=sj0;
si0=6464U;
si0=i32_load(i->m0,si0);
l0=si0;
si0=-81920U;
l5=si0;
L21:;
W2C2_LOOP_START
{
si0=l5;
si1=3070800U;
si0+=si1;
si1=l0;
si2=13U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=17U;
si1>>=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=5U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=100U;
si1=REM_U(si1,si2);
si2=50U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l5;
si1=3070804U;
si0+=si1;
si1=l0;
si2=13U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=17U;
si1>>=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=5U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=20U;
si1=REM_U(si1,si2);
i32_store(i->m0,si0,si1);
si0=l5;
si1=3070808U;
si0+=si1;
si1=l0;
si2=13U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=17U;
si1>>=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=5U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=5U;
si1=REM_U(si1,si2);
i32_store(i->m0,si0,si1);
si0=l5;
si1=3070812U;
si0+=si1;
si1=l0;
si2=13U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=17U;
si1>>=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=3U;
si1&=si2;
si2=2U;
si1<<=(si2&31);
si2=1392U;
si1+=si2;
si1=i32_load(i->m0,si1);
i32_store(i->m0,si0,si1);
si0=l5;
si1=3070816U;
si0+=si1;
si1=l0;
si2=5U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=13U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=17U;
si1>>=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=5U;
si1<<=(si2&31);
si2=l0;
si1^=si2;
l0=si1;
si2=4095U;
si1&=si2;
si2=20U;
si1*=si2;
si2=2988880U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l5;
si1=20U;
si0+=si1;
l5=si0;
if(si0){
goto L21;
}
}
si0=6464U;
si1=l0;
i32_store(i->m0,si0,si1);
si0=0U;
l4=si0;
si0=0U;
l1=si0;
L22:;
W2C2_LOOP_START
{
si0=l1;
si1=20U;
si0*=si1;
si1=2988880U;
si0+=si1;
l5=si0;
si0=100000U;
l8=si0;
L23:;
W2C2_LOOP_START
{
si0=l5;
si1=l5;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
si2=6U;
si1+=si2;
si2=l5;
si2=i32_load(i->m0,(WasmMemoryAddress)si2+12U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si2,&m5_benchFuncType1);
si0=((U32 (*)(wasmModuleInstance*,U32,U32))entry.func)(entry.instance,si0,si1);
}
l0=si0;
si0=l5;
si1=l5;
si1=i32_load(i->m0,si1);
si2=l0;
si1-=si2;
l0=si1;
i32_store(i->m0,si0,si1);
si0=l0;
si1=0U;
si0=(U32)((I32)si0<=(I32)si1);
if(si0){
si0=l5;
si1=l0;
si2=80U;
si1+=si2;
l0=si1;
i32_store(i->m0,si0,si1);
si0=l5;
si1=l5;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=5U;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
}
L24:;
si0=l0;
si1=l4;
si0+=si1;
l4=si0;
si0=l5;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
l5=si0;
si0=l8;
si1=1U;
si0-=si1;
l8=si0;
if(si0){
goto L23;
}
}
si0=l1;
si1=1U;
si0+=si1;
l1=si0;
si1=30U;
si0=si0 != si1;
if(si0){
goto L22;
}
}
si0=l7;
si1=96U;
si0+=si1;
f11(instance,si0);
si0=l7;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+36U,si1);
si0=l7;
si1=l7;
sj1=i64_load32_s(i->m0,(WasmMemoryAddress)si1+104U);
sj2=W2C2_LL(1000U);
sj1=DIV_U(sj1,sj2);
sj2=l18;
sj3=W2C2_LL(1000U);
sj2=DIV_U(sj2,sj3);
sj1-=sj2;
si2=l7;
sj2=i64_load(i->m0,(WasmMemoryAddress)si2+96U);
sj3=l19;
sj2-=sj3;
sj3=W2C2_LL(1000000U);
sj2*=sj3;
sj1+=sj2;
l34=sj1;
i64_store32(i->m0,(WasmMemoryAddress)si0+32U,sj1);
si0=1151U;
si1=l7;
si2=32U;
si1+=si2;
f15(instance,si0,si1);
si0=l7;
si1=96U;
si0+=si1;
f11(instance,si0);
si0=l7;
sj0=i64_load32_s(i->m0,(WasmMemoryAddress)si0+104U);
l35=sj0;
si0=l7;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+96U);
l36=sj0;
sj0=W2C2_LL(-7046029254386353131U);
l19=sj0;
si0=0U;
l0=si0;
L25:;
W2C2_LOOP_START
{
si0=1U;
l5=si0;
sj0=W2C2_LL(1U);
l18=sj0;
L26:;
W2C2_LOOP_START
{
sj0=l18;
sj1=l19;
sj2=W2C2_LL(6364136223846793005U);
sj1*=sj2;
sj0+=sj1;
l19=sj0;
sj1=W2C2_LL(29U);
sj0>>=(sj1&63);
sj1=l19;
sj0^=sj1;
l19=sj0;
sj1=l19;
sj2=l18;
sj3=W2C2_LL(4294967294U);
sj2&=sj3;
sj3=W2C2_LL(1U);
sj2|=sj3;
sj1=DIV_U(sj1,sj2);
sj0+=sj1;
l19=sj0;
sd0=l11;
si1=i->g0;
si2=16U;
si1-=si2;
l9=si1;
i->g0=si1;
si1=l5;
sd1=(F64)(si1);
sd2=f64_reinterpret_i64(W2C2_LL(0x3F50624DD2F1A9FCU));
sd1*=sd2;
l14=sd1;
sj1=i64_reinterpret_f64(sd1);
sj2=W2C2_LL(32U);
sj1>>=(sj2&63);
si1=(U32)(sj1);
si2=2147483647U;
si1&=si2;
l3=si1;
si2=1072243195U;
si1=si1 <= si2;
if(si1){
si1=l3;
si2=1045430272U;
si1=si1 < si2;
if(si1){
goto L28;
}
sd1=l14;
sd2=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
si3=0U;
sd1=f12(instance,sd1,sd2,si3);
l14=sd1;
goto L28;
}
L29:;
si1=l3;
si2=2146435072U;
si1=si1 >= si2;
if(si1){
sd1=l14;
sd2=l14;
sd1-=sd2;
l14=sd1;
goto L28;
}
L30:;
si1=0U;
l4=si1;
si1=i->g0;
si2=48U;
si1-=si2;
l21=si1;
i->g0=si1;
sd1=l14;
l2=sd1;
sj1=i64_reinterpret_f64(sd1);
l25=sj1;
sj2=W2C2_LL(32U);
sj1>>=(sj2&63);
si1=(U32)(sj1);
l3=si1;
si2=2147483647U;
si1&=si2;
l1=si1;
si2=1074752122U;
si1=si1 <= si2;
if(si1){
si1=l3;
si2=1048575U;
si1&=si2;
si2=598523U;
si1=si1 == si2;
if(si1){
goto L33;
}
si1=l1;
si2=1073928572U;
si1=si1 <= si2;
if(si1){
sj1=l25;
sj2=W2C2_LL(0U);
si1=(U64)((I64)sj1>=(I64)sj2);
if(si1){
si1=l9;
sd2=l2;
sd3=f64_reinterpret_i64(W2C2_LL(0xBFF921FB54400000U));
sd2+=sd3;
l2=sd2;
sd3=f64_reinterpret_i64(W2C2_LL(0xBDD0B4611A626331U));
sd2+=sd3;
l12=sd2;
f64_store(i->m0,si1,sd2);
si1=l9;
sd2=l2;
sd3=l12;
sd2-=sd3;
sd3=f64_reinterpret_i64(W2C2_LL(0xBDD0B4611A626331U));
sd2+=sd3;
f64_store(i->m0,(WasmMemoryAddress)si1+8U,sd2);
si1=1U;
l3=si1;
goto L31;
}
L36:;
si1=l9;
sd2=l2;
sd3=f64_reinterpret_i64(W2C2_LL(0x3FF921FB54400000U));
sd2+=sd3;
l2=sd2;
sd3=f64_reinterpret_i64(W2C2_LL(0x3DD0B4611A626331U));
sd2+=sd3;
l12=sd2;
f64_store(i->m0,si1,sd2);
si1=l9;
sd2=l2;
sd3=l12;
sd2-=sd3;
sd3=f64_reinterpret_i64(W2C2_LL(0x3DD0B4611A626331U));
sd2+=sd3;
f64_store(i->m0,(WasmMemoryAddress)si1+8U,sd2);
si1=-1U;
l3=si1;
goto L31;
}
L35:;
sj1=l25;
sj2=W2C2_LL(0U);
si1=(U64)((I64)sj1>=(I64)sj2);
if(si1){
si1=l9;
sd2=l2;
sd3=f64_reinterpret_i64(W2C2_LL(0xC00921FB54400000U));
sd2+=sd3;
l2=sd2;
sd3=f64_reinterpret_i64(W2C2_LL(0xBDE0B4611A626331U));
sd2+=sd3;
l12=sd2;
f64_store(i->m0,si1,sd2);
si1=l9;
sd2=l2;
sd3=l12;
sd2-=sd3;
sd3=f64_reinterpret_i64(W2C2_LL(0xBDE0B4611A626331U));
sd2+=sd3;
f64_store(i->m0,(WasmMemoryAddress)si1+8U,sd2);
si1=2U;
l3=si1;
goto L31;
}
L37:;
si1=l9;
sd2=l2;
sd3=f64_reinterpret_i64(W2C2_LL(0x400921FB54400000U));
sd2+=sd3;
l2=sd2;
sd3=f64_reinterpret_i64(W2C2_LL(0x3DE0B4611A626331U));
sd2+=sd3;
l12=sd2;
f64_store(i->m0,si1,sd2);
si1=l9;
sd2=l2;
sd3=l12;
sd2-=sd3;
sd3=f64_reinterpret_i64(W2C2_LL(0x3DE0B4611A626331U));
sd2+=sd3;
f64_store(i->m0,(WasmMemoryAddress)si1+8U,sd2);
si1=-2U;
l3=si1;
goto L31;
}
L34:;
si1=l1;
si2=1075594811U;
si1=si1 <= si2;
if(si1){
si1=l1;
si2=1075183036U;
si1=si1 <= si2;
if(si1){
si1=l1;
si2=1074977148U;
si1=si1 == si2;
if(si1){
goto L33;
}
sj1=l25;
sj2=W2C2_LL(0U);
si1=(U64)((I64)sj1>=(I64)sj2);
if(si1){
si1=l9;
sd2=l2;
sd3=f64_reinterpret_i64(W2C2_LL(0xC012D97C7F300000U));
sd2+=sd3;
l2=sd2;
sd3=f64_reinterpret_i64(W2C2_LL(0xBDE90E91A79394CAU));
sd2+=sd3;
l12=sd2;
f64_store(i->m0,si1,sd2);
si1=l9;
sd2=l2;
sd3=l12;
sd2-=sd3;
sd3=f64_reinterpret_i64(W2C2_LL(0xBDE90E91A79394CAU));
sd2+=sd3;
f64_store(i->m0,(WasmMemoryAddress)si1+8U,sd2);
si1=3U;
l3=si1;
goto L31;
}
L40:;
si1=l9;
sd2=l2;
sd3=f64_reinterpret_i64(W2C2_LL(0x4012D97C7F300000U));
sd2+=sd3;
l2=sd2;
sd3=f64_reinterpret_i64(W2C2_LL(0x3DE90E91A79394CAU));
sd2+=sd3;
l12=sd2;
f64_store(i->m0,si1,sd2);
si1=l9;
sd2=l2;
sd3=l12;
sd2-=sd3;
sd3=f64_reinterpret_i64(W2C2_LL(0x3DE90E91A79394CAU));
sd2+=sd3;
f64_store(i->m0,(WasmMemoryAddress)si1+8U,sd2);
si1=-3U;
l3=si1;
goto L31;
}
L39:;
si1=l1;
si2=1075388923U;
si1=si1 == si2;
if(si1){
goto L33;
}
sj1=l25;
sj2=W2C2_LL(0U);
si1=(U64)((I64)sj1>=(I64)sj2);
if(si1){
si1=l9;
sd2=l2;
sd3=f64_reinterpret_i64(W2C2_LL(0xC01921FB54400000U));
sd2+=sd3;
l2=sd2;
sd3=f64_reinterpret_i64(W2C2_LL(0xBDF0B4611A626331U));
sd2+=sd3;
l12=sd2;
f64_store(i->m0,si1,sd2);
si1=l9;
sd2=l2;
sd3=l12;
sd2-=sd3;
sd3=f64_reinterpret_i64(W2C2_LL(0xBDF0B4611A626331U));
sd2+=sd3;
f64_store(i->m0,(WasmMemoryAddress)si1+8U,sd2);
si1=4U;
l3=si1;
goto L31;
}
L41:;
si1=l9;
sd2=l2;
sd3=f64_reinterpret_i64(W2C2_LL(0x401921FB54400000U));
sd2+=sd3;
l2=sd2;
sd3=f64_reinterpret_i64(W2C2_LL(0x3DF0B4611A626331U));
sd2+=sd3;
l12=sd2;
f64_store(i->m0,si1,sd2);
si1=l9;
sd2=l2;
sd3=l12;
sd2-=sd3;
sd3=f64_reinterpret_i64(W2C2_LL(0x3DF0B4611A626331U));
sd2+=sd3;
f64_store(i->m0,(WasmMemoryAddress)si1+8U,sd2);
si1=-4U;
l3=si1;
goto L31;
}
L38:;
si1=l1;
si2=1094263290U;
si1=si1 > si2;
if(si1){
goto L32;
}
L33:;
si1=l9;
sd2=l2;
sd3=l2;
sd4=f64_reinterpret_i64(W2C2_LL(0x3FE45F306DC9C883U));
sd3*=sd4;
sd4=f64_reinterpret_i64(W2C2_LL(0x4338000000000000U));
sd3+=sd4;
sd4=f64_reinterpret_i64(W2C2_LL(0xC338000000000000U));
sd3+=sd4;
l12=sd3;
sd4=f64_reinterpret_i64(W2C2_LL(0xBFF921FB54400000U));
sd3*=sd4;
sd2+=sd3;
l11=sd2;
sd3=l12;
sd4=f64_reinterpret_i64(W2C2_LL(0x3DD0B4611A626331U));
sd3*=sd4;
l29=sd3;
sd2-=sd3;
l2=sd2;
f64_store(i->m0,si1,sd2);
si1=l1;
si2=20U;
si1>>=(si2&31);
l1=si1;
sd2=l2;
sj2=i64_reinterpret_f64(sd2);
sj3=W2C2_LL(52U);
sj2>>=(sj3&63);
si2=(U32)(sj2);
si3=2047U;
si2&=si3;
si1-=si2;
si2=17U;
si1=(U32)((I32)si1<(I32)si2);
l15=si1;
sd1=l12;
sd1=fabs(sd1);
sd2=f64_reinterpret_i64(W2C2_LL(0x41E0000000000000U));
si1=sd1 < sd2;
if(si1){
sd1=l12;
si1=I32_TRUNC_S_F64(sd1);
goto L42;
}
L43:;
si1=-2147483648U;
L42:;
l3=si1;
si1=l15;
if(si1){
goto L44;
}
si1=l9;
sd2=l11;
sd3=l12;
sd4=f64_reinterpret_i64(W2C2_LL(0x3DD0B4611A600000U));
sd3*=sd4;
l2=sd3;
sd2-=sd3;
l14=sd2;
sd3=l12;
sd4=f64_reinterpret_i64(W2C2_LL(0x3BA3198A2E037073U));
sd3*=sd4;
sd4=l11;
sd5=l14;
sd4-=sd5;
sd5=l2;
sd4-=sd5;
sd3-=sd4;
l29=sd3;
sd2-=sd3;
l2=sd2;
f64_store(i->m0,si1,sd2);
si1=l1;
sd2=l2;
sj2=i64_reinterpret_f64(sd2);
sj3=W2C2_LL(52U);
sj2>>=(sj3&63);
si2=(U32)(sj2);
si3=2047U;
si2&=si3;
si1-=si2;
si2=50U;
si1=(U32)((I32)si1<(I32)si2);
if(si1){
sd1=l14;
l11=sd1;
goto L44;
}
L45:;
si1=l9;
sd2=l14;
sd3=l12;
sd4=f64_reinterpret_i64(W2C2_LL(0x3BA3198A2E000000U));
sd3*=sd4;
l2=sd3;
sd2-=sd3;
l11=sd2;
sd3=l12;
sd4=f64_reinterpret_i64(W2C2_LL(0x397B839A252049C1U));
sd3*=sd4;
sd4=l14;
sd5=l11;
sd4-=sd5;
sd5=l2;
sd4-=sd5;
sd3-=sd4;
l29=sd3;
sd2-=sd3;
l2=sd2;
f64_store(i->m0,si1,sd2);
L44:;
si1=l9;
sd2=l11;
sd3=l2;
sd2-=sd3;
sd3=l29;
sd2-=sd3;
f64_store(i->m0,(WasmMemoryAddress)si1+8U,sd2);
goto L31;
L32:;
si1=l1;
si2=2146435072U;
si1=si1 >= si2;
if(si1){
si1=l9;
sd2=l2;
sd3=l2;
sd2-=sd3;
l2=sd2;
f64_store(i->m0,si1,sd2);
si1=l9;
sd2=l2;
f64_store(i->m0,(WasmMemoryAddress)si1+8U,sd2);
si1=0U;
l3=si1;
goto L31;
}
L46:;
si1=l21;
sj2=l25;
sj3=W2C2_LL(4503599627370495U);
sj2&=sj3;
sj3=W2C2_LL(4710765210229538816U);
sj2|=sj3;
sd2=f64_reinterpret_i64(sj2);
l2=sd2;
sd2=fabs(sd2);
sd3=f64_reinterpret_i64(W2C2_LL(0x41E0000000000000U));
si2=sd2 < sd3;
if(si2){
sd2=l2;
si2=I32_TRUNC_S_F64(sd2);
goto L47;
}
L48:;
si2=-2147483648U;
L47:;
sd2=(F64)(I32)(si2);
l12=sd2;
f64_store(i->m0,(WasmMemoryAddress)si1+16U,sd2);
si1=l21;
sd2=l2;
sd3=l12;
sd2-=sd3;
sd3=f64_reinterpret_i64(W2C2_LL(0x4170000000000000U));
sd2*=sd3;
l2=sd2;
sd2=fabs(sd2);
sd3=f64_reinterpret_i64(W2C2_LL(0x41E0000000000000U));
si2=sd2 < sd3;
if(si2){
sd2=l2;
si2=I32_TRUNC_S_F64(sd2);
goto L49;
}
L50:;
si2=-2147483648U;
L49:;
sd2=(F64)(I32)(si2);
l12=sd2;
f64_store(i->m0,(WasmMemoryAddress)si1+24U,sd2);
si1=l21;
sd2=l2;
sd3=l12;
sd2-=sd3;
sd3=f64_reinterpret_i64(W2C2_LL(0x4170000000000000U));
sd2*=sd3;
f64_store(i->m0,(WasmMemoryAddress)si1+32U,sd2);
si1=l21;
si2=32U;
si1+=si2;
l3=si1;
si1=4U;
l15=si1;
L51:;
W2C2_LOOP_START
{
si1=l15;
si2=1U;
si1-=si2;
l15=si1;
si1=l3;
sd1=f64_load(i->m0,si1);
l2=sd1;
si1=l3;
si2=8U;
si1-=si2;
l3=si1;
sd1=l2;
sd2=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
si1=sd1 == sd2;
if(si1){
goto L51;
}
}
si1=l21;
si2=16U;
si1+=si2;
l24=si1;
si1=i->g0;
si2=560U;
si1-=si2;
l10=si1;
i->g0=si1;
si1=l1;
si2=20U;
si1>>=(si2&31);
si2=1046U;
si1-=si2;
l1=si1;
si2=l1;
si3=3U;
si2-=si3;
si3=24U;
si2=I32_DIV_S(si2,si3);
l6=si2;
si3=0U;
si4=l6;
si5=0U;
si4=(U32)((I32)si4>(I32)si5);
si2=si4?si2:si3;
l30=si2;
si3=-24U;
si2*=si3;
si1+=si2;
l8=si1;
si1=1428U;
si1=i32_load(i->m0,si1);
l23=si1;
si2=l15;
si3=1U;
si2-=si3;
l28=si2;
si1+=si2;
l1=si1;
si2=0U;
si1=(U32)((I32)si1<(I32)si2);
if(si1){
goto L52;
}
si1=l30;
si2=l28;
si1-=si2;
l3=si1;
si1=l15;
si2=l23;
si1+=si2;
l20=si1;
si2=1U;
si1&=si2;
l16=si1;
si1=l1;
if(si1){
si1=l30;
si2=2U;
si1<<=(si2&31);
si2=l15;
si3=2U;
si2<<=(si3&31);
si1-=si2;
si2=1448U;
si1+=si2;
l6=si1;
si1=l20;
si2=-2U;
si1&=si2;
l13=si1;
si1=l10;
si2=320U;
si1+=si2;
l1=si1;
L54:;
W2C2_LOOP_START
{
si1=l1;
si2=l3;
si3=l4;
si2+=si3;
l20=si2;
si3=0U;
si2=(U32)((I32)si2>=(I32)si3);
if(si2){
si2=l6;
si3=4U;
si2-=si3;
si2=i32_load(i->m0,si2);
sd2=(F64)(I32)(si2);
}else{
sd2=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
}
L55:;
f64_store(i->m0,si1,sd2);
si1=l1;
si2=8U;
si1+=si2;
si2=l20;
si3=-1U;
si2=(U32)((I32)si2>=(I32)si3);
if(si2){
si2=l6;
si2=i32_load(i->m0,si2);
sd2=(F64)(I32)(si2);
}else{
sd2=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
}
L56:;
f64_store(i->m0,si1,sd2);
si1=l1;
si2=16U;
si1+=si2;
l1=si1;
si1=l6;
si2=8U;
si1+=si2;
l6=si1;
si1=l13;
si2=l4;
si3=2U;
si2+=si3;
l4=si2;
si1=si1 != si2;
if(si1){
goto L54;
}
}
si1=l3;
si2=l4;
si1+=si2;
l3=si1;
}
L53:;
si1=l16;
si1=!(si1);
if(si1){
goto L52;
}
si1=l10;
si2=320U;
si1+=si2;
si2=l4;
si3=3U;
si2<<=(si3&31);
si1+=si2;
si2=l3;
si3=0U;
si2=(U32)((I32)si2<(I32)si3);
if(si2){
sd2=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
}else{
si2=l3;
si3=2U;
si2<<=(si3&31);
si3=1440U;
si2+=si3;
si2=i32_load(i->m0,si2);
sd2=(F64)(I32)(si2);
}
L57:;
f64_store(i->m0,si1,sd2);
L52:;
si1=l8;
si2=24U;
si1-=si2;
l17=si1;
si1=0U;
l1=si1;
si1=l23;
si2=0U;
si3=l23;
si4=0U;
si3=(U32)((I32)si3>(I32)si4);
si1=si3?si1:si2;
l16=si1;
si1=l15;
si2=2147483646U;
si1&=si2;
l20=si1;
si1=l15;
si2=1U;
si1&=si2;
l22=si1;
si1=l15;
si2=3U;
si1<<=(si2&31);
si2=l10;
si1+=si2;
si2=304U;
si1+=si2;
l13=si1;
L58:;
W2C2_LOOP_START
{
si1=l1;
l3=si1;
si1=l15;
si2=0U;
si1=(U32)((I32)si1<=(I32)si2);
if(si1){
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
l2=sd1;
goto L59;
}
L60:;
si1=0U;
l6=si1;
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
l2=sd1;
si1=l28;
if(si1){
si1=l13;
l1=si1;
si1=l24;
l4=si1;
L62:;
W2C2_LOOP_START
{
si1=l4;
si2=8U;
si1+=si2;
sd1=f64_load(i->m0,si1);
si2=l1;
sd2=f64_load(i->m0,si2);
sd1*=sd2;
si2=l4;
sd2=f64_load(i->m0,si2);
si3=l1;
si4=8U;
si3+=si4;
sd3=f64_load(i->m0,si3);
sd2*=sd3;
sd3=l2;
sd2+=sd3;
sd1+=sd2;
l2=sd1;
si1=l1;
si2=16U;
si1-=si2;
l1=si1;
si1=l4;
si2=16U;
si1+=si2;
l4=si1;
si1=l20;
si2=l6;
si3=2U;
si2+=si3;
l6=si2;
si1=si1 != si2;
if(si1){
goto L62;
}
}
}
L61:;
si1=l22;
si1=!(si1);
if(si1){
goto L59;
}
si1=l24;
si2=l6;
si3=3U;
si2<<=(si3&31);
si1+=si2;
sd1=f64_load(i->m0,si1);
si2=l10;
si3=320U;
si2+=si3;
si3=l3;
si4=l28;
si3+=si4;
si4=l6;
si3-=si4;
si4=3U;
si3<<=(si4&31);
si2+=si3;
sd2=f64_load(i->m0,si2);
sd1*=sd2;
sd2=l2;
sd1+=sd2;
l2=sd1;
L59:;
si1=l10;
si2=l3;
si3=3U;
si2<<=(si3&31);
si1+=si2;
sd2=l2;
f64_store(i->m0,si1,sd2);
si1=l13;
si2=8U;
si1+=si2;
l13=si1;
si1=l3;
si2=1U;
si1+=si2;
l1=si1;
si1=l3;
si2=l16;
si1=si1 != si2;
if(si1){
goto L58;
}
}
si1=l15;
si2=2147483646U;
si1&=si2;
l20=si1;
si1=l15;
si2=1U;
si1&=si2;
l37=si1;
si1=47U;
si2=l8;
si1-=si2;
l38=si1;
si1=48U;
si2=l8;
si1-=si2;
l31=si1;
si1=l10;
si2=476U;
si1+=si2;
l39=si1;
si2=l23;
si3=2U;
si2<<=(si3&31);
si1+=si2;
l40=si1;
si1=l10;
si2=312U;
si1+=si2;
l41=si1;
si1=l10;
si2=464U;
si1+=si2;
l42=si1;
si1=l10;
si2=16U;
si1-=si2;
l43=si1;
si1=l8;
si2=25U;
si1-=si2;
l44=si1;
si1=l23;
l3=si1;
L64:;
W2C2_LOOP_START
{
si1=l10;
si2=l3;
si3=3U;
si2<<=(si3&31);
l1=si2;
si1+=si2;
sd1=f64_load(i->m0,si1);
l2=sd1;
si1=l3;
si2=0U;
si1=(U32)((I32)si1<=(I32)si2);
l22=si1;
if(si1){
goto L65;
}
si1=0U;
l6=si1;
si1=l3;
si2=1U;
si1=si1 == si2;
if(si1){
si1=l3;
}else{
si1=l3;
si2=2147483646U;
si1&=si2;
l16=si1;
si1=l1;
si2=l43;
si1+=si2;
l1=si1;
si1=l10;
si2=480U;
si1+=si2;
l4=si1;
L67:;
W2C2_LOOP_START
{
si1=l4;
sd2=l2;
sd3=f64_reinterpret_i64(W2C2_LL(0x3E70000000000000U));
sd2*=sd3;
l11=sd2;
sd2=fabs(sd2);
sd3=f64_reinterpret_i64(W2C2_LL(0x41E0000000000000U));
si2=sd2 < sd3;
if(si2){
sd2=l11;
si2=I32_TRUNC_S_F64(sd2);
goto L69;
}
L70:;
si2=-2147483648U;
L69:;
sd2=(F64)(I32)(si2);
l11=sd2;
sd3=f64_reinterpret_i64(W2C2_LL(0xC170000000000000U));
sd2*=sd3;
sd3=l2;
sd2+=sd3;
l2=sd2;
sd2=fabs(sd2);
sd3=f64_reinterpret_i64(W2C2_LL(0x41E0000000000000U));
si2=sd2 < sd3;
if(si2){
sd2=l2;
si2=I32_TRUNC_S_F64(sd2);
goto L68;
}
L71:;
si2=-2147483648U;
L68:;
i32_store(i->m0,si1,si2);
si1=l4;
si2=4U;
si1+=si2;
si2=l1;
si3=8U;
si2+=si3;
sd2=f64_load(i->m0,si2);
sd3=l11;
sd2+=sd3;
l2=sd2;
sd3=f64_reinterpret_i64(W2C2_LL(0x3E70000000000000U));
sd2*=sd3;
l11=sd2;
sd2=fabs(sd2);
sd3=f64_reinterpret_i64(W2C2_LL(0x41E0000000000000U));
si2=sd2 < sd3;
if(si2){
sd2=l11;
si2=I32_TRUNC_S_F64(sd2);
goto L73;
}
L74:;
si2=-2147483648U;
L73:;
sd2=(F64)(I32)(si2);
l11=sd2;
sd3=f64_reinterpret_i64(W2C2_LL(0xC170000000000000U));
sd2*=sd3;
sd3=l2;
sd2+=sd3;
l2=sd2;
sd2=fabs(sd2);
sd3=f64_reinterpret_i64(W2C2_LL(0x41E0000000000000U));
si2=sd2 < sd3;
if(si2){
sd2=l2;
si2=I32_TRUNC_S_F64(sd2);
goto L72;
}
L75:;
si2=-2147483648U;
L72:;
i32_store(i->m0,si1,si2);
si1=l1;
sd1=f64_load(i->m0,si1);
sd2=l11;
sd1+=sd2;
l2=sd1;
si1=l4;
si2=8U;
si1+=si2;
l4=si1;
si1=l1;
si2=16U;
si1-=si2;
l1=si1;
si1=l16;
si2=l6;
si3=2U;
si2+=si3;
l6=si2;
si1=si1 != si2;
if(si1){
goto L67;
}
}
si1=l3;
si2=l6;
si1-=si2;
}
L66:;
l1=si1;
si1=l3;
si2=1U;
si1&=si2;
si1=!(si1);
if(si1){
goto L65;
}
si1=l10;
si2=480U;
si1+=si2;
si2=l6;
si3=2U;
si2<<=(si3&31);
si1+=si2;
sd2=l2;
sd3=f64_reinterpret_i64(W2C2_LL(0x3E70000000000000U));
sd2*=sd3;
l11=sd2;
sd2=fabs(sd2);
sd3=f64_reinterpret_i64(W2C2_LL(0x41E0000000000000U));
si2=sd2 < sd3;
if(si2){
sd2=l11;
si2=I32_TRUNC_S_F64(sd2);
goto L77;
}
L78:;
si2=-2147483648U;
L77:;
sd2=(F64)(I32)(si2);
l11=sd2;
sd3=f64_reinterpret_i64(W2C2_LL(0xC170000000000000U));
sd2*=sd3;
sd3=l2;
sd2+=sd3;
l2=sd2;
sd2=fabs(sd2);
sd3=f64_reinterpret_i64(W2C2_LL(0x41E0000000000000U));
si2=sd2 < sd3;
if(si2){
sd2=l2;
si2=I32_TRUNC_S_F64(sd2);
goto L76;
}
L79:;
si2=-2147483648U;
L76:;
i32_store(i->m0,si1,si2);
si1=l1;
si2=3U;
si1<<=(si2&31);
si2=l10;
si1+=si2;
si2=8U;
si1-=si2;
sd1=f64_load(i->m0,si1);
sd2=l11;
sd1+=sd2;
l2=sd1;
L65:;
sd1=l2;
si2=l17;
sd1=f13(instance,sd1,si2);
l2=sd1;
sd2=f64_reinterpret_i64(W2C2_LL(0x3FC0000000000000U));
sd1*=sd2;
sd1=floor(sd1);
sd2=f64_reinterpret_i64(W2C2_LL(0xC020000000000000U));
sd1*=sd2;
sd2=l2;
sd1+=sd2;
l2=sd1;
sd1=fabs(sd1);
sd2=f64_reinterpret_i64(W2C2_LL(0x41E0000000000000U));
si1=sd1 < sd2;
if(si1){
sd1=l2;
si1=I32_TRUNC_S_F64(sd1);
goto L80;
}
L81:;
si1=-2147483648U;
L80:;
l26=si1;
sd1=l2;
si2=l26;
sd2=(F64)(I32)(si2);
sd1-=sd2;
l2=sd1;
si1=l17;
si2=0U;
si1=(U32)((I32)si1<=(I32)si2);
l45=si1;
si1=!(si1);
if(si1){
si1=l3;
si2=2U;
si1<<=(si2&31);
si2=l10;
si1+=si2;
si2=476U;
si1+=si2;
l1=si1;
si2=l1;
si2=i32_load(i->m0,si2);
l1=si2;
si3=l1;
si4=l31;
si3=(U32)((I32)si3>>(si4&31));
l1=si3;
si4=l31;
si3<<=(si4&31);
si2-=si3;
l4=si2;
i32_store(i->m0,si1,si2);
si1=l1;
si2=l26;
si1+=si2;
l26=si1;
si1=l4;
si2=l38;
si1=(U32)((I32)si1>>(si2&31));
goto L85;
}
L86:;
si1=l17;
if(si1){
goto L84;
}
si1=l3;
si2=2U;
si1<<=(si2&31);
si2=l10;
si1+=si2;
si2=476U;
si1+=si2;
si1=i32_load(i->m0,si1);
si2=23U;
si1=(U32)((I32)si1>>(si2&31));
L85:;
l27=si1;
si2=0U;
si1=(U32)((I32)si1<=(I32)si2);
if(si1){
goto L82;
}
goto L83;
L84:;
si1=2U;
l27=si1;
sd1=l2;
sd2=f64_reinterpret_i64(W2C2_LL(0x3FE0000000000000U));
si1=sd1 >= sd2;
if(si1){
goto L83;
}
si1=0U;
l27=si1;
goto L82;
L83:;
si1=l22;
if(si1){
si1=0U;
l4=si1;
goto L87;
}
L88:;
si1=0U;
l13=si1;
si1=0U;
l4=si1;
si1=l3;
si2=1U;
si1=si1 != si2;
if(si1){
si1=l3;
si2=2147483646U;
si1&=si2;
l22=si1;
si1=l10;
si2=480U;
si1+=si2;
l1=si1;
L90:;
W2C2_LOOP_START
{
si1=l1;
si1=i32_load(i->m0,si1);
l6=si1;
si1=l1;
si2=l4;
if(si2){
si2=16777215U;
}else{
si2=l6;
si2=!(si2);
if(si2){
goto L92;
}
si2=16777216U;
}
L93:;
si3=l6;
si2-=si3;
i32_store(i->m0,si1,si2);
si1=0U;
goto L91;
L92:;
si1=1U;
L91:;
l6=si1;
si1=l1;
si2=4U;
si1+=si2;
l16=si1;
si1=i32_load(i->m0,si1);
l4=si1;
si1=l16;
si2=l6;
if(si2){
si2=l4;
si2=!(si2);
if(si2){
goto L95;
}
si2=16777216U;
}else{
si2=16777215U;
}
L96:;
si3=l4;
si2-=si3;
i32_store(i->m0,si1,si2);
si1=1U;
goto L94;
L95:;
si1=0U;
L94:;
l4=si1;
si1=l1;
si2=8U;
si1+=si2;
l1=si1;
si1=l22;
si2=l13;
si3=2U;
si2+=si3;
l13=si2;
si1=si1 != si2;
if(si1){
goto L90;
}
}
}
L89:;
si1=l3;
si2=1U;
si1&=si2;
si1=!(si1);
if(si1){
goto L87;
}
si1=l10;
si2=480U;
si1+=si2;
si2=l13;
si3=2U;
si2<<=(si3&31);
si1+=si2;
l6=si1;
si1=i32_load(i->m0,si1);
l1=si1;
si1=l6;
si2=l4;
if(si2){
si2=16777215U;
}else{
si2=l1;
si2=!(si2);
if(si2){
goto L97;
}
si2=16777216U;
}
L98:;
si3=l1;
si2-=si3;
i32_store(i->m0,si1,si2);
si1=1U;
l4=si1;
goto L87;
L97:;
si1=0U;
l4=si1;
L87:;
si1=l45;
if(si1){
goto L99;
}
si1=8388607U;
l1=si1;
si1=l44;
switch(si1){
case 0:
goto L100;
case 1:
goto L101;
default:
goto L99;
}
L101:;
si1=4194303U;
l1=si1;
L100:;
si1=l3;
si2=2U;
si1<<=(si2&31);
si2=l10;
si1+=si2;
si2=476U;
si1+=si2;
l6=si1;
si2=l6;
si2=i32_load(i->m0,si2);
si3=l1;
si2&=si3;
i32_store(i->m0,si1,si2);
L99:;
si1=l26;
si2=1U;
si1+=si2;
l26=si1;
si1=l27;
si2=2U;
si1=si1 != si2;
if(si1){
goto L82;
}
sd1=f64_reinterpret_i64(W2C2_LL(0x3FF0000000000000U));
sd2=l2;
sd1-=sd2;
l2=sd1;
si1=2U;
l27=si1;
si1=l4;
si1=!(si1);
if(si1){
goto L82;
}
sd1=l2;
sd2=f64_reinterpret_i64(W2C2_LL(0x3FF0000000000000U));
si3=l17;
sd2=f13(instance,sd2,si3);
sd1-=sd2;
l2=sd1;
L82:;
sd1=l2;
sd2=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
si1=sd1 == sd2;
if(si1){
si1=l3;
si2=l23;
si1=(U32)((I32)si1<=(I32)si2);
if(si1){
goto L103;
}
si1=l3;
si2=l23;
si1-=si2;
l1=si1;
si2=3U;
si1&=si2;
l6=si1;
si1=0U;
l4=si1;
si1=l3;
l13=si1;
si1=l23;
si2=l3;
si1-=si2;
si2=-4U;
si1=si1 <= si2;
if(si1){
si1=l1;
si2=-4U;
si1&=si2;
l16=si1;
si1=l42;
si2=l3;
si3=2U;
si2<<=(si3&31);
si1+=si2;
l1=si1;
L105:;
W2C2_LOOP_START
{
si1=l1;
si1=i32_load(i->m0,si1);
si2=l1;
si3=4U;
si2+=si3;
si2=i32_load(i->m0,si2);
si3=l1;
si4=8U;
si3+=si4;
si3=i32_load(i->m0,si3);
si4=l1;
si5=12U;
si4+=si5;
si4=i32_load(i->m0,si4);
si5=l4;
si4|=si5;
si3|=si4;
si2|=si3;
si1|=si2;
l4=si1;
si1=l1;
si2=16U;
si1-=si2;
l1=si1;
si1=l13;
si2=4U;
si1-=si2;
l13=si1;
si1=l16;
si2=4U;
si1-=si2;
l16=si1;
if(si1){
goto L105;
}
}
}
L104:;
si1=l6;
if(si1){
si1=l39;
si2=l13;
si3=2U;
si2<<=(si3&31);
si1+=si2;
l1=si1;
L107:;
W2C2_LOOP_START
{
si1=l1;
si1=i32_load(i->m0,si1);
si2=l4;
si1|=si2;
l4=si1;
si1=l1;
si2=4U;
si1-=si2;
l1=si1;
si1=l6;
si2=1U;
si1-=si2;
l6=si1;
if(si1){
goto L107;
}
}
}
L106:;
si1=l4;
si1=!(si1);
if(si1){
goto L103;
}
si1=l3;
si2=2U;
si1<<=(si2&31);
si2=l10;
si1+=si2;
si2=476U;
si1+=si2;
l1=si1;
si1=l17;
l8=si1;
L108:;
W2C2_LOOP_START
{
si1=l3;
si2=1U;
si1-=si2;
l3=si1;
si1=l8;
si2=24U;
si1-=si2;
l8=si1;
si1=l1;
si1=i32_load(i->m0,si1);
l4=si1;
si1=l1;
si2=4U;
si1-=si2;
l1=si1;
si1=l4;
si1=!(si1);
if(si1){
goto L108;
}
}
goto L63;
L103:;
si1=l40;
l1=si1;
si1=l3;
l13=si1;
L109:;
W2C2_LOOP_START
{
si1=l13;
si2=1U;
si1+=si2;
l13=si1;
si1=l1;
si1=i32_load(i->m0,si1);
l4=si1;
si1=l1;
si2=4U;
si1-=si2;
l1=si1;
si1=l4;
si1=!(si1);
if(si1){
goto L109;
}
}
si1=l41;
si2=l3;
si3=l15;
si2+=si3;
si3=3U;
si2<<=(si3&31);
si1+=si2;
l16=si1;
L110:;
W2C2_LOOP_START
{
si1=l10;
si2=320U;
si1+=si2;
si2=l3;
si3=l15;
si2+=si3;
l22=si2;
si3=3U;
si2<<=(si3&31);
si1+=si2;
si2=l3;
si3=1U;
si2+=si3;
l3=si2;
si3=l30;
si2+=si3;
si3=2U;
si2<<=(si3&31);
si3=1440U;
si2+=si3;
si2=i32_load(i->m0,si2);
sd2=(F64)(I32)(si2);
f64_store(i->m0,si1,sd2);
si1=l15;
si2=0U;
si1=(U32)((I32)si1<=(I32)si2);
if(si1){
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
l2=sd1;
goto L111;
}
L112:;
si1=0U;
l6=si1;
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
l2=sd1;
si1=l28;
if(si1){
si1=l16;
l1=si1;
si1=l24;
l4=si1;
L114:;
W2C2_LOOP_START
{
si1=l4;
si2=8U;
si1+=si2;
sd1=f64_load(i->m0,si1);
si2=l1;
sd2=f64_load(i->m0,si2);
sd1*=sd2;
si2=l4;
sd2=f64_load(i->m0,si2);
si3=l1;
si4=8U;
si3+=si4;
sd3=f64_load(i->m0,si3);
sd2*=sd3;
sd3=l2;
sd2+=sd3;
sd1+=sd2;
l2=sd1;
si1=l1;
si2=16U;
si1-=si2;
l1=si1;
si1=l4;
si2=16U;
si1+=si2;
l4=si1;
si1=l20;
si2=l6;
si3=2U;
si2+=si3;
l6=si2;
si1=si1 != si2;
if(si1){
goto L114;
}
}
}
L113:;
si1=l37;
si1=!(si1);
if(si1){
goto L111;
}
si1=l24;
si2=l6;
si3=3U;
si2<<=(si3&31);
si1+=si2;
sd1=f64_load(i->m0,si1);
si2=l10;
si3=320U;
si2+=si3;
si3=l22;
si4=l6;
si3-=si4;
si4=3U;
si3<<=(si4&31);
si2+=si3;
sd2=f64_load(i->m0,si2);
sd1*=sd2;
sd2=l2;
sd1+=sd2;
l2=sd1;
L111:;
si1=l10;
si2=l3;
si3=3U;
si2<<=(si3&31);
si1+=si2;
sd2=l2;
f64_store(i->m0,si1,sd2);
si1=l16;
si2=8U;
si1+=si2;
l16=si1;
si1=l3;
si2=l13;
si1=(U32)((I32)si1<(I32)si2);
if(si1){
goto L110;
}
}
si1=l13;
l3=si1;
goto L64;
}
L102:;
}
sd1=l2;
si2=24U;
si3=l8;
si2-=si3;
sd1=f13(instance,sd1,si2);
l2=sd1;
sd2=f64_reinterpret_i64(W2C2_LL(0x4170000000000000U));
si1=sd1 >= sd2;
if(si1){
si1=l10;
si2=480U;
si1+=si2;
si2=l3;
si3=2U;
si2<<=(si3&31);
si1+=si2;
sd2=l2;
sd3=f64_reinterpret_i64(W2C2_LL(0x3E70000000000000U));
sd2*=sd3;
l11=sd2;
sd2=fabs(sd2);
sd3=f64_reinterpret_i64(W2C2_LL(0x41E0000000000000U));
si2=sd2 < sd3;
if(si2){
sd2=l11;
si2=I32_TRUNC_S_F64(sd2);
goto L118;
}
L119:;
si2=-2147483648U;
L118:;
l1=si2;
sd2=(F64)(I32)(si2);
sd3=f64_reinterpret_i64(W2C2_LL(0xC170000000000000U));
sd2*=sd3;
sd3=l2;
sd2+=sd3;
l2=sd2;
sd2=fabs(sd2);
sd3=f64_reinterpret_i64(W2C2_LL(0x41E0000000000000U));
si2=sd2 < sd3;
if(si2){
sd2=l2;
si2=I32_TRUNC_S_F64(sd2);
goto L117;
}
L120:;
si2=-2147483648U;
L117:;
i32_store(i->m0,si1,si2);
si1=l3;
si2=1U;
si1+=si2;
l3=si1;
goto L115;
}
L116:;
sd1=l2;
sd1=fabs(sd1);
sd2=f64_reinterpret_i64(W2C2_LL(0x41E0000000000000U));
si1=sd1 < sd2;
if(si1){
sd1=l2;
si1=I32_TRUNC_S_F64(sd1);
goto L121;
}
L122:;
si1=-2147483648U;
L121:;
l1=si1;
si1=l17;
l8=si1;
L115:;
si1=l10;
si2=480U;
si1+=si2;
si2=l3;
si3=2U;
si2<<=(si3&31);
si1+=si2;
si2=l1;
i32_store(i->m0,si1,si2);
L63:;
si1=l3;
si2=0U;
si1=(U32)((I32)si1<(I32)si2);
if(si1){
goto L123;
}
sd1=f64_reinterpret_i64(W2C2_LL(0x3FF0000000000000U));
si2=l8;
sd1=f13(instance,sd1,si2);
l2=sd1;
si1=l3;
si2=1U;
si1&=si2;
if(si1){
si1=l3;
}else{
si1=l10;
si2=l3;
si3=3U;
si2<<=(si3&31);
si1+=si2;
sd2=l2;
si3=l10;
si4=480U;
si3+=si4;
si4=l3;
si5=2U;
si4<<=(si5&31);
si3+=si4;
si3=i32_load(i->m0,si3);
sd3=(F64)(I32)(si3);
sd2*=sd3;
f64_store(i->m0,si1,sd2);
sd1=l2;
sd2=f64_reinterpret_i64(W2C2_LL(0x3E70000000000000U));
sd1*=sd2;
l2=sd1;
si1=l3;
si2=1U;
si1-=si2;
}
L124:;
l4=si1;
si1=l3;
if(si1){
si1=l4;
si2=1U;
si1+=si2;
l6=si1;
si1=l4;
si2=2U;
si1<<=(si2&31);
si2=l10;
si1+=si2;
si2=476U;
si1+=si2;
l1=si1;
si1=l4;
si2=3U;
si1<<=(si2&31);
si2=l10;
si1+=si2;
si2=8U;
si1-=si2;
l4=si1;
L126:;
W2C2_LOOP_START
{
si1=l4;
sd2=l2;
sd3=f64_reinterpret_i64(W2C2_LL(0x3E70000000000000U));
sd2*=sd3;
l11=sd2;
si3=l1;
si3=i32_load(i->m0,si3);
sd3=(F64)(I32)(si3);
sd2*=sd3;
f64_store(i->m0,si1,sd2);
si1=l4;
si2=8U;
si1+=si2;
sd2=l2;
si3=l1;
si4=4U;
si3+=si4;
si3=i32_load(i->m0,si3);
sd3=(F64)(I32)(si3);
sd2*=sd3;
f64_store(i->m0,si1,sd2);
si1=l1;
si2=8U;
si1-=si2;
l1=si1;
si1=l4;
si2=16U;
si1-=si2;
l4=si1;
sd1=l11;
sd2=f64_reinterpret_i64(W2C2_LL(0x3E70000000000000U));
sd1*=sd2;
l2=sd1;
si1=l6;
si2=2U;
si1-=si2;
l6=si1;
if(si1){
goto L126;
}
}
}
L125:;
si1=l3;
si2=0U;
si1=(U32)((I32)si1<(I32)si2);
if(si1){
goto L123;
}
si1=l10;
si2=l3;
si3=3U;
si2<<=(si3&31);
si1+=si2;
l20=si1;
si1=l3;
l1=si1;
L127:;
W2C2_LOOP_START
{
si1=l23;
si2=l3;
si3=l1;
l16=si3;
si2-=si3;
l15=si2;
si3=l15;
si4=l23;
si3=(U32)((I32)si3>(I32)si4);
si1=si3?si1:si2;
l22=si1;
si2=0U;
si1=(U32)((I32)si1<(I32)si2);
if(si1){
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
l2=sd1;
goto L128;
}
L129:;
si1=l22;
si1=!(si1);
if(si1){
si1=0U;
l4=si1;
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
l2=sd1;
goto L130;
}
L131:;
si1=l22;
si2=1U;
si1+=si2;
si2=-2U;
si1&=si2;
l13=si1;
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
l2=sd1;
si1=0U;
l1=si1;
si1=0U;
l4=si1;
L132:;
W2C2_LOOP_START
{
si1=l1;
si2=4216U;
si1+=si2;
sd1=f64_load(i->m0,si1);
si2=l1;
si3=l20;
si2+=si3;
l6=si2;
si3=8U;
si2+=si3;
sd2=f64_load(i->m0,si2);
sd1*=sd2;
si2=l1;
si3=4208U;
si2+=si3;
sd2=f64_load(i->m0,si2);
si3=l6;
sd3=f64_load(i->m0,si3);
sd2*=sd3;
sd3=l2;
sd2+=sd3;
sd1+=sd2;
l2=sd1;
si1=l1;
si2=16U;
si1+=si2;
l1=si1;
si1=l13;
si2=l4;
si3=2U;
si2+=si3;
l4=si2;
si1=si1 != si2;
if(si1){
goto L132;
}
}
L130:;
si1=l22;
si2=1U;
si1&=si2;
if(si1){
goto L128;
}
si1=l4;
si2=3U;
si1<<=(si2&31);
si2=4208U;
si1+=si2;
sd1=f64_load(i->m0,si1);
si2=l10;
si3=l4;
si4=l16;
si3+=si4;
si4=3U;
si3<<=(si4&31);
si2+=si3;
sd2=f64_load(i->m0,si2);
sd1*=sd2;
sd2=l2;
sd1+=sd2;
l2=sd1;
L128:;
si1=l10;
si2=160U;
si1+=si2;
si2=l15;
si3=3U;
si2<<=(si3&31);
si1+=si2;
sd2=l2;
f64_store(i->m0,si1,sd2);
si1=l20;
si2=8U;
si1-=si2;
l20=si1;
si1=l16;
si2=1U;
si1-=si2;
l1=si1;
si1=l16;
si2=0U;
si1=(U32)((I32)si1>(I32)si2);
if(si1){
goto L127;
}
}
L123:;
si1=l3;
si2=0U;
si1=(U32)((I32)si1<(I32)si2);
if(si1){
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
l2=sd1;
goto L133;
}
L134:;
si1=l3;
si2=1U;
si1+=si2;
si2=3U;
si1&=si2;
l6=si1;
si1=!(si1);
if(si1){
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
l2=sd1;
si1=l3;
l4=si1;
goto L135;
}
L136:;
si1=l10;
si2=160U;
si1+=si2;
si2=l3;
si3=3U;
si2<<=(si3&31);
si1+=si2;
l1=si1;
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
l2=sd1;
si1=l3;
l4=si1;
L137:;
W2C2_LOOP_START
{
si1=l4;
si2=1U;
si1-=si2;
l4=si1;
sd1=l2;
si2=l1;
sd2=f64_load(i->m0,si2);
sd1+=sd2;
l2=sd1;
si1=l1;
si2=8U;
si1-=si2;
l1=si1;
si1=l6;
si2=1U;
si1-=si2;
l6=si1;
if(si1){
goto L137;
}
}
L135:;
si1=l3;
si2=3U;
si1=si1 < si2;
if(si1){
goto L133;
}
si1=l4;
si2=1U;
si1+=si2;
l6=si1;
si1=l4;
si2=3U;
si1<<=(si2&31);
si2=l10;
si1+=si2;
si2=136U;
si1+=si2;
l1=si1;
L138:;
W2C2_LOOP_START
{
sd1=l2;
si2=l1;
si3=24U;
si2+=si3;
sd2=f64_load(i->m0,si2);
sd1+=sd2;
si2=l1;
si3=16U;
si2+=si3;
sd2=f64_load(i->m0,si2);
sd1+=sd2;
si2=l1;
si3=8U;
si2+=si3;
sd2=f64_load(i->m0,si2);
sd1+=sd2;
si2=l1;
sd2=f64_load(i->m0,si2);
sd1+=sd2;
l2=sd1;
si1=l1;
si2=32U;
si1-=si2;
l1=si1;
si1=l6;
si2=4U;
si1-=si2;
l6=si1;
if(si1){
goto L138;
}
}
L133:;
si1=l21;
sd2=l2;
sd2=-(sd2);
sd3=l2;
si4=l27;
sd2=si4?sd2:sd3;
f64_store(i->m0,si1,sd2);
si1=l10;
sd1=f64_load(i->m0,(WasmMemoryAddress)si1+160U);
sd2=l2;
sd1-=sd2;
l2=sd1;
si1=1U;
l1=si1;
si1=l3;
si2=0U;
si1=(U32)((I32)si1<=(I32)si2);
if(si1){
goto L139;
}
si1=l3;
si2=3U;
si1&=si2;
l4=si1;
si1=l3;
si2=1U;
si1-=si2;
si2=3U;
si1=si1 >= si2;
if(si1){
si1=l3;
si2=2147483644U;
si1&=si2;
l20=si1;
si1=l10;
si2=192U;
si1+=si2;
l1=si1;
si1=0U;
l6=si1;
L141:;
W2C2_LOOP_START
{
sd1=l2;
si2=l1;
si3=24U;
si2-=si3;
sd2=f64_load(i->m0,si2);
sd1+=sd2;
si2=l1;
si3=16U;
si2-=si3;
sd2=f64_load(i->m0,si2);
sd1+=sd2;
si2=l1;
si3=8U;
si2-=si3;
sd2=f64_load(i->m0,si2);
sd1+=sd2;
si2=l1;
sd2=f64_load(i->m0,si2);
sd1+=sd2;
l2=sd1;
si1=l1;
si2=32U;
si1+=si2;
l1=si1;
si1=l20;
si2=l6;
si3=4U;
si2+=si3;
l6=si2;
si1=si1 != si2;
if(si1){
goto L141;
}
}
si1=l6;
si2=1U;
si1+=si2;
l1=si1;
}
L140:;
si1=l4;
si1=!(si1);
if(si1){
goto L139;
}
si1=l10;
si2=160U;
si1+=si2;
si2=l1;
si3=3U;
si2<<=(si3&31);
si1+=si2;
l1=si1;
L142:;
W2C2_LOOP_START
{
sd1=l2;
si2=l1;
sd2=f64_load(i->m0,si2);
sd1+=sd2;
l2=sd1;
si1=l1;
si2=8U;
si1+=si2;
l1=si1;
si1=l4;
si2=1U;
si1-=si2;
l4=si1;
if(si1){
goto L142;
}
}
L139:;
si1=l21;
sd2=l2;
sd2=-(sd2);
sd3=l2;
si4=l27;
sd2=si4?sd2:sd3;
f64_store(i->m0,(WasmMemoryAddress)si1+8U,sd2);
si1=l10;
si2=560U;
si1+=si2;
i->g0=si1;
si1=l26;
si2=7U;
si1&=si2;
l3=si1;
si1=l21;
sd1=f64_load(i->m0,si1);
l2=sd1;
sj1=l25;
sj2=W2C2_LL(0U);
si1=(U64)((I64)sj1<(I64)sj2);
if(si1){
si1=l9;
sd2=l2;
sd2=-(sd2);
f64_store(i->m0,si1,sd2);
si1=l9;
si2=l21;
sd2=f64_load(i->m0,(WasmMemoryAddress)si2+8U);
sd2=-(sd2);
f64_store(i->m0,(WasmMemoryAddress)si1+8U,sd2);
si1=0U;
si2=l3;
si1-=si2;
l3=si1;
goto L31;
}
L143:;
si1=l9;
sd2=l2;
f64_store(i->m0,si1,sd2);
si1=l9;
si2=l21;
sd2=f64_load(i->m0,(WasmMemoryAddress)si2+8U);
f64_store(i->m0,(WasmMemoryAddress)si1+8U,sd2);
L31:;
si1=l21;
si2=48U;
si1+=si2;
i->g0=si1;
si1=l9;
sd1=f64_load(i->m0,(WasmMemoryAddress)si1+8U);
l14=sd1;
si1=l9;
sd1=f64_load(i->m0,si1);
l11=sd1;
si1=l3;
si2=3U;
si1&=si2;
si2=1U;
si1-=si2;
switch(si1){
case 0:
goto L146;
case 1:
goto L145;
case 2:
goto L144;
default:
goto L147;
}
L147:;
sd1=l11;
sd2=l14;
si3=1U;
sd1=f12(instance,sd1,sd2,si3);
l14=sd1;
goto L28;
L146:;
sd1=l11;
sd2=l14;
sd1=f14(instance,sd1,sd2);
l14=sd1;
goto L28;
L145:;
sd1=l11;
sd2=l14;
si3=1U;
sd1=f12(instance,sd1,sd2,si3);
sd1=-(sd1);
l14=sd1;
goto L28;
L144:;
sd1=l11;
sd2=l14;
sd1=f14(instance,sd1,sd2);
sd1=-(sd1);
l14=sd1;
L28:;
si1=l9;
si2=16U;
si1+=si2;
i->g0=si1;
si1=l5;
si2=1023U;
si1&=si2;
sd1=(F64)(si1);
sd1=sqrt(sd1);
sd2=f64_reinterpret_i64(W2C2_LL(0x3FE0000000000000U));
sd1*=sd2;
sd2=l14;
sd1+=sd2;
L27:;
sd0+=sd1;
l11=sd0;
si0=l5;
si1=1U;
si0+=si1;
l5=si0;
sj0=l18;
sj1=W2C2_LL(1U);
sj0+=sj1;
l18=sj0;
sj1=W2C2_LL(200000U);
si0=sj0 != sj1;
if(si0){
goto L26;
}
}
si0=l0;
si1=1U;
si0+=si1;
l0=si0;
si1=10U;
si0=si0 != si1;
if(si0){
goto L25;
}
}
si0=l7;
si1=96U;
si0+=si1;
f11(instance,si0);
si0=l7;
sd1=l11;
sd2=f64_reinterpret_i64(W2C2_LL(0x408F400000000000U));
sd1*=sd2;
l11=sd1;
sd1=fabs(sd1);
sd2=f64_reinterpret_i64(W2C2_LL(0x43E0000000000000U));
si1=sd1 < sd2;
if(si1){
sd1=l11;
sj1=I64_TRUNC_S_F64(sd1);
goto L148;
}
L149:;
sj1=W2C2_LL(-9223372036854775808U);
L148:;
sj2=l19;
sj1^=sj2;
l18=sj1;
i64_store32(i->m0,(WasmMemoryAddress)si0+24U,sj1);
si0=l7;
sj1=l18;
sj2=W2C2_LL(32U);
sj1>>=(sj2&63);
i64_store32(i->m0,(WasmMemoryAddress)si0+20U,sj1);
si0=l7;
si1=l7;
sj1=i64_load32_s(i->m0,(WasmMemoryAddress)si1+104U);
sj2=W2C2_LL(1000U);
sj1=DIV_U(sj1,sj2);
sj2=l35;
sj3=W2C2_LL(1000U);
sj2=DIV_U(sj2,sj3);
sj1-=sj2;
si2=l7;
sj2=i64_load(i->m0,(WasmMemoryAddress)si2+96U);
sj3=l36;
sj2-=sj3;
sj3=W2C2_LL(1000000U);
sj2*=sj3;
sj1+=sj2;
l18=sj1;
i64_store32(i->m0,(WasmMemoryAddress)si0+16U,sj1);
si0=1117U;
si1=l7;
si2=16U;
si1+=si2;
f15(instance,si0,si1);
si0=l7;
sj1=l32;
sj2=l33;
sj1+=sj2;
sj2=l34;
sj1+=sj2;
sj2=l18;
sj1+=sj2;
i64_store32(i->m0,si0,sj1);
si0=1241U;
si1=l7;
f15(instance,si0,si1);
si0=1089U;
f19(instance,si0);
si0=l7;
si1=112U;
si0+=si1;
i->g0=si0;
si0=3070804U;
si0=i32_load(i->m0,si0);
l0=si0;
if(si0){
L151:;
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
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_benchFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
}
L152:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l5=si0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l3=si1;
si0=si0 != si1;
if(si0){
si0=l0;
si1=l5;
si2=l3;
si1-=si2;
sj1=(U64)(I64)(I32)(si1);
si2=1U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+36U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_benchFuncType2);
sj0=((U64 (*)(wasmModuleInstance*,U32,U64,U32))entry.func)(entry.instance,si0,sj1,si2);
}
}
L153:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+52U);
l0=si0;
if(si0){
goto L151;
}
}
}
L150:;
si0=3070808U;
si0=i32_load(i->m0,si0);
l0=si0;
si0=!(si0);
if(si0){
goto L154;
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
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_benchFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
}
L155:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l5=si0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l3=si1;
si0=si0 == si1;
if(si0){
goto L154;
}
si0=l0;
si1=l5;
si2=l3;
si1-=si2;
sj1=(U64)(I64)(I32)(si1);
si2=1U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+36U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_benchFuncType2);
sj0=((U64 (*)(wasmModuleInstance*,U32,U64,U32))entry.func)(entry.instance,si0,sj1,si2);
}
L154:;
si0=6584U;
si0=i32_load(i->m0,si0);
l0=si0;
si0=!(si0);
if(si0){
goto L156;
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
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_benchFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
}
L157:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l5=si0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l3=si1;
si0=si0 == si1;
if(si0){
goto L156;
}
si0=l0;
si1=l5;
si2=l3;
si1-=si2;
sj1=(U64)(I64)(I32)(si1);
si2=1U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+36U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_benchFuncType2);
sj0=((U64 (*)(wasmModuleInstance*,U32,U64,U32))entry.func)(entry.instance,si0,sj1,si2);
}
L156:;
si0=6704U;
si0=i32_load(i->m0,si0);
l0=si0;
si0=!(si0);
if(si0){
goto L158;
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
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_benchFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
}
L159:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l5=si0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l3=si1;
si0=si0 == si1;
if(si0){
goto L158;
}
si0=l0;
si1=l5;
si2=l3;
si1-=si2;
sj1=(U64)(I64)(I32)(si1);
si2=1U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+36U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_benchFuncType2);
sj0=((U64 (*)(wasmModuleInstance*,U32,U64,U32))entry.func)(entry.instance,si0,sj1,si2);
}
L158:;
goto L0;
}
L1:;
UNREACHABLE;
L0:;
}

U32 f6(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 si0,si1,si2,si3;
si0=-1U;
si1=l0;
si1=i32_load(i->m0,si1);
l0=si1;
si2=l1;
si2=i32_load(i->m0,si2);
l1=si2;
si1=si1 > si2;
si2=l0;
si3=l1;
si2=si2 < si3;
si0=si2?si0:si1;
L0:;
return si0;
}

U32 f7(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 si0;
si0=l1;
L0:;
return si0;
}

U32 f8(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 si0,si1,si2,si3,si4,si5;
si0=l0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
l0=si1;
si2=l0;
si3=l1;
si4=l0;
si5=l1;
si4=(U32)((I32)si4<(I32)si5);
si2=si4?si2:si3;
l0=si2;
si1-=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l1;
si1=l0;
si0-=si1;
L0:;
return si0;
}

U32 f9(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 si0,si1;
si0=l1;
si1=3U;
si0*=si1;
si1=4U;
si0=I32_DIV_S(si0,si1);
L0:;
return si0;
}

U32 f10(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 si0,si1;
si0=l1;
si1=2U;
si0=I32_DIV_S(si0,si1);
si1=l1;
si0+=si1;
L0:;
return si0;
}

void f11(wasmModuleInstance* instance,U32 l0) {
m5_benchInstance* i = (m5_benchInstance*)instance;
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
si0=1408U;
si0=i32_load(i->m0,si0);
sj1=W2C2_LL(1U);
si2=l1;
si3=8U;
si2+=si3;
si0=i22_wasiX5FsnapshotX5Fpreview114_clockX5FtimeX5Fget(instance,si0,sj1,si2);
si1=65535U;
si0&=si1;
l2=si0;
if(si0){
si0=3070800U;
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

F64 f12(wasmModuleInstance* instance,F64 l0,F64 l1,U32 l2) {
m5_benchInstance* i = (m5_benchInstance*)instance;
F64 l3=0;
F64 l4=0;
F64 l5=0;
U32 si0;
F64 sd0,sd1,sd2,sd3,sd4;
sd0=l0;
sd1=l0;
sd0*=sd1;
l3=sd0;
sd1=l3;
sd2=l3;
sd1*=sd2;
sd0*=sd1;
sd1=l3;
sd2=f64_reinterpret_i64(W2C2_LL(0x3DE5D93A5ACFD57CU));
sd1*=sd2;
sd2=f64_reinterpret_i64(W2C2_LL(0xBE5AE5E68A2B9CEBU));
sd1+=sd2;
sd0*=sd1;
sd1=l3;
sd2=l3;
sd3=f64_reinterpret_i64(W2C2_LL(0x3EC71DE357B1FE7DU));
sd2*=sd3;
sd3=f64_reinterpret_i64(W2C2_LL(0xBF2A01A019C161D5U));
sd2+=sd3;
sd1*=sd2;
sd2=f64_reinterpret_i64(W2C2_LL(0x3F8111111110F8A6U));
sd1+=sd2;
sd0+=sd1;
l5=sd0;
sd0=l3;
sd1=l0;
sd0*=sd1;
l4=sd0;
si0=l2;
si0=!(si0);
if(si0){
sd0=l4;
sd1=l3;
sd2=l5;
sd1*=sd2;
sd2=f64_reinterpret_i64(W2C2_LL(0xBFC5555555555549U));
sd1+=sd2;
sd0*=sd1;
sd1=l0;
sd0+=sd1;
goto L0;
}
L1:;
sd0=l0;
sd1=l3;
sd2=l1;
sd3=f64_reinterpret_i64(W2C2_LL(0x3FE0000000000000U));
sd2*=sd3;
sd3=l5;
sd4=l4;
sd3*=sd4;
sd2-=sd3;
sd1*=sd2;
sd2=l1;
sd1-=sd2;
sd2=l4;
sd3=f64_reinterpret_i64(W2C2_LL(0x3FC5555555555549U));
sd2*=sd3;
sd1+=sd2;
sd0-=sd1;
L0:;
return sd0;
}

F64 f13(wasmModuleInstance* instance,F64 l0,U32 l1) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 si0,si1,si2,si3;
U64 sj1,sj2;
F64 sd0,sd1;
si0=l1;
si1=1024U;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
sd0=l0;
sd1=f64_reinterpret_i64(W2C2_LL(0x7FE0000000000000U));
sd0*=sd1;
l0=sd0;
si0=l1;
si1=2047U;
si0=si0 < si1;
if(si0){
si0=l1;
si1=1023U;
si0-=si1;
l1=si0;
goto L1;
}
L3:;
sd0=l0;
sd1=f64_reinterpret_i64(W2C2_LL(0x7FE0000000000000U));
sd0*=sd1;
l0=sd0;
si0=l1;
si1=3069U;
si2=l1;
si3=3069U;
si2=si2 < si3;
si0=si2?si0:si1;
si1=2046U;
si0-=si1;
l1=si0;
goto L1;
}
L2:;
si0=l1;
si1=-1023U;
si0=(U32)((I32)si0>(I32)si1);
if(si0){
goto L1;
}
sd0=l0;
sd1=f64_reinterpret_i64(W2C2_LL(0x0360000000000000U));
sd0*=sd1;
l0=sd0;
si0=l1;
si1=-1992U;
si0=si0 > si1;
if(si0){
si0=l1;
si1=969U;
si0+=si1;
l1=si0;
goto L1;
}
L4:;
sd0=l0;
sd1=f64_reinterpret_i64(W2C2_LL(0x0360000000000000U));
sd0*=sd1;
l0=sd0;
si0=l1;
si1=-2960U;
si2=l1;
si3=-2960U;
si2=si2 > si3;
si0=si2?si0:si1;
si1=1938U;
si0+=si1;
l1=si0;
L1:;
sd0=l0;
si1=l1;
si2=1023U;
si1+=si2;
sj1=(U64)(si1);
sj2=W2C2_LL(52U);
sj1<<=(sj2&63);
sd1=f64_reinterpret_i64(sj1);
sd0*=sd1;
L0:;
return sd0;
}

F64 f14(wasmModuleInstance* instance,F64 l0,F64 l1) {
m5_benchInstance* i = (m5_benchInstance*)instance;
F64 l2=0;
F64 l3=0;
F64 l4=0;
F64 sd0,sd1,sd2,sd3,sd4,sd5,sd6,sd7;
sd0=f64_reinterpret_i64(W2C2_LL(0x3FF0000000000000U));
sd1=l0;
sd2=l0;
sd1*=sd2;
l2=sd1;
sd2=f64_reinterpret_i64(W2C2_LL(0x3FE0000000000000U));
sd1*=sd2;
l3=sd1;
sd0-=sd1;
l4=sd0;
sd1=f64_reinterpret_i64(W2C2_LL(0x3FF0000000000000U));
sd2=l4;
sd1-=sd2;
sd2=l3;
sd1-=sd2;
sd2=l2;
sd3=l2;
sd4=l2;
sd5=l2;
sd6=f64_reinterpret_i64(W2C2_LL(0x3EFA01A019CB1590U));
sd5*=sd6;
sd6=f64_reinterpret_i64(W2C2_LL(0xBF56C16C16C15177U));
sd5+=sd6;
sd4*=sd5;
sd5=f64_reinterpret_i64(W2C2_LL(0x3FA555555555554CU));
sd4+=sd5;
sd3*=sd4;
sd4=l2;
sd5=l2;
sd4*=sd5;
l3=sd4;
sd5=l3;
sd4*=sd5;
sd5=l2;
sd6=l2;
sd7=f64_reinterpret_i64(W2C2_LL(0xBDA8FAE9BE8838D4U));
sd6*=sd7;
sd7=f64_reinterpret_i64(W2C2_LL(0x3E21EE9EBDB4B1C4U));
sd6+=sd7;
sd5*=sd6;
sd6=f64_reinterpret_i64(W2C2_LL(0xBE927E4F809C52ADU));
sd5+=sd6;
sd4*=sd5;
sd3+=sd4;
sd2*=sd3;
sd3=l0;
sd4=l1;
sd3*=sd4;
sd2-=sd3;
sd1+=sd2;
sd0+=sd1;
L0:;
return sd0;
}

void f15(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 l2=0;
U32 si0,si1,si2;
si0=i->g0;
si1=16U;
si0-=si1;
l2=si0;
i->g0=si0;
si0=l2;
si1=l1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=6472U;
si1=l0;
si2=l1;
f27(instance,si0,si1,si2);
si0=l2;
si1=16U;
si0+=si1;
i->g0=si0;
L0:;
}

U32 f16(wasmModuleInstance* instance,U32 l0) {
m5_benchInstance* i = (m5_benchInstance*)instance;
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

void f17(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_benchInstance* i = (m5_benchInstance*)instance;
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
si1=f16(instance,si1);
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
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_benchFuncType0);
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
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_benchFuncType0);
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
si0=f36(instance,si0,si1,si2);
si0=l2;
si1=l2;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+20U);
si2=l1;
si1+=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
L1:;
L0:;
}

U32 f18(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 l2=0;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 l6=0;
U32 si0,si1,si2,si3,si4,si5;
si0=-1U;
si1=0U;
si2=l0;
l3=si2;
si3=3U;
si2&=si3;
si2=!(si2);
if(si2){
goto L4;
}
si2=0U;
si3=l3;
si3=i32_load8_u(i->m0,si3);
si3=!(si3);
if(si3){
goto L2;
}
si2=l3;
si3=1U;
si2+=si3;
l0=si2;
si3=3U;
si2&=si3;
si2=!(si2);
if(si2){
goto L4;
}
si2=l0;
si2=i32_load8_u(i->m0,si2);
si2=!(si2);
if(si2){
goto L3;
}
si2=l3;
si3=2U;
si2+=si3;
l0=si2;
si3=3U;
si2&=si3;
si2=!(si2);
if(si2){
goto L4;
}
si2=l0;
si2=i32_load8_u(i->m0,si2);
si2=!(si2);
if(si2){
goto L3;
}
si2=l3;
si3=3U;
si2+=si3;
l0=si2;
si3=3U;
si2&=si3;
si2=!(si2);
if(si2){
goto L4;
}
si2=l0;
si2=i32_load8_u(i->m0,si2);
si2=!(si2);
if(si2){
goto L3;
}
si2=l3;
si3=4U;
si2+=si3;
l0=si2;
si3=3U;
si2&=si3;
if(si2){
goto L3;
}
L4:;
si2=l0;
si3=4U;
si2-=si3;
l2=si2;
si2=l0;
si3=5U;
si2-=si3;
l0=si2;
L5:;
W2C2_LOOP_START
{
si2=l0;
si3=4U;
si2+=si3;
l0=si2;
si2=16843008U;
si3=l2;
si4=4U;
si3+=si4;
l2=si3;
si3=i32_load(i->m0,si3);
l4=si3;
si2-=si3;
si3=l4;
si2|=si3;
si3=-2139062144U;
si2&=si3;
si3=-2139062144U;
si2=si2 == si3;
if(si2){
goto L5;
}
}
L6:;
W2C2_LOOP_START
{
si2=l0;
si3=1U;
si2+=si3;
l0=si2;
si2=l2;
si2=i32_load8_u(i->m0,si2);
l4=si2;
si2=l2;
si3=1U;
si2+=si3;
l2=si2;
si2=l4;
if(si2){
goto L6;
}
}
L3:;
si2=l0;
si3=l3;
si2-=si3;
L2:;
l0=si2;
si2=0U;
l4=si2;
si2=l0;
si3=l1;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+16U);
l2=si3;
if(si3){
si3=l2;
}else{
si3=l1;
si3=f16(instance,si3);
if(si3){
goto L7;
}
si3=l1;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+16U);
}
L8:;
si4=l1;
si4=i32_load(i->m0,(WasmMemoryAddress)si4+20U);
l6=si4;
si3-=si4;
si2=si2 > si3;
if(si2){
si2=l1;
si3=l3;
si4=l0;
si5=l1;
si5=i32_load(i->m0,(WasmMemoryAddress)si5+32U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si5,&m5_benchFuncType0);
si2=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si2,si3,si4);
}
l4=si2;
goto L7;
}
L9:;
si2=l0;
si3=l0;
si3=!(si3);
if(si3){
goto L10;
}
si2=0U;
l2=si2;
si2=l0;
si3=l1;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+64U);
si4=0U;
si3=(U32)((I32)si3<(I32)si4);
if(si3){
goto L10;
}
si2=l0;
si3=l3;
si2+=si3;
l4=si2;
L12:;
W2C2_LOOP_START
{
si2=l2;
si3=l4;
si2+=si3;
si3=1U;
si2-=si3;
si2=i32_load8_u(i->m0,si2);
si3=10U;
si2=si2 == si3;
if(si2){
goto L11;
}
si2=l0;
si3=l2;
si4=1U;
si3-=si4;
l2=si3;
si2+=si3;
if(si2){
goto L12;
}
}
si2=l0;
goto L10;
L11:;
si2=l1;
si3=l3;
si4=l0;
si5=l2;
si4+=si5;
l5=si4;
si5=l1;
si5=i32_load(i->m0,(WasmMemoryAddress)si5+32U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si5,&m5_benchFuncType0);
si2=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si2,si3,si4);
}
l4=si2;
si3=l5;
si2=si2 < si3;
if(si2){
goto L7;
}
si2=l3;
si3=l5;
si2+=si3;
l3=si2;
si2=l1;
si2=i32_load(i->m0,(WasmMemoryAddress)si2+20U);
l6=si2;
si2=0U;
si3=l2;
si2-=si3;
L10:;
l2=si2;
si2=l6;
si3=l3;
si4=l2;
si2=f36(instance,si2,si3,si4);
si2=l1;
si3=l1;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+20U);
si4=l2;
si3+=si4;
i32_store(i->m0,(WasmMemoryAddress)si2+20U,si3);
si2=l2;
si3=l5;
si2+=si3;
l4=si2;
L7:;
si2=l0;
si3=l0;
si4=l4;
si3=si3 == si4;
if(si3){
goto L1;
}
si2=l4;
L1:;
si3=l0;
si2=si2 != si3;
si0=si2?si0:si1;
L0:;
return si0;
}

void f19(wasmModuleInstance* instance,U32 l0) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 l1=0;
U32 si0,si1,si2,si3;
si0=l0;
si1=6472U;
si0=f18(instance,si0,si1);
si1=0U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
goto L0;
}
L1:;
si0=6536U;
si0=i32_load(i->m0,si0);
si1=10U;
si0=si0 == si1;
if(si0){
goto L2;
}
si0=6492U;
si0=i32_load(i->m0,si0);
l0=si0;
si1=6488U;
si1=i32_load(i->m0,si1);
si0=si0 == si1;
if(si0){
goto L2;
}
si0=6492U;
si1=l0;
si2=1U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l0;
si1=10U;
i32_store8(i->m0,si0,si1);
goto L0;
L2:;
si0=i->g0;
si1=16U;
si0-=si1;
l0=si0;
i->g0=si0;
si0=l0;
si1=10U;
i32_store8(i->m0,(WasmMemoryAddress)si0+15U,si1);
si0=6488U;
si0=i32_load(i->m0,si0);
l1=si0;
if(si0){
si0=l1;
}else{
si0=6472U;
si0=f16(instance,si0);
if(si0){
goto L3;
}
si0=6488U;
si0=i32_load(i->m0,si0);
}
L5:;
si1=6492U;
si1=i32_load(i->m0,si1);
l1=si1;
si0=si0 == si1;
if(si0){
goto L4;
}
si0=6536U;
si0=i32_load(i->m0,si0);
si1=10U;
si0=si0 == si1;
if(si0){
goto L4;
}
si0=6492U;
si1=l1;
si2=1U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l1;
si1=10U;
i32_store8(i->m0,si0,si1);
goto L3;
L4:;
si0=6472U;
si1=l0;
si2=15U;
si1+=si2;
si2=1U;
si3=6504U;
si3=i32_load(i->m0,si3);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_benchFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
si1=1U;
si0=si0 != si1;
if(si0){
goto L3;
}
si0=l0;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+15U);
L3:;
si0=l0;
si1=16U;
si0+=si1;
i->g0=si0;
L0:;
}

U32 f20(wasmModuleInstance* instance,U32 l0) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 si0,si1;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
si0=i22_wasiX5FsnapshotX5Fpreview18_fdX5Fclose(instance,si0);
si1=65535U;
si0&=si1;
l0=si0;
if(si0){
si0=3070800U;
si1=l0;
i32_store(i->m0,si0,si1);
si0=-1U;
}else{
si0=0U;
}
L1:;
L0:;
return si0;
}

U32 f21(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_benchInstance* i = (m5_benchInstance*)instance;
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
si0=3070800U;
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
si0=3070800U;
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

U32 f22(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_benchInstance* i = (m5_benchInstance*)instance;
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
si0=f21(instance,si0,si1,si2);
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
si1=f21(instance,si1,si2,si3);
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

U32 f23(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 l3=0;
U32 l4=0;
U32 si0,si1,si2;
si0=l0;
si1=6U;
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
l3=si0;
si0=i->g0;
si1=32U;
si0-=si1;
l4=si0;
i->g0=si0;
si0=l3;
si1=l4;
si2=8U;
si1+=si2;
si0=i22_wasiX5FsnapshotX5Fpreview113_fdX5FfdstatX5Fget(instance,si0,si1);
si1=65535U;
si0&=si1;
l3=si0;
if(si0){
goto L3;
}
si0=59U;
l3=si0;
si0=l4;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+8U);
si1=2U;
si0=si0 != si1;
if(si0){
goto L3;
}
si0=l4;
si0=i32_load8_u(i->m0,(WasmMemoryAddress)si0+16U);
si1=36U;
si0&=si1;
if(si0){
goto L3;
}
si0=1U;
goto L2;
L3:;
si0=3070800U;
si1=l3;
i32_store(i->m0,si0,si1);
si0=0U;
L2:;
l3=si0;
si0=l4;
si1=32U;
si0+=si1;
i->g0=si0;
si0=l3;
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
si0=f22(instance,si0,si1,si2);
L0:;
return si0;
}

U64 f24(wasmModuleInstance* instance,U32 l0,U64 l1,U32 l2) {
m5_benchInstance* i = (m5_benchInstance*)instance;
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
si0=3070800U;
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

U32 f25(wasmModuleInstance* instance,U32 l0,U32 l1) {
m5_benchInstance* i = (m5_benchInstance*)instance;
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
si0=3071872U;
si0=i32_load(i->m0,si0);
l2=si0;
if(si0){
si0=l2;
}else{
si0=3071872U;
si1=3071848U;
i32_store(i->m0,si0,si1);
si0=3071848U;
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
si0=3070800U;
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
si0=3070800U;
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

F64 f26(wasmModuleInstance* instance,F64 l0,U32 l1) {
m5_benchInstance* i = (m5_benchInstance*)instance;
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
sd0=f26(instance,sd0,si1);
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

void f27(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 si0,si1,si2,si3,si4,si5;
U64 sj1;
si0=i->g0;
si1=208U;
si0-=si1;
l3=si0;
i->g0=si0;
si0=l3;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+204U,si1);
si0=l3;
si1=192U;
si0+=si1;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=l3;
si1=184U;
si0+=si1;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=l3;
si1=176U;
si0+=si1;
sj1=W2C2_LL(0U);
i64_store(i->m0,si0,sj1);
si0=l3;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+168U,sj1);
si0=l3;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+160U,sj1);
si0=l3;
si1=l2;
i32_store(i->m0,(WasmMemoryAddress)si0+200U,si1);
si0=0U;
si1=l1;
si2=l3;
si3=200U;
si2+=si3;
si3=l3;
si4=80U;
si3+=si4;
si4=l3;
si5=160U;
si4+=si5;
si0=f28(instance,si0,si1,si2,si3,si4);
si1=0U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
si0=-1U;
}else{
si0=l0;
si0=i32_load(i->m0,si0);
l5=si0;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+60U);
si1=0U;
si0=(U32)((I32)si0<=(I32)si1);
if(si0){
si0=l0;
si1=l5;
si2=-33U;
si1&=si2;
i32_store(i->m0,si0,si1);
}
L2:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+44U);
si0=!(si0);
if(si0){
si0=l0;
si1=80U;
i32_store(i->m0,(WasmMemoryAddress)si0+44U,si1);
si0=l0;
si1=0U;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l0;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+16U,sj1);
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+40U);
l4=si0;
si0=l0;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+40U,si1);
goto L5;
}
L6:;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+16U);
if(si0){
goto L4;
}
L5:;
si0=-1U;
si1=l0;
si1=f16(instance,si1);
if(si1){
goto L3;
}
L4:;
si0=l0;
si1=l1;
si2=l3;
si3=200U;
si2+=si3;
si3=l3;
si4=80U;
si3+=si4;
si4=l3;
si5=160U;
si4+=si5;
si0=f28(instance,si0,si1,si2,si3,si4);
L3:;
l2=si0;
si0=l5;
si1=32U;
si0&=si1;
l1=si0;
si0=l4;
if(si0){
si0=l0;
si1=0U;
si2=0U;
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+32U);
{
wasmTableEntry entry=wasmTableGet(&i->t0,si3,&m5_benchFuncType0);
si0=((U32 (*)(wasmModuleInstance*,U32,U32,U32))entry.func)(entry.instance,si0,si1,si2);
}
si0=l0;
si1=0U;
i32_store(i->m0,(WasmMemoryAddress)si0+44U,si1);
si0=l0;
si1=l4;
i32_store(i->m0,(WasmMemoryAddress)si0+40U,si1);
si0=l0;
si1=0U;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
l4=si0;
si0=l0;
sj1=W2C2_LL(0U);
i64_store(i->m0,(WasmMemoryAddress)si0+16U,sj1);
si0=0U;
}else{
si0=l2;
}
L7:;
si0=l0;
si1=l0;
si1=i32_load(i->m0,si1);
si2=l1;
si1|=si2;
i32_store(i->m0,si0,si1);
si0=0U;
}
L1:;
si0=l3;
si1=208U;
si0+=si1;
i->g0=si0;
L0:;
}

U32 f28(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2,U32 l3,U32 l4) {
m5_benchInstance* i = (m5_benchInstance*)instance;
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
U32 l16=0;
U32 l17=0;
F64 l18=0;
U64 l19=0;
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
f17(instance,si0,si1,si2);
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
l16=si0;
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
l16=si0;
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
l15=si0;
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
l15=si0;
si1=0U;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L12;
}
si0=0U;
si1=l15;
si0-=si1;
l15=si0;
si0=l11;
si1=8192U;
si0|=si1;
l11=si0;
goto L12;
}
L24:;
si0=0U;
l15=si0;
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
si0=l15;
si1=214748364U;
si0=si0 <= si1;
if(si0){
si0=-1U;
si1=l15;
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
l15=si0;
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
l15=si0;
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
f29(instance,si0,si1,si2);
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
f29(instance,si0,si1,si2);
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
f29(instance,si0,si1,si2);
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
f29(instance,si0,si1,si2);
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
f29(instance,si0,si1,si2);
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
f29(instance,si0,si1,si2);
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
f29(instance,si0,si1,si2);
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
f29(instance,si0,si1,si2);
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
f29(instance,si0,si1,si2);
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
l17=si0;
si0=l1;
l8=si0;
L41:;
W2C2_LOOP_START
{
si0=-1U;
l9=si0;
si0=l17;
si1=214748364U;
si0=si0 <= si1;
if(si0){
si0=-1U;
si1=l17;
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
l17=si0;
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
si1=5919U;
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
si0=l16;
si1=0U;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
si0=l4;
si1=l16;
si2=2U;
si1<<=(si2&31);
si0+=si1;
si1=l5;
i32_store(i->m0,si0,si1);
si0=l7;
si1=l3;
si2=l16;
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
f29(instance,si0,si1,si2);
goto L44;
}
L46:;
si0=l16;
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
l17=si0;
si1=l11;
si2=l11;
si3=8192U;
si2&=si3;
si0=si2?si0:si1;
l14=si0;
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
goto L63;
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
goto L58;
case 35:
goto L61;
case 36:
goto L49;
case 37:
goto L49;
case 38:
goto L49;
case 39:
goto L10;
case 40:
goto L61;
case 41:
goto L10;
case 42:
goto L10;
case 43:
goto L10;
case 44:
goto L57;
case 45:
goto L65;
case 46:
goto L62;
case 47:
goto L64;
case 48:
goto L10;
case 49:
goto L10;
case 50:
goto L56;
case 51:
goto L10;
case 52:
goto L66;
case 53:
goto L10;
case 54:
goto L10;
case 55:
goto L63;
default:
goto L10;
}
L66:;
si0=0U;
l13=si0;
si0=l7;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+56U);
l19=sj0;
si0=1024U;
goto L60;
L65:;
si0=0U;
l5=si0;
si0=l8;
si1=255U;
si0&=si1;
switch(si0){
case 0:
goto L73;
case 1:
goto L72;
case 2:
goto L71;
case 3:
goto L70;
case 4:
goto L69;
case 5:
goto L3;
case 6:
goto L68;
case 7:
goto L67;
default:
goto L3;
}
L73:;
si0=l7;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
si1=l20;
i32_store(i->m0,si0,si1);
goto L3;
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
sj1=(U64)(I64)(I32)(si1);
i64_store(i->m0,si0,sj1);
goto L3;
L70:;
si0=l7;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
si1=l20;
i32_store16(i->m0,si0,si1);
goto L3;
L69:;
si0=l7;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
si1=l20;
i32_store8(i->m0,si0,si1);
goto L3;
L68:;
si0=l7;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
si1=l20;
i32_store(i->m0,si0,si1);
goto L3;
L67:;
si0=l7;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
si1=l20;
sj1=(U64)(I64)(I32)(si1);
i64_store(i->m0,si0,sj1);
goto L3;
L64:;
si0=l9;
si1=8U;
si2=l9;
si3=8U;
si2=si2 > si3;
si0=si2?si0:si1;
l9=si0;
si0=l14;
si1=8U;
si0|=si1;
l14=si0;
si0=120U;
l21=si0;
L63:;
si0=0U;
l13=si0;
si0=1024U;
l16=si0;
si0=l7;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+56U);
l19=sj0;
si0=!(sj0);
if(si0){
si0=l24;
l10=si0;
goto L59;
}
L74:;
si0=l21;
si1=32U;
si0&=si1;
l8=si0;
si0=l24;
l10=si0;
L75:;
W2C2_LOOP_START
{
si0=l10;
si1=1U;
si0-=si1;
l10=si0;
sj1=l19;
si1=(U32)(sj1);
si2=15U;
si1&=si2;
si2=6448U;
si1+=si2;
si1=i32_load8_u(i->m0,si1);
si2=l8;
si1|=si2;
i32_store8(i->m0,si0,si1);
sj0=l19;
sj1=W2C2_LL(15U);
si0=sj0 > sj1;
l5=si0;
sj0=l19;
sj1=W2C2_LL(4U);
sj0>>=(sj1&63);
l19=sj0;
si0=l5;
if(si0){
goto L75;
}
}
si0=l14;
si1=8U;
si0&=si1;
si0=!(si0);
if(si0){
goto L59;
}
si0=l21;
si1=4U;
si0=(U32)((I32)si0>>(si1&31));
si1=1024U;
si0+=si1;
l16=si0;
si0=2U;
l13=si0;
goto L59;
L62:;
si0=l24;
l10=si0;
si0=l7;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+56U);
l19=sj0;
si0=!(sj0);
si0=!(si0);
if(si0){
L77:;
W2C2_LOOP_START
{
si0=l10;
si1=1U;
si0-=si1;
l10=si0;
sj1=l19;
si1=(U32)(sj1);
si2=7U;
si1&=si2;
si2=48U;
si1|=si2;
i32_store8(i->m0,si0,si1);
sj0=l19;
sj1=W2C2_LL(7U);
si0=sj0 > sj1;
l5=si0;
sj0=l19;
sj1=W2C2_LL(3U);
sj0>>=(sj1&63);
l19=sj0;
si0=l5;
if(si0){
goto L77;
}
}
}
L76:;
si0=0U;
l13=si0;
si0=1024U;
l16=si0;
si0=l14;
si1=8U;
si0&=si1;
si0=!(si0);
if(si0){
goto L59;
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
goto L59;
L61:;
si0=l7;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+56U);
l19=sj0;
sj1=W2C2_LL(0U);
si0=(U64)((I64)sj0<(I64)sj1);
if(si0){
si0=l7;
sj1=W2C2_LL(0U);
sj2=l19;
sj1-=sj2;
l19=sj1;
i64_store(i->m0,(WasmMemoryAddress)si0+56U,sj1);
si0=1U;
l13=si0;
si0=1024U;
goto L60;
}
L78:;
si0=l14;
si1=2048U;
si0&=si1;
if(si0){
si0=1U;
l13=si0;
si0=1025U;
goto L60;
}
L79:;
si0=1026U;
si1=1024U;
si2=l14;
si3=1U;
si2&=si3;
l13=si2;
si0=si2?si0:si1;
L60:;
l16=si0;
sj0=l19;
sj1=W2C2_LL(4294967296U);
si0=sj0 < sj1;
if(si0){
sj0=l19;
l28=sj0;
si0=l24;
l10=si0;
goto L80;
}
L81:;
si0=l24;
l10=si0;
L82:;
W2C2_LOOP_START
{
si0=l10;
si1=1U;
si0-=si1;
l10=si0;
sj1=l19;
sj2=l19;
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
sj0=l19;
sj1=W2C2_LL(42949672959U);
si0=sj0 > sj1;
l5=si0;
sj0=l28;
l19=sj0;
si0=l5;
if(si0){
goto L82;
}
}
L80:;
sj0=l28;
si0=!(sj0);
if(si0){
goto L59;
}
sj0=l28;
si0=(U32)(sj0);
l5=si0;
L83:;
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
goto L83;
}
}
L59:;
si0=l12;
si1=l9;
si2=0U;
si1=(U32)((I32)si1<(I32)si2);
si0&=si1;
if(si0){
goto L4;
}
si0=l14;
si1=-65537U;
si0&=si1;
si1=l14;
si2=l12;
si0=si2?si0:si1;
l17=si0;
si0=l7;
sj0=i64_load(i->m0,(WasmMemoryAddress)si0+56U);
l19=sj0;
sj1=W2C2_LL(0U);
si0=sj0 != sj1;
if(si0){
goto L84;
}
si0=0U;
l11=si0;
si0=l9;
if(si0){
goto L84;
}
si0=l24;
l10=si0;
l5=si0;
goto L9;
L84:;
si0=l9;
sj1=l19;
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
L58:;
si0=l7;
si1=l7;
sj1=i64_load(i->m0,(WasmMemoryAddress)si1+56U);
i64_store8(i->m0,(WasmMemoryAddress)si0+55U,sj1);
si0=0U;
l13=si0;
si0=1024U;
l16=si0;
si0=1U;
l11=si0;
si0=l38;
l10=si0;
si0=l24;
l5=si0;
goto L9;
L57:;
si0=3070800U;
si0=i32_load(i->m0,si0);
l10=si0;
si0=3071872U;
si0=i32_load(i->m0,si0);
l5=si0;
if(si0){
si0=l5;
}else{
si0=3071872U;
si1=3071848U;
i32_store(i->m0,si0,si1);
si0=3071848U;
}
L85:;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+20U);
si0=0U;
si1=l10;
si2=l10;
si3=76U;
si2=si2 > si3;
si0=si2?si0:si1;
si1=1U;
si0<<=(si1&31);
si1=5824U;
si0+=si1;
si0=i32_load16_u(i->m0,si0);
si1=4272U;
si0+=si1;
goto L55;
L56:;
si0=l7;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+56U);
l5=si0;
si1=1110U;
si2=l5;
si0=si2?si0:si1;
L55:;
l10=si0;
l5=si0;
si0=l9;
si1=2147483647U;
si2=l9;
si3=2147483647U;
si2=si2 < si3;
si0=si2?si0:si1;
l8=si0;
l16=si0;
si1=0U;
si0=si0 != si1;
l11=si0;
si0=l5;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L89;
}
si0=l16;
si0=!(si0);
if(si0){
goto L89;
}
si0=l5;
si0=i32_load8_u(i->m0,si0);
si0=!(si0);
if(si0){
si0=l5;
l14=si0;
si0=l16;
l12=si0;
goto L87;
}
L90:;
si0=l16;
si1=1U;
si0-=si1;
l12=si0;
si1=0U;
si0=si0 != si1;
l11=si0;
si0=l5;
si1=1U;
si0+=si1;
l14=si0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L88;
}
si0=l12;
si0=!(si0);
if(si0){
goto L88;
}
si0=l14;
si0=i32_load8_u(i->m0,si0);
si0=!(si0);
if(si0){
goto L87;
}
si0=l16;
si1=2U;
si0-=si1;
l12=si0;
si1=0U;
si0=si0 != si1;
l11=si0;
si0=l5;
si1=2U;
si0+=si1;
l14=si0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L88;
}
si0=l12;
si0=!(si0);
if(si0){
goto L88;
}
si0=l14;
si0=i32_load8_u(i->m0,si0);
si0=!(si0);
if(si0){
goto L87;
}
si0=l16;
si1=3U;
si0-=si1;
l12=si0;
si1=0U;
si0=si0 != si1;
l11=si0;
si0=l5;
si1=3U;
si0+=si1;
l14=si0;
si1=3U;
si0&=si1;
si0=!(si0);
if(si0){
goto L88;
}
si0=l12;
si0=!(si0);
if(si0){
goto L88;
}
si0=l14;
si0=i32_load8_u(i->m0,si0);
si0=!(si0);
if(si0){
goto L87;
}
si0=l5;
si1=4U;
si0+=si1;
l14=si0;
si0=l16;
si1=4U;
si0-=si1;
l12=si0;
si1=0U;
si0=si0 != si1;
l11=si0;
goto L88;
L89:;
si0=l16;
l12=si0;
si0=l5;
l14=si0;
L88:;
si0=l11;
si0=!(si0);
if(si0){
goto L86;
}
si0=l14;
si0=i32_load8_u(i->m0,si0);
si0=!(si0);
if(si0){
goto L91;
}
si0=l12;
si1=4U;
si0=si0 < si1;
if(si0){
goto L91;
}
L92:;
W2C2_LOOP_START
{
si0=16843008U;
si1=l14;
si1=i32_load(i->m0,si1);
l16=si1;
si0-=si1;
si1=l16;
si0|=si1;
si1=-2139062144U;
si0&=si1;
si1=-2139062144U;
si0=si0 != si1;
if(si0){
goto L87;
}
si0=l14;
si1=4U;
si0+=si1;
l14=si0;
si0=l12;
si1=4U;
si0-=si1;
l12=si0;
si1=3U;
si0=si0 > si1;
if(si0){
goto L92;
}
}
L91:;
si0=l12;
si0=!(si0);
if(si0){
goto L86;
}
L87:;
L93:;
W2C2_LOOP_START
{
si0=l14;
si1=l14;
si1=i32_load8_u(i->m0,si1);
si1=!(si1);
if(si1){
goto L54;
}
si0=l14;
si1=1U;
si0+=si1;
l14=si0;
si0=l12;
si1=1U;
si0-=si1;
l12=si0;
if(si0){
goto L93;
}
}
L86:;
si0=0U;
L54:;
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
si0=1024U;
l16=si0;
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
L94:;
W2C2_LOOP_START
{
si0=l6;
si0=i32_load(i->m0,si0);
l8=si0;
si0=!(si0);
if(si0){
goto L95;
}
si0=l7;
si1=4U;
si0+=si1;
si1=l8;
si0=f25(instance,si0,si1);
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
goto L95;
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
goto L94;
}
L95:;
}
si0=l5;
si1=0U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
goto L4;
}
L50:;
si0=l14;
si1=73728U;
si0&=si1;
l11=si0;
if(si0){
goto L96;
}
si0=l5;
si1=l15;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L96;
}
si0=l7;
si1=112U;
si0+=si1;
si1=32U;
si2=l15;
si3=l5;
si2-=si3;
l6=si2;
si3=256U;
si4=l6;
si5=256U;
si4=si4 < si5;
l8=si4;
si2=si4?si2:si3;
si0=f37(instance,si0,si1,si2);
si0=l8;
si0=!(si0);
if(si0){
L98:;
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
f17(instance,si0,si1,si2);
}
L99:;
si0=l6;
si1=256U;
si0-=si1;
l6=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L98;
}
}
}
L97:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L96;
}
si0=l7;
si1=112U;
si0+=si1;
si1=l6;
si2=l0;
f17(instance,si0,si1,si2);
L96:;
si0=l5;
si0=!(si0);
if(si0){
goto L100;
}
si0=0U;
l6=si0;
L101:;
W2C2_LOOP_START
{
si0=l10;
si0=i32_load(i->m0,si0);
l8=si0;
si0=!(si0);
if(si0){
goto L100;
}
si0=l7;
si1=4U;
si0+=si1;
si1=l8;
si0=f25(instance,si0,si1);
l8=si0;
si1=l6;
si0+=si1;
l6=si0;
si1=l5;
si0=si0 > si1;
if(si0){
goto L100;
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
f17(instance,si0,si1,si2);
}
L102:;
si0=l10;
si1=4U;
si0+=si1;
l10=si0;
si0=l5;
si1=l6;
si0=si0 > si1;
if(si0){
goto L101;
}
}
L100:;
si0=l11;
si1=8192U;
si0=si0 != si1;
if(si0){
goto L103;
}
si0=l5;
si1=l15;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L103;
}
si0=l7;
si1=112U;
si0+=si1;
si1=32U;
si2=l15;
si3=l5;
si2-=si3;
l6=si2;
si3=256U;
si4=l6;
si5=256U;
si4=si4 < si5;
l8=si4;
si2=si4?si2:si3;
si0=f37(instance,si0,si1,si2);
si0=l8;
si0=!(si0);
if(si0){
L105:;
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
f17(instance,si0,si1,si2);
}
L106:;
si0=l6;
si1=256U;
si0-=si1;
l6=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L105;
}
}
}
L104:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L103;
}
si0=l7;
si1=112U;
si0+=si1;
si1=l6;
si2=l0;
f17(instance,si0,si1,si2);
L103:;
si0=l15;
si1=l5;
si2=l5;
si3=l15;
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
l18=sd0;
si0=l7;
si1=0U;
i32_store(i->m0,(WasmMemoryAddress)si0+108U,si1);
sd0=l18;
sj0=i64_reinterpret_f64(sd0);
sj1=W2C2_LL(0U);
si0=(U64)((I64)sj0<(I64)sj1);
if(si0){
sd0=l18;
sd0=-(sd0);
l18=sd0;
si0=1U;
l25=si0;
si0=1034U;
l31=si0;
si0=0U;
goto L107;
}
L108:;
si0=l14;
si1=2048U;
si0&=si1;
if(si0){
si0=1U;
l25=si0;
si0=1037U;
l31=si0;
si0=0U;
goto L107;
}
L109:;
si0=1040U;
si1=1035U;
si2=l14;
si3=1U;
si2&=si3;
l25=si2;
si0=si2?si0:si1;
l31=si0;
si0=l25;
si0=!(si0);
L107:;
l33=si0;
sd0=l18;
sd1=f64_reinterpret_i64(W2C2_LL(0x7FF0000000000000U));
si0=sd0 < sd1;
si0=!(si0);
if(si0){
si0=l25;
si1=3U;
si0+=si1;
l6=si0;
si0=l14;
si1=8192U;
si0&=si1;
if(si0){
goto L111;
}
si0=l6;
si1=l15;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L111;
}
si0=l7;
si1=624U;
si0+=si1;
si1=32U;
si2=l15;
si3=l6;
si2-=si3;
l5=si2;
si3=256U;
si4=l5;
si5=256U;
si4=si4 < si5;
l8=si4;
si2=si4?si2:si3;
si0=f37(instance,si0,si1,si2);
si0=l8;
si0=!(si0);
if(si0){
L113:;
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
f17(instance,si0,si1,si2);
}
L114:;
si0=l5;
si1=256U;
si0-=si1;
l5=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L113;
}
}
}
L112:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L111;
}
si0=l7;
si1=624U;
si0+=si1;
si1=l5;
si2=l0;
f17(instance,si0,si1,si2);
L111:;
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
f17(instance,si0,si1,si2);
si0=l0;
si0=i32_load(i->m0,si0);
}
L115:;
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=1081U;
si1=1100U;
si2=l21;
si3=32U;
si2&=si3;
l5=si2;
si0=si2?si0:si1;
si1=1085U;
si2=1104U;
si3=l5;
si1=si3?si1:si2;
sd2=l18;
sd3=l18;
si2=sd2 != sd3;
si0=si2?si0:si1;
si1=3U;
si2=l0;
f17(instance,si0,si1,si2);
}
L116:;
si0=l14;
si1=73728U;
si0&=si1;
si1=8192U;
si0=si0 != si1;
if(si0){
goto L117;
}
si0=l6;
si1=l15;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L117;
}
si0=l7;
si1=624U;
si0+=si1;
si1=32U;
si2=l15;
si3=l6;
si2-=si3;
l5=si2;
si3=256U;
si4=l5;
si5=256U;
si4=si4 < si5;
l8=si4;
si2=si4?si2:si3;
si0=f37(instance,si0,si1,si2);
si0=l8;
si0=!(si0);
if(si0){
L119:;
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
f17(instance,si0,si1,si2);
}
L120:;
si0=l5;
si1=256U;
si0-=si1;
l5=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L119;
}
}
}
L118:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L117;
}
si0=l7;
si1=624U;
si0+=si1;
si1=l5;
si2=l0;
f17(instance,si0,si1,si2);
L117:;
si0=l6;
si1=l15;
si2=l6;
si3=l15;
si2=(U32)((I32)si2>(I32)si3);
si0=si2?si0:si1;
l5=si0;
goto L3;
}
L110:;
sd0=l18;
si1=l7;
si2=108U;
si1+=si2;
sd0=f26(instance,sd0,si1);
l18=sd0;
sd1=l18;
sd0+=sd1;
l18=sd0;
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
goto L122;
}
goto L6;
}
L123:;
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
goto L121;
L122:;
si0=l7;
si1=l6;
si2=29U;
si1-=si2;
l10=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+108U,si1);
sd0=l18;
sd1=f64_reinterpret_i64(W2C2_LL(0x41B0000000000000U));
sd0*=sd1;
l18=sd0;
si0=6U;
si1=l9;
si2=l5;
si0=si2?si0:si1;
L121:;
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
l16=si0;
l6=si0;
L124:;
W2C2_LOOP_START
{
si0=l6;
sd1=l18;
sd2=f64_reinterpret_i64(W2C2_LL(0x41F0000000000000U));
si1=sd1 < sd2;
sd2=l18;
sd3=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
si2=sd2 >= sd3;
si1&=si2;
if(si1){
sd1=l18;
si1=I32_TRUNC_U_F64(sd1);
goto L125;
}
L126:;
si1=0U;
L125:;
l5=si1;
i32_store(i->m0,si0,si1);
si0=l6;
si1=4U;
si0+=si1;
l6=si0;
sd0=l18;
si1=l5;
sd1=(F64)(si1);
sd0-=sd1;
sd1=f64_reinterpret_i64(W2C2_LL(0x41CDCD6500000000U));
sd0*=sd1;
l18=sd0;
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
si0=sd0 != sd1;
if(si0){
goto L124;
}
}
si0=l10;
si1=0U;
si0=(U32)((I32)si0<=(I32)si1);
if(si0){
si0=l6;
l5=si0;
si0=l16;
l8=si0;
goto L127;
}
L128:;
si0=l16;
l8=si0;
L129:;
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
goto L130;
}
si0=l10;
sj0=(U64)(si0);
l42=sj0;
sj0=W2C2_LL(0U);
l19=sj0;
L131:;
W2C2_LOOP_START
{
si0=l5;
sj1=l19;
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
l19=sj2;
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
goto L131;
}
}
sj0=l28;
sj1=W2C2_LL(1000000000U);
si0=sj0 < sj1;
if(si0){
goto L130;
}
si0=l8;
si1=4U;
si0-=si1;
l8=si0;
sj1=l19;
i64_store32(i->m0,si0,sj1);
L130:;
L132:;
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
goto L132;
}
}
L133:;
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
goto L129;
}
}
L127:;
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
L135:;
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
goto L136;
}
L137:;
si0=1000000000U;
si1=l9;
si0>>=(si1&31);
l17=si0;
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
L138:;
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
si1=l17;
si0*=si1;
l10=si0;
si0=l6;
si1=4U;
si0+=si1;
l6=si0;
si1=l5;
si0=si0 < si1;
if(si0){
goto L138;
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
goto L136;
}
si0=l5;
si1=l10;
i32_store(i->m0,si0,si1);
si0=l5;
si1=4U;
si0+=si1;
l5=si0;
L136:;
si0=l7;
si1=l7;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+108U);
si2=l9;
si1+=si2;
l10=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+108U,si1);
si0=l16;
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
goto L135;
}
}
}
L134:;
si0=0U;
l11=si0;
si0=l5;
si1=l8;
si0=si0 <= si1;
if(si0){
goto L139;
}
si0=l16;
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
goto L139;
}
si0=10U;
l6=si0;
L140:;
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
goto L140;
}
}
L139:;
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
si2=l16;
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
l17=si0;
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
goto L142;
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
L144:;
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
goto L144;
}
}
}
L143:;
si0=l10;
si0=!(si0);
if(si0){
goto L142;
}
L145:;
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
goto L145;
}
}
L142:;
si0=l17;
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
si1=l17;
si2=4U;
si1+=si2;
l22=si1;
si2=l5;
si1=si1 == si2;
si0&=si1;
if(si0){
goto L146;
}
si0=l29;
si1=1U;
si0&=si1;
si0=!(si0);
if(si0){
sd0=f64_reinterpret_i64(W2C2_LL(0x4340000000000000U));
l18=sd0;
si0=l6;
si1=1000000000U;
si0=si0 != si1;
if(si0){
goto L147;
}
si0=l8;
si1=l17;
si0=si0 >= si1;
if(si0){
goto L147;
}
si0=l17;
si1=4U;
si0-=si1;
si0=i32_load8_u(i->m0,si0);
si1=1U;
si0&=si1;
si0=!(si0);
if(si0){
goto L147;
}
}
L148:;
sd0=f64_reinterpret_i64(W2C2_LL(0x4340000000000001U));
l18=sd0;
L147:;
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
goto L149;
}
si0=l31;
si0=i32_load8_u(i->m0,si0);
si1=45U;
si0=si0 != si1;
if(si0){
goto L149;
}
sd0=l23;
sd0=-(sd0);
l23=sd0;
sd0=l18;
sd0=-(sd0);
l18=sd0;
L149:;
si0=l17;
si1=l10;
si2=l9;
si1-=si2;
l10=si1;
i32_store(i->m0,si0,si1);
sd0=l18;
sd1=l23;
sd0+=sd1;
sd1=l18;
si0=sd0 == sd1;
if(si0){
goto L146;
}
si0=l17;
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
L151:;
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
L152:;
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
goto L151;
}
}
si0=l6;
si1=4U;
si0+=si1;
l17=si0;
}
L150:;
si0=l16;
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
goto L146;
}
si0=10U;
l6=si0;
L153:;
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
goto L153;
}
}
L146:;
si0=l17;
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
L141:;
si0=l5;
si1=l16;
si0-=si1;
l6=si0;
L154:;
W2C2_LOOP_START
{
si0=l6;
l10=si0;
si0=l5;
l17=si0;
si1=l8;
si0=si0 <= si1;
l9=si0;
if(si0){
goto L155;
}
si0=l10;
si1=4U;
si0-=si1;
l6=si0;
si0=l17;
si1=4U;
si0-=si1;
l5=si0;
si0=i32_load(i->m0,si0);
si0=!(si0);
if(si0){
goto L154;
}
L155:;
}
si0=l13;
si0=!(si0);
if(si0){
si0=l14;
si1=8U;
si0&=si1;
l22=si0;
goto L156;
}
L157:;
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
si0=l14;
si1=8U;
si0&=si1;
l22=si0;
if(si0){
goto L156;
}
si0=-9U;
l5=si0;
si0=l9;
if(si0){
goto L158;
}
si0=l17;
si1=4U;
si0-=si1;
si0=i32_load(i->m0,si0);
l9=si0;
si0=!(si0);
if(si0){
goto L158;
}
si0=0U;
l5=si0;
si0=l9;
si1=10U;
si0=REM_U(si0,si1);
if(si0){
goto L158;
}
si0=10U;
l6=si0;
L159:;
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
goto L159;
}
}
L158:;
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
goto L156;
}
L160:;
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
L156:;
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
goto L161;
}
L162:;
si0=l11;
si0=!(si0);
if(si0){
si0=l27;
l10=si0;
l6=si0;
goto L163;
}
L164:;
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
L165:;
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
goto L165;
}
}
L163:;
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
si0=f37(instance,si0,si1,si2);
}
L166:;
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
L161:;
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
si0=l14;
si1=73728U;
si0&=si1;
l14=si0;
if(si0){
goto L167;
}
si0=l13;
si1=l15;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L167;
}
si0=l7;
si1=624U;
si0+=si1;
si1=32U;
si2=l15;
si3=l13;
si2-=si3;
l5=si2;
si3=256U;
si4=l5;
si5=256U;
si4=si4 < si5;
l6=si4;
si2=si4?si2:si3;
si0=f37(instance,si0,si1,si2);
si0=l6;
si0=!(si0);
if(si0){
L169:;
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
f17(instance,si0,si1,si2);
}
L170:;
si0=l5;
si1=256U;
si0-=si1;
l5=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L169;
}
}
}
L168:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L167;
}
si0=l7;
si1=624U;
si0+=si1;
si1=l5;
si2=l0;
f17(instance,si0,si1,si2);
L167:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l31;
si1=l25;
si2=l0;
f17(instance,si0,si1,si2);
}
L171:;
si0=l14;
si1=65536U;
si0=si0 != si1;
if(si0){
goto L172;
}
si0=l13;
si1=l15;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L172;
}
si0=l7;
si1=624U;
si0+=si1;
si1=48U;
si2=l15;
si3=l13;
si2-=si3;
l5=si2;
si3=256U;
si4=l5;
si5=256U;
si4=si4 < si5;
l6=si4;
si2=si4?si2:si3;
si0=f37(instance,si0,si1,si2);
si0=l6;
si0=!(si0);
if(si0){
L174:;
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
f17(instance,si0,si1,si2);
}
L175:;
si0=l5;
si1=256U;
si0-=si1;
l5=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L174;
}
}
}
L173:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L172;
}
si0=l7;
si1=624U;
si0+=si1;
si1=l5;
si2=l0;
f17(instance,si0,si1,si2);
L172:;
si0=l34;
if(si0){
goto L8;
}
si0=l16;
si1=l8;
si2=l8;
si3=l16;
si2=si2 > si3;
si0=si2?si0:si1;
l11=si0;
l9=si0;
L176:;
W2C2_LOOP_START
{
si0=l9;
si0=i32_load(i->m0,si0);
l5=si0;
if(si0){
si0=8U;
l6=si0;
L181:;
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
goto L181;
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
goto L177;
}
goto L178;
}
L182:;
si0=l6;
si1=8U;
si0=si0 != si1;
if(si0){
goto L177;
}
goto L179;
}
L180:;
si0=9U;
l8=si0;
si0=l9;
si1=l11;
si0=si0 != si1;
if(si0){
goto L178;
}
L179:;
si0=l7;
si1=48U;
i32_store8(i->m0,(WasmMemoryAddress)si0+88U,si1);
si0=l36;
l5=si0;
goto L177;
L178:;
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
si0=f37(instance,si0,si1,si2);
L177:;
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
f17(instance,si0,si1,si2);
}
L183:;
si0=l9;
si1=4U;
si0+=si1;
l9=si0;
si1=l16;
si0=si0 <= si1;
if(si0){
goto L176;
}
}
si0=l26;
si0=!(si0);
if(si0){
goto L184;
}
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L184;
}
si0=1108U;
si1=1U;
si2=l0;
f17(instance,si0,si1,si2);
L184:;
si0=l12;
si1=0U;
si0=(U32)((I32)si0<=(I32)si1);
if(si0){
si0=l12;
l5=si0;
goto L185;
}
L186:;
si0=l9;
si1=l17;
si0=si0 >= si1;
if(si0){
si0=l12;
l5=si0;
goto L185;
}
L187:;
L188:;
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
goto L190;
}
L191:;
si0=l32;
l8=si0;
l6=si0;
L192:;
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
goto L192;
}
}
si0=l6;
si1=l7;
si2=80U;
si1+=si2;
si0=si0 <= si1;
if(si0){
goto L189;
}
L190:;
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
si0=f37(instance,si0,si1,si2);
L189:;
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
f17(instance,si0,si1,si2);
}
L193:;
si0=l12;
si1=9U;
si0-=si1;
l5=si0;
si0=l9;
si1=4U;
si0+=si1;
l9=si0;
si1=l17;
si0=si0 >= si1;
if(si0){
goto L185;
}
si0=l12;
si1=9U;
si0=(U32)((I32)si0>(I32)si1);
l6=si0;
si0=l5;
l12=si0;
si0=l6;
if(si0){
goto L188;
}
}
L185:;
si0=l0;
si1=l5;
si2=9U;
si1+=si2;
si2=9U;
f30(instance,si0,si1,si2);
goto L7;
L11:;
si0=3070800U;
si1=28U;
i32_store(i->m0,si0,si1);
goto L2;
L10:;
si0=0U;
l13=si0;
si0=1024U;
l16=si0;
si0=l24;
l5=si0;
si0=l14;
l17=si0;
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
si0=l15;
si1=l12;
si2=l13;
si1+=si2;
l8=si1;
si2=l8;
si3=l15;
si2=(U32)((I32)si2<(I32)si3);
si0=si2?si0:si1;
l5=si0;
si1=l6;
si0=(U32)((I32)si0>(I32)si1);
if(si0){
goto L4;
}
si0=l17;
si1=73728U;
si0&=si1;
l17=si0;
if(si0){
goto L194;
}
si0=l8;
si1=l15;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L194;
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
l14=si4;
si2=si4?si2:si3;
si0=f37(instance,si0,si1,si2);
si0=l14;
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
f17(instance,si0,si1,si2);
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
f17(instance,si0,si1,si2);
L194:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l16;
si1=l13;
si2=l0;
f17(instance,si0,si1,si2);
}
L198:;
si0=l17;
si1=65536U;
si0=si0 != si1;
if(si0){
goto L199;
}
si0=l8;
si1=l15;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L199;
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
si0=f37(instance,si0,si1,si2);
si0=l13;
si0=!(si0);
if(si0){
L201:;
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
f17(instance,si0,si1,si2);
}
L202:;
si0=l6;
si1=256U;
si0-=si1;
l6=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L201;
}
}
}
L200:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L199;
}
si0=l7;
si1=112U;
si0+=si1;
si1=l6;
si2=l0;
f17(instance,si0,si1,si2);
L199:;
si0=l9;
si1=l11;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L203;
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
si0=f37(instance,si0,si1,si2);
si0=l11;
si0=!(si0);
if(si0){
L205:;
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
f17(instance,si0,si1,si2);
}
L206:;
si0=l6;
si1=256U;
si0-=si1;
l6=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L205;
}
}
}
L204:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L203;
}
si0=l7;
si1=112U;
si0+=si1;
si1=l6;
si2=l0;
f17(instance,si0,si1,si2);
L203:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l10;
si1=l9;
si2=l0;
f17(instance,si0,si1,si2);
}
L207:;
si0=l17;
si1=8192U;
si0=si0 != si1;
if(si0){
goto L3;
}
si0=l8;
si1=l15;
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
si0=f37(instance,si0,si1,si2);
si0=l8;
si0=!(si0);
if(si0){
L209:;
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
f17(instance,si0,si1,si2);
}
L210:;
si0=l6;
si1=256U;
si0-=si1;
l6=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L209;
}
}
}
L208:;
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
f17(instance,si0,si1,si2);
goto L3;
L8:;
si0=l12;
si1=0U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
goto L211;
}
si0=l17;
si1=l8;
si2=4U;
si1+=si2;
si2=l8;
si3=l17;
si2=si2 < si3;
si0=si2?si0:si1;
l17=si0;
si0=l8;
l9=si0;
L212:;
W2C2_LOOP_START
{
si0=l9;
si0=i32_load(i->m0,si0);
l5=si0;
if(si0){
si0=l32;
l6=si0;
L215:;
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
goto L215;
}
}
goto L213;
}
L214:;
si0=l7;
si1=48U;
i32_store8(i->m0,(WasmMemoryAddress)si0+88U,si1);
si0=l36;
l6=si0;
L213:;
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
goto L216;
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
si0=f37(instance,si0,si1,si2);
si0=l7;
si1=80U;
si0+=si1;
l6=si0;
goto L216;
}
L217:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l6;
si1=1U;
si2=l0;
f17(instance,si0,si1,si2);
}
L218:;
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
goto L216;
}
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L216;
}
si0=1108U;
si1=1U;
si2=l0;
f17(instance,si0,si1,si2);
L216:;
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
f17(instance,si0,si1,si2);
}
L219:;
si0=l12;
si1=l5;
si0-=si1;
l12=si0;
si0=l9;
si1=4U;
si0+=si1;
l9=si0;
si1=l17;
si0=si0 >= si1;
if(si0){
goto L211;
}
si0=l12;
si1=0U;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L212;
}
}
L211:;
si0=l0;
si1=l12;
si2=18U;
si1+=si2;
si2=18U;
f30(instance,si0,si1,si2);
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
f17(instance,si0,si1,si2);
L7:;
si0=l14;
si1=8192U;
si0=si0 != si1;
if(si0){
goto L5;
}
si0=l13;
si1=l15;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L5;
}
si0=l7;
si1=624U;
si0+=si1;
si1=32U;
si2=l15;
si3=l13;
si2-=si3;
l5=si2;
si3=256U;
si4=l5;
si5=256U;
si4=si4 < si5;
l6=si4;
si2=si4?si2:si3;
si0=f37(instance,si0,si1,si2);
si0=l6;
si0=!(si0);
if(si0){
L221:;
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
f17(instance,si0,si1,si2);
}
L222:;
si0=l5;
si1=256U;
si0-=si1;
l5=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L221;
}
}
}
L220:;
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
f17(instance,si0,si1,si2);
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
l16=si0;
si0=l9;
si1=11U;
si0=si0 > si1;
if(si0){
goto L223;
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
goto L224;
}
L225:;
si0=l9;
si1=12U;
si0-=si1;
l5=si0;
sd0=f64_reinterpret_i64(W2C2_LL(0x4030000000000000U));
l23=sd0;
L226:;
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
goto L226;
}
}
si0=0U;
si1=l5;
si0-=si1;
l5=si0;
L224:;
si0=l9;
si1=5U;
si0-=si1;
si1=7U;
si0=si0 >= si1;
if(si0){
L228:;
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
goto L228;
}
}
}
L227:;
si0=l16;
si0=i32_load8_u(i->m0,si0);
si1=45U;
si0=si0 == si1;
if(si0){
sd0=l23;
sd1=l18;
sd1=-(sd1);
sd2=l23;
sd1-=sd2;
sd0+=sd1;
sd0=-(sd0);
l18=sd0;
goto L223;
}
L229:;
sd0=l18;
sd1=l23;
sd0+=sd1;
sd1=l23;
sd0-=sd1;
l18=sd0;
L223:;
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
L232:;
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
goto L232;
}
}
goto L230;
}
L231:;
si0=l7;
si1=48U;
i32_store8(i->m0,(WasmMemoryAddress)si0+79U,si1);
si0=l39;
l6=si0;
L230:;
si0=l25;
si1=2U;
si0|=si1;
l17=si0;
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
si0=l14;
si1=8U;
si0&=si1;
l10=si0;
si0=l7;
si1=80U;
si0+=si1;
l6=si0;
L233:;
W2C2_LOOP_START
{
si0=l6;
l5=si0;
sd1=l18;
sd1=fabs(sd1);
sd2=f64_reinterpret_i64(W2C2_LL(0x41E0000000000000U));
si1=sd1 < sd2;
if(si1){
sd1=l18;
si1=I32_TRUNC_S_F64(sd1);
goto L234;
}
L235:;
si1=-2147483648U;
L234:;
l6=si1;
si2=6448U;
si1+=si2;
si1=i32_load8_u(i->m0,si1);
si2=l8;
si1|=si2;
i32_store8(i->m0,si0,si1);
sd0=l18;
si1=l6;
sd1=(F64)(I32)(si1);
sd0-=sd1;
sd1=f64_reinterpret_i64(W2C2_LL(0x4030000000000000U));
sd0*=sd1;
l18=sd0;
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
goto L236;
}
si0=l10;
if(si0){
goto L237;
}
si0=l9;
si1=0U;
si0=(U32)((I32)si0>(I32)si1);
if(si0){
goto L237;
}
sd0=l18;
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
si0=sd0 == sd1;
if(si0){
goto L236;
}
L237:;
si0=l5;
si1=46U;
i32_store8(i->m0,(WasmMemoryAddress)si0+1U,si1);
si0=l5;
si1=2U;
si0+=si1;
l6=si0;
L236:;
sd0=l18;
sd1=f64_reinterpret_i64(W2C2_LL(0x0000000000000000U));
si0=sd0 != sd1;
if(si0){
goto L233;
}
}
si0=2147483645U;
si1=l27;
si2=l12;
si1-=si2;
l11=si1;
si2=l17;
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
si0=l14;
si1=73728U;
si0&=si1;
l8=si0;
if(si0){
goto L238;
}
si0=l13;
si1=l15;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L238;
}
si0=l7;
si1=624U;
si0+=si1;
si1=32U;
si2=l15;
si3=l13;
si2-=si3;
l5=si2;
si3=256U;
si4=l5;
si5=256U;
si4=si4 < si5;
l9=si4;
si2=si4?si2:si3;
si0=f37(instance,si0,si1,si2);
si0=l9;
si0=!(si0);
if(si0){
L240:;
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
f17(instance,si0,si1,si2);
}
L241:;
si0=l5;
si1=256U;
si0-=si1;
l5=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L240;
}
}
}
L239:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L238;
}
si0=l7;
si1=624U;
si0+=si1;
si1=l5;
si2=l0;
f17(instance,si0,si1,si2);
L238:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l16;
si1=l17;
si2=l0;
f17(instance,si0,si1,si2);
}
L242:;
si0=l8;
si1=65536U;
si0=si0 != si1;
if(si0){
goto L243;
}
si0=l13;
si1=l15;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L243;
}
si0=l7;
si1=624U;
si0+=si1;
si1=48U;
si2=l15;
si3=l13;
si2-=si3;
l5=si2;
si3=256U;
si4=l5;
si5=256U;
si4=si4 < si5;
l9=si4;
si2=si4?si2:si3;
si0=f37(instance,si0,si1,si2);
si0=l9;
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
f17(instance,si0,si1,si2);
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
goto L243;
}
si0=l7;
si1=624U;
si0+=si1;
si1=l5;
si2=l0;
f17(instance,si0,si1,si2);
L243:;
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
f17(instance,si0,si1,si2);
}
L247:;
si0=l10;
si1=l6;
si0-=si1;
l5=si0;
si1=0U;
si0=(U32)((I32)si0<=(I32)si1);
if(si0){
goto L248;
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
si0=f37(instance,si0,si1,si2);
si0=l6;
si0=!(si0);
if(si0){
L250:;
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
f17(instance,si0,si1,si2);
}
L251:;
si0=l5;
si1=256U;
si0-=si1;
l5=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L250;
}
}
}
L249:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
if(si0){
goto L248;
}
si0=l7;
si1=624U;
si0+=si1;
si1=l5;
si2=l0;
f17(instance,si0,si1,si2);
L248:;
si0=l0;
si0=i32_load8_u(i->m0,si0);
si1=32U;
si0&=si1;
si0=!(si0);
if(si0){
si0=l12;
si1=l11;
si2=l0;
f17(instance,si0,si1,si2);
}
L252:;
si0=l8;
si1=8192U;
si0=si0 != si1;
if(si0){
goto L5;
}
si0=l13;
si1=l15;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L5;
}
si0=l7;
si1=624U;
si0+=si1;
si1=32U;
si2=l15;
si3=l13;
si2-=si3;
l5=si2;
si3=256U;
si4=l5;
si5=256U;
si4=si4 < si5;
l6=si4;
si2=si4?si2:si3;
si0=f37(instance,si0,si1,si2);
si0=l6;
si0=!(si0);
if(si0){
L254:;
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
f17(instance,si0,si1,si2);
}
L255:;
si0=l5;
si1=256U;
si0-=si1;
l5=si0;
si1=255U;
si0=si0 > si1;
if(si0){
goto L254;
}
}
}
L253:;
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
f17(instance,si0,si1,si2);
L5:;
si0=l13;
si1=l15;
si2=l13;
si3=l15;
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
si0=3070800U;
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

void f29(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_benchInstance* i = (m5_benchInstance*)instance;
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
si0=1259U;
si1=6592U;
si0=f18(instance,si0,si1);
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

void f30(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_benchInstance* i = (m5_benchInstance*)instance;
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
si0=f37(instance,si0,si1,si2);
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
f17(instance,si0,si1,si2);
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
f17(instance,si0,si1,si2);
L1:;
si0=l3;
si1=256U;
si0+=si1;
i->g0=si0;
L0:;
}

U32 f31(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 l6=0;
U32 l7=0;
U32 si0,si1,si2,si3;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+68U);
l3=si0;
si0=i32_load(i->m0,si0);
l5=si0;
si0=l3;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l4=si0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+20U);
si2=l0;
si2=i32_load(i->m0,(WasmMemoryAddress)si2+24U);
l7=si2;
si1-=si2;
l6=si1;
si2=l4;
si3=l6;
si2=si2 < si3;
si0=si2?si0:si1;
l6=si0;
if(si0){
si0=l5;
si1=l7;
si2=l6;
si0=f36(instance,si0,si1,si2);
si0=l3;
si1=l3;
si1=i32_load(i->m0,si1);
si2=l6;
si1+=si2;
l5=si1;
i32_store(i->m0,si0,si1);
si0=l3;
si1=l3;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=l6;
si1-=si2;
l4=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
}
L1:;
si0=l4;
si1=l2;
si2=l2;
si3=l4;
si2=si2 > si3;
si0=si2?si0:si1;
l4=si0;
if(si0){
si0=l5;
si1=l1;
si2=l4;
si0=f36(instance,si0,si1,si2);
si0=l3;
si1=l3;
si1=i32_load(i->m0,si1);
si2=l4;
si1+=si2;
l5=si1;
i32_store(i->m0,si0,si1);
si0=l3;
si1=l3;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+4U);
si2=l4;
si1-=si2;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
}
L2:;
si0=l5;
si1=0U;
i32_store8(i->m0,si0,si1);
si0=l0;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+40U);
l3=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+24U,si1);
si0=l0;
si1=l3;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=l2;
L0:;
return si0;
}

void f32(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 l6=0;
U32 l7=0;
U32 l8=0;
U32 l9=0;
U32 l10=0;
U32 si0,si1,si2,si3,si4,si5;
si0=4U;
l8=si0;
si0=i->g0;
si1=496U;
si0-=si1;
l5=si0;
i->g0=si0;
si0=l5;
si1=l0;
i32_store(i->m0,si0,si1);
si0=l1;
si1=1U;
si0=(U32)((I32)si0<=(I32)si1);
if(si0){
goto L1;
}
si0=l5;
si1=4U;
si0|=si1;
l6=si0;
si0=1U;
l4=si0;
si0=l0;
l3=si0;
L2:;
W2C2_LOOP_START
{
si0=l0;
si1=l3;
si2=4U;
si1-=si2;
l3=si1;
si2=l2;
si3=l1;
si4=2U;
si3-=si4;
l9=si3;
si4=2U;
si3<<=(si4&31);
si2+=si3;
si2=i32_load(i->m0,si2);
si1-=si2;
l7=si1;
si2=1U;
si0=f35(instance,si0,si1,si2);
si1=0U;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
si0=l0;
si1=l3;
si2=1U;
si0=f35(instance,si0,si1,si2);
si1=0U;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L3;
}
}
L4:;
si0=l6;
si1=l7;
si2=l3;
si3=l7;
si4=l3;
si5=1U;
si3=f35(instance,si3,si4,si5);
si4=0U;
si3=(U32)((I32)si3>=(I32)si4);
l10=si3;
si1=si3?si1:si2;
l3=si1;
i32_store(i->m0,si0,si1);
si0=l6;
si1=4U;
si0+=si1;
l6=si0;
si0=l4;
si1=1U;
si0+=si1;
l4=si0;
si0=l1;
si1=1U;
si0-=si1;
si1=l9;
si2=l10;
si0=si2?si0:si1;
l1=si0;
si1=1U;
si0=(U32)((I32)si0>(I32)si1);
if(si0){
goto L2;
}
L3:;
}
si0=l4;
si1=2U;
si0=si0 < si1;
if(si0){
goto L1;
}
si0=l5;
si1=l4;
si2=2U;
si1<<=(si2&31);
si0+=si1;
l9=si0;
si1=l5;
si2=240U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l4;
si1=-2U;
si0&=si1;
l10=si0;
si0=l4;
si1=1U;
si0&=si1;
l4=si0;
L5:;
W2C2_LOOP_START
{
si0=l9;
si0=i32_load(i->m0,si0);
si1=l5;
si1=i32_load(i->m0,si1);
l6=si1;
si2=l8;
si3=256U;
si4=l8;
si5=256U;
si4=si4 < si5;
si2=si4?si2:si3;
l3=si2;
si0=f36(instance,si0,si1,si2);
si0=l5;
l1=si0;
si0=l10;
l7=si0;
L6:;
W2C2_LOOP_START
{
si0=l1;
si1=l6;
si2=l1;
si3=4U;
si2+=si3;
l2=si2;
si2=i32_load(i->m0,si2);
l0=si2;
si3=l3;
si1=f36(instance,si1,si2,si3);
si2=l3;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l2;
si1=l0;
si2=l1;
si3=8U;
si2+=si3;
l1=si2;
si2=i32_load(i->m0,si2);
l6=si2;
si3=l3;
si1=f36(instance,si1,si2,si3);
si2=l3;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l7;
si1=2U;
si0-=si1;
l7=si0;
if(si0){
goto L6;
}
}
si0=l4;
if(si0){
si0=l1;
si1=l6;
si2=l1;
si3=4U;
si2+=si3;
si2=i32_load(i->m0,si2);
si3=l3;
si1=f36(instance,si1,si2,si3);
si2=l3;
si1+=si2;
i32_store(i->m0,si0,si1);
}
L7:;
si0=l8;
si1=l3;
si0-=si1;
l8=si0;
if(si0){
goto L5;
}
}
L1:;
si0=l5;
si1=496U;
si0+=si1;
i->g0=si0;
L0:;
}

void f33(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2,U32 l3,U32 l4) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 l5=0;
U32 l6=0;
U32 l7=0;
U32 l8=0;
U32 l9=0;
U32 l10=0;
U32 l11=0;
U32 l12=0;
U32 l13=0;
U32 si0,si1,si2,si3,si4,si5;
si0=i->g0;
si1=496U;
si0-=si1;
l11=si0;
i->g0=si0;
si0=l11;
si1=l0;
i32_store(i->m0,si0,si1);
si0=l1;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+4U);
l8=si0;
si0=0U;
si1=l1;
si1=i32_load(i->m0,si1);
l1=si1;
si2=1U;
si1=si1 == si2;
si2=l8;
si0=si2?si0:si1;
if(si0){
goto L6;
}
si0=l0;
si1=l4;
si2=l2;
si3=2U;
si2<<=(si3&31);
si1+=si2;
l5=si1;
si1=i32_load(i->m0,si1);
si0-=si1;
l6=si0;
si1=l0;
si2=1U;
si0=f35(instance,si0,si1,si2);
si1=0U;
si0=(U32)((I32)si0<=(I32)si1);
if(si0){
goto L6;
}
si0=l3;
if(si0){
goto L7;
}
si0=l2;
si1=2U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
goto L7;
}
si0=l5;
si1=8U;
si0-=si1;
si0=i32_load(i->m0,si0);
l3=si0;
si0=l0;
si1=4U;
si0-=si1;
l5=si0;
si1=l6;
si2=1U;
si0=f35(instance,si0,si1,si2);
si1=0U;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L5;
}
si0=l5;
si1=l3;
si0-=si1;
si1=l6;
si2=1U;
si0=f35(instance,si0,si1,si2);
si1=0U;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
goto L5;
}
L7:;
si0=l11;
si1=l6;
i32_store(i->m0,(WasmMemoryAddress)si0+4U,si1);
si0=l1;
si1=1U;
si0-=si1;
si0=I32_CTZ(si0);
l5=si0;
if(si0){
si0=32U;
si1=l5;
si2=31U;
si1=si1 > si2;
if(si1){
goto L8;
}
si0=l5;
l3=si0;
si0=l8;
l7=si0;
si0=l1;
l8=si0;
goto L3;
}
L9:;
si0=l8;
si0=I32_CTZ(si0);
l3=si0;
si0=!(si0);
if(si0){
goto L4;
}
si0=l3;
si1=32U;
si0+=si1;
L8:;
l3=si0;
si1=32U;
si0-=si1;
l5=si0;
goto L3;
L6:;
si0=l3;
if(si0){
goto L1;
}
L5:;
si0=l0;
l9=si0;
goto L2;
L4:;
si0=0U;
l3=si0;
si0=l8;
l7=si0;
si0=l1;
l8=si0;
si0=0U;
l5=si0;
L3:;
si0=l2;
si1=l3;
si0+=si1;
l2=si0;
si0=l7;
si1=l5;
si0>>=(si1&31);
l3=si0;
si0=0U;
si1=l7;
si2=32U;
si3=l5;
si2-=si3;
si1<<=(si2&31);
si2=l8;
si3=l5;
si2>>=(si3&31);
si1|=si2;
l7=si1;
si2=1U;
si1=si1 == si2;
si2=l3;
si0=si2?si0:si1;
si0=!(si0);
if(si0){
si0=l11;
si1=8U;
si0|=si1;
l8=si0;
si0=2U;
l10=si0;
L12:;
W2C2_LOOP_START
{
si0=l6;
si1=l4;
si2=l2;
si3=2U;
si2<<=(si3&31);
si1+=si2;
l1=si1;
si1=i32_load(i->m0,si1);
si0-=si1;
l9=si0;
si1=l0;
si2=1U;
si0=f35(instance,si0,si1,si2);
si1=0U;
si0=(U32)((I32)si0<=(I32)si1);
if(si0){
si0=l6;
l9=si0;
goto L13;
}
L14:;
si0=l2;
si1=2U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
goto L15;
}
si0=l1;
si1=8U;
si0-=si1;
si0=i32_load(i->m0,si0);
l1=si0;
si0=l6;
si1=4U;
si0-=si1;
l5=si0;
si1=l9;
si2=1U;
si0=f35(instance,si0,si1,si2);
si1=0U;
si0=(U32)((I32)si0>=(I32)si1);
if(si0){
si0=l6;
l9=si0;
goto L13;
}
L16:;
si0=l5;
si1=l1;
si0-=si1;
si1=l9;
si2=1U;
si0=f35(instance,si0,si1,si2);
si1=0U;
si0=(U32)((I32)si0<(I32)si1);
if(si0){
goto L15;
}
si0=l6;
l9=si0;
goto L13;
L15:;
si0=l8;
si1=l9;
i32_store(i->m0,si0,si1);
si0=l7;
si1=1U;
si0-=si1;
si0=I32_CTZ(si0);
l1=si0;
si0=!(si0);
if(si0){
si0=l3;
si0=I32_CTZ(si0);
l1=si0;
if(si0){
si0=l1;
si1=32U;
si0+=si1;
l6=si0;
goto L18;
}
L20:;
si0=0U;
l6=si0;
si0=l3;
l5=si0;
si0=l7;
l3=si0;
si0=0U;
l1=si0;
goto L17;
}
L19:;
si0=32U;
l6=si0;
si0=l1;
si1=32U;
si0=si0 >= si1;
if(si0){
goto L18;
}
si0=l1;
l6=si0;
si0=l3;
l5=si0;
si0=l7;
l3=si0;
goto L17;
L18:;
si0=l6;
si1=32U;
si0-=si1;
l1=si0;
si0=0U;
l5=si0;
L17:;
si0=l3;
si1=l1;
si0>>=(si1&31);
l7=si0;
si0=l10;
si1=1U;
si0+=si1;
l10=si0;
si0=l8;
si1=4U;
si0+=si1;
l8=si0;
si0=l2;
si1=l6;
si0+=si1;
l2=si0;
si0=l5;
si1=l1;
si0>>=(si1&31);
l3=si0;
si0=l9;
l6=si0;
si0=l7;
si1=l5;
si2=32U;
si3=l1;
si2-=si3;
si1<<=(si2&31);
si0|=si1;
l7=si0;
si1=1U;
si0=si0 != si1;
if(si0){
goto L12;
}
si0=l3;
if(si0){
goto L12;
}
L13:;
}
si0=l10;
si1=2U;
si0=si0 < si1;
if(si0){
goto L2;
}
goto L10;
}
L11:;
si0=2U;
l10=si0;
si0=l6;
l9=si0;
L10:;
si0=l11;
si1=l10;
si2=2U;
si1<<=(si2&31);
si0+=si1;
l12=si0;
si1=l11;
si2=240U;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l10;
si1=-2U;
si0&=si1;
l13=si0;
si0=l10;
si1=1U;
si0&=si1;
l10=si0;
si0=4U;
l7=si0;
L21:;
W2C2_LOOP_START
{
si0=l12;
si0=i32_load(i->m0,si0);
si1=l11;
si1=i32_load(i->m0,si1);
l6=si1;
si2=l7;
si3=256U;
si4=l7;
si5=256U;
si4=si4 < si5;
si2=si4?si2:si3;
l1=si2;
si0=f36(instance,si0,si1,si2);
si0=l11;
l0=si0;
si0=l13;
l3=si0;
L22:;
W2C2_LOOP_START
{
si0=l0;
si1=l6;
si2=l0;
si3=4U;
si2+=si3;
l8=si2;
si2=i32_load(i->m0,si2);
l5=si2;
si3=l1;
si1=f36(instance,si1,si2,si3);
si2=l1;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l8;
si1=l5;
si2=l0;
si3=8U;
si2+=si3;
l0=si2;
si2=i32_load(i->m0,si2);
l6=si2;
si3=l1;
si1=f36(instance,si1,si2,si3);
si2=l1;
si1+=si2;
i32_store(i->m0,si0,si1);
si0=l3;
si1=2U;
si0-=si1;
l3=si0;
if(si0){
goto L22;
}
}
si0=l10;
if(si0){
si0=l0;
si1=l6;
si2=l0;
si3=4U;
si2+=si3;
si2=i32_load(i->m0,si2);
si3=l1;
si1=f36(instance,si1,si2,si3);
si2=l1;
si1+=si2;
i32_store(i->m0,si0,si1);
}
L23:;
si0=l7;
si1=l1;
si0-=si1;
l7=si0;
if(si0){
goto L21;
}
}
L2:;
si0=l9;
si1=l2;
si2=l4;
f32(instance,si0,si1,si2);
L1:;
si0=l11;
si1=496U;
si0+=si1;
i->g0=si0;
L0:;
}

void f34(wasmModuleInstance* instance) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 l0=0;
U32 l1=0;
U32 l2=0;
U32 l3=0;
U32 l4=0;
U32 l5=0;
U32 l6=0;
U32 l7=0;
U32 l8=0;
U32 si0,si1,si2,si3,si4,si5,si6;
U64 sj1;
si0=2188880U;
l3=si0;
si0=i->g0;
si1=208U;
si0-=si1;
l0=si0;
i->g0=si0;
si0=l0;
sj1=W2C2_LL(1U);
i64_store(i->m0,(WasmMemoryAddress)si0+8U,sj1);
si0=l0;
si1=4U;
i32_store(i->m0,(WasmMemoryAddress)si0+16U,si1);
si0=l0;
si1=4U;
i32_store(i->m0,(WasmMemoryAddress)si0+20U,si1);
si0=l0;
si1=16U;
si0+=si1;
si1=8U;
si0|=si1;
l2=si0;
si0=4U;
l1=si0;
l6=si0;
L1:;
W2C2_LOOP_START
{
si0=l2;
si1=l6;
si2=4U;
si1+=si2;
si2=l1;
l6=si2;
si1+=si2;
l1=si1;
i32_store(i->m0,si0,si1);
si0=l2;
si1=4U;
si0+=si1;
l2=si0;
si0=l1;
si1=800000U;
si0=si0 < si1;
if(si0){
goto L1;
}
}
si0=799996U;
l6=si0;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+12U);
l4=si0;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l7=si0;
si0=1U;
l2=si0;
si0=1U;
l1=si0;
L2:;
W2C2_LOOP_START
{
si0=l1;
si1=3U;
si0&=si1;
si1=3U;
si0=si0 == si1;
if(si0){
si0=l3;
si1=l2;
si2=l0;
si3=16U;
si2+=si3;
f32(instance,si0,si1,si2);
si0=l0;
si1=l5;
si2=2U;
si1>>=(si2&31);
l4=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l5;
si1=30U;
si0<<=(si1&31);
si1=l1;
si2=2U;
si1>>=(si2&31);
si0|=si1;
l1=si0;
si0=l2;
si1=2U;
si0+=si1;
goto L3;
}
L4:;
si0=l6;
si1=l0;
si2=16U;
si1+=si2;
si2=l2;
si3=1U;
si2-=si3;
l8=si2;
si3=2U;
si2<<=(si3&31);
si1+=si2;
si1=i32_load(i->m0,si1);
si0=si0 <= si1;
if(si0){
si0=l3;
si1=l0;
si2=8U;
si1+=si2;
si2=l2;
si3=0U;
si4=l0;
si5=16U;
si4+=si5;
f33(instance,si0,si1,si2,si3,si4);
goto L5;
}
L6:;
si0=l3;
si1=l2;
si2=l0;
si3=16U;
si2+=si3;
f32(instance,si0,si1,si2);
L5:;
si0=l2;
si1=1U;
si0=si0 == si1;
if(si0){
si0=l0;
si1=l5;
si2=1U;
si1<<=(si2&31);
si2=l1;
si3=31U;
si2>>=(si3&31);
si1|=si2;
l4=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l1;
si1=1U;
si0<<=(si1&31);
l1=si0;
si0=0U;
goto L3;
}
L7:;
si0=l0;
si1=l7;
si2=l4;
si3=l8;
si4=31U;
si3=si3 > si4;
l5=si3;
si1=si3?si1:si2;
si2=l2;
si3=33U;
si2-=si3;
si3=l8;
si4=l5;
si2=si4?si2:si3;
l2=si2;
si1<<=(si2&31);
si2=0U;
si3=l1;
si4=l5;
si2=si4?si2:si3;
l1=si2;
si3=32U;
si4=l2;
si3-=si4;
si2>>=(si3&31);
si1|=si2;
l4=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l1;
si1=l2;
si0<<=(si1&31);
l1=si0;
si0=1U;
L3:;
l2=si0;
si0=l0;
si1=l1;
si2=1U;
si1|=si2;
l7=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l6;
si1=4U;
si0-=si1;
l6=si0;
si0=l4;
l5=si0;
si0=l7;
l1=si0;
si0=l3;
si1=4U;
si0+=si1;
l3=si0;
si1=2988876U;
si0=si0 < si1;
if(si0){
goto L2;
}
}
si0=l3;
si1=l0;
si2=8U;
si1+=si2;
si2=l2;
si3=0U;
si4=l0;
si5=16U;
si4+=si5;
f33(instance,si0,si1,si2,si3,si4);
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+12U);
l4=si0;
si0=l0;
si0=i32_load(i->m0,(WasmMemoryAddress)si0+8U);
l1=si0;
si0=l2;
si1=1U;
si0=si0 != si1;
if(si0){
goto L9;
}
si0=l1;
si1=1U;
si0=si0 != si1;
if(si0){
goto L9;
}
si0=l4;
si0=!(si0);
if(si0){
goto L8;
}
L9:;
si0=l3;
si1=4U;
si0-=si1;
l6=si0;
L10:;
W2C2_LOOP_START
{
si0=l0;
si1=l2;
si2=1U;
si1=(U32)((I32)si1<=(I32)si2);
if(si1){
si1=l1;
si2=1U;
si1-=si2;
si1=I32_CTZ(si1);
l3=si1;
si1=!(si1);
if(si1){
si1=l4;
si1=I32_CTZ(si1);
l3=si1;
if(si1){
si1=l3;
si2=32U;
si1+=si2;
l5=si1;
goto L13;
}
L16:;
si1=0U;
l5=si1;
si1=0U;
l3=si1;
si1=l4;
goto L12;
}
L15:;
si1=32U;
l5=si1;
si1=l3;
si2=32U;
si1=si1 >= si2;
if(si1){
goto L13;
}
si1=l3;
l5=si1;
si1=l4;
goto L12;
}
L14:;
si1=l0;
si2=l1;
si3=30U;
si2>>=(si3&31);
l3=si2;
si3=l4;
si4=2U;
si3<<=(si4&31);
si2|=si3;
si3=1U;
si2>>=(si3&31);
i32_store(i->m0,(WasmMemoryAddress)si1+12U,si2);
si1=l0;
si2=l1;
si3=1U;
si2<<=(si3&31);
si3=2147483646U;
si2&=si3;
si3=l3;
si4=31U;
si3<<=(si4&31);
si2|=si3;
si3=3U;
si2^=si3;
i32_store(i->m0,(WasmMemoryAddress)si1+8U,si2);
si1=l2;
si2=1U;
si1-=si2;
l1=si1;
si1=l6;
si2=l0;
si3=16U;
si2+=si3;
si3=l2;
si4=2U;
si3-=si4;
l2=si3;
si4=2U;
si3<<=(si4&31);
si2+=si3;
si2=i32_load(i->m0,si2);
si1-=si2;
si2=l0;
si3=8U;
si2+=si3;
si3=l1;
si4=1U;
si5=l0;
si6=16U;
si5+=si6;
f33(instance,si1,si2,si3,si4,si5);
si1=l0;
si2=l0;
si2=i32_load(i->m0,(WasmMemoryAddress)si2+12U);
si3=1U;
si2<<=(si3&31);
si3=l0;
si3=i32_load(i->m0,(WasmMemoryAddress)si3+8U);
l1=si3;
si4=31U;
si3>>=(si4&31);
si2|=si3;
i32_store(i->m0,(WasmMemoryAddress)si1+12U,si2);
si1=l0;
si2=l1;
si3=1U;
si2<<=(si3&31);
si3=1U;
si2|=si3;
i32_store(i->m0,(WasmMemoryAddress)si1+8U,si2);
si1=l6;
si2=l0;
si3=8U;
si2+=si3;
si3=l2;
si4=1U;
si5=l0;
si6=16U;
si5+=si6;
f33(instance,si1,si2,si3,si4,si5);
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+12U);
l4=si1;
si1=l0;
si1=i32_load(i->m0,(WasmMemoryAddress)si1+8U);
l1=si1;
goto L11;
L13:;
si1=l5;
si2=32U;
si1-=si2;
l3=si1;
si1=l4;
l1=si1;
si1=0U;
L12:;
l7=si1;
si2=l3;
si1>>=(si2&31);
l4=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+12U,si1);
si0=l0;
si1=l7;
si2=32U;
si3=l3;
si2-=si3;
si1<<=(si2&31);
si2=l1;
si3=l3;
si2>>=(si3&31);
si1|=si2;
l1=si1;
i32_store(i->m0,(WasmMemoryAddress)si0+8U,si1);
si0=l2;
si1=l5;
si0+=si1;
l2=si0;
L11:;
si0=l6;
si1=4U;
si0-=si1;
l6=si0;
si0=l2;
si1=1U;
si0=si0 != si1;
if(si0){
goto L10;
}
si0=l1;
si1=1U;
si0=si0 != si1;
if(si0){
goto L10;
}
si0=l4;
if(si0){
goto L10;
}
}
L8:;
si0=l0;
si1=208U;
si0+=si1;
i->g0=si0;
L0:;
}

U32 f35(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_benchInstance* i = (m5_benchInstance*)instance;
U32 si0,si1,si2;
si0=l0;
si1=l1;
si2=l2;
{
wasmTableEntry entry=wasmTableGet(&i->t0,si2,&m5_benchFuncType1);
si0=((U32 (*)(wasmModuleInstance*,U32,U32))entry.func)(entry.instance,si0,si1);
}
L0:;
return si0;
}

U32 f36(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_benchInstance* i = (m5_benchInstance*)instance;
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

U32 f37(wasmModuleInstance* instance,U32 l0,U32 l1,U32 l2) {
m5_benchInstance* i = (m5_benchInstance*)instance;
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

#if W2C2_RUNTIME_CHECKS
const wasmFuncType m5_benchFuncType0={3,"iii",1,"i"};
const wasmFuncType m5_benchFuncType1={2,"ii",1,"i"};
const wasmFuncType m5_benchFuncType2={3,"iji",1,"j"};
const wasmFuncType m5_benchFuncType3={3,"iii",0,""};
const wasmFuncType m5_benchFuncType4={1,"i",1,"i"};
const wasmFuncType m5_benchFuncType5={0,"",0,""};
const wasmFuncType m5_benchFuncType6={1,"i",0,""};
const wasmFuncType m5_benchFuncType7={2,"di",1,"d"};
const wasmFuncType m5_benchFuncType8={3,"iji",1,"i"};
const wasmFuncType m5_benchFuncType9={4,"ijii",1,"i"};
const wasmFuncType m5_benchFuncType10={4,"iiii",1,"i"};
const wasmFuncType m5_benchFuncType11={3,"ddi",1,"d"};
const wasmFuncType m5_benchFuncType12={2,"dd",1,"d"};
const wasmFuncType m5_benchFuncType13={2,"ii",0,""};
const wasmFuncType m5_benchFuncType14={5,"iiiii",1,"i"};
const wasmFuncType m5_benchFuncType15={5,"iiiii",0,""};
#endif

wasmFuncExport m5_benchFuncExports[2] = {
{(wasmFunc)f5,{"_start",6}},
{NULL,{NULL,0}}
};

static void m5_benchInitMemories(m5_benchInstance* i, m5_benchInstance* parent) {
i->m0 = wasmMemoryAllocate(48, 65536, false);
}

static void m5_benchInitDataSegments(m5_benchInstance* i) {
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[0]=0;
#endif
LOAD_DATA((*i->m0), 1024U, m5_benchData0, 385);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[1]=0;
#endif
LOAD_DATA((*i->m0), 1424U, m5_benchData1, 2775);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[2]=0;
#endif
LOAD_DATA((*i->m0), 4211U, m5_benchData2, 1838);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[3]=0;
#endif
LOAD_DATA((*i->m0), 6065U, m5_benchData3, 33);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[4]=0;
#endif
LOAD_DATA((*i->m0), 6123U, m5_benchData4, 1);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[5]=0;
#endif
LOAD_DATA((*i->m0), 6135U, m5_benchData5, 21);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[6]=0;
#endif
LOAD_DATA((*i->m0), 6181U, m5_benchData6, 1);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[7]=0;
#endif
LOAD_DATA((*i->m0), 6193U, m5_benchData7, 21);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[8]=0;
#endif
LOAD_DATA((*i->m0), 6239U, m5_benchData8, 1);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[9]=0;
#endif
LOAD_DATA((*i->m0), 6251U, m5_benchData9, 30);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[10]=0;
#endif
LOAD_DATA((*i->m0), 6306U, m5_benchData10, 14);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[11]=0;
#endif
LOAD_DATA((*i->m0), 6355U, m5_benchData11, 1);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[12]=0;
#endif
LOAD_DATA((*i->m0), 6367U, m5_benchData12, 21);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[13]=0;
#endif
LOAD_DATA((*i->m0), 6413U, m5_benchData13, 1);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[14]=0;
#endif
LOAD_DATA((*i->m0), 6425U, m5_benchData14, 39);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[15]=0;
#endif
LOAD_DATA((*i->m0), 6464U, m5_benchData15, 9);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[16]=0;
#endif
LOAD_DATA((*i->m0), 6484U, m5_benchData16, 1);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[17]=0;
#endif
LOAD_DATA((*i->m0), 6504U, m5_benchData17, 14);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[18]=0;
#endif
LOAD_DATA((*i->m0), 6528U, m5_benchData18, 9);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[19]=0;
#endif
LOAD_DATA((*i->m0), 6584U, m5_benchData19, 9);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[20]=0;
#endif
LOAD_DATA((*i->m0), 6604U, m5_benchData20, 1);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[21]=0;
#endif
LOAD_DATA((*i->m0), 6624U, m5_benchData21, 11);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[22]=0;
#endif
LOAD_DATA((*i->m0), 6648U, m5_benchData22, 12);
#if W2C2_RUNTIME_CHECKS
i->dataSegmentSizes[23]=0;
#endif
LOAD_DATA((*i->m0), 6704U, m5_benchData23, 2);
}

static void m5_benchInitTables(m5_benchInstance* i) {
U32 offset;
wasmTableAllocate(&i->t0, 12, 12);
offset=1U;
wasmTableCheckRange(&i->t0,offset,11);
wasmTableSetTyped(&i->t0,offset+0,(wasmFunc)&f6,&i->common,&m5_benchFuncType1);
wasmTableSetTyped(&i->t0,offset+1,(wasmFunc)&f7,&i->common,&m5_benchFuncType1);
wasmTableSetTyped(&i->t0,offset+2,(wasmFunc)&f8,&i->common,&m5_benchFuncType1);
wasmTableSetTyped(&i->t0,offset+3,(wasmFunc)&f9,&i->common,&m5_benchFuncType1);
wasmTableSetTyped(&i->t0,offset+4,(wasmFunc)&f10,&i->common,&m5_benchFuncType1);
wasmTableSetTyped(&i->t0,offset+5,(wasmFunc)&f22,&i->common,&m5_benchFuncType0);
wasmTableSetTyped(&i->t0,offset+6,(wasmFunc)&f20,&i->common,&m5_benchFuncType4);
wasmTableSetTyped(&i->t0,offset+7,(wasmFunc)&f23,&i->common,&m5_benchFuncType0);
wasmTableSetTyped(&i->t0,offset+8,(wasmFunc)&f24,&i->common,&m5_benchFuncType2);
wasmTableSetTyped(&i->t0,offset+9,(wasmFunc)&f31,&i->common,&m5_benchFuncType0);
wasmTableSetTyped(&i->t0,offset+10,(wasmFunc)&f35,&i->common,&m5_benchFuncType0);
}

static void m5_benchInitGlobals(m5_benchInstance* i) {
i->g0=3137424U;
}

static void m5_benchInitImports(m5_benchInstance* i, wasmImportResolver resolve) {
if (resolve == NULL) { return; }
}

wasmMemory*m5_benchExport6_memory(m5_benchInstance* i){
return i->m0;
}

void m5_benchExport6_X5Fstart(m5_benchInstance*i){
f5(&i->common);
}

static void m5_benchFreeChild(wasmModuleInstance* child) {
m5_benchInstance* i = (m5_benchInstance*)child;
wasmMemoryFree(i->m0);
i->m0=NULL;
wasmTableFree(&i->t0);
free(i);
}

static wasmModuleInstance* m5_benchNewChild(wasmModuleInstance* instance) {
m5_benchInstance* self = (m5_benchInstance*)instance;
m5_benchInstance* child = (m5_benchInstance*)calloc(1, sizeof(m5_benchInstance));
child->common.funcExports = self->common.funcExports;
child->common.resolveImports = self->common.resolveImports;
child->common.newChild = self->common.newChild;
child->common.freeChild = self->common.freeChild;
m5_benchInitImports(child, self->common.resolveImports);
m5_benchInitMemories(child, self);
m5_benchInitDataSegments(child);
m5_benchInitTables(child);
m5_benchInitGlobals(child);
return &child->common;
}

void m5_benchInstantiate(m5_benchInstance* i, wasmImportResolver resolveImports) {
i->common.funcExports = m5_benchFuncExports;
i->common.resolveImports = resolveImports;
i->common.newChild = m5_benchNewChild;
i->common.freeChild = m5_benchFreeChild;
m5_benchInitImports(i, resolveImports);
m5_benchInitMemories(i, NULL);
m5_benchInitDataSegments(i);
m5_benchInitTables(i);
m5_benchInitGlobals(i);
}

void m5_benchFreeInstance(m5_benchInstance* i) {
wasmMemoryFree(i->m0);
i->m0=NULL;
wasmTableFree(&i->t0);
}

