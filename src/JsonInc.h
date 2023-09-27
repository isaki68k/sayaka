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

#pragma once

// https://github.com/nlohmann/json/releases 3.11.2
#include "nlohmann/json.hpp"

using Json = nlohmann::json;

// JsonAsString(j) .. 文字列を取り出しやすくするマクロ。
// j が string なら std::string を返す。
// j がそれ以外なら empty を返す。
//
// json["key"] (Json の [] 演算子) は "key" が存在しない時は null 型
// (Json(nullptr)) を返すので、合わせ技で
// json = { "key":"value", "nullkey":null }
// の場合に
// JsonAsString(json["key"]) => "value"  // (1)
// JsonAsString(json["nullkey"]) => ""   // (2)
// JsonAsString(json["notexist"]) => ""  // (3)
// となる。
// Json::value(name, defval) は (1), (3) は動作するが (2) で例外を出すので
// 使えない。
#define JsonAsString(j) ((j).is_string() ? (j).get<std::string>() : "")
