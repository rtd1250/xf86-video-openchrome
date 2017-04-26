/*
 * Copyright 2004-2005 The Unichrome Project  [unichrome.sf.net]
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _VIA_MODE_H_
#define _VIA_MODE_H_ 1

/*
 * Bandwidth
 *
 */
#define VIA_BW_MIN       74000000 /* > 640x480@60Hz@32bpp */
#define VIA_BW_DDR200   394000000
#define VIA_BW_DDR400   553000000 /* > 1920x1200@60Hz@32bpp */
#define VIA_BW_DDR667   922000000
#define VIA_BW_DDR1066  922000000

union pllparams {
    struct {
        CARD32 dtz : 2;
        CARD32 dr  : 3;
        CARD32 dn  : 7;
        CARD32 dm  :10;
    } params;
    CARD32 packed;
};

/*
 * simple lookup table for dotclocks
 *
 */
static struct ViaDotClock {
    int DotClock;
    CARD16 UniChrome;
    union pllparams UniChromePro;
} ViaDotClocks[] = {
    {  25200, 0x513C, /* 0xa79004 */ { { 1, 4, 6, 169 } } },
    {  25312, 0xC763, /* 0xc49005 */ { { 1, 4, 7, 198 } } },
    {  26591, 0x471A, /* 0xce9005 */ { { 1, 4, 7, 208 } } },
    {  31500, 0xC558, /* 0xae9003 */ { { 1, 4, 5, 176 } } },
    {  31704, 0x471F, /* 0xaf9002 */ { { 1, 4, 4, 177 } } },
    {  32663, 0xC449, /* 0x479000 */ { { 1, 4, 2,  73 } } },
    {  33750, 0x4721, /* 0x959002 */ { { 1, 4, 4, 151 } } },
    {  35500, 0x5877, /* 0x759001 */ { { 1, 4, 3, 119 } } },
    {  36000, 0x5879, /* 0x9f9002 */ { { 1, 4, 4, 161 } } },
    {  39822, 0xC459, /* 0x578c02 */ { { 1, 3, 4,  89 } } },
    {  40000, 0x515F, /* 0x848c04 */ { { 1, 3, 6, 134 } } },
    {  41164, 0x4417, /* 0x2c8c00 */ { { 1, 3, 2,  46 } } },
    {  46981, 0x5069, /* 0x678c02 */ { { 1, 3, 4, 105 } } },
    {  49500, 0xC353, /* 0xa48c04 */ { { 3, 3, 5, 138 } } },
    {  50000, 0xC354, /* 0x368c00 */ { { 1, 3, 2,  56 } } },
    {  56300, 0x4F76, /* 0x3d8c00 */ { { 1, 3, 2,  63 } } },
    {  57275,      0, /* 0x3e8c00 */ { { 1, 3, 5, 157 } } }, /* For XO 1.5 no need for a unichrome clock */
    {  57284, 0x4E70, /* 0x3e8c00 */ { { 1, 3, 2,  64 } } },
    {  64995, 0x0D3B, /* 0x6b8c01 */ { { 1, 3, 3, 109 } } },
    {  65000, 0x0D3B, /* 0x6b8c01 */ { { 1, 3, 3, 109 } } }, /* Slightly unstable on PM800 */
    {  65028, 0x866D, /* 0x6b8c01 */ { { 1, 3, 3, 109 } } },
    {  74480, 0x156E, /* 0x288800 */ { { 1, 2, 2,  42 } } },
    {  75000, 0x156E, /* 0x288800 */ { { 1, 2, 2,  42 } } },
    {  78800, 0x442C, /* 0x2a8800 */ { { 1, 2, 2,  44 } } },
    {  81135, 0x0622, /* 0x428801 */ { { 1, 2, 3,  68 } } },
    {  81613, 0x4539, /* 0x708803 */ { { 1, 2, 5, 114 } } },
    {  94500, 0x4542, /* 0x4d8801 */ { { 1, 2, 3,  79 } } },
    { 108000, 0x0B53, /* 0x778802 */ { { 1, 2, 4, 121 } } },
    { 108280, 0x4879, /* 0x778802 */ { { 1, 2, 4, 121 } } },
    { 122000, 0x0D6F, /* 0x428800 */ { { 1, 2, 2,  68 } } },
    { 122726, 0x073C, /* 0x878802 */ { { 1, 2, 4, 137 } } },
    { 135000, 0x0742, /* 0x6f8801 */ { { 1, 2, 3, 113 } } },
    { 148500, 0x0853, /* 0x518800 */ { { 1, 2, 2,  83 } } },
    { 155800, 0x0857, /* 0x558402 */ { { 1, 1, 4,  87 } } },
    { 157500, 0x422C, /* 0x2a8400 */ { { 1, 1, 2,  44 } } },
    { 161793, 0x4571, /* 0x6f8403 */ { { 1, 1, 5, 113 } } },
    { 162000, 0x0A71, /* 0x6f8403 */ { { 1, 1, 5, 113 } } },
    { 175500, 0x4231, /* 0x2f8400 */ { { 1, 1, 2,  49 } } },
    { 189000, 0x0542, /* 0x4d8401 */ { { 1, 1, 3,  79 } } },
    { 202500, 0x0763, /* 0x6F8402 */ { { 1, 1, 4, 113 } } },
    { 204800, 0x0764, /* 0x548401 */ { { 1, 1, 3,  86 } } },
    { 218300, 0x043D, /* 0x3b8400 */ { { 1, 1, 2,  61 } } },
    { 229500, 0x0660, /* 0x3e8400 */ { { 1, 1, 2,  64 } } }, /* Not tested on Pro } */
    {      0,      0,                { { 0, 0, 0,   0 } } }
};

#endif /* _VIA_MODE_H_ */
