
// peek 操作可能な InputStream

public class PeekableInputStream
 : InputStream
{
	private InputStream target;
	private uint8[] peekbuffer;

	public PeekableInputStream(InputStream baseStream)
	{
		target = baseStream;
	}

	public override bool close(Cancellable? cancellable = null) throws IOError
	{
		target.close();
		return true;
	}

	public override ssize_t read(uint8[] buffer, Cancellable? cancellable = null) throws IOError
	{
		if (peekbuffer.length > 0 && peekbuffer.length < buffer.length) {
			var n = peekbuffer.length;
			Memory.copy(buffer, peekbuffer, peekbuffer.length);
			peekbuffer.length = 0;
			return n;
		}
		return (ssize_t)target.read(buffer);
	}

	public ssize_t peek(uint8[] buffer) throws IOError
	{
		peekbuffer = new uint8[buffer.length];
		var n = target.read(peekbuffer);
		Memory.copy(buffer, peekbuffer, buffer.length);
		return n;
	}
}

