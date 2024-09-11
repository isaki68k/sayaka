/* vi:set ts=4: */
/*
 * Copyright (C) 2022-2024 Tetsuya Isaki
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
// Mathematical Alphanumeric Symbols を全角英数字にマップする。
//

#include "sayaka.h"

// そのままマップするのは無駄が多いので一旦 1バイトにマッピングして、
// 実行時に Unicode コードポイントに変換する。
// 英数字は ASCII コードに、ギリシア文字は 0x91〜0xc9 にマッピングする
// (ギリシア文字のコードポイントが U+391〜U+3c9 なので)。
static const uint8 mathalpha_table[1024] = {
 'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',	// U+1d400
 'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',	// U+1d410
 'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',	// U+1d420
 'w','x','y','z','A','B','C','D','E','F','G','H','I','J','K','L',	// U+1d430
 'M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','a','b',	// U+1d440
 'c','d','e','f','g',' ','i','j','k','l','m','n','o','p','q','r',	// U+1d450
 's','t','u','v','w','x','y','z','A','B','C','D','E','F','G','H',	// U+1d460
 'I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X',	// U+1d470
 'Y','Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n',	// U+1d480
 'o','p','q','r','s','t','u','v','w','x','y','z','A',' ','C','D',	// U+1d490
 ' ',' ','G',' ',' ','J','K',' ',' ','N','O','P','Q',' ','S','T',	// U+1d4a0
 'U','V','W','X','Y','Z','a','b','c','d',' ','f',' ','h','i','j',	// U+1d4b0
 'k','l','m','n',' ','p','q','r','s','t','u','v','w','x','y','z',	// U+1d4c0
 'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',	// U+1d4d0
 'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',	// U+1d4e0
 'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',	// U+1d4f0
 'w','x','y','z','A','B',' ','D','E','F','G',' ',' ','J','K','L',	// U+1d500
 'M','N','O','P','Q',' ','S','T','U','V','W','X','Y',' ','a','b',	// U+1d510
 'c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r',	// U+1d520
 's','t','u','v','w','x','y','z','A','B',' ','D','E','F','G',' ',	// U+1d530
 'I','J','K','L','M',' ','O',' ',' ',' ','S','T','U','V','W','X',	// U+1d540
 'Y',' ','a','b','c','d','e','f','g','h','i','j','k','l','m','n',	// U+1d550
 'o','p','q','r','s','t','u','v','w','x','y','z','A','B','C','D',	// U+1d560
 'E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T',	// U+1d570
 'U','V','W','X','Y','Z','a','b','c','d','e','f','g','h','i','j',	// U+1d580
 'k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',	// U+1d590
 'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',	// U+1d5a0
 'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',	// U+1d5b0
 'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',	// U+1d5c0
 'w','x','y','z','A','B','C','D','E','F','G','H','I','J','K','L',	// U+1d5d0
 'M','N','O','P','Q','R','S','T','U','V','W','X','Y','Z','a','b',	// U+1d5e0
 'c','d','e','f','g','h','i','j','k','l','m','n','o','p','q','r',	// U+1d5f0
 's','t','u','v','w','x','y','z','A','B','C','D','E','F','G','H',	// U+1d600
 'I','J','K','L','M','N','O','P','Q','R','S','T','U','V','W','X',	// U+1d610
 'Y','Z','a','b','c','d','e','f','g','h','i','j','k','l','m','n',	// U+1d620
 'o','p','q','r','s','t','u','v','w','x','y','z','A','B','C','D',	// U+1d630
 'E','F','G','H','I','J','K','L','M','N','O','P','Q','R','S','T',	// U+1d640
 'U','V','W','X','Y','Z','a','b','c','d','e','f','g','h','i','j',	// U+1d650
 'k','l','m','n','o','p','q','r','s','t','u','v','w','x','y','z',	// U+1d660
 'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',	// U+1d670
 'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',	// U+1d680
 'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',	// U+1d690
 'w','x','y','z',' ',' ',' ',' ',									// U+1d6a0
 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,					// U+1d6a8
 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0,					// U+1d6b0
 0xa1, ' ',  0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8,
 0xa9, ' ',  0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,					// U+1d6c0
 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe,
 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6,					// U+1d6d0
 0xc7, 0xc8, 0xc9, ' ',  ' ',  ' ',  ' ',  ' ',
 ' ',  ' ',  0x91, 0x92, 0x93, 0x94, 0x95, 0x96,					// U+1d6e0
 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e,
 0x9f, 0xa0, 0xa1, ' ',  0xa3, 0xa4, 0xa5, 0xa6,					// U+1d6f0
 0xa7, 0xa8, 0xa9, ' ',  0xb1, 0xb2, 0xb3, 0xb4,
 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc,					// U+1d700
 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4,
 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, ' ',  ' ',  ' ',						// U+1d710
 ' ',  ' ',  ' ',  ' ',  0x91, 0x92, 0x93, 0x94,
 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c,					// U+1d720
 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, ' ',  0xa3, 0xa4,
 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, ' ',  0xb1, 0xb2,					// U+1d730
 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba,
 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc2,					// U+1d740
 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, ' ',
 ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  0x91, 0x92,					// U+1d750
 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a,
 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, ' ',						// U+1d760
 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9, ' ',
 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8,					// U+1d770
 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf, 0xc0,
 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8,					// U+1d780
 0xc9, ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  ' ',
 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,					// U+1d790
 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0,
 0xa1, ' ',  0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8,					// U+1d7a0
 0xa9, ' ',  0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe,					// U+1d7b0
 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6,
 0xc7, 0xc8, 0xc9, ' ',  ' ',  ' ',  ' ',  ' ',						// U+1d7c0
 ' ',  ' ',  ' ',  ' ',  ' ',  ' ',  '0',  '1',
 '2','3','4','5','6','7','8','9','0','1','2','3','4','5','6','7', // U+1d7d0
 '8','9','0','1','2','3','4','5','6','7','8','9','0','1','2','3', // U+1d7e0
 '4','5','6','7','8','9','0','1','2','3','4','5','6','7','8','9', // U+1d7f0
};

// src が英数字(と一部のギリシア文字)の飾り文字類なら対応する文字の
// 全角文字コードを返す。src が飾り文字類でなければ 0 を返す。
unichar
conv_mathalpha(unichar src)
{
	if (__predict_false(0x1d400 <= src && src <= 0x1d7ff)) {
		src -= 0x1d400;
		unichar dst = mathalpha_table[src];
		if (__predict_false(dst == ' ')) {
			// 変換先がない。
			return 0;
		}
		if (__predict_true(dst < 0x80)) {
			// 全角英数字は ASCII と並び順が同じ。
			return 0xff00 + (dst - ' ');
		} else {
			// ギリシア文字は対応が簡単なやつだけ。
			return 0x300 + dst;
		}
	}

	return 0;
}
