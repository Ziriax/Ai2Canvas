// IndentableStream.cpp
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

#include "IllustratorSDK.h"
#include "IndentableStream.h"

namespace CanvasExport
{
	IndentationBuffer::IndentationBuffer(std::streambuf* sbuf)
		: m_streamBuffer(sbuf)
		, m_indentationLevel(0)
		, m_shouldIndent(true)
	{
	}

	std::basic_streambuf<char>::int_type IndentationBuffer::overflow(const int_type c)
	{
		if (traits_type::eq_int_type(c, traits_type::eof()))
			return m_streamBuffer->sputc(char(c));

		if (m_shouldIndent)
		{
			fill_n(std::ostreambuf_iterator<char>(m_streamBuffer), m_indentationLevel * 2, ' ');
			m_shouldIndent = false;
		}

		if (traits_type::eq_int_type(m_streamBuffer->sputc(char(c)), traits_type::eof()))
			return traits_type::eof();

		if (traits_type::eq_int_type(c, traits_type::to_char_type('\n')))
			m_shouldIndent = true;

		return traits_type::not_eof(c);
	}

}