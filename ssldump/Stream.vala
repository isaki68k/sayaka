
public interface IStream : Object
{
	// 読み込めたバイト数を返します。
	public abstract int Read(uint8[] buf);

	// 書き込めたバイト数を返します。
	public abstract int Write(uint8[] buf);

	public abstract bool EOF();
}

public class MemoryStream
	: Object, IStream
{
	private uint8[] mem;

	public int Position { get; private set; }
	public void SetPosition(int newPosition)
	{
		Position = newPosition;
	}
	
	public MemoryStream()
	{
		mem = new uint8[0];
		Position = 0;
	}

	// ----- Append

	// GIO InputStream から指定バイト数を読み込み、メモリの末尾に追加します。
	public void AppendFromInputStream(InputStream gioInputStream, int length)
		throws IOError
	{
		int append_position = mem.length;
		mem.resize(mem.length + length);
		gioInputStream.read(mem[append_position:mem.length]);
	}

	// buf をメモリの末尾に追加します。
	public void Append(uint8[] buf)
	{
		int append_position = mem.length;
		mem.resize(mem.length + buf.length);
		for (int i = 0; i < buf.length; i++) {
			mem[append_position++] = buf[i];
		}
	}

	public int Read(uint8[] buf)
	{
		int avail = mem.length - Position;
		int n = int.min(avail, buf.length);

		for (int i = 0; i < n; i++) {
			buf[i] = mem[Position++];
		}
		return n;
	}

	public int Write(uint8[] buf)
	{
		var avail = mem.length - Position;
		if (avail < buf.length) {
			mem.resize(mem.length + buf.length - avail);
		}

		int n = buf.length;

		for (int i = 0; i < n; i++) {
			buf[i] = mem[Position++];
		}
		return n;
	}

	public bool EOF()
	{
		return Position >= mem.length;
	}


	// ----- BE accessor

	public uint8 read_uint8()
	{
		uint8[] buf = new uint8[1];
		Read(buf);
		return buf[0];
	}

	public uint16 read_uint16()
	{
		uint8[] buf = new uint8[2];
		Read(buf);
		// big endian
		return buf[0] << 8 + buf[1];
	}

	public uint32 read_uint24()
	{
		uint8[] buf = new uint8[3];
		Read(buf);
		// big endian
		return buf[0] << 16 + buf[1] << 8 + buf[2];
	}

	public uint32 read_uint32()
	{
		uint8[] buf = new uint8[4];
		Read(buf);
		// big endian
		return buf[0] << 24 + buf[1] << 16 + buf[2] << 8 + buf[3];
	}

	public void write_uint8(uint8 v)
	{
		uint8[] buf = new uint8[1];
		buf[0] = v;
		Write(buf);
	}

	public void write_uint16(uint16 v)
	{
		uint8[] buf = new uint8[2];
		// big endian
		buf[0] = (uint8)(v >> 8);
		buf[1] = (uint8)v;
		Write(buf);
	}

	public void write_uint24(uint32 v)
	{
		uint8[] buf = new uint8[3];
		// big endian
		buf[0] = (uint8)(v >> 16);
		buf[1] = (uint8)(v >> 8);
		buf[2] = (uint8)v;
		Write(buf);
	}

	public void write_uint32(uint32 v)
	{
		uint8[] buf = new uint8[4];
		// big endian
		buf[0] = (uint8)(v >> 24);
		buf[1] = (uint8)(v >> 16);
		buf[2] = (uint8)(v >> 8);
		buf[3] = (uint8)v;
		Write(buf);
	}
}


// BIG ENDIAN でストリームアクセス
public class StreamAccessorBE
{
	private IStream Stream;

	public StreamAccessorBE(IStream stream)
	{
		Stream = stream;
	}

	public uint8 read_uint8()
	{
		uint8[] buf = new uint8[1];
		Stream.Read(buf);
		return buf[0];
	}

	public uint16 read_uint16()
	{
		uint8[] buf = new uint8[2];
		Stream.Read(buf);
		// big endian
		return buf[0] << 8 + buf[1];
	}

	public uint32 read_uint24()
	{
		uint8[] buf = new uint8[3];
		Stream.Read(buf);
		// big endian
		return buf[0] << 16 + buf[1] << 8 + buf[2];
	}

	public uint32 read_uint32()
	{
		uint8[] buf = new uint8[4];
		Stream.Read(buf);
		// big endian
		return buf[0] << 24 + buf[1] << 16 + buf[2] << 8 + buf[3];
	}

	public void write_uint8(uint8 v)
	{
		uint8[] buf = new uint8[1];
		buf[0] = v;
		Stream.Write(buf);
	}

	public void write_uint16(uint16 v)
	{
		uint8[] buf = new uint8[2];
		// big endian
		buf[0] = (uint8)(v >> 8);
		buf[1] = (uint8)v;
		Stream.Write(buf);
	}

	public void write_uint24(uint32 v)
	{
		uint8[] buf = new uint8[3];
		// big endian
		buf[0] = (uint8)(v >> 16);
		buf[1] = (uint8)(v >> 8);
		buf[2] = (uint8)v;
		Stream.Write(buf);
	}

	public void write_uint32(uint32 v)
	{
		uint8[] buf = new uint8[4];
		// big endian
		buf[0] = (uint8)(v >> 24);
		buf[1] = (uint8)(v >> 16);
		buf[2] = (uint8)(v >> 8);
		buf[3] = (uint8)v;
		Stream.Write(buf);
	}
}

