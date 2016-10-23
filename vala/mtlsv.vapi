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

[CCode(cheader_filename="mtls.h")]
namespace Native.mTLS
{
	[CCode(cname="struct mtlsctx")]
	[Compact]
	public struct mtlsctx
	{
		// internal
	}

	[CCode(cname="mtls_set_debuglevel")]
	public static void set_debuglevel(int level);

	[CCode(cname="mtls_alloc")]
	public static mtlsctx* alloc();
	[CCode(cname="mtls_free")]
	public static void free(mtlsctx* ctx);
	[CCode(cname="mtls_init")]
	public static int init(mtlsctx* ctx);
	[CCode(cname="mtls_close")]
	public static int close(mtlsctx* ctx);
	[CCode(cname="mtls_setssl")]
	public static void setssl(mtlsctx* ctx, bool value);
	[CCode(cname="mtls_usersa")]
	public static void usersa(mtlsctx* ctx);
	[CCode(cname="mtls_connect")]
	public static int connect(mtlsctx* ctx, char* hostname, char* servname);

	[CCode(cname="mtls_read")]
	public static int read(mtlsctx* ctx, uint8* buf, int len);
	[CCode(cname="mtls_write")]
	public static int write(mtlsctx* ctx, uint8* buf, int len);
}

