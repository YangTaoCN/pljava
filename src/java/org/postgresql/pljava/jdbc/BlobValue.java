/*
 * This file contains software that has been made available under The Mozilla
 * Public License 1.1. Use and distribution hereof are subject to the
 * restrictions set forth therein.
 * 
 * Copyright (c) 2003 TADA AB - Taby Sweden All Rights Reserved
 */
package org.postgresql.pljava.jdbc;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.sql.Blob;
import java.sql.SQLException;

/**
 * @author Thomas Hallgren
 */
public class BlobValue extends InputStream implements Blob
{
	private final InputStream m_stream;
	private final long m_nBytes;
	private long m_streamPos;
	private long m_markPos;

	public BlobValue(byte[] bytes)
	{
		this(new ByteArrayInputStream(bytes), bytes.length);
	}

	public BlobValue(InputStream stream, long nBytes)
	{
		m_stream    = stream;
		m_nBytes    = nBytes;
		m_streamPos = 0L;
		m_markPos   = 0L;
	}

	//***************************************
	// Implementation of java.sql.Blob
	//***************************************
	public long length()
	{
		return m_nBytes;
	}

	public InputStream getBinaryStream()
	{
		return this;
	}

	public byte[] getBytes(long pos, int length)
	throws SQLException
	{
		if(pos < 0L || length < 0)
			throw new IllegalArgumentException();
		if(length == 0)
			return new byte[0];

		if(pos + length > m_nBytes)
			throw new SQLException("Attempt to read beyond end of Blob data");

		long skip = pos - m_streamPos;
		if(skip < 0)
			throw new SQLException("Cannot position Blob stream backwards");

		try
		{
			if(skip > 0)
				this.skip(skip);

			byte[] buf = new byte[length];
			this.read(buf);
			return buf;
		}
		catch(IOException e)
		{
			throw new SQLException("Error reading Blob data: " + e.getMessage());
		}
	}

	/**
	 * In Frameworx this method is not supported.
	 */
	public long position(byte[] pattern, long start)
	{
		throw new UnsupportedOperationException();
	}

	/**
	 * In Frameworx this method is not supported.
	 */
	public long position(Blob pattern, long start)
	{
		throw new UnsupportedOperationException();
	}

	//*************************************************************************
	// Implementation of java.sql.Blob JDK 1.4 methods
	//
	// Those method are intended to provide a channel to the underlying data
	// storage as an alternatvie to the setBinaryStream
	// on the preparedStatement and are not implemented by the BlobValue.
	//
	//*************************************************************************
	/**
	 * In this method is not supported by <code>BlobValue</code>
	 */
	public OutputStream setBinaryStream(long pos)
	{
		throw new UnsupportedOperationException();
	}

	/**
	 * In this method is not supported by <code>BlobValue</code>
	 */
	public int setBytes(long pos, byte[] bytes)
	{
		throw new UnsupportedOperationException();
	}

	/**
	 * In this method is not supported by <code>BlobValue</code>
	 */
	public int setBytes(long pos, byte[] bytes, int offset, int len)
	{
		throw new UnsupportedOperationException();
	}

	/**
	 * In this method is not supported by <code>BlobValue</code>
	 */
	public void truncate(long len)
	{
		throw new UnsupportedOperationException();
	}

	//***************************************
	// Implementation of java.io.InputStream
	//***************************************
	public int available()
	throws IOException
	{
		return m_stream.available();
	}

	public synchronized void mark(int readLimit)
	{
		m_stream.mark(readLimit);
		m_markPos = m_streamPos;
	}

	public boolean markSupported()
	{
		return m_stream.markSupported();
	}

	public synchronized int read()
	throws IOException
	{
		int rs = m_stream.read();
		m_streamPos++;
		return rs;
	}

	public synchronized int read(byte[] b)
	throws IOException
	{
		int rs = m_stream.read(b);
		m_streamPos += rs;
		return rs;
	}

	public synchronized int read(byte[] b, int off,  int len)
	throws IOException
	{
		int rs = m_stream.read(b, off, len);
		m_streamPos += rs;
		return rs;
	}

	public synchronized long skip(long nBytes)
	throws IOException
	{
		long skipped = m_stream.skip(nBytes);
		m_streamPos += skipped;
		return skipped;
	}

	public synchronized void reset()
	throws IOException
	{
		m_stream.reset();
		m_streamPos = m_markPos;
	}
}