/* vi:set ts=4: */
/*
 * Copyright (C) 2025 Tetsuya Isaki
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

//
// JXL 読み込み
//

#include "common.h"
#include "image_priv.h"
#include <err.h>
#include <string.h>
#include <jxl/decode.h>
#include <jxl/version.h>

#if defined(SLOW_ARCH)
#define BUFSIZE	(4096)
#else
#define BUFSIZE	(65536)
#endif

static const char *status2str(JxlDecoderStatus);

bool
image_jxl_match(FILE *fp, const struct diag *diag)
{
	uint8 buf[12];

	size_t n = fread(buf, sizeof(buf), 1, fp);
	if (n == 0) {
		Debug(diag, "%s: fread failed: %s", __func__, strerrno());
		return false;
	}

	JxlSignature sig = JxlSignatureCheck(buf, sizeof(buf));
	if (sig != JXL_SIG_CODESTREAM && sig != JXL_SIG_CONTAINER) {
		return false;
	}

	return true;
}

struct image *
image_jxl_read(FILE *fp, const image_read_hint *hint, const struct diag *diag)
{
	struct image *img = NULL;
	uint8 *buf;
	JxlBasicInfo info;
	bool success = false;
	size_t readbytes = 0;
	bool is_progressive = hint->progressive;

	buf = malloc(BUFSIZE);
	if (buf == NULL) {
		warnx("%s: malloc(%u) failed", __func__, BUFSIZE);
		return NULL;
	}

	JxlDecoder *dec = JxlDecoderCreate(NULL);
	JxlDecoderSubscribeEvents(dec,
		JXL_DEC_BASIC_INFO |
		JXL_DEC_FRAME_PROGRESSION |
		JXL_DEC_FULL_IMAGE);

	JxlDecoderSetProgressiveDetail(dec,
		(JxlProgressiveDetail)kPasses);

	memset(&info, 0, sizeof(info));
	for (;;) {
		// 処理を進める。
		JxlDecoderStatus status = JxlDecoderProcessInput(dec);
		if (__predict_false(status == JXL_DEC_ERROR)) {
			warnx("%s: JxlDecoderProcessInput failed", __func__);
			break;
		}

		if (status == JXL_DEC_NEED_MORE_INPUT) {
			JxlDecoderReleaseInput(dec);
			Trace(diag, "%s: %s", __func__, status2str(status));

			// 読み込み。
			size_t n = fread(buf, 1, BUFSIZE, fp);
			if (ferror(fp)) {
				warn("%s: fread failed", __func__);
				break;
			}
			if (n == 0) {
				break;
			}
			readbytes += n;

			// 入力バッファをセット。
			status = JxlDecoderSetInput(dec, buf, n);
			if (status == JXL_DEC_ERROR) {
				warnx("%s: JxlDecoderSetInput failed", __func__);
				break;
			}
			continue;
		}

		if (status == JXL_DEC_BASIC_INFO) {
			Trace(diag, "%s: %s", __func__, status2str(status));
			JxlDecoderGetBasicInfo(dec, &info);
			Debug(diag, "%s: ImageSize=(%u, %u) Color=%s%s", __func__,
				info.xsize,
				info.ysize,
				(info.num_color_channels == 1 ? "Grayscale" : "RGB"),
				(info.alpha_bits == 0 ? "" : "+Alpha"));
			Debug(diag, "%s: have_preview=%u have_animation=%u", __func__,
				info.have_preview,
				info.have_animation);
			continue;
		}

		if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
			Trace(diag, "%s: %s", __func__, status2str(status));
			// imgfmt は image_create() に指定するフォーマット形式。
			// jxlfmt は JXL デコーダに指示する出力形式。
			uint imgfmt;
			JxlPixelFormat jxlfmt;
			if (info.alpha_bits) {
				imgfmt = IMAGE_FMT_ARGB32;
				jxlfmt.num_channels = 4;
			} else {
				imgfmt = IMAGE_FMT_RGB24;
				jxlfmt.num_channels = 3;
			}
			jxlfmt.data_type = JXL_TYPE_UINT8;
			jxlfmt.endianness = JXL_NATIVE_ENDIAN;
			jxlfmt.align = 0;

			uint xsize = info.xsize;
			uint ysize = info.ysize;
			img = image_create(xsize, ysize, imgfmt);
			if (img == NULL) {
				break;
			}

			JxlDecoderSetImageOutBuffer(dec, &jxlfmt, img->buf,
				xsize * ysize * jxlfmt.num_channels);

			if (is_progressive) {
				uint hint_w = hint->width <= 0 ? xsize : hint->width;
				uint hint_h = hint->height <= 0 ? ysize : hint->height;
				uint k = xsize / hint_w * ysize / hint_h;
				Debug(diag, "%s: k=%u", __func__, k);
				if (k < 7) {
					is_progressive = false;
				}
			}

			continue;
		}

		if (status == JXL_DEC_FULL_IMAGE) {
			Trace(diag, "%s: %s", __func__, status2str(status));
			success = true;
			break;
		}

		if (status == JXL_DEC_FRAME_PROGRESSION) {
			Trace(diag, "%s: %s %zu bytes read", __func__,
				status2str(status),
				readbytes);

			if (is_progressive) {
				Debug(diag, "%s: use progressive", __func__);
				JxlDecoderFlushImage(dec);
				success = true;
				break;
			}
			continue;
		}

		/*else*/
		{
			Trace(diag, "%s: %s", __func__, status2str(status));
			if (status == JXL_DEC_SUCCESS) {
				success = true;
				break;
			}
		}
	}

	// どちらにしてもリソースを解放。
	JxlDecoderDestroy(dec);
	free(buf);

	if (!success) {
		image_free(img);
		img = NULL;
	}
	return img;
}

static const char *
status2str(JxlDecoderStatus status)
{
	switch (status) {
	 case JXL_DEC_SUCCESS:				return "JXL_DEC_SUCCESS";
	 case JXL_DEC_ERROR:				return "JXL_DEC_ERROR";
	 case JXL_DEC_NEED_MORE_INPUT:		return "JXL_DEC_NEED_MORE_INPUT";
	 case JXL_DEC_NEED_PREVIEW_OUT_BUFFER:
		return "JXL_DEC_NEED_PREVIEW_OUT_BUFFER";
	 case JXL_DEC_NEED_IMAGE_OUT_BUFFER:
		return "JXL_DEC_NEED_IMAGE_OUT_BUFFER";
	 case JXL_DEC_JPEG_NEED_MORE_OUTPUT:
		return "JXL_DEC_JPEG_NEED_MORE_OUTPUT";
	 case JXL_DEC_BOX_NEED_MORE_OUTPUT:
		return "JXL_DEC_BOX_NEED_MORE_OUTPUT";
	 case JXL_DEC_BASIC_INFO:			return "JXL_DEC_BASIC_INFO";
	 case JXL_DEC_COLOR_ENCODING:		return "JXL_DEC_COLOR_ENCODING";
	 case JXL_DEC_PREVIEW_IMAGE:		return "JXL_DEC_PREVIEW_IMAGE";
	 case JXL_DEC_FRAME:				return "JXL_DEC_FRAME";
	 case JXL_DEC_FULL_IMAGE:			return "JXL_DEC_FULL_IMAGE";
	 case JXL_DEC_JPEG_RECONSTRUCTION:	return "JXL_DEC_JPEG_RECONSTRUCTION";
	 case JXL_DEC_BOX:					return "JXL_DEC_BOX";
	 case JXL_DEC_FRAME_PROGRESSION:	return "JXL_DEC_FRAME_PROGRESSION";
#if JPEGXL_NUMERIC_VERSION >= JPEGXL_COMPUTE_NUMERIC_VERSION(0, 8, 0)
	 case JXL_DEC_BOX_COMPLETE:			return "JXL_DEC_BOX_COMPLETE";
#endif
	 default:
		break;
	}
	return "UnknownStatus";
}
