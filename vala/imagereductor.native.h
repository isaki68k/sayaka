/*
 * Copyright (C) 2016 Y.Sugahara (moveccr)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

//-------- VALA から参照される定義宣言

// カラーモード
typedef enum
{
	RCM_Mono,
	RCM_Gray,
	RCM_GrayMean,
	RCM_Fixed8,
	RCM_FixedX68k,
	RCM_CustomX68k,
	RCM_FixedANSI16,
	RCM_Fixed256,
	RCM_Fixed256RGBI,
	RCM_Custom,
} ReductorColorMode;

// ファインダーモード
typedef enum
{
	RFM_Default,
	RFM_HSV,
} ReductorFinderMode;

// リターンコード
typedef enum
{
	RIC_OK = 0,
	RIC_ARG_NULL = 1,
	RIC_ABORT_JPEG = 2,
} ReductorImageCode;

// 誤差拡散アルゴリズム
typedef enum
{
	RDM_FS,			// Floyd Steinberg
	RDM_ATKINSON,	// Atkinson
	RDM_JAJUNI,		// Jarvis, Judice, Ninke
	RDM_STUCKI,		// Stucki
	RDM_BURKES,		// Burkes
	RDM_2,			// (x+1,y), (x,y+1)
	RDM_3,			// (x+1,y), (x,y+1), (x+1,y+1)
	RDM_RGB,		// RGB color sepalated
} ReductorDiffuseMethod;

//-------- 色の型

typedef struct ColorRGBint_t
{
	int r;
	int g;
	int b;
} ColorRGBint;

typedef struct ColorRGBuint8_t
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
} ColorRGBuint8;

typedef struct ColorRGBint8_t
{
	int8_t r;
	int8_t g;
	int8_t b;
} ColorRGBint8;

typedef struct ColorRGBint16_t
{
	int16_t r;
	int16_t g;
	int16_t b;
} ColorRGBint16;

typedef struct ColorHSVuint8_t
{
	uint8_t h;	// 0..239, 255=gray
	uint8_t s;	// 0..255
	uint8_t v;	// 0..255
} ColorHSVuint8;

//-------- グローバル変数

extern int ImageReductor_Debug;
extern const ColorRGBuint8 *Palette;
extern int PaletteCount;
extern ColorRGBuint8 Palette_Custom[256];

