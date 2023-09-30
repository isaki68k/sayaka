/*
 * Copyright (C) 2023 Tetsuya Isaki
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

#include "ImageLoaderSTB.h"

// stb is too dirty against strict warnings...
#if defined(__clang__)
_Pragma("clang diagnostic push")
_Pragma("clang diagnostic ignored \"-Wcast-qual\"")
_Pragma("clang diagnostic ignored \"-Wdisabled-macro-expansion\"")
#else
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wcast-qual\"")
_Pragma("GCC diagnostic ignored \"-Wunused-but-set-variable\"")
#endif

// sayaka uses the stb_image as public domain.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#include "stb/stb_image.h"

#if defined(__clang__)
_Pragma("clang diagnostic pop")
#else
_Pragma("GCC diagnostic pop")
#endif

static int stb_check_read(void *, char *, int);
static void stb_check_skip(void *, int);
static int stb_check_eof(void *);
static int stb_load_read(void *, char *, int);
static void stb_load_skip(void *, int);
static int stb_load_eof(void *);

// コールバック (ヘッダに出さないため、ここに用意)
static stbi_io_callbacks check_callback = {
	.read = stb_check_read,
	.skip = stb_check_skip,
	.eof  = stb_check_eof,
};
static stbi_io_callbacks load_callback = {
	.read = stb_load_read,
	.skip = stb_load_skip,
	.eof  = stb_load_eof,
};


// コンストラクタ
ImageLoaderSTB::ImageLoaderSTB(InputStream *stream_, const Diag& diag_)
	: inherited(stream_, diag_)
{
}

// デストラクタ
ImageLoaderSTB::~ImageLoaderSTB()
{
}

// stream がサポートしている画像形式なら true を返す。
bool
ImageLoaderSTB::Check() const
{
	int r;
	int x;
	int y;
	int comp;

	r = stbi_info_from_callbacks(&check_callback, stream, &x, &y, &comp);
	return r;
}

// stream から画像をロードする。
bool
ImageLoaderSTB::Load(Image& img)
{
	stbi_uc *data;
	int width;
	int height;
	int nch;

	data = stbi_load_from_callbacks(&load_callback, stream,
		&width, &height, &nch, 3);
	if (data == NULL) {
		return false;
	}

	img.Create(width, height);
	memcpy(&img.buf[0], data, img.buf.size());

	stbi_image_free(data);

	return true;
}

// Check 用 read
int
stb_check_read(void *user, char *data, int size)
{
	InputStream *stream = (InputStream *)user;

	int total = 0;
	while (total < size) {
		auto r = stream->Peek(data + total, size - total);
		if (r <= 0)
			break;
		total += r;
	}
	return total;
}

// Check 用 skip
void
stb_check_skip(void *user, int nbytes)
{
	char buf[4096];

	while (nbytes >= 0) {
		auto n = std::min(nbytes, (int)sizeof(buf));
		auto r = stb_check_read(user, buf, n);
		if (r <= 0)
			break;
		nbytes -= r;
	}
}

// Check 用 EOF
int
stb_check_eof(void *user)
{
	InputStream *stream = (InputStream *)user;

	// Peek で1バイトも取り出せなければ EOF。
	char buf[1];
	auto r = stream->Peek(buf, 1);
	return (r == 0);
}

// Load 用 read
int
stb_load_read(void *user, char *data, int size)
{
	InputStream *stream = (InputStream *)user;

	int total = 0;
	while (total < size) {
		auto r = stream->Read(data + total, size - total);
		if (r <= 0)
			break;
		total += r;
	}
	return total;
}

// Load 用 skip
void
stb_load_skip(void *user, int nbytes)
{
	char buf[4096];

	while (nbytes >= 0) {
		auto n = std::min(nbytes, (int)sizeof(buf));
		auto r = stb_load_read(user, buf, n);
		if (r <= 0)
			break;
		nbytes -= r;
	}
}

// Load 用 EOF
int
stb_load_eof(void *user)
{
	// 今の所判定できないけど、呼ばれてなさげ?
	return 0;
}
