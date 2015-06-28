public class MyJsonObject
{
	public Json.Object JsonObject { get; private set; }

	public MyJsonObject(Json.Object json_object)
	{
		JsonObject = json_object;
	}

	public bool Has(string member)
	{
		return JsonObject.has_member(member);
	}

	public Json.Array GetArray(string member, Json.Array? defval = null)
	{
		if (JsonObject.has_member(member)) {
			return JsonObject.get_array_member(member);
		} else {
			return defval;
		}
	}

	public bool GetBool(string member, bool defval = false)
	{
		if (JsonObject.has_member(member)) {
			return JsonObject.get_boolean_member(member);
		} else {
			return defval;
		}
	}

	public int GetInt(string member, int defval = 0)
	{
		if (JsonObject.has_member(member)) {
			return (int)JsonObject.get_int_member(member);
		} else {
			return defval;
		}
	}

	public int64 GetInt64(string member, int64 defval = 0)
	{
		if (JsonObject.has_member(member)) {
			return JsonObject.get_int_member(member);
		} else {
			return defval;
		}
	}

	public string GetString(string member, string defval = "")
	{
		if (JsonObject.has_member(member)) {
			var tmp = JsonObject.get_string_member(member);
			return tmp;
		} else {
			return defval;
		}
	}

	public MyJsonObject GetObject(string member, MyJsonObject? defval = null)
	{
		if (JsonObject.has_member(member)) {
			var tmp = JsonObject.get_object_member(member);
			return new MyJsonObject(tmp);
		} else {
			return defval;
		}
	}
}

