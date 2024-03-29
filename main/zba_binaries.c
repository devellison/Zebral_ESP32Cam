#include "zba_binaries.h"

// Favicon .png file (zebral zebra logo)
const uint8_t favicon_16x16[801] = {
    0x1f, 0x8b, 0x8,  0x8,  0xd0, 0x55, 0xe5, 0x62, 0x0,  0xb,  0x66, 0x61, 0x76, 0x69, 0x63, 0x6f,
    0x6e, 0x2e, 0x70, 0x6e, 0x67, 0x0,  0xeb, 0xc,  0xf0, 0x73, 0xe7, 0xe5, 0x92, 0xe2, 0x62, 0x60,
    0x60, 0xe0, 0xf5, 0xf4, 0x70, 0x9,  0x2,  0xd2, 0x2,  0x20, 0xcc, 0xc1, 0xc,  0x24, 0x35, 0x74,
    0xf9, 0x83, 0x81, 0x94, 0x42, 0xb2, 0x47, 0x90, 0x2f, 0x3,  0x43, 0x95, 0x1a, 0x3,  0x43, 0x43,
    0xb,  0x3,  0xc3, 0x2f, 0xa0, 0x50, 0xc3, 0xb,  0x6,  0x86, 0x52, 0x3,  0x6,  0x86, 0x57, 0x9,
    0xc,  0xc,  0x56, 0x33, 0x18, 0x18, 0xc4, 0xb,  0xe6, 0xec, 0xa,  0xb4, 0x61, 0x60, 0x60, 0x74,
    0x9,  0xf0, 0x9,  0x71, 0x5,  0x2a, 0xf8, 0xff, 0x9f, 0x81, 0xf5, 0x3f, 0x90, 0x62, 0xf8, 0x3f,
    0x59, 0xee, 0x3f, 0x88, 0x2f, 0xa,  0x12, 0x2,  0xf2, 0x19, 0xff, 0xbf, 0x12, 0x65, 0x98, 0xad,
    0xd9, 0x39, 0x53, 0xb3, 0xb,  0x28, 0x80, 0xb,  0x3d, 0x86, 0xeb, 0x9d, 0x20, 0xff, 0x7f, 0x99,
    0xe4, 0xff, 0x3f, 0x8c, 0xfb, 0xc0, 0x22, 0x20, 0x41, 0x20, 0xc1, 0xc9, 0xf4, 0xbf, 0x4b, 0x6,
    0x24, 0x3b, 0x55, 0xf6, 0xff, 0x6b, 0xb6, 0x3d, 0x50, 0x29, 0x46, 0x86, 0xff, 0x46, 0x30, 0x55,
    0x60, 0x64, 0xc9, 0xf3, 0x7f, 0x92, 0xdc, 0xff, 0x49, 0xf2, 0xff, 0xf,  0xa,  0x2,  0xcd, 0x84,
    0x29, 0x53, 0x66, 0xf8, 0xff, 0xa,  0x68, 0x7,  0x8a, 0x4a, 0x17, 0x3e, 0x90, 0x69, 0xf3, 0xa4,
    0xff, 0xff, 0x64, 0x82, 0x29, 0xab, 0x5,  0xb9, 0xe3, 0x3e, 0xaa, 0x32, 0xa0, 0xd,  0xfe, 0x2,
    0x20, 0x33, 0x1f, 0x71, 0xec, 0x0,  0xbb, 0x83, 0xe1, 0xff, 0xd,  0x86, 0x7,  0xff, 0x19, 0xd4,
    0xff, 0x33, 0x30, 0x20, 0xa9, 0x64, 0x65, 0xfc, 0x9f, 0x29, 0xf6, 0xff, 0x1,  0x4f, 0x17, 0x88,
    0xe3, 0xce, 0xf0, 0xfc, 0x37, 0x83, 0xc1, 0x7f, 0x66, 0x86, 0xbf, 0x1a, 0xdc, 0xf,  0x96, 0xe8,
    0x34, 0x81, 0x43, 0xe3, 0x1,  0x43, 0xfc, 0x7f, 0x79, 0x8e, 0xe7, 0x85, 0x72, 0x2b, 0x27, 0xaa,
    0x4d, 0x9c, 0xa2, 0xde, 0x37, 0x57, 0xab, 0x7d, 0xba, 0x46, 0xcf, 0x64, 0xf5, 0xfe, 0xff, 0x10,
    0xf,  0xfe, 0xff, 0x7f, 0x71, 0x6a, 0x2e, 0x30, 0x3c, 0x19, 0x32, 0x4b, 0x82, 0xfc, 0x40, 0x71,
    0x80, 0x0,  0x5c, 0xac, 0xc6, 0x8d, 0x5a, 0x8c, 0x2c, 0xb3, 0x1e, 0xa5, 0xf0, 0x33, 0xca, 0xba,
    0xef, 0x3a, 0xba, 0x4a, 0x93, 0x59, 0xa1, 0xe0, 0xe6, 0xc3, 0xd3, 0xc7, 0x3f, 0x6e, 0xe6, 0x63,
    0x54, 0xeb, 0xba, 0x7d, 0xe0, 0xe5, 0xef, 0x7d, 0xdf, 0x4e, 0x7c, 0xb8, 0x98, 0xc9, 0xca, 0x33,
    0xed, 0xe7, 0x87, 0xdd, 0xef, 0x2e, 0xdf, 0x39, 0xf9, 0x62, 0xff, 0xf3, 0x50, 0xa1, 0x98, 0x83,
    0xbf, 0xde, 0xee, 0xbf, 0xbc, 0xf2, 0xd0, 0x85, 0xb3, 0x97, 0xf6, 0xbb, 0xa,  0x46, 0x1f, 0xd9,
    0x61, 0x2c, 0xea, 0xdc, 0xba, 0xf3, 0xd4, 0x8b, 0x6e, 0x2e, 0x39, 0x76, 0x83, 0xe9, 0xa7, 0xb6,
    0x89, 0xc9, 0xe4, 0xd8, 0x4b, 0x66, 0xb0, 0x94, 0x0,  0xad, 0x60, 0x4c, 0xf2, 0x76, 0x77, 0xc9,
    0xe,  0x4a, 0x5d, 0xa,  0x8c, 0x59, 0x6,  0xf6, 0x12, 0x4f, 0x5f, 0x57, 0xf6, 0x67, 0xec, 0xd2,
    0x1c, 0xea, 0xa2, 0xd,  0xbf, 0x22, 0xdb, 0x81, 0x42, 0xfe, 0x55, 0x21, 0x11, 0x25, 0x41, 0x89,
    0xe5, 0xa,  0x5,  0x45, 0xf9, 0x69, 0x99, 0x39, 0xa9, 0xa,  0x25, 0x95, 0x5,  0xa9, 0xa,  0x99,
    0x5,  0x25, 0xc9, 0xc,  0xc,  0x15, 0x73, 0x1e, 0x9f, 0xd2, 0xd1, 0xf0, 0x7c, 0x16, 0xc4, 0xc0,
    0xac, 0xcc, 0xad, 0x97, 0xcc, 0x2d, 0xac, 0x2c, 0xec, 0x3d, 0x59, 0x84, 0x59, 0x58, 0xc1, 0xa5,
    0xc1, 0xe4, 0x70, 0xa,  0xb3, 0xf2, 0xe6, 0x10, 0x85, 0xd3, 0x37, 0xae, 0x9c, 0x38, 0x72, 0xe6,
    0x88, 0xcc, 0x11, 0xf6, 0xd3, 0xd,  0x1e, 0xb,  0xbc, 0xf4, 0x18, 0x34, 0xa6, 0xf2, 0x3d, 0xd6,
    0x52, 0xca, 0x95, 0x7,  0x1a, 0x3d, 0xd3, 0xd3, 0xc5, 0x31, 0x44, 0xe2, 0x72, 0x72, 0x82, 0x2,
    0x9f, 0xc1, 0xa7, 0x4f, 0x86, 0x8b, 0xb8, 0x3e, 0x70, 0x73, 0x70, 0x29, 0x72, 0x1b, 0x72, 0x75,
    0x77, 0x74, 0x75, 0x1a, 0x1b, 0x3a, 0xef, 0xd4, 0x11, 0x9a, 0x14, 0x74, 0x69, 0xa6, 0xd1, 0xa6,
    0xa2, 0x33, 0x9,  0x82, 0x61, 0x95, 0xac, 0xae, 0xaa, 0xa9, 0x53, 0x7d, 0x42, 0x4d, 0x4b, 0x79,
    0xcd, 0x14, 0xe2, 0x83, 0xbd, 0xa3, 0xdd, 0xc3, 0xfd, 0xef, 0x1f, 0xb8, 0x70, 0xe2, 0xc6, 0x91,
    0x2b, 0x3d, 0x73, 0x1b, 0xf,  0xde, 0x39, 0x64, 0x74, 0xfa, 0xd4, 0xad, 0x63, 0xd7, 0xce, 0xdd,
    0x3b, 0x78, 0xf1, 0xe4, 0xcc, 0xe6, 0xc9, 0x8d, 0x57, 0x8f, 0x5e, 0x3d, 0x7b, 0xf7, 0xf0, 0xe5,
    0xd3, 0xb7, 0x8f, 0x5f, 0x3f, 0x6f, 0xaf, 0x30, 0x81, 0x39, 0x83, 0x2d, 0x3b, 0xa0, 0xc0, 0x23,
    0x23, 0xa2, 0xc2, 0x25, 0x25, 0xa4, 0xc4, 0x27, 0x47, 0xe6, 0x46, 0x44, 0xd8, 0xf4, 0x8a, 0x30,
    0xbd, 0x6f, 0x2d, 0x47, 0x96, 0x3c, 0x99, 0x12, 0xa2, 0x72, 0xbc, 0xa0, 0x60, 0xf9, 0xd2, 0xf4,
    0xf,  0x2,  0x2f, 0xcd, 0x53, 0x18, 0x18, 0xaa, 0x95, 0xc5, 0x45, 0xbe, 0x85, 0x94, 0x9c, 0x7,
    0x3a, 0x52, 0xb5, 0xc4, 0x35, 0xa2, 0x24, 0x25, 0xb1, 0x24, 0xd5, 0x2a, 0xb9, 0x28, 0x15, 0x48,
    0x31, 0x18, 0x19, 0x18, 0x19, 0xe9, 0x1a, 0x98, 0xeb, 0x1a, 0x99, 0x87, 0x18, 0x58, 0x58, 0x19,
    0x5b, 0x5a, 0x19, 0x19, 0x6a, 0x1b, 0x18, 0x58, 0x19, 0x18, 0xcc, 0x9a, 0x78, 0xef, 0x9,  0x8a,
    0x86, 0xdc, 0xfc, 0x94, 0xcc, 0xb4, 0x4a, 0xdc, 0x1a, 0x5e, 0x9f, 0x49, 0x8b, 0x0,  0xc5, 0xb2,
    0xa7, 0xab, 0x9f, 0xcb, 0x3a, 0xa7, 0x84, 0x26, 0x0,  0x62, 0x79, 0x8b, 0x16, 0xa0, 0x3,  0x0,
    0x0};
