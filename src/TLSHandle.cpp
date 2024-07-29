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

#include "header.h"
#include "TLSHandle.h"
#include <sys/socket.h>
#include <sys/time.h>

// コンストラクタ
TLSHandleBase::TLSHandleBase()
{
	family = AF_UNSPEC;
	timeout = -1;
}

// デストラクタ
TLSHandleBase::~TLSHandleBase()
{
}

// 初期化。
bool
TLSHandleBase::Init()
{
	return true;
}

// HTTPS を使うかどうかを設定する。
void
TLSHandleBase::UseSSL(bool value)
{
	usessl = value;
}

// タイムアウトを設定する。
void
TLSHandleBase::SetTimeout(int timeout_)
{
	timeout = timeout_;
}

/*static*/ void
TLSHandleBase::PrintTime(const struct timeval *tvp)
{
	struct timeval tv;

	if (tvp == NULL) {
		gettimeofday(&tv, NULL);
		tvp = &tv;
	}
	fprintf(stderr, "[%02d:%02d.%06d] ",
		(int)((tvp->tv_sec / 60) % 60),
		(int)((tvp->tv_sec     ) % 60),
		(int)(tvp->tv_usec));
}

/*static*/ void
TLSHandleBase::SetLevel(int val)
{
	diag.SetLevel(val);
}

/*static*/ Diag TLSHandleBase::diag("TLSHandle");
