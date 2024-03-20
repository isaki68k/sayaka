/*
 * Copyright (C) 2021 Tetsuya Isaki
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

#include "ImageLoaderJPEG.h"
#include "PeekableStream.h"
#include "subr.h"
#include <cassert>
#include <cstring>
#include <jpeglib.h>

static void jpeg_output_message(j_common_ptr);
static void jpeg_init_source(j_decompress_ptr);
static boolean jpeg_fill_input_buffer(j_decompress_ptr);
static void jpeg_skip_input_data(j_decompress_ptr, long num_bytes);
static void jpeg_term_source(j_decompress_ptr);

// コンストラクタ
ImageLoaderJPEG::ImageLoaderJPEG(PeekableStream *stream_, const Diag& diag_)
	: inherited(stream_, diag_)
{
}

// デストラクタ
ImageLoaderJPEG::~ImageLoaderJPEG()
{
}

// stream が JPEG なら true を返す。
bool
ImageLoaderJPEG::Check() const
{
	uint8 magic[2];

	auto n = stream->Peek(magic, sizeof(magic));
	if (n < sizeof(magic)) {
		Trace(diag, "%s: Peek() failed: %s", __method__, strerrno());
		return false;
	}
	// マジックを確認
	if (magic[0] != 0xff || magic[1] != 0xd8) {
		Trace(diag, "%s: Bad magic", __method__);
		return false;
	}
	Trace(diag, "%s: OK", __method__);
	return true;
}

// stream から画像をロードする。
bool
ImageLoaderJPEG::Load(Image& img)
{
	struct jpeg_decompress_struct jinfo;
	struct jpeg_source_mgr jsrc;
	struct jpeg_error_mgr jerr;

	memset(&jinfo, 0, sizeof(jinfo));
	memset(&jsrc, 0, sizeof(jsrc));
	memset(&jerr, 0, sizeof(jerr));
	jinfo.client_data = this;
	jinfo.err = jpeg_std_error(&jerr);
	jerr.output_message = jpeg_output_message;

	jpeg_create_decompress(&jinfo);

	jsrc.init_source		= jpeg_init_source;
	jsrc.fill_input_buffer	= jpeg_fill_input_buffer;
	jsrc.skip_input_data	= jpeg_skip_input_data;
	jsrc.resync_to_restart	= jpeg_resync_to_restart;
	jsrc.term_source		= jpeg_term_source;
	jsrc.next_input_byte	= NULL;
	jsrc.bytes_in_buffer	= 0;
	jinfo.src = &jsrc;

	// ヘッダ読み込み
	Trace(diag, "%s read header", __method__);
	jpeg_read_header(&jinfo, (boolean)TRUE);
	Trace(diag, "%s read header done", __method__);

	Size origsize;
	Size reqsize;
	origsize.w = jinfo.image_width;
	origsize.h = jinfo.image_height;

	// スケールの計算
	CalcResize(reqsize, resize_axis, origsize);
	if (reqsize.w < 1) {
		reqsize.w = 1;
	}
	if (reqsize.h < 1) {
		reqsize.h = 1;
	}

	// libjpeg では 1/16 までサポート。
	// 1/1, 1/2, 1/4/, 1/8 しかサポートしないとも書いてある
	int scalew = origsize.w / reqsize.w;
	int scaleh = origsize.h / reqsize.h;
	int scale;
	if (scalew < scaleh) {
		scale = scalew;
	} else {
		scale = scaleh;
	}
	if (scale < 1) {
		scale = 1;
	} else if (scale > 16) {
		scale = 16;
	}

	Debug(diag, "%s size=(%d,%d) scalewh=(%d,%d) scale=%d", __method__,
		origsize.w, origsize.h, scalew, scaleh, scale);

	jinfo.scale_num = 1;
	jinfo.scale_denom = scale;
	jinfo.do_fancy_upsampling = (boolean)FALSE;
	jinfo.do_block_smoothing = (boolean)FALSE;
	jinfo.dct_method = JDCT_FASTEST;
	jinfo.out_color_space = JCS_RGB;
	jinfo.output_components = 3;

	jpeg_calc_output_dimensions(&jinfo);

	int width  = jinfo.output_width;
	int height = jinfo.output_height;
	img.Create(width, height);

	// スキャンラインメモリのポインタ配列
	std::vector<uint8 *> lines(img.GetHeight());
	for (int y = 0, end = lines.size(); y < end; y++) {
		lines[y] = img.buf.data() + (y * img.GetStride());
	}

	Trace(diag, "%s start_decompress", __method__);
	jpeg_start_decompress(&jinfo);
	Trace(diag, "%s start_decompress done", __method__);

	while (jinfo.output_scanline < jinfo.output_height) {
		int prev_scanline = jinfo.output_scanline;

		jpeg_read_scanlines(&jinfo,
			&lines[jinfo.output_scanline],
			jinfo.rec_outbuf_height);

		if (jinfo.output_scanline == prev_scanline) {
			// スキャンラインが進まない
			jpeg_abort_decompress(&jinfo);
			lines.clear();
			return false; //RIC_ABORT_JPEG;
		}
	}

	Trace(diag, "%s finish_decompress", __method__);
	jpeg_finish_decompress(&jinfo);
	Trace(diag, "%s finish_decompress done", __method__);

	jpeg_destroy_decompress(&jinfo);

	return true;
}

// リサイズ計算
void
ImageLoaderJPEG::CalcResize(Size& req, int axis, Size& orig)
{
	int scaledown = 
		(axis == ResizeAxisMode::ScaleDownBoth)
	 || (axis == ResizeAxisMode::ScaleDownWidth)
	 || (axis == ResizeAxisMode::ScaleDownHeight)
	 || (axis == ResizeAxisMode::ScaleDownLong)
	 || (axis == ResizeAxisMode::ScaleDownShort);

	// まず丸めていく
	switch (axis) {
	 case ResizeAxisMode::Both:
	 case ResizeAxisMode::ScaleDownBoth:
		if (req.w <= 0) {
			axis = ResizeAxisMode::Height;
		} else if (req.h <= 0) {
			axis = ResizeAxisMode::Width;
		} else {
			axis = ResizeAxisMode::Both;
		}
		break;
	 case ResizeAxisMode::Long:
	 case ResizeAxisMode::ScaleDownLong:
		if (orig.w >= orig.h) {
			axis = ResizeAxisMode::Width;
		} else {
			axis = ResizeAxisMode::Height;
		}
		break;
	 case ResizeAxisMode::Short:
	 case ResizeAxisMode::ScaleDownShort:
		if (orig.w <= orig.h) {
			axis = ResizeAxisMode::Width;
		} else {
			axis = ResizeAxisMode::Height;
		}
		break;
	 case ResizeAxisMode::ScaleDownWidth:
		axis = ResizeAxisMode::Width;
		break;
	 case ResizeAxisMode::ScaleDownHeight:
		axis = ResizeAxisMode::Height;
		break;
	}

	if (req.w <= 0)
		req.w = orig.w;
	if (req.h <= 0)
		req.h = orig.h;

	// 縮小のみ指示
	if (scaledown) {
		if (orig.w < req.w)
			req.w = orig.w;
		if (orig.h < req.h)
			req.h = orig.h;
	}

	switch (axis) {
	 case ResizeAxisMode::Width:
		req.h = orig.h * req.w / orig.w;
		break;
	 case ResizeAxisMode::Height:
		req.w = orig.w * req.h / orig.h;
		break;
	}
}

int
ImageLoaderJPEG::Read()
{
	return stream->Read(localbuf.data(), localbuf.size());
}


//
// ----- libjpeg から呼ばれるコールバック関数群
//

static const JOCTET faked_eoi[] = {
	0xff, JPEG_EOI
};

// デバッグメッセージを表示
static void
jpeg_output_message(j_common_ptr cinfo)
{
	char msg[JMSG_LENGTH_MAX];

	(*cinfo->err->format_message)(cinfo, msg);

	assert(cinfo->client_data);
	ImageLoaderJPEG *loader = (ImageLoaderJPEG *)cinfo->client_data;
	Debug(loader->GetDiag(), "%s", msg);
}

// 本来ここで src に必要な初期化するのだが、
// うちではクラス内で行っているので不要
static void
jpeg_init_source(j_decompress_ptr cinfo)
{
}

// src にデータを読み込む
static boolean
jpeg_fill_input_buffer(j_decompress_ptr cinfo)
{
	assert(cinfo->client_data);
	assert(cinfo->src);

	ImageLoaderJPEG *loader = (ImageLoaderJPEG *)cinfo->client_data;
	jpeg_source_mgr *src = cinfo->src;

	int n = loader->Read();
	if (n > 0) {
		src->next_input_byte = loader->localbuf.data();
		src->bytes_in_buffer = n;
	} else {
		// 読めなければ fake EOI を返す
		src->next_input_byte = faked_eoi;
		src->bytes_in_buffer = sizeof(faked_eoi);
	}
	return (boolean)TRUE;
}

// src の入力をスキップする
static void
jpeg_skip_input_data(j_decompress_ptr cinfo, long num_bytes)
{
	assert(cinfo->src);
	jpeg_source_mgr *src = cinfo->src;

	while (num_bytes > (long)src->bytes_in_buffer) {
		num_bytes -= src->bytes_in_buffer;
		jpeg_fill_input_buffer(cinfo);
	}
	src->next_input_byte += num_bytes;
	src->bytes_in_buffer -= num_bytes;
}

// 本来ならここで src を解放などするのだが、うちでは不要
static void
jpeg_term_source(j_decompress_ptr cinfo)
{
}
