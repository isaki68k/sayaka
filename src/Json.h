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
