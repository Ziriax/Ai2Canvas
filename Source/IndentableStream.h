// IndentableStream.h
//
// Copyright (c) 2018- Peter Verswyvelen (http://github.com/Ziriax)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#ifndef INDENTABLESTREAM_H
#define INDENTABLESTREAM_H

#include <streambuf>
#include <ostream>

namespace CanvasExport
{
	class IndentationBuffer : public std::streambuf
	{
	public:
		IndentationBuffer(std::streambuf* sbuf);

		int indentationLevel() const { return m_indentationLevel; }

		void indent() { ++m_indentationLevel; }

		void undent() { m_indentationLevel = max(0, m_indentationLevel - 1); }

	protected:

		int_type overflow(int_type c) override;

		std::streambuf* m_streamBuffer;
		int m_indentationLevel;
		bool m_shouldIndent;
	};

	class IndentableStream : public std::ostream
	{
	public:
		explicit IndentableStream(std::ostream& os)
			: std::ostream(&m_indentationBuffer)
			, m_indentationBuffer(os.rdbuf())
			, m_itemsPerLine(1)
		{
		}

		IndentableStream& indent()
		{
			m_indentationBuffer.indent();
			return *this;
		}

		IndentableStream& undent()
		{
			m_indentationBuffer.undent();
			return *this;
		}

		friend std::ostream& itemsPerLine(std::ostream& out, size_t ipl)
		{
			const auto is = dynamic_cast<IndentableStream*>(&out);
			if (is)
			{
				is->m_itemsPerLine = ipl;
			}

			return out;
		}

		size_t itemsPerLine() const { return m_itemsPerLine; }

	private:
		IndentationBuffer m_indentationBuffer;
		size_t m_itemsPerLine;
	};

	inline std::ostream& indent(std::ostream& stream)
	{
		auto is = dynamic_cast<IndentableStream*>(&stream);
		return is ? is->indent() : stream;
	}

	inline std::ostream& undent(std::ostream& stream)
	{
		auto is = dynamic_cast<IndentableStream*>(&stream);
		return is ? is->undent() : stream;
	}

	class Indentation
	{
	private:
		std::ostream& os;

	public:
		Indentation(std::ostream& os) :os(os)
		{
			os << indent;
		}

		~Indentation()
		{
			os << undent;
		}
	};
}

#endif
