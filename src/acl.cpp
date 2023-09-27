/*
 * Copyright (C) 2020-2021 Tetsuya Isaki
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

#include "sayaka.h"
#include "acl.h"
#include "Diag.h"
#include "Dictionary.h"
#include "JsonInc.h"
#include "StringUtil.h"

// ここの diagShow は Debug, Trace, Verbose という分類というより
// 単に表示の詳しさを 1, 2, 3 としているだけなので、別名にする。
#define DiagPrint(lv, fmt...)	do {	\
	if (diagShow >= (lv))	\
		diagShow.Print(fmt);	\
} while (0)

static bool acl_me(const std::string& user_id, const std::string& user_name,
	const StringDictionary& replies);
#if 0
static bool acl_home(const Json& status, const std::string& user_id,
	const std::string& user_name);
#endif
static StringDictionary GetReplies(const Json& status,
	const std::string& user_id, const std::string& user_name);

// 表示判定のおおまかなルール
//
// ブロック氏: false
// 俺氏      : true
// * to 俺氏 : true
// ミュート氏: false
// * rt 俺氏: true
// * rt (ブロック to 俺氏): false
// * rt (* to 俺氏): true
//
// if (ホームTL) {
//   RT非表示氏 rt *: false
//   他人氏: false
//   ; これ以降残ってるのはフォロー氏のみ
//   フォロー to 他人: false
// }
//
// * to ブロック: false
// * to ミュート: false
// * rt ブロック: false
// * rt ミュート: false
// * rt (* to ブロック): false
// * rt (* to ミュート): false
// * rt *: true
// *: true

// このツイートを表示するか判定する。表示するなら true。
// NG ワード判定はここではない。
bool
acl(const Json& status, bool is_quoted)
{
	if (__predict_false(status.contains("user") == false)) {
		return false;
	}
	const Json& user = status["user"];
	// このツイートの発言者
	const auto& user_id = user.value("id_str", "");
	std::string user_name;
	if (diagShow >= 1) {
		user_name = user.value("screen_name", "");
	}

	// ブロック氏の発言はすべて非表示
	if (blocklist.ContainsKey(user_id)) {
		DiagPrint(3, "acl: block(@%s) -> false", user_name.c_str());
		return false;
	}

	// このツイートの返信周りを先に洗い出す。
	// (俺氏宛てのために先にここで使うけど、
	// 後からもフォロー同士の関係性を調べるためにまた使う)
	auto replies = GetReplies(status, user_id, user_name);

	// 俺氏発と俺氏宛てはすべて表示
	if (acl_me(user_id, user_name, replies)) {
		return true;
	}

	// 俺氏宛てを表示した後でミュート氏の発言はすべて非表示
	if (mutelist.ContainsKey(user_id)) {
		// フォローしていれば Lv1 で表示する
		// フォローしてなければ Lv3 のみで表示する
		if (diagShow >= 1) {
			int lv = followlist.ContainsKey(user_id) ? 1 : 3;
			DiagPrint(lv, "acl: mute(@%s) -> false", user_name.c_str());
		}
		return false;
	}

	// リツイートを持っていればその中の俺氏関係分だけ表示
	// 俺氏関係分なのでRT非表示氏や他人氏でも可。
	if (status.contains("retweeted_status")) {
		const Json& rt_status = status["retweeted_status"];
		if (__predict_false(rt_status.contains("user") == false)) {
			return false;
		}
		const Json& rt_user = rt_status["user"];
		const auto& rt_user_id = rt_user.value("id_str", "");
		std::string rt_user_name;
		if (diagShow >= 1) {
			rt_user_name = rt_user.value("screen_name", "");
		}
		const auto rt_replies = GetReplies(rt_status, rt_user_id, rt_user_name);
		if (acl_me(rt_user_id, rt_user_name, rt_replies)) {
			return true;
		}
	}

#if 0
	// ホーム TL 用の判定
	if (is_quoted == false && opt_pseudo_home) {
		if (acl_home(status, user_id, user_name) == false) {
			return false;
		}
	}
#endif

	// ここからはホームでもフィルタでも
	// ブロック氏かミュート氏がどこかに登場するツイートをひたすら弾く。

	// 他人氏を弾いたのでここで返信先関係のデバッグメッセージを表示
	if (diagShow >= 1) {
		diagShow.Print("%s", replies[""].c_str());
		replies.Remove("");
	}

#if 0
	// ブロック氏宛て、ミュート氏宛てを弾く。
	auto reply_to_follow = false;
	for (const auto& kv : replies) {
		const auto& id = kv.first;
		const auto& name = kv.second;

		if (blocklist.ContainsKey(id)) {
			DiagPrint(1, "acl: @%s replies block(@%s) -> false",
				user_name.c_str(), name.c_str());
			return false;
		}
		if (mutelist.ContainsKey(id)) {
			DiagPrint(1, "acl: @%s replies mute(@%s) -> false",
				user_name.c_str(), name.c_str());
			return false;
		}
		if (followlist.ContainsKey(id)) {
			reply_to_follow = true;
		}
	}
	// ホーム TL なら、フォロー氏から他人氏宛てのリプを弾く。
	// この時点で生き残ってる発言者はホーム TL ならフォロー氏だけ。
	if (is_quoted == false && opt_pseudo_home) {
		// 宛先があって、かつ、フォロー氏が一人も含まれてなければ
		if (!replies.empty() && reply_to_follow == false) {
			if (diagShow >= 1) {
				std::string names;
				for (const auto& kv : replies) {
					const auto& name = kv.second;
					if (!names.empty()) {
						names += ",";
					}
					names += "@" + name;
				}
				diagShow.Print("acl: @%s replies others(%s) -> false",
					user_name.c_str(), names.c_str());
			}
			return false;
		}
	}
#endif

	// リツイートがあれば
	if (status.contains("retweeted_status")) {
		const Json& rt_status = status["retweeted_status"];

		if (__predict_false(rt_status.contains("user") == false)) {
			return false;
		}
		const Json& rt_user = rt_status["user"];
		const auto& rt_user_id = rt_user.value("id_str", "");
		std::string rt_user_name;
		if (diagShow >= 1) {
			rt_user_name = rt_user.value("screen_name", "");
		}

		// RT 先発言者がブロック氏かミュート氏なら弾く
		if (blocklist.ContainsKey(rt_user_id)) {
			DiagPrint(1, "acl: @%s retweets block(@%s) -> false",
				user_name.c_str(), rt_user_name.c_str());
			return false;
		}
		if (mutelist.ContainsKey(rt_user_id)) {
			DiagPrint(1, "acl: @%s retweets mute(@%s) -> false",
				user_name.c_str(), rt_user_name.c_str());
			return false;
		}

		// RT 先のリプ先がブロック氏かミュート氏なら弾く
		replies = GetReplies(rt_status, rt_user_id, rt_user_name);
		if (diagShow >= 1) {
			DiagPrint(2, "%s", replies[""].c_str());
			replies.Remove("");
		}
		for (const auto& kv : replies) {
			const auto& id = kv.first;
			const auto& name = kv.second;
			if (blocklist.ContainsKey(id)) {
				DiagPrint(1, "acl: @%s retweets (* to block(@%s)) -> false",
					user_name.c_str(), name.c_str());
				return false;
			}
			if (mutelist.ContainsKey(id)) {
				DiagPrint(1, "acl: @%s retweets (* to mute(@%s)) -> false",
					user_name.c_str(), name.c_str());
				return false;
			}
		}
	}

	// それ以外のツイートは表示してよい
	return true;
}

// このツイートが俺氏発か俺氏宛てで表示するなら true。
// 本ツイートとリツイート先とから呼ばれる。
// この時点で、デバッグレベルが 1 以上なら replies には "" => debug message
// のエントリが含まれている場合がある。
static bool
acl_me(const std::string& user_id, const std::string& user_name,
	const StringDictionary& replies)
{
	// user_id(, user_name) はこのツイートの発言者
	// replies はこのツイートの返信関係の情報

	// 俺氏の発言はすべて表示
	if (user_id == myid) {
		DiagPrint(1, "acl_me: myid -> true");
		return true;
	}

	// 俺氏宛てはブロック以外からは表示
	for (const auto& kv : replies) {
		const auto& id = kv.first;
		if (id.empty())
			continue;
		if (id == myid) {
			if (blocklist.ContainsKey(user_id)) {
				DiagPrint(1, "acl_me: block(@%s) to myid -> false",
					user_name.c_str());
				return false;
			}
			if (diagShow >= 2 && replies.ContainsKey("")) {
				diagShow.Print("%s", replies.at("").c_str());
			}
			DiagPrint(1, "acl_me: * to myid -> true");
			return true;
		}
	}

	return false;
}

#if 0
// ホーム TL のみで行う追加判定。
static bool
acl_home(const Json& status, const std::string& user_id,
	const std::string& user_name)
{
	// user_id(, user_name) はこのツイートの発言者

	// RT非表示氏のリツイートは弾く。
	if (status.contains("retweeted_status") &&
	    nortlist.ContainsKey(user_id)) {
		DiagPrint(1, "acl_home: nort(@%s) retweet -> false",
			user_name.c_str());
		return false;
	}

	// 他人氏の発言はもう全部弾いてよい
	if (!followlist.ContainsKey(user_id)) {
		DiagPrint(3, "acl_home: others(@%s) -> false", user_name.c_str());
		return false;
	}

	// これ以降は、
	// フォロー氏、フォロー氏から誰か宛て、フォロー氏がRT、
	// RT非表示氏、RT非表示氏から誰か宛て、
	// だけになっているので、ホーム/フィルタ共通の判定に戻る。
	return true;
}
#endif

// リプライ + ユーザメンションの宛先リストを返す。
//
// 誰か宛てのツイートは
// ・in_reply_to_user_id なし、本文前ユーザメンションあり
//   (誰か宛ての最初のツイート)
// ・in_reply_to_user_id があるけど発言者自身宛て
//   (通常のツリー発言)
// ・in_reply_to_user_id あり、本文前ユーザメンションなし?
//   (返信?)
// ・in_reply_to_user_id あり、本文前ユーザメンションあり
//   (返信?、もしくは複数人との会話)
// が考えられるため、in_reply_to_user_id のユーザだけ見たのではいけない。
//
// in_reply_to_user_id に本文前ユーザメンションの全員を加えてここから
// 発言者本人を引いた集合が、おそらく知りたい宛先リスト。
//
// 戻り値は Dictionary<string,string> で user_id => screen_name に
// なっている (デバッグレベル 0 なら screen_name は "")。
// またデバッグレベル 1 以上なら、"" => debug message というエントリを
// 紛れ込ませてあるので列挙する前に取り除くか、列挙中で避けるかすること。
//
// XXX 本文前ではなく本文内の先頭から始まるメンションはテキスト上
// 見分けが付かないけどこれは無理というか仕様バグでは…。
static StringDictionary
GetReplies(const Json& status,
	const std::string& user_id, const std::string& user_name)
{
	// user_id(, user_name) はこのツイートの発言者

	// display_text_range の一つ目だけ取得。これより前が本文前。
	// なければとりあえず全域としておくか。
	int text_start = 0;
	if (status.contains("display_text_range")) {
		auto text_range = status["display_text_range"];
		text_start = text_range[0];
	}

	// ユーザメンション(entities.user_mentions)、なければ空配列
	const Json null_array = Json::array();
	const Json *user_mentions = &null_array;
	if (status.contains("entities")) {
		const Json& entities = status["entities"];
		if (entities.contains("user_mentions")) {
			user_mentions = &entities["user_mentions"];
		}
	}
	// screen_name は判定自体には不要なのでデバッグ表示の時だけ有効。
	StringDictionary dict;
	for (const auto& um : *user_mentions) {
		// ここで um は user_mentions[] の1人分
		// {
		//   "id":..,
		//   "id_str":"...",
		//   "indices":[start,end],
		//   "name":"...",
		//   "screen_name":"...",
		// }
		int um_start = 0;
		// このユーザメンションの開始位置が
		if (um.contains("indices")) {
			auto indices = um["indices"];
			if (indices.is_array() && indices.size() >= 1) {
				um_start = indices[0].get<int>();
			}
		}
		// 本文以降なら、これは宛先ではないという認識
		if (um_start >= text_start) {
			continue;
		}

		// dict に追加
		const auto& id_str = um.value("id_str", "");
		std::string screen_name;
		if (diagShow >= 1) {
			screen_name = um.value("screen_name", "");
		}
		dict.AddOrUpdate(id_str, screen_name);
	}
	// デバッグメッセージ
	std::string msg;
	std::string msgdict;
	if (diagShow >= 2) {
		msg = "user=@" + user_name;
		for (const auto& kv : dict) {
			const auto& name = kv.second;
			if (!msgdict.empty()) {
				msgdict += ",";
			}
			msgdict += "@" + name;
		}
	}

	// in_reply_to_user_id を追加
	// フィールド自体があって json::null ということもあるようだ。
	std::string replyto_id;
	if (status.contains("in_reply_to_user_id_str") &&
	    status["in_reply_to_user_id_str"].is_string()) {
		replyto_id = status["in_reply_to_user_id_str"].get<std::string>();
	}
	if (!replyto_id.empty()) {
		std::string replyto_name;
		if (diagShow >= 1) {
			replyto_name = status.value("in_reply_to_screen_name", "");
			msg += " reply_to=@" + replyto_name;
		}
		dict.AddOrUpdate(replyto_id, replyto_name);
	}

	// デバッグメッセージ
	if (diagShow >= 1) {
		if (!msgdict.empty()) {
			msg += " mention=" + msgdict;
		}
		dict.AddOrUpdate("", msg);
	}

	// ここから発言者自身を引く
	dict.Remove(user_id);

	return dict;
}
