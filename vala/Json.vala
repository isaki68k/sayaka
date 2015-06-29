using System.Collections.Generic;

namespace ULib
{
	public errordomain JsonError
	{
		Argument,
		Invalid,
		Format,
	}

	public class Json
		: Object
	{
		public enum JsonType
		{
			/// <summary>
			/// この Json オブジェクトの値が null であることを示します。
			/// Value の状態は不定です。
			/// </summary>
			Null,

			/// <summary>
			/// この Json オブジェクトの値が true または false であることを示します。
			/// (bool)Value が値です。
			/// </summary>
			Bool,

			/// <summary>
			/// この Json オブジェクトが string 型であることを示します。
			/// (string)Value はエスケープなどをデコードした後の文字列です。
			/// </summary>
			String,

			/// <summary>
			/// この Json オブジェクトが Number 型であることを示します。
			/// (string)Value は数値を文字列状態のまま保持しています。
			/// どの数値型(int か double か独自型かなど)として解釈するかは呼び出し側の問題です。
			/// </summary>
			Number,

			/// <summary>
			/// この Json オブジェクトが Array 型であることを示します。
			/// Value は Array＜Json＞ です。
			/// </summary>
			Array,

			/// <summary>
			/// この Json オブジェクトが Object 型であることを示します。
			/// Value は Dictionary＜string, Json＞ です。
			/// </summary>
			Object,
		}

		/// <summary>
		/// この Json オブジェクトの型です。
		/// </summary>
		public JsonType Type { get; private set; }

		/// <summary>
		/// この Json オブジェクトの値です。
		/// </summary>
	//	public object Value { get; private set; }

		public bool ValueBool { get; private set; }
		// Number と String
		public string ValueString { get; private set; }

		public Array<Json> ValueArray {
			get { return ValueArray_; }
			private set { ValueArray_ = value; }
		}
		private Array<Json> ValueArray_;

		public Dictionary<string, Json> ValueDictionary
		{
			get { return ValueDictionary_; }
			private set { ValueDictionary_ = value; }
		}
		private Dictionary<string, Json> ValueDictionary_;

		/// <summary>
		/// 値が null の Json クラスの新しいインスタンスを初期化します。
		/// </summary>
		public Json()
		{
			this.Type = JsonType.Null;
		}

		/// <summary>
		/// src を使用して、Json クラスの新しいインスタンスを初期化します。
		/// </summary>
		/// <param name="src"></param>
		public Json.json(Json src)
		{
			this.Type = src.Type;
			switch (this.Type) {
			 case JsonType.Bool: this.ValueBool = src.ValueBool; break;
			 case JsonType.Number:	// FALLTHROUGH
			 case JsonType.String: this.ValueString = src.ValueString; break;
			 case JsonType.Array: this.ValueArray_ = src.ValueArray; break;
			 case JsonType.Object:
				this.ValueDictionary = src.ValueDictionary;
				break;
			 case JsonType.Null:
			 default:
				break;
			}
		}

		/// <summary>
		/// value を使用して値が true または false の Json クラスの新しいインスタンスを初期化します。
		/// </summary>
		/// <param name="value"></param>
		public Json.Bool(bool value)
		{
			Type = JsonType.Bool;
			ValueBool = value;
		}

		/// <summary>
		/// value を使用して Number 型の Json クラスの新しいインスタンスを初期化します。
		/// </summary>
		/// <param name="value"></param>
		public Json.Number(string value)
		{
			Type = JsonType.Number;
			ValueString = value;
		}

		/// <summary>
		/// value を使用して String 型の Json クラスの新しいインスタンスを初期化します。
		/// value は生の文字列を指定します。JSON デコードなどが必要な場合は Json.fromString() を使用してください。
		/// </summary>
		/// <param name="value"></param>
		public Json.String(string value)
		{
			Type = JsonType.String;
			ValueString = value;
		}

		/// <summary>
		/// value を使用して type 型の Json クラスの新しいインスタンスを初期化します。
		/// type は String か Number です。それ以外は例外になります。
		/// value は生の値を指定します。JSON デコードなどが必要な場合は Json.fromString() を使用してください。
		/// </summary>
		/// <param name="type"></param>
		/// <param name="value"></param>
		public Json.type(JsonType type, string value) throws JsonError
		{
			if (type == JsonType.String) {
				Type = type;
				ValueString = value;
			}
			else if (type == JsonType.Number) {
				Type = type;
				ValueString = value;
			}
			else {
				throw new JsonError.Argument("type");
			}
		}

		/// <summary>
		/// value を使用して Array 型の Json クラスの新しいインスタンスを初期化します。
		/// </summary>
		/// <param name="value"></param>
		public Json.Array(Array<Json> value)
		{
			Type = JsonType.Array;
			ValueArray_ = value;
		}

		/// <summary>
		/// value を使用して Object 型の Json クラスの新しいインスタンスを初期化します。
		/// </summary>
		/// <param name="value"></param>
		public Json.Object(Dictionary<string, Json> value)
		{
			Type = JsonType.Object;
			ValueDictionary = value;
		}

		/// <summary>
		/// この Json オブジェクトの値が null かどうかを返します。
		/// </summary>
		public bool IsNull { get { return (Type == JsonType.Null); } }

		/// <summary>
		/// この Json オブジェクトの値が true または false かどうかを返します。
		/// </summary>
		public bool IsBool { get { return (Type == JsonType.Bool); } }

		/// <summary>
		/// この Json オブジェクトの値が String 型かどうかを返します。
		/// </summary>
		public bool IsString { get { return (Type == JsonType.String); } }

		/// <summary>
		/// この Json オブジェクトの値が Number 型かどうかを返します。
		/// </summary>
		public bool IsNumber { get { return (Type == JsonType.Number); } }

		/// <summary>
		/// この Json オブジェクトの値が Array 型かどうかを返します。
		/// </summary>
		public bool IsArray { get { return (Type == JsonType.Array); } }

		/// <summary>
		/// この Json オブジェクトの値が Object 型かどうかを返します。
		/// </summary>
		public bool IsObject { get { return (Type == JsonType.Object); } }

		/// <summary>
		/// この Json オブジェクトの値を bool として取得します。
		/// JsonType が Bool でない場合の結果は不定です。
		/// </summary>
		public bool AsBool { get { return ValueBool; } }

		public string AsNumber { get { return ValueString; } }
		public double AsDouble { get { return double.parse(ValueString); } }
		public int64 AsInt64 { get { return int64.parse(ValueString); } }

		/// <summary>
		/// この Json オブジェクトの値を string として取得します。
		/// JsonType が String か Number でない場合の結果は不定です。
		/// </summary>
		public string AsString
		{
			get {
				switch (Type) {
				 case JsonType.String:
					return ValueString;
				 case JsonType.Number:
//					return ValueDouble.to_string();
				 default:
		//			throw new JsonError.Invalid("AsString type error");
					return "????";
				}
			}
		}

		/// <summary>
		/// この Json オブジェクトの値を Array として取得します。
		/// JsonType が Array でない場合の結果は不定です。
		/// </summary>
		public Array<Json> AsArray { get { return ValueArray; } }

		/// <summary>
		/// この Json オブジェクトの値を Object として取得します。
		/// JsonType が Object でない場合の結果は不定です。
		/// </summary>
		public Dictionary<string, Json> AsObject { get { return ValueDictionary; } }

		public bool Has(string member)
		{
			return IsObject && AsObject.ContainsKey(member);
		}

		public bool GetBool(string member, bool defval = false)
		{
			var json = GetJson(member);
			if (json != null && json.IsBool) {
				return json.AsBool;
			} 
			return defval;
		}

		public int GetInt(string member, int defval = 0)
		{
			var json = GetJson(member);
			if (json != null && json.IsNumber) {
				return (int)json.AsInt64;
			}
			return defval;
		}

		public int64 GetInt64(string member, int64 defval = 0)
		{
			var json = GetJson(member);
			if (json != null && json.IsNumber) {
				return json.AsInt64;
			}
			return defval;
		}

		public double GetDouble(string member, double defval = 0d)
		{
			var json = GetJson(member);
			if (json != null && json.IsNumber) {
				return json.AsDouble;
			}
			return defval;
		}

		public string GetString(string member, string defval = "")
		{
			var json = GetJson(member);
			if (json != null && json.IsString) {
				return json.AsString;
			}
			return defval;
		}

		public Array<Json>? GetArray(string member)
		{
			var json = GetJson(member);
			if (json != null && json.IsArray) {
				return json.AsArray;
			}
			return null;
		}

		public Dictionary<string, Json>? GetObject(string member)
		{
			var json = GetJson(member);
			if (json != null && json.IsObject) {
				return json.AsObject;
			}
			return null;
		}

		public Json? GetJson(string member)
		{
			if (IsObject) {
				// member がなければ null が返ってくる規約になっている。
				return AsObject[member];
			}
			return null;
		}

		public void Dump(Json json)
		{
			stderr.printf("json=%p\n", json);
			stderr.printf("ValueDictionary=%p\n", json.ValueDictionary);
			Dictionary<string, Json> obj = json.AsObject;
			stderr.printf("AsObject=%p\n", obj);
			var sb = new StringBuilder();
			sb.append("{");
			stderr.printf("start\n");
			for (int i = 0; i < obj.Count; i++) {
				if (i != 0) {
					sb.append(",");
				}
				var kv = obj.At(i);
				sb.append("\"" + kv.Key + "\"" + ":" + kv.Value.ToString());
			}
			sb.append("}");
			stderr.printf("Dump: type=%d %s\n", (int)json.Type, sb.str);
		}

		public void DumpDictionary(Dictionary<string, Json> obj)
		{
			var sb = new StringBuilder();
			sb.append("{");
			for (int i = 0; i < obj.Count; i++) {
				if (i != 0) {
					sb.append(",");
				}
				var kv = obj.At(i);
				sb.append("\"" + kv.Key + "\"" + ":" + kv.Value.ToString());
				sb.append("\n");
			}
			sb.append("}");
			stderr.printf("DumpDict: %s\n", sb.str);
		}


		/// <summary>
		/// JSON 文字列をデコードし、Json クラスの新しいインスタンスを返します。
		/// </summary>
		/// <param name="src"></param>
		public static Json FromString(string src) throws JsonError
		{
			JsonParser parser = new JsonParser();
			return parser.Parse(src);
		}

		public string ToString()
		{
			var sb = new StringBuilder();
			sb.append("this=%p\n".printf(this));
			if (IsNull) {
				sb.append("null");
			} else if (IsBool) {
				if (AsBool) {
					sb.append("true");
				} else {
					sb.append("false");
				}
			} else if (IsString) {
				sb.append(EncodeJsonString(AsString));
			} else if (IsNumber) {
				sb.append(AsNumber);
			} else if (IsArray) {
				Array<Json> ar = ValueArray;
				sb.append("[");
				for (int i = 0; i < ar.length; i++) {
					if (i != 0) {
						sb.append(",");
					}
					sb.append(ar.data[i].ToString());
				}
				sb.append("]");
			} else if (IsObject) {
				Dictionary<string, Json> obj = ValueDictionary;
				sb.append("obj=%p\n".printf(obj));
				sb.append("{");
				for (int i = 0; i < obj.Count; i++) {
					if (i != 0) {
						sb.append(",");
					}
					var kv = obj.At(i);
					sb.append("\"" + kv.Key + "\"" + ":" + kv.Value.ToString());
				}
				sb.append("}");
			} else {
				sb.append("<invalid object?> type=%d".printf((int)Type));
			}
			return sb.str;
		}

		/// <summary>
		/// src を JSON の String としてエンコード(エスケープ)します。
		/// 開始、終了のダブルクォートも付加します。
		///
		/// JSON に既定のない制御コードと 0x7F(DEL) は \uXXXX 形式にエスケープしますが、
		/// 0x80 以上の文字はそのまま出力します。
		/// </summary>
		/// <param name="src"></param>
		/// <returns></returns>
		public static string EncodeJsonString(string src)
		{
			StringBuilder sb = new StringBuilder();
			unichar c;
			for (int i = 0; src.get_next_char(ref i, out c);) {
				switch (c) {
					case '\"':
						sb.append("\\\"");
						break;
					case '\\':
						sb.append("""\\""");
						break;
					case '/':
						sb.append("""\/""");
						break;
					case '\b':
						sb.append("""\b""");
						break;
					case '\f':
						sb.append("""\f""");
						break;
					case '\n':
						sb.append("""\n""");
						break;
					case '\r':
						sb.append("""\r""");
						break;
					case '\t':
						sb.append("""\t""");
						break;
					default:
						if (c < 0x20 || c == 0x7f) {
							sb.append("""\u""");
							sb.append("%04X".printf(c));
						} else {
							sb.append_unichar(c);
						}
						break;
				}
			}
			return "\"" + sb.str + "\"";
		}
	}

	/// <summary>
	/// JSON をパース(デコード)します。
	/// </summary>
	public class JsonParser
	{
		private string Src;
		public int Pos { get; private set; }

		/// <summary>
		/// 現在の Pos を含めたメッセージを返します。
		/// </summary>
		/// <param name="fmt"></param>
		/// <param name="args"></param>
		/// <returns></returns>
		public string ErrorMsg(string fmt, ...)
		{
			va_list va = va_list();
			string msg = fmt.vprintf(va);
			return "%s at position %d".printf(msg, Pos);
		}

		private unichar GetChar() throws JsonError
		{
			if (Pos >= Src.length)
				throw new JsonError.Format(ErrorMsg("unexpected EOS"));
			return Src.get_char(Pos++);
		}

		private void UnGetChar()
		{
			Pos--;
		}

		/// <summary>
		/// JSON で定義されている White Space を読み飛ばして、次の1文字を返します。
		/// </summary>
		/// <returns></returns>
		private unichar GetCharSkipSpace() throws JsonError
		{
			while (true) {
				unichar c = GetChar();
				if ((c == ' ' || c == '\t' || c == '\r' || c == '\n') == false)
					return c;
			}
		}

		/// <summary>
		/// 現在位置から len 文字取り出して返します。
		/// </summary>
		/// <param name="len"></param>
		/// <returns></returns>
		private string GetString(int len) throws JsonError
		{
			string rv;
			if (Pos + len > Src.length) {
				throw new JsonError.Format(ErrorMsg("unexpected EOS in literal"));
			}
			rv = Src.substring(Pos, len);
			// 取得した文字数分だけ進む
			Pos += len;
			return rv;
		}

		/// <summary>
		/// JSON 文字列 src をデコード(パース)して Json を返します。
		/// 途中で文法エラーなどが起きると例外を発生します。
		/// 1つの Json オブジェクト(インスタンス)で表現できるだけ読み取ったところで終了し、それ以降については関知しません。
		/// </summary>
		/// <param name="src"></param>
		/// <returns></returns>
		public Json Parse(string src) throws JsonError
		{
			if (src == null) throw new JsonError.Argument("src");
			if (src == "") throw new JsonError.Argument("src is empty");

			Src = src;
			Pos = 0;
			return ParseValue();
		}

		/// <summary>
		/// Value をパースして Json を返します。
		/// 書式は RFC7159 準拠のはずです。
		/// </summary>
		/// <returns></returns>
		private Json ParseValue() throws JsonError
		{
			TRACE("ParseValue");
			unichar c = GetCharSkipSpace();

			if (c == '{') {
				return ParseObject();
			}
			if (c == '[') {
				return ParseArray();
			}
			if (c == '\"') {
				return ParseString();
			}
			if (c == '-' || ('0' <= c && c <= '9')) {
				UnGetChar();
				return ParseNumber();
			}
			if (c == 't') {
				return ParseLiteral(c, "true", new Json.Bool(true));
			}
			if (c == 'f') {
				return ParseLiteral(c, "false", new Json.Bool(false));
			}
			if (c == 'n') {
				return ParseLiteral(c, "null", new Json());
			}
			throw new JsonError.Format(ErrorMsg("Syntax error in Value"));
		}

		/// <summary>
		/// true, false, null のパース用下請け関数です。
		/// </summary>
		/// <param name="firstchar"></param>
		/// <param name="compare"></param>
		/// <param name="retval"></param>
		/// <returns></returns>
		private Json ParseLiteral(unichar firstchar, string compare, Json retval) throws JsonError
		{
			TRACE("ParseLiteral");
			if (firstchar.to_string() + GetString(compare.length - 1) == compare) {
				return retval;
			} else {
				throw new JsonError.Format(ErrorMsg("Syntax error in literal"));
			}
		}

		/// <summary>
		/// Object をパースして Json を返します。
		/// 書式は RFC7159 準拠のはずです。
		/// キーが重複した場合は、最後のものだけが採用されます。
		/// </summary>
		/// <returns></returns>
		private Json ParseObject() throws JsonError
		{
			TRACE("ParseObject");
			Dictionary<string, Json> dict = new Dictionary<string, Json>();

			// 空オブジェクトだけ先に判定
			if (GetCharSkipSpace() == '}') {
				TRACE("ParseObject=EMPTY");
				return new Json.Object(dict);
			} else {
				UnGetChar();
			}

			for (; ; ) {
				unichar c;

				// キー
				c = GetCharSkipSpace();
				if (c != '"') throw new JsonError.Format(ErrorMsg("Syntax error in Object key"));
				string key = ParseString().AsString;

				// 続いて ':'
				c = GetCharSkipSpace();
				if (c != ':') throw new JsonError.Format(ErrorMsg("Syntax error in Object separator"));

				// 値
				Json value = ParseValue();

				// キーがすでに存在する場合は上書きする。
				// RFC7159 Section 4 によると、キーが重複した Object を受け取った側の動作は未定義なのだが一応
				// Many implementations report the last name/value pair only.
				// とか書いてあるので、上書きにしてみる。
				dict[key] = value;

				// カンマか閉じ括弧
				c = GetCharSkipSpace();
				if (c == ',') continue;
				if (c == '}') {
					TRACE("ParseObject=%s".printf(dict.DumpString()));
					return new Json.Object(dict);
				}
				throw new JsonError.Format(ErrorMsg("Syntax error in Object"));
			}
		}

		/// <summary>
		/// Array をパースして Json を返します。
		/// 書式は RFC7159 準拠のはずです。
		/// </summary>
		/// <returns></returns>
		private Json ParseArray() throws JsonError
		{
			TRACE("ParseArray");
			Array<Json> list = new Array<Json>();

			if (GetCharSkipSpace() == ']') {
				return new Json.Array(list);
			} else {
				UnGetChar();
			}

			for (; ; ) {
				list.append_val(ParseValue());

				unichar c = GetCharSkipSpace();
				if (c == ',') continue;
				if (c == ']') return new Json.Array(list);
				throw new JsonError.Format(ErrorMsg("Syntax error in Array"));
			}
		}

		/// <summary>
		/// String 型をパースして Json を返します。
		/// String の開始符号である '"'(ダブルクォート) の次の文字を指した状態で呼び出してください。
		///
		/// 書式は RFC7159 におおむね準拠です。ただし文字列中への生制御コードの混入は許容しています。
		/// </summary>
		/// <returns></returns>
		private Json ParseString() throws JsonError
		{
			TRACE("ParseString");
			StringBuilder obj = new StringBuilder();
			bool escape = false;

			for (; ; ) {
				unichar c = GetChar();
				if (escape) {
					if (c == '"') {
						obj.append("\"");
					} else if (c == '\\') {
						obj.append("\\");
					} else if (c == '/') {
						obj.append("/");
					} else if (c == 'b') {
						obj.append("\b");
					} else if (c == 'f') {
						obj.append("\f");
					} else if (c == 'n') {
						obj.append("\n");
					} else if (c == 'r') {
						obj.append("\r");
					} else if (c == 't') {
						obj.append("\t");
					} else if (c == 'u') {
						unichar uni = '\0';
						try {
							string hex = GetString(4);
							hex.scanf("%04X", &uni);
						} catch {
							throw new JsonError.Format(ErrorMsg("Syntax error in String (invalid \\u format)"));
						}
						obj.append_unichar(uni);
					} else {
						throw new JsonError.Format(ErrorMsg("Syntax error in String (unknown escape sequence '\\{0}')", c));
					}
					escape = false;
				} else if (c == '\\') {
					escape = true;
				} else if (c == '\"') {
					TRACE("ParseString=%s".printf(obj.str));
					return new Json.String(obj.str);
				} else {
					// 本当は制御コードは弾かなければいけないが、許容している。
					obj.append_unichar(c);
				}
			}
			// unreachable
			//throw new JsonError.Format(ErrorMsg("Syntax error in String"));
		}

		/// <summary>
		/// Number 型をパースする際の状態です。
		/// </summary>
		public enum NumberStateKind
		{
			Begin = 0,	// 開始
			Minus,		// 仮数部の負号
			IntZero,	// 整数部のゼロ
			IntNum,		// 整数部
			Point,		// 小数点
			Frac,		// 小数部
			Exp,		// E
			ExpSign,	// 指数部の符号
			ExpNum,		// 指数部の値
			End,		// 終了
			Error,		// エラー
		}

		/// <summary>
		/// Number 型を構成する文字種です。
		/// </summary>
		public enum NumberCharKind
		{
			Zero = 0,	// '0'
			Digit,		// '1'-'9'
			Point,		// '.'
			Exp,		// 'e'/'E'
			Plus,		// '+'
			Minus,		// '-'
			Other,		//
			Terminate,	// Space / '}' / ']' / ',' / EOS
		}

		public ParseNumberHelper PNHDummy = new ParseNumberHelper();
		public class ParseNumberHelper
		{
			// 表のためのエイリアス
			private const NumberCharKind Ze = NumberCharKind.Zero;
			private const NumberCharKind Di = NumberCharKind.Digit;
			private const NumberCharKind Pt = NumberCharKind.Point;
			private const NumberCharKind Ex = NumberCharKind.Exp;
			private const NumberCharKind Pl = NumberCharKind.Plus;
			private const NumberCharKind Mi = NumberCharKind.Minus;
			private const NumberCharKind __ = NumberCharKind.Other;
			private const NumberCharKind Te = NumberCharKind.Terminate;

			// 文字から文字種への変換テーブル
			public static NumberCharKind[] charTrans = new NumberCharKind[] {
				// -1
				Te,
				// +01 +02 +03 +04 +05 +06 +07 +08 +09 +0A +0B +0C +0D +0E +0F
				__, __, __, __, __, __, __, __, __, Te, Te, __, __, Te, __, __,	// +00
				__, __, __, __, __, __, __, __, __, __, __, __, __, __, __, __,	// +10
				Te, __, __, __, __, __, __, __, __, __, __, Pl, Te, Mi, Pt, __,	// +20
				Ze, Di, Di, Di, Di, Di, Di, Di, Di, Di, __, __, __, __, __, __,	// +30
				__, __, __, __, __, Ex, __, __, __, __, __, __, __, __, __, __,	// +40
				__, __, __, __, __, __, __, __, __, __, __, __, __, Te, __, __,	// +50
				__, __, __, __, __, Ex, __, __, __, __, __, __, __, __, __, __,	// +60
				__, __, __, __, __, __, __, __, __, __, __, __, __, Te, __, __,	// +70
			};

			// 表のためのエイリアス
			private const NumberStateKind Begin_ = NumberStateKind.Begin;
			private const NumberStateKind Minus_ = NumberStateKind.Minus;
			private const NumberStateKind IntZer = NumberStateKind.IntZero;
			private const NumberStateKind IntNum = NumberStateKind.IntNum;
			private const NumberStateKind Point_ = NumberStateKind.Point;
			private const NumberStateKind Frac__ = NumberStateKind.Frac;
			private const NumberStateKind Exp___ = NumberStateKind.Exp;
			private const NumberStateKind ExpSig = NumberStateKind.ExpSign;
			private const NumberStateKind ExpNum = NumberStateKind.ExpNum;
			private const NumberStateKind End___ = NumberStateKind.End;
			private const NumberStateKind Error_ = NumberStateKind.Error;

			// 状態遷移表
			// たとえば0行0列目は state が Begin の時に文字 Zero が来ると IntZero に遷移する
			public static NumberStateKind[,] trans = new NumberStateKind[,] {
				//  Zero	Digit	Point	Exp 	Plus	Minus	Other	Terminate
				{ IntZer, IntNum, Error_, Error_, Error_, Minus_, Error_,	Error_ },	// Begin
				{ IntZer, IntNum, Error_, Error_, Error_, Error_, Error_,	Error_ },	// Minus
				{ Error_, Error_, Point_, Exp___, Error_, Error_, Error_,	End___ },	// IntZero
				{ IntNum, IntNum, Point_, Exp___, Error_, Error_, Error_,	End___ },	// IntNum
				{ Frac__, Frac__, Error_, Error_, Error_, Error_, Error_,	Error_ },	// Point
				{ Frac__, Frac__, Error_, Exp___, Error_, Error_, Error_,	End___ },	// Frac
				{ ExpNum, ExpNum, Error_, Error_, ExpSig, ExpSig, Error_,	Error_ },	// Exp
				{ ExpNum, ExpNum, Error_, Error_, Error_, Error_, Error_,	Error_ },	// ExpSign
				{ ExpNum, ExpNum, Error_, Error_, Error_, Error_, Error_,	End___ },	// ExpNum
			};
		}

		/// <summary>
		/// Number をパースして Json を返します。
		/// Number かどうかの判定をした上で Number 文字列の先頭を指した状態で呼び出してください。
		///
		/// 書式は RFC7159 準拠のはずです。
		/// </summary>
		/// <returns></returns>
		private Json ParseNumber() throws JsonError
		{
			TRACE("ParseNumber");

			StringBuilder sb = new StringBuilder();
			NumberCharKind type;
			NumberStateKind state = NumberStateKind.Begin;
			NumberStateKind nextstate;

			for (; ; ) {
				int c = -1;
				try {
					c = (int)GetChar();
				} catch {
					// nop
				}

				// 文字(c)から文字種(type)にマッピング
				if (c < 0x80) {
					type = ParseNumberHelper.charTrans[c + 1];
				} else {
					type = NumberCharKind.Other;
				}

				// 状態遷移テーブルを引く
				int s = (int)state;
				int t = (int)type;
				nextstate = ParseNumberHelper.trans[s, t];
				if (nextstate == NumberStateKind.Error) {
					// 遷移できないので文法エラー
					throw new JsonError.Format(ErrorMsg("Syntax error in Number (state={0}, type={1})",
						state, type));
				} else if (nextstate == NumberStateKind.End) {
					// 正常に終端したので...
					if (c != -1) {
						// 取り出しすぎた文字を戻して終了
						UnGetChar();
					}
					break;
				} else {
					// 状態遷移できたのでこの文字を追加
					sb.append_unichar((unichar)c);
					state = nextstate;
				}
			}
			TRACE("ParseNumber=%s".printf(sb.str));

			return new Json.Number(sb.str);
		}

		private void TRACE(string msg)
		{
			//stderr.printf("%s\n", msg);
		}
	}
}
