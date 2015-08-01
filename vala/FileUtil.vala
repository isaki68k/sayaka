namespace ULib
{
	// .Net File.ReadAllText 相当品です。
	// エンコーディングはサポートしていません。
	public static string FileReadAllText(string filename) throws Error
	{
		var sb = new StringBuilder();

		var f = File.new_for_path(filename);
		var stream = new DataInputStream(f.read());
		string buf;
		while ((buf = stream.read_line()) != null) {
			sb.append(buf);
		}

		return sb.str;
	}

	// .Net File.WriteAllText 相当品です。
	public static void FileWriteAllText(string filename, string text)
		throws Error
	{
		var f = File.new_for_path(filename);
		var stream = new DataOutputStream(f.create(FileCreateFlags.PRIVATE));
		stream.put_string(text);
	}

}
