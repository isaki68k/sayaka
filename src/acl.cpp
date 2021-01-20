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

#include "acl.h"
#include "Diag.h"
#include "Dictionary.h"
#include "StringUtil.h"
#include "main.h"

static bool acl_me(const std::string& user_id, const std::string& user_name,
	const StringDictionary& replies);
static bool acl_home(const Json& status, const std::string& user_id,
	const std::string& user_name);
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
		diagShow.Print(3, "acl: block(@%s) -> false", user_name.c_str());
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
			diagShow.Print(lv, "acl: mute(@%s) -> false", user_name.c_str());
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

	// ホーム TL 用の判定
	if (is_quoted == false && opt_pseudo_home) {
		if (acl_home(status, user_id, user_name) == false) {
			return false;
		}
	}

	// ここからはホームでもフィルタでも
	// ブロック氏かミュート氏がどこかに登場するツイートをひたすら弾く。

	// 他人氏を弾いたのでここで返信先関係のデバッグメッセージを表示
	if (diagShow >= 1) {
		diagShow.Print("%s", replies[""].c_str());
		replies.Remove("");
	}

	// ブロック氏宛て、ミュート氏宛てを弾く。
	auto reply_to_follow = false;
	for (const auto& kv : replies) {
		const auto& id = kv.first;
		const auto& name = kv.second;

		if (blocklist.ContainsKey(id)) {
			diagShow.Print(1, "acl: @%s replies block(@%s) -> false",
				user_name.c_str(), name.c_str());
			return false;
		}
		if (mutelist.ContainsKey(id)) {
			diagShow.Print(1, "acl: @%s replies mute(@%s) -> false",
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
			diagShow.Print(1, "acl: @%s retweets block(@%s) -> false",
				user_name.c_str(), rt_user_name.c_str());
			return false;
		}
		if (mutelist.ContainsKey(rt_user_id)) {
			diagShow.Print(1, "acl: @%s retweets mute(@%s) -> false",
				user_name.c_str(), rt_user_name.c_str());
			return false;
		}

		// RT 先のリプ先がブロック氏かミュート氏なら弾く
		replies = GetReplies(rt_status, rt_user_id, rt_user_name);
		if (diagShow >= 1) {
			if (diagShow >= 2) {
				diagShow.Print("%s", replies[""].c_str());
			}
			replies.Remove("");
		}
		for (const auto& kv : replies) {
			const auto& id = kv.first;
			const auto& name = kv.second;
			if (blocklist.ContainsKey(id)) {
				diagShow.Print(1,
					"acl: @%s retweets (* to block(@%s)) -> false",
					user_name.c_str(), name.c_str());
				return false;
			}
			if (mutelist.ContainsKey(id)) {
				diagShow.Print(1,
					"acl: @%s retweets (* to mute(@%s)) -> false",
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
		diagShow.Print(1, "acl_me: myid -> true");
		return true;
	}

	// 俺氏宛てはブロック以外からは表示
	for (const auto& kv : replies) {
		const auto& id = kv.first;
		if (id.empty())
			continue;
		if (id == myid) {
			if (blocklist.ContainsKey(user_id)) {
				diagShow.Print(1, "acl_me: block(@%s) to myid -> false",
					user_name.c_str());
				return false;
			}
			if (diagShow >= 2 && replies.ContainsKey("")) {
				diagShow.Print("%s", replies.at("").c_str());
			}
			diagShow.Print(1, "acl_me: * to myid -> true");
			return true;
		}
	}

	return false;
}

// ホーム TL のみで行う追加判定。
static bool
acl_home(const Json& status, const std::string& user_id,
	const std::string& user_name)
{
	// user_id(, user_name) はこのツイートの発言者

	// RT非表示氏のリツイートは弾く。
	if (status.contains("retweeted_status") &&
	    nortlist.ContainsKey(user_id)) {
		diagShow.Print(1, "acl_home: nort(@%s) retweet -> false",
			user_name.c_str());
		return false;
	}

	// 他人氏の発言はもう全部弾いてよい
	if (!followlist.ContainsKey(user_id)) {
		diagShow.Print(3, "acl_home: others(@%s) -> false", user_name.c_str());
		return false;
	}

	// これ以降は、
	// フォロー氏、フォロー氏から誰か宛て、フォロー氏がRT、
	// RT非表示氏、RT非表示氏から誰か宛て、
	// だけになっているので、ホーム/フィルタ共通の判定に戻る。
	return true;
}

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

#if defined(SELFTEST)
#include "test.h"

void
test_showstatus_acl()
{
	printf("%s\n", __func__);

	// id:1 が自分、id:2,3 がフォロー、
	// id:4 はミュートしているフォロー、
	// id:5 はRTを表示しないフォロー
	// id:6,7 はブロック、
	// id:8,9 がフォロー外
	myid = "1";
	followlist.AddOrUpdate("1", "1");	// 自身もフォローに入れてある
	followlist.AddOrUpdate("2", "2");
	followlist.AddOrUpdate("3", "3");
	followlist.AddOrUpdate("4", "4");
	followlist.AddOrUpdate("5", "5");
	mutelist.AddOrUpdate("4", "4");
	nortlist.AddOrUpdate("5", "5");
	blocklist.AddOrUpdate("6", "6");
	blocklist.AddOrUpdate("7", "7");

	// 簡易 JSON みたいな独自書式でテストを書いてコード中で JSON にする。
	// o 発言者 id (number) -> user.id_str (string)
	// o リプ先 reply (number) -> in_reply_to_user_id_str (string)
	// o リツイート rt (number) -> retweeted_status.user.id_str (string)
	// o リツイート先のリプライ先 rt_rep (number) ->
	//                 retweeted_status.in_reply_to_user_id_str (string)
	// 結果はホームタイムラインとフィルタモードによって期待値が異なり
	// それぞれ home, filt で表す。あれば表示、省略は非表示を意味する。
	// h---, f--- はテストしないことを示す。俺氏とブロック氏とに同時に
	// 返信された場合のように判定不能なケースをとりあえず。
	std::vector<std::string> table = {
		// 平文
		"{id:1,        home,filt}",		// 俺氏
		"{id:2,        home,filt}",		// フォロー氏
		"{id:4,                 }",		// ミュート氏
		"{id:5,        home,filt}",		// RT非表示氏
		"{id:6,                 }",		// ブロック氏
		"{id:8,             filt}",		// 他人氏

		// 俺氏がリプ
		"{id:1,reply:1,home,filt}",		// 自分自身へ
		"{id:1,reply:2,home,filt}",		// フォローへ
		"{id:1,reply:4,home,filt}",		// ミュートへ
		"{id:1,reply:5,home,filt}",		// RT非表示へ
		"{id:1,reply:6,home,filt}",		// ブロックへ
		"{id:1,reply:8,home,filt}",		// 他人へ

		// フォロー氏がリプ (RT非表示氏も同じになるはずなので以下参照)
		"{id:2,reply:1,home,filt}",		// 自分へ
		"{id:2,reply:2,home,filt}",		// フォローへ
		"{id:2,reply:4,         }",		// ミュートへ
		"{id:2,reply:5,home,filt}",		// RT非表示へ
		"{id:2,reply:6,         }",		// ブロックへ
		"{id:2,reply:8,     filt}",		// 他人へ

		// ミュート氏がリプ
		"{id:4,reply:1,home,filt}",		// 自分へ
		"{id:4,reply:2,         }",		// フォローへ
		"{id:4,reply:4,         }",		// ミュートへ
		"{id:4,reply:5,         }",		// RT非表示へ
		"{id:4,reply:6,         }",		// ブロックへ
		"{id:4,reply:8,         }",		// 他人へ

		// RT非表示氏がリプ (リプはフォロー氏発言と同じ扱いでよいはず)
		"{id:5,reply:1,home,filt}",		// 自分へ
		"{id:5,reply:2,home,filt}",		// フォローへ
		"{id:5,reply:4,         }",		// ミュートへ
		"{id:5,reply:5,home,filt}",		// RT非表示へ
		"{id:5,reply:6,         }",		// ブロックへ
		"{id:5,reply:8,     filt}",		// 他人へ

		// ブロック氏がリプ
		"{id:6,reply:1,         }",		// 自分へ
		"{id:6,reply:2,         }",		// フォローへ
		"{id:6,reply:4,         }",		// ミュートへ
		"{id:6,reply:5,         }",		// RT非表示へ
		"{id:6,reply:6,         }",		// ブロックへ
		"{id:6,reply:8,         }",		// 他人へ

		// 他人氏がリプ
		"{id:8,reply:1,home,filt}",		// 自分へ
		"{id:8,reply:2,     filt}",		// フォローへ
		"{id:8,reply:4,         }",		// ミュートへ
		"{id:8,reply:5,     filt}",		// RT非表示へ
		"{id:8,reply:6,         }",		// ブロックへ
		"{id:8,reply:8,     filt}",		// 他人へ

		// 俺氏、メンションのみ
		"{id:1,ment:1,home,filt}",			// リプなし、UM 俺氏
		"{id:1,ment:2,home,filt}",			// リプなし、UM フォロー氏
		"{id:1,ment:4,home,filt}",			// リプなし、UM ミュート氏
		"{id:1,ment:5,home,filt}",			// リプなし、UM RT非表示氏
		"{id:1,ment:6,home,filt}",			// リプなし、UM ブロック氏
		"{id:1,ment:8,home,filt}",			// リプなし、UM 他人氏

		// フォロー氏、メンションのみ
		"{id:2,ment:1,home,filt}",			// リプなし、UM 俺氏
		"{id:2,ment:2,home,filt}",			// リプなし、UM フォロー氏自
		"{id:2,ment:3,home,filt}",			// リプなし、UM フォロー氏他
		"{id:2,ment:4,         }",			// リプなし、UM ミュート氏
		"{id:2,ment:5,home,filt}",			// リプなし、UM RT非表示氏
		"{id:2,ment:6,         }",			// リプなし、UM ブロック氏
		"{id:2,ment:8,     filt}",			// リプなし、UM 他人氏

		// ミュート氏、メンションのみ
		"{id:4,ment:1,home,filt}",			// リプなし、UM 俺氏
		"{id:4,ment:2,         }",			// リプなし、UM フォロー氏
		"{id:4,ment:4,         }",			// リプなし、UM ミュート氏
		"{id:4,ment:5,         }",			// リプなし、UM RT非表示氏
		"{id:4,ment:6,         }",			// リプなし、UM ブロック氏
		"{id:4,ment:8,         }",			// リプなし、UM 他人氏

		// RT非表示氏、メンションのみ (フォロー氏と同じになる)
		"{id:5,ment:1,home,filt}",			// リプなし、UM 俺氏
		"{id:5,ment:2,home,filt}",			// リプなし、UM フォロー氏
		"{id:5,ment:4,         }",			// リプなし、UM ミュート氏
		"{id:5,ment:5,home,filt}",			// リプなし、UM RT非表示氏
		"{id:5,ment:6,         }",			// リプなし、UM ブロック氏
		"{id:5,ment:8,     filt}",			// リプなし、UM 他人氏

		// ブロック氏、メンションのみ
		"{id:6,ment:1,         }",			// リプなし、UM 俺氏
		"{id:6,ment:2,         }",			// リプなし、UM フォロー氏
		"{id:6,ment:4,         }",			// リプなし、UM ミュート氏
		"{id:6,ment:5,         }",			// リプなし、UM RT非表示氏
		"{id:6,ment:6,         }",			// リプなし、UM ブロック氏
		"{id:6,ment:8,         }",			// リプなし、UM 他人氏

		// 他人氏、メンションのみ
		"{id:8,ment:1,home,filt}",			// リプなし、UM 俺氏
		"{id:8,ment:2,     filt}",			// リプなし、UM フォロー氏
		"{id:8,ment:4,         }",			// リプなし、UM ミュート氏
		"{id:8,ment:5,     filt}",			// リプなし、UM RT非表示氏
		"{id:8,ment:6,         }",			// リプなし、UM ブロック氏
		"{id:8,ment:8,     filt}",			// リプなし、UM 他人氏

		// 俺氏、リプ+メンション
		"{id:1,reply:1,ment:1,home,filt}",	// rep俺氏、UM 俺氏
		"{id:1,reply:1,ment:2,home,filt}",	// rep俺氏、UM フォロー氏
		"{id:1,reply:1,ment:4,home,filt}",	// rep俺氏、UM ミュート氏
		"{id:1,reply:1,ment:5,home,filt}",	// rep俺氏、UM RT非表示氏
		"{id:1,reply:1,ment:6,home,filt}",	// rep俺氏、UM ブロック氏
		"{id:1,reply:1,ment:8,home,filt}",	// rep俺氏、UM 他人氏
		"{id:1,reply:2,ment:1,home,filt}",	// repフォロー氏、UM 俺氏
		"{id:1,reply:2,ment:2,home,filt}",	// repフォロー氏、UM フォロー氏
		"{id:1,reply:2,ment:4,home,filt}",	// repフォロー氏、UM ミュート氏
		"{id:1,reply:2,ment:5,home,filt}",	// repフォロー氏、UM RT非表示氏
		"{id:1,reply:2,ment:6,home,filt}",	// repフォロー氏、UM ブロック氏
		"{id:1,reply:2,ment:8,home,filt}",	// repフォロー氏、UM 他人氏
		"{id:1,reply:4,ment:1,home,filt}",	// repミュート氏、UM 俺氏
		"{id:1,reply:4,ment:2,home,filt}",	// repミュート氏、UM フォロー氏
		"{id:1,reply:4,ment:4,home,filt}",	// repミュート氏、UM ミュート氏
		"{id:1,reply:4,ment:5,home,filt}",	// repミュート氏、UM RT非表示氏
		"{id:1,reply:4,ment:6,home,filt}",	// repミュート氏、UM ブロック氏
		"{id:1,reply:4,ment:8,home,filt}",	// repミュート氏、UM 他人氏
		"{id:1,reply:5,ment:1,home,filt}",	// repRT非表示氏、UM 俺氏
		"{id:1,reply:5,ment:2,home,filt}",	// repRT非表示氏、UM フォロー氏
		"{id:1,reply:5,ment:4,home,filt}",	// repRT非表示氏、UM ミュート氏
		"{id:1,reply:5,ment:5,home,filt}",	// repRT非表示氏、UM RT非表示氏
		"{id:1,reply:5,ment:6,home,filt}",	// repRT非表示氏、UM ブロック氏
		"{id:1,reply:5,ment:8,home,filt}",	// repRT非表示氏、UM 他人氏
		"{id:1,reply:6,ment:1,home,filt}",	// repブロック氏、UM 俺氏
		"{id:1,reply:6,ment:2,home,filt}",	// repブロック氏、UM フォロー氏
		"{id:1,reply:6,ment:4,home,filt}",	// repブロック氏、UM ミュート氏
		"{id:1,reply:6,ment:5,home,filt}",	// repブロック氏、UM RT非表示氏
		"{id:1,reply:6,ment:6,home,filt}",	// repブロック氏、UM ブロック氏
		"{id:1,reply:6,ment:8,home,filt}",	// repブロック氏、UM 他人氏
		"{id:1,reply:8,ment:1,home,filt}",	// rep他人氏、UM 俺氏
		"{id:1,reply:8,ment:2,home,filt}",	// rep他人氏、UM フォロー氏
		"{id:1,reply:8,ment:4,home,filt}",	// rep他人氏、UM ミュート氏
		"{id:1,reply:8,ment:5,home,filt}",	// rep他人氏、UM RT非表示氏
		"{id:1,reply:8,ment:6,home,filt}",	// rep他人氏、UM ブロック氏
		"{id:1,reply:8,ment:8,home,filt}",	// rep他人氏、UM 他人氏

		// フォロー氏、リプ+メンション
		"{id:2,reply:1,ment:1,home,filt}",	// rep俺氏、UM 俺氏
		"{id:2,reply:1,ment:2,home,filt}",	// rep俺氏、UM フォロー氏
		"{id:2,reply:1,ment:4,home,filt}",	// rep俺氏、UM ミュート氏
		"{id:2,reply:1,ment:5,home,filt}",	// rep俺氏、UM RT非表示氏
		"{id:2,reply:1,ment:6,h---,f---}",	// rep俺氏、UM ブロック氏
		"{id:2,reply:1,ment:8,home,filt}",	// rep俺氏、UM 他人氏
		"{id:2,reply:2,ment:1,home,filt}",	// repフォロー氏、UM 俺氏
		"{id:2,reply:2,ment:2,home,filt}",	// repフォロー自、UM フォロー氏
		"{id:2,reply:2,ment:3,home,filt}",	// repフォロー他、UM フォロー氏
		"{id:2,reply:2,ment:4,         }",	// repフォロー自、UM ミュート氏
		"{id:2,reply:2,ment:5,home,filt}",	// repフォロー自、UM RT非表示氏
		"{id:2,reply:2,ment:6,         }",	// repフォロー自、UM ブロック氏
		"{id:2,reply:2,ment:8,     filt}",	// repフォロー自、UM 他人氏
		"{id:2,reply:4,ment:1,home,filt}",	// repミュート氏、UM 俺氏
		"{id:2,reply:4,ment:2,         }",	// repミュート氏、UM フォロー自
		"{id:2,reply:4,ment:3,         }",	// repミュート氏、UM フォロー他
		"{id:2,reply:4,ment:4,         }",	// repミュート氏、UM ミュート氏
		"{id:2,reply:4,ment:5,         }",	// repミュート氏、UM RT非表示氏
		"{id:2,reply:4,ment:6,         }",	// repミュート氏、UM ブロック氏
		"{id:2,reply:4,ment:8,         }",	// repミュート氏、UM 他人氏
		"{id:2,reply:5,ment:1,home,filt}",	// repRT非表示氏、UM 俺氏
		"{id:2,reply:5,ment:2,home,filt}",	// repRT非表示氏、UM フォロー氏
		"{id:2,reply:5,ment:4,         }",	// repRT非表示氏、UM ミュート氏
		"{id:2,reply:5,ment:5,home,filt}",	// repRT非表示氏、UM RT非表示氏
		"{id:2,reply:5,ment:6,         }",	// repRT非表示氏、UM ブロック氏
		"{id:2,reply:5,ment:8,home,filt}",	// repRT非表示氏、UM 他人氏
		"{id:2,reply:6,ment:1,h---,f---}",	// repブロック氏、UM 俺氏
		"{id:2,reply:6,ment:2,         }",	// repブロック氏、UM フォロー自
		"{id:2,reply:6,ment:3,         }",	// repブロック氏、UM フォロー他
		"{id:2,reply:6,ment:4,         }",	// repブロック氏、UM ミュート氏
		"{id:2,reply:6,ment:5,         }",	// repブロック氏、UM RT非表示氏
		"{id:2,reply:6,ment:6,         }",	// repブロック氏、UM ブロック氏
		"{id:2,reply:6,ment:8,         }",	// repブロック氏、UM 他人氏
		"{id:2,reply:8,ment:1,home,filt}",	// rep他人氏、UM 俺氏
		"{id:2,reply:8,ment:2,     filt}",	// rep他人氏、UM フォロー氏
		"{id:2,reply:8,ment:4,         }",	// rep他人氏、UM ミュート氏
		"{id:2,reply:8,ment:5,home,filt}",	// rep他人氏、UM RT非表示氏
		"{id:2,reply:8,ment:6,         }",	// rep他人氏、UM ブロック氏
		"{id:2,reply:8,ment:8,     filt}",	// rep他人氏、UM 他人氏

		// ミュート氏、リプ+メンション
		"{id:4,reply:1,ment:1,home,filt}",	// rep俺氏、UM 俺氏
		"{id:4,reply:1,ment:2,home,filt}",	// rep俺氏、UM フォロー氏
		"{id:4,reply:1,ment:4,home,filt}",	// rep俺氏、UM ミュート氏
		"{id:4,reply:1,ment:5,home,filt}",	// rep俺氏、UM RT非表示氏
		"{id:4,reply:1,ment:6,h---,f---}",	// rep俺氏、UM ブロック氏
		"{id:4,reply:1,ment:8,home,filt}",	// rep俺氏、UM 他人氏
		"{id:4,reply:2,ment:1,home,filt}",	// repフォロー氏、UM 俺氏
		"{id:4,reply:2,ment:2,         }",	// repフォロー氏、UM フォロー氏
		"{id:4,reply:2,ment:4,         }",	// repフォロー氏、UM ミュート氏
		"{id:4,reply:2,ment:5,         }",	// repフォロー氏、UM RT非表示氏
		"{id:4,reply:2,ment:6,         }",	// repフォロー氏、UM ブロック氏
		"{id:4,reply:2,ment:8,         }",	// repフォロー氏、UM 他人氏
		"{id:4,reply:4,ment:1,home,filt}",	// repミュート氏、UM 俺氏
		"{id:4,reply:4,ment:2,         }",	// repミュート氏、UM フォロー氏
		"{id:4,reply:4,ment:4,         }",	// repミュート氏、UM ミュート氏
		"{id:4,reply:4,ment:5,         }",	// repミュート氏、UM RT非表示氏
		"{id:4,reply:4,ment:6,         }",	// repミュート氏、UM ブロック氏
		"{id:4,reply:4,ment:8,         }",	// repミュート氏、UM 他人氏
		"{id:4,reply:5,ment:1,home,filt}",	// repRT非表示氏、UM 俺氏
		"{id:4,reply:5,ment:2,         }",	// repRT非表示氏、UM フォロー氏
		"{id:4,reply:5,ment:4,         }",	// repRT非表示氏、UM ミュート氏
		"{id:4,reply:5,ment:5,         }",	// repRT非表示氏、UM RT非表示氏
		"{id:4,reply:5,ment:6,         }",	// repRT非表示氏、UM ブロック氏
		"{id:4,reply:5,ment:8,         }",	// repRT非表示氏、UM 他人氏
		"{id:4,reply:6,ment:1,h---,f---}",	// repブロック氏、UM 俺氏
		"{id:4,reply:6,ment:2,         }",	// repブロック氏、UM フォロー氏
		"{id:4,reply:6,ment:4,         }",	// repブロック氏、UM ミュート氏
		"{id:4,reply:6,ment:5,         }",	// repブロック氏、UM RT非表示氏
		"{id:4,reply:6,ment:6,         }",	// repブロック氏、UM ブロック氏
		"{id:4,reply:6,ment:8,         }",	// repブロック氏、UM 他人氏
		"{id:4,reply:8,ment:1,home,filt}",	// rep他人氏、UM 俺氏
		"{id:4,reply:8,ment:2,         }",	// rep他人氏、UM フォロー氏
		"{id:4,reply:8,ment:4,         }",	// rep他人氏、UM ミュート氏
		"{id:4,reply:8,ment:5,         }",	// rep他人氏、UM RT非表示氏
		"{id:4,reply:8,ment:6,         }",	// rep他人氏、UM ブロック氏
		"{id:4,reply:8,ment:8,         }",	// rep他人氏、UM 他人氏

		// RT非表示氏、リプ+メンション
		"{id:5,reply:1,ment:1,home,filt}",	// rep俺氏、UM 俺氏
		"{id:5,reply:1,ment:2,home,filt}",	// rep俺氏、UM フォロー氏
		"{id:5,reply:1,ment:4,home,filt}",	// rep俺氏、UM ミュート氏
		"{id:5,reply:1,ment:5,home,filt}",	// rep俺氏、UM RT非表示氏
		"{id:5,reply:1,ment:6,h---,f---}",	// rep俺氏、UM ブロック氏
		"{id:5,reply:1,ment:8,home,filt}",	// rep俺氏、UM 他人氏
		"{id:5,reply:2,ment:1,home,filt}",	// repフォロー氏、UM 俺氏
		"{id:5,reply:2,ment:2,home,filt}",	// repフォロー氏、UM フォロー氏
		"{id:5,reply:2,ment:4,         }",	// repフォロー氏、UM ミュート氏
		"{id:5,reply:2,ment:5,home,filt}",	// repフォロー氏、UM RT非表示氏
		"{id:5,reply:2,ment:6,         }",	// repフォロー氏、UM ブロック氏
		"{id:5,reply:2,ment:8,home,filt}",	// repフォロー氏、UM 他人氏
		"{id:5,reply:4,ment:1,home,filt}",	// repミュート氏、UM 俺氏
		"{id:5,reply:4,ment:2,         }",	// repミュート氏、UM フォロー氏
		"{id:5,reply:4,ment:4,         }",	// repミュート氏、UM ミュート氏
		"{id:5,reply:4,ment:5,         }",	// repミュート氏、UM RT非表示氏
		"{id:5,reply:4,ment:6,         }",	// repミュート氏、UM ブロック氏
		"{id:5,reply:4,ment:8,         }",	// repミュート氏、UM 他人氏
		"{id:5,reply:5,ment:1,home,filt}",	// repRT非表示氏、UM 俺氏
		"{id:5,reply:5,ment:2,home,filt}",	// repRT非表示氏、UM フォロー氏
		"{id:5,reply:5,ment:4,         }",	// repRT非表示氏、UM ミュート氏
		"{id:5,reply:5,ment:5,home,filt}",	// repRT非表示氏、UM RT非表示氏
		"{id:5,reply:5,ment:6,         }",	// repRT非表示氏、UM ブロック氏
		"{id:5,reply:5,ment:8,     filt}",	// repRT非表示氏、UM 他人氏
		"{id:5,reply:6,ment:1,h---,f---}",	// repブロック氏、UM 俺氏
		"{id:5,reply:6,ment:2,         }",	// repブロック氏、UM フォロー氏
		"{id:5,reply:6,ment:4,         }",	// repブロック氏、UM ミュート氏
		"{id:5,reply:6,ment:5,         }",	// repブロック氏、UM RT非表示氏
		"{id:5,reply:6,ment:6,         }",	// repブロック氏、UM ブロック氏
		"{id:5,reply:6,ment:8,         }",	// repブロック氏、UM 他人氏
		"{id:5,reply:8,ment:1,home,filt}",	// rep他人氏、UM 俺氏
		"{id:5,reply:8,ment:2,home,filt}",	// rep他人氏、UM フォロー氏
		"{id:5,reply:8,ment:4,         }",	// rep他人氏、UM ミュート氏
		"{id:5,reply:8,ment:5,     filt}",	// rep他人氏、UM RT非表示氏
		"{id:5,reply:8,ment:6,         }",	// rep他人氏、UM ブロック氏
		"{id:5,reply:8,ment:8,     filt}",	// rep他人氏、UM 他人氏

		// ブロック氏、リプ+メンション
		"{id:6,reply:1,ment:1,h---,f---}",	// rep俺氏、UM 俺氏
		"{id:6,reply:1,ment:2,h---,f---}",	// rep俺氏、UM フォロー氏
		"{id:6,reply:1,ment:4,h---,f---}",	// rep俺氏、UM ミュート氏
		"{id:6,reply:1,ment:5,h---,f---}",	// rep俺氏、UM RT非表示氏
		"{id:6,reply:1,ment:6,h---,f---}",	// rep俺氏、UM ブロック氏
		"{id:6,reply:1,ment:8,h---,f---}",	// rep俺氏、UM 他人氏
		"{id:6,reply:2,ment:1,h---,f---}",	// repフォロー氏、UM 俺氏
		"{id:6,reply:2,ment:2,         }",	// repフォロー自、UM フォロー氏
		"{id:6,reply:2,ment:3,         }",	// repフォロー他、UM フォロー氏
		"{id:6,reply:2,ment:4,         }",	// repフォロー自、UM ミュート氏
		"{id:6,reply:2,ment:5,         }",	// repフォロー自、UM RT非表示氏
		"{id:6,reply:2,ment:6,         }",	// repフォロー自、UM ブロック氏
		"{id:6,reply:2,ment:8,         }",	// repフォロー自、UM 他人氏
		"{id:6,reply:4,ment:1,h---,f---}",	// repミュート氏、UM 俺氏
		"{id:6,reply:4,ment:2,         }",	// repミュート氏、UM フォロー氏
		"{id:6,reply:4,ment:4,         }",	// repミュート氏、UM ミュート氏
		"{id:6,reply:4,ment:5,         }",	// repミュート氏、UM RT非表示氏
		"{id:6,reply:4,ment:6,         }",	// repミュート氏、UM ブロック氏
		"{id:6,reply:4,ment:8,         }",	// repミュート氏、UM 他人氏
		"{id:6,reply:5,ment:1,h---,f---}",	// repRT非表示氏、UM 俺氏
		"{id:6,reply:5,ment:2,         }",	// repRT非表示氏、UM フォロー氏
		"{id:6,reply:5,ment:4,         }",	// repRT非表示氏、UM ミュート氏
		"{id:6,reply:5,ment:5,         }",	// repRT非表示氏、UM RT非表示氏
		"{id:6,reply:5,ment:6,         }",	// repRT非表示氏、UM ブロック氏
		"{id:6,reply:5,ment:8,         }",	// repRT非表示氏、UM 他人氏
		"{id:6,reply:6,ment:1,h---,f---}",	// repブロック氏、UM 俺氏
		"{id:6,reply:6,ment:2,         }",	// repブロック氏、UM フォロー氏
		"{id:6,reply:6,ment:4,         }",	// repブロック氏、UM ミュート氏
		"{id:6,reply:6,ment:5,         }",	// repブロック氏、UM RT非表示氏
		"{id:6,reply:6,ment:6,         }",	// repブロック氏、UM ブロック氏
		"{id:6,reply:6,ment:8,         }",	// repブロック氏、UM 他人氏
		"{id:6,reply:8,ment:1,h---,f---}",	// rep他人氏、UM 俺氏
		"{id:6,reply:8,ment:2,         }",	// rep他人氏、UM フォロー氏
		"{id:6,reply:8,ment:4,         }",	// rep他人氏、UM ミュート氏
		"{id:6,reply:8,ment:5,         }",	// rep他人氏、UM RT非表示氏
		"{id:6,reply:8,ment:6,         }",	// rep他人氏、UM ブロック氏
		"{id:6,reply:8,ment:8,         }",	// rep他人氏、UM 他人氏

		// 他人氏、リプ+メンション
		"{id:8,reply:1,ment:1,home,filt}",	// rep俺氏、UM 俺氏
		"{id:8,reply:1,ment:2,home,filt}",	// rep俺氏、UM フォロー氏
		"{id:8,reply:1,ment:4,home,filt}",	// rep俺氏、UM ミュート氏
		"{id:8,reply:1,ment:5,home,filt}",	// rep俺氏、UM RT非表示氏
		"{id:8,reply:1,ment:6,h---,f---}",	// rep俺氏、UM ブロック氏
		"{id:8,reply:1,ment:8,home,filt}",	// rep俺氏、UM 他人氏
		"{id:8,reply:2,ment:1,home,filt}",	// repフォロー氏、UM 俺氏
		"{id:8,reply:2,ment:2,     filt}",	// repフォロー氏、UM フォロー氏
		"{id:8,reply:2,ment:4,         }",	// repフォロー氏、UM ミュート氏
		"{id:8,reply:2,ment:5,     filt}",	// repフォロー氏、UM RT非表示氏
		"{id:8,reply:2,ment:6,         }",	// repフォロー氏、UM ブロック氏
		"{id:8,reply:2,ment:8,     filt}",	// repフォロー氏、UM 他人氏
		"{id:8,reply:4,ment:1,home,filt}",	// repミュート氏、UM 俺氏
		"{id:8,reply:4,ment:2,         }",	// repミュート氏、UM フォロー氏
		"{id:8,reply:4,ment:4,         }",	// repミュート氏、UM ミュート氏
		"{id:8,reply:4,ment:5,         }",	// repミュート氏、UM RT非表示氏
		"{id:8,reply:4,ment:6,         }",	// repミュート氏、UM ブロック氏
		"{id:8,reply:4,ment:8,         }",	// repミュート氏、UM 他人氏
		"{id:8,reply:5,ment:1,home,filt}",	// repRT非表示氏、UM 俺氏
		"{id:8,reply:5,ment:2,     filt}",	// repRT非表示氏、UM フォロー氏
		"{id:8,reply:5,ment:4,         }",	// repRT非表示氏、UM ミュート氏
		"{id:8,reply:5,ment:5,     filt}",	// repRT非表示氏、UM RT非表示氏
		"{id:8,reply:5,ment:6,         }",	// repRT非表示氏、UM ブロック氏
		"{id:8,reply:5,ment:8,     filt}",	// repRT非表示氏、UM 他人氏
		"{id:8,reply:6,ment:1,h---,f---}",	// repブロック氏、UM 俺氏
		"{id:8,reply:6,ment:2,         }",	// repブロック氏、UM フォロー氏
		"{id:8,reply:6,ment:4,         }",	// repブロック氏、UM ミュート氏
		"{id:8,reply:6,ment:5,         }",	// repブロック氏、UM RT非表示氏
		"{id:8,reply:6,ment:6,         }",	// repブロック氏、UM ブロック氏
		"{id:8,reply:6,ment:8,         }",	// repブロック氏、UM 他人氏
		"{id:8,reply:8,ment:1,home,filt}",	// rep他人氏、UM 俺氏
		"{id:8,reply:8,ment:2,     filt}",	// rep他人氏、UM フォロー氏
		"{id:8,reply:8,ment:4,         }",	// rep他人氏、UM ミュート氏
		"{id:8,reply:8,ment:5,     filt}",	// rep他人氏、UM RT非表示氏
		"{id:8,reply:8,ment:6,         }",	// rep他人氏、UM ブロック氏
		"{id:8,reply:8,ment:8,     filt}",	// rep他人氏、UM 他人氏

		// 俺氏がリツイート
		"{id:1,rt:1,home,filt}",		// 自分自身を
		"{id:1,rt:2,home,filt}",		// フォローを
		"{id:1,rt:4,home,filt}",		// ミュートを
		"{id:1,rt:5,home,filt}",		// RT非表示を
		"{id:1,rt:6,home,filt}",		// ブロックを
		"{id:1,rt:8,home,filt}",		// 他人を

		// フォロー氏がリツイート
		"{id:2,rt:1,home,filt}",		// 自分を
		"{id:2,rt:2,home,filt}",		// フォローを
		"{id:2,rt:4,         }",		// ミュートを
		"{id:2,rt:5,home,filt}",		// RT非表示を
		"{id:2,rt:6,         }",		// ブロックを
		"{id:2,rt:8,home,filt}",		// 他人を

		// ミュート氏がリツイート
		"{id:4,rt:1,         }",		// 自分を
		"{id:4,rt:2,         }",		// フォローを
		"{id:4,rt:4,         }",		// ミュートを
		"{id:4,rt:5,         }",		// RT非表示を
		"{id:4,rt:6,         }",		// ブロックを
		"{id:4,rt:8,         }",		// 他人を

		// RT非表示氏がリツイート
		// 自分の発言をリツイートは表示してもいいだろう
		// フィルタストリームなら表示してもいいだろうか
		"{id:5,rt:1,home,filt}",		// 自分を
		"{id:5,rt:2,     filt}",		// フォローを
		"{id:5,rt:4,         }",		// ミュートを
		"{id:5,rt:5,     filt}",		// RT非表示を
		"{id:5,rt:6,         }",		// ブロックを
		"{id:5,rt:8,     filt}",		// 他人を

		// ブロック氏がリツイート (そもそも来ないような気がするけど一応)
		"{id:6,rt:1,         }",		// 自分を
		"{id:6,rt:2,         }",		// フォローを
		"{id:6,rt:4,         }",		// ミュートを
		"{id:6,rt:5,         }",		// RT非表示を
		"{id:6,rt:6,         }",		// ブロックを
		"{id:6,rt:8,         }",		// 他人を

		// 他人氏がリツイート
		"{id:8,rt:1,home,filt}",		// 自分を
		"{id:8,rt:2,     filt}",		// フォローを
		"{id:8,rt:4,         }",		// ミュートを
		"{id:8,rt:5,     filt}",		// RT非表示を
		"{id:8,rt:6,         }",		// ブロックを
		"{id:8,rt:8,     filt}",		// 他人を

		//
		// フォロー氏がリツイート
		"{id:2,rt:1,rt_rep:1,home,filt}",	// 俺氏から俺氏宛リプ
		"{id:2,rt:1,rt_rep:2,home,filt}",	// 俺氏からフォロー宛リプ
		"{id:2,rt:1,rt_rep:4,home,filt}",	// 俺氏からミュート宛リプ
		"{id:2,rt:1,rt_rep:5,home,filt}",	// 俺氏からRT非表示宛リプ
		"{id:2,rt:1,rt_rep:6,home,filt}",	// 俺氏からブロック宛リプ
		"{id:2,rt:1,rt_rep:8,home,filt}",	// 俺氏から他人宛リプ
		"{id:2,rt:2,rt_rep:1,home,filt}",	// フォローから俺氏宛リプ
		"{id:2,rt:2,rt_rep:2,home,filt}",	// フォローからフォロー宛リプ
		"{id:2,rt:2,rt_rep:4,         }",	// フォローからミュート宛リプ
		"{id:2,rt:2,rt_rep:5,home,filt}",	// フォローからRT非表示宛リプ
		"{id:2,rt:2,rt_rep:6,         }",	// フォローからブロック宛リプ
		"{id:2,rt:2,rt_rep:8,home,filt}",	// フォローから他人宛リプ
		"{id:2,rt:4,rt_rep:1,home,filt}",	// ミュートから俺氏宛リプ
		"{id:2,rt:4,rt_rep:2,         }",	// ミュートからフォロー宛リプ
		"{id:2,rt:4,rt_rep:4,         }",	// ミュートからミュート宛リプ
		"{id:2,rt:4,rt_rep:5,         }",	// ミュートからRT非表示宛リプ
		"{id:2,rt:4,rt_rep:6,         }",	// ミュートからブロック宛リプ
		"{id:2,rt:4,rt_rep:8,         }",	// ミュートから他人宛リプ
		"{id:2,rt:5,rt_rep:1,home,filt}",	// RT非表示から俺氏宛リプ
		"{id:2,rt:5,rt_rep:2,home,filt}",	// RT非表示からフォロー宛リプ
		"{id:2,rt:5,rt_rep:4,         }",	// RT非表示からミュート宛リプ
		"{id:2,rt:5,rt_rep:5,home,filt}",	// RT非表示からRT非表示宛リプ
		"{id:2,rt:5,rt_rep:6,         }",	// RT非表示からブロック宛リプ
		"{id:2,rt:5,rt_rep:8,home,filt}",	// RT非表示から他人宛リプ
		"{id:2,rt:6,rt_rep:1,         }",	// ブロックから俺氏宛リプ
		"{id:2,rt:6,rt_rep:2,         }",	// ブロックからフォロー宛リプ
		"{id:2,rt:6,rt_rep:4,         }",	// ブロックからブロック宛リプ
		"{id:2,rt:6,rt_rep:5,         }",	// ブロックからRT非表示宛リプ
		"{id:2,rt:6,rt_rep:6,         }",	// ブロックからブロック宛リプ
		"{id:2,rt:6,rt_rep:8,         }",	// ブロックから他人宛リプ
		"{id:2,rt:8,rt_rep:1,home,filt}",	// 他人から俺氏宛リプ
		"{id:2,rt:8,rt_rep:2,home,filt}",	// 他人からフォロー宛リプ
		"{id:2,rt:8,rt_rep:4,         }",	// 他人からブロック宛リプ
		"{id:2,rt:8,rt_rep:5,home,filt}",	// 他人からRT非表示宛リプ
		"{id:2,rt:8,rt_rep:6,         }",	// 他人からブロック宛リプ
		"{id:2,rt:8,rt_rep:8,home,filt}",	// 他人から他人宛リプ
		// ミュート氏がリツイート
		"{id:4,rt:1,rt_rep:1,         }",	// 俺氏から俺氏宛リプ
		"{id:4,rt:1,rt_rep:2,         }",	// 俺氏からフォロー宛リプ
		"{id:4,rt:1,rt_rep:4,         }",	// 俺氏からミュート宛リプ
		"{id:4,rt:1,rt_rep:5,         }",	// 俺氏からRT非表示宛リプ
		"{id:4,rt:1,rt_rep:6,         }",	// 俺氏からブロック宛リプ
		"{id:4,rt:1,rt_rep:8,         }",	// 俺氏から他人宛リプ
		"{id:4,rt:2,rt_rep:1,         }",	// フォローから俺氏宛リプ
		"{id:4,rt:2,rt_rep:2,         }",	// フォローからフォロー宛リプ
		"{id:4,rt:2,rt_rep:4,         }",	// フォローからミュート宛リプ
		"{id:4,rt:2,rt_rep:5,         }",	// フォローからRT非表示宛リプ
		"{id:4,rt:2,rt_rep:6,         }",	// フォローからブロック宛リプ
		"{id:4,rt:2,rt_rep:8,         }",	// フォローから他人宛リプ
		"{id:4,rt:4,rt_rep:1,         }",	// ミュートから俺氏宛リプ
		"{id:4,rt:4,rt_rep:2,         }",	// ミュートからフォロー宛リプ
		"{id:4,rt:4,rt_rep:4,         }",	// ミュートからミュート宛リプ
		"{id:4,rt:4,rt_rep:5,         }",	// ミュートからRT非表示宛リプ
		"{id:4,rt:4,rt_rep:6,         }",	// ミュートからブロック宛リプ
		"{id:4,rt:4,rt_rep:8,         }",	// ミュートから他人宛リプ
		"{id:4,rt:5,rt_rep:1,         }",	// RT非表示から俺氏宛リプ
		"{id:4,rt:5,rt_rep:2,         }",	// RT非表示からフォロー宛リプ
		"{id:4,rt:5,rt_rep:4,         }",	// RT非表示からミュート宛リプ
		"{id:4,rt:5,rt_rep:5,         }",	// RT非表示からRT非表示宛リプ
		"{id:4,rt:5,rt_rep:6,         }",	// RT非表示からブロック宛リプ
		"{id:4,rt:5,rt_rep:8,         }",	// RT非表示から他人宛リプ
		"{id:4,rt:6,rt_rep:1,         }",	// ブロックから俺氏宛リプ
		"{id:4,rt:6,rt_rep:2,         }",	// ブロックからフォロー宛リプ
		"{id:4,rt:6,rt_rep:4,         }",	// ブロックからブロック宛リプ
		"{id:4,rt:6,rt_rep:5,         }",	// ブロックからRT非表示宛リプ
		"{id:4,rt:6,rt_rep:6,         }",	// ブロックからブロック宛リプ
		"{id:4,rt:6,rt_rep:8,         }",	// ブロックから他人宛リプ
		"{id:4,rt:8,rt_rep:1,         }",	// 他人から俺氏宛リプ
		"{id:4,rt:8,rt_rep:2,         }",	// 他人からフォロー宛リプ
		"{id:4,rt:8,rt_rep:4,         }",	// 他人からブロック宛リプ
		"{id:4,rt:8,rt_rep:5,         }",	// 他人からRT非表示宛リプ
		"{id:4,rt:8,rt_rep:6,         }",	// 他人からブロック宛リプ
		"{id:4,rt:8,rt_rep:8,         }",	// 他人から他人宛リプ
		// 他人がリツイート
		"{id:8,rt:1,rt_rep:1,home,filt}",	// 俺氏から俺氏宛リプ
		"{id:8,rt:1,rt_rep:2,home,filt}",	// 俺氏からフォロー宛リプ
		"{id:8,rt:1,rt_rep:4,home,filt}",	// 俺氏からミュート宛リプ
		"{id:8,rt:1,rt_rep:5,home,filt}",	// 俺氏からRT非表示宛リプ
		"{id:8,rt:1,rt_rep:6,home,filt}",	// 俺氏からブロック宛リプ
		"{id:8,rt:1,rt_rep:8,home,filt}",	// 俺氏から他人宛リプ
		"{id:8,rt:2,rt_rep:1,home,filt}",	// フォローから俺氏宛リプ
		"{id:8,rt:2,rt_rep:2,     filt}",	// フォローからフォロー宛リプ
		"{id:8,rt:2,rt_rep:4,         }",	// フォローからミュート宛リプ
		"{id:8,rt:2,rt_rep:5,     filt}",	// フォローからRT非表示宛リプ
		"{id:8,rt:2,rt_rep:6,         }",	// フォローからブロック宛リプ
		"{id:8,rt:2,rt_rep:8,     filt}",	// フォローから他人宛リプ
		"{id:8,rt:4,rt_rep:1,home,filt}",	// ミュートから俺氏宛リプ
		"{id:8,rt:4,rt_rep:2,         }",	// ミュートからフォロー宛リプ
		"{id:8,rt:4,rt_rep:4,         }",	// ミュートからミュート宛リプ
		"{id:8,rt:4,rt_rep:5,         }",	// ミュートからRT非表示宛リプ
		"{id:8,rt:4,rt_rep:6,         }",	// ミュートからブロック宛リプ
		"{id:8,rt:4,rt_rep:8,         }",	// ミュートから他人宛リプ
		"{id:8,rt:5,rt_rep:1,home,filt}",	// RT非表示から俺氏宛リプ
		"{id:8,rt:5,rt_rep:2,     filt}",	// RT非表示からフォロー宛リプ
		"{id:8,rt:5,rt_rep:4,         }",	// RT非表示からミュート宛リプ
		"{id:8,rt:5,rt_rep:5,     filt}",	// RT非表示からRT非表示宛リプ
		"{id:8,rt:5,rt_rep:6,         }",	// RT非表示からブロック宛リプ
		"{id:8,rt:5,rt_rep:8,     filt}",	// RT非表示から他人宛リプ
		"{id:8,rt:6,rt_rep:1,         }",	// ブロックから俺氏宛リプ
		"{id:8,rt:6,rt_rep:2,         }",	// ブロックからフォロー宛リプ
		"{id:8,rt:6,rt_rep:4,         }",	// ブロックからブロック宛リプ
		"{id:8,rt:6,rt_rep:5,         }",	// ブロックからRT非表示宛リプ
		"{id:8,rt:6,rt_rep:6,         }",	// ブロックからブロック宛リプ
		"{id:8,rt:6,rt_rep:8,         }",	// ブロックから他人宛リプ
		"{id:8,rt:8,rt_rep:1,home,filt}",	// 他人から俺氏宛リプ
		"{id:8,rt:8,rt_rep:2,     filt}",	// 他人からフォロー宛リプ
		"{id:8,rt:8,rt_rep:4,         }",	// 他人からブロック宛リプ
		"{id:8,rt:8,rt_rep:5,     filt}",	// 他人からRT非表示宛リプ
		"{id:8,rt:8,rt_rep:6,         }",	// 他人からブロック宛リプ
		"{id:8,rt:8,rt_rep:8,     filt}",	// 他人から他人宛リプ
	};
	int ntest = 0;
	int nfail = 0;
	for (const auto& input_sq : table) {
		auto input_str = input_sq;
		input_str = string_replace(input_str, " ", "");
		input_str = string_replace(input_str, "id:",	"\"id\":");
		input_str = string_replace(input_str, "reply:",	"\"reply\":");
		input_str = string_replace(input_str, "rt:",	"\"rt\":");
		input_str = string_replace(input_str, "rt_rep:","\"rt_rep\":");
		input_str = string_replace(input_str, "ment:",	"\"ment\":");
		input_str = string_replace(input_str, "home",	"\"home\":1");
		input_str = string_replace(input_str, "filt",	"\"filt\":1");
		input_str = string_replace(input_str, "h---",	"\"home\":-1");
		input_str = string_replace(input_str, "f---",	"\"filt\":-1");
		// 末尾カンマは許容しておいてここで消すほうが楽
		input_str = string_replace(input_str, ",}",		"}");

		Json input = Json::parse(input_str);
		if (input.is_null()) {
			printf("Json::parse(%s) failed\n", input_str.c_str());
			exit(1);
		}
		// 取り出しやすいよう、home, filt はなければ 0 をセットしておく。
		if (!input.contains("home"))
			input["home"] = 0;
		if (!input.contains("filt"))
			input["filt"] = 0;

		// それらから status をでっちあげる
		Json status;
		// user
		int id = input["id"].get<int>();
		auto id_str = std::to_string(id);
		Json user;
		user["id_str"] = id_str;
		user["screen_name"] = id_str;
		status["user"] = user;
		// in_reply_to_user_id_str
		if (input.contains("reply")) {
			int reply = input["reply"].get<int>();
			auto reply_str = std::to_string(reply);
			status["in_reply_to_user_id_str"] = reply_str;
			status["in_reply_to_screen_name"] = reply_str;
		}
		// retweeted_status.user.id_str
		if (input.contains("rt")) {
			Json rt;

			int rtid = input["rt"].get<int>();
			auto rtid_str = std::to_string(rtid);
			Json rtuser;
			rtuser["id_str"] = rtid_str;
			rtuser["screen_name"] = rtid_str;
			rt["user"] = rtuser;

			// retweeted_status.in_reply_to_user_id_str
			if (input.contains("rt_rep")) {
				int rtrep = input["rt_rep"].get<int>();
				auto rtrep_str = std::to_string(rtrep);
				rt["in_reply_to_user_id_str"] = rtrep_str;
				rt["in_reply_to_screen_name"] = rtrep_str;
			}

			status["retweeted_status"] = rt;
		}
		// entities.user_mentions[]
		if (input.contains("ment")) {
			int umid = input["ment"].get<int>();
			auto umid_str = std::to_string(umid);
			Json um;
			um["id_str"] = umid_str;
			um["screen_name"] = umid_str;
			um["indices"] = { 0, 2 };
			status["entities"]["user_mentions"] = { um };

			// display_text_range
			status["display_text_range"] = { 3, 5 };
		}

		// 期待値 (入力は 1=true, 0=false, -1 ならテストしない)
		std::optional<bool> expected_home;
		std::optional<bool> expected_filt;
		int expected_home_int = input["home"];
		int expected_filt_int = input["filt"];
		if (expected_home_int != -1)
			expected_home = (bool)expected_home_int;
		if (expected_filt_int != -1)
			expected_filt = (bool)expected_filt_int;

		if (diagShow >= 1) {
			// 歴史的経緯により Diag は stderr、テスト結果は stdout に出るので
			// おそらく |& とかして表示しないといけないことになる。
			fprintf(stderr, "%s\n", input_str.c_str());
		}

		if (expected_home) {
			// テスト (home)
			ntest++;
			opt_pseudo_home = true;
			auto result = acl(status, false);
			if (result != expected_home.value()) {
				fprintf(stderr, "%s (for home) expects %d but %d\n",
					input_str.c_str(), expected_home.value(), result);
				nfail++;
			}
		}

		if (expected_filt) {
			// テスト (home/quoted)
			ntest++;
			opt_pseudo_home = true;
			auto result = acl(status, true);
			if (result != expected_filt.value()) {
				fprintf(stderr, "%s (for home/quoted) expectes %d but %d\n",
					input_str.c_str(), expected_filt.value(), result);
				nfail++;
			}

			// テスト (filter)
			ntest++;
			opt_pseudo_home = false;
			result = acl(status, false);
			if (result != expected_filt.value()) {
				fprintf(stderr, "%s (for filter) expectes %d but %d\n",
					input_str.c_str(), expected_filt.value(), result);
				nfail++;
			}

			// テスト (filter)
			ntest++;
			opt_pseudo_home = false;
			result = acl(status, true);
			if (result != expected_filt.value()) {
				fprintf(stderr,
					"%s (for filter/quoted) expectes %d but %d\n",
					input_str.c_str(), expected_filt.value(), result);
				nfail++;
			}
		}
	}
	printf("%d tests, %d passed", ntest, ntest - nfail);
	if (nfail > 0) {
		printf(", %d FAILED!", nfail);
	}
	printf("\n");
}

void
test_acl()
{
	test_showstatus_acl();
}
#endif // SELFTEST
