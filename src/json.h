#pragma once
#include "json.h"

// Copied from: https://github.com/nlohmann/json/blob/e906933f39389f68df8b0e312b2daf0e069d6cfc/src/json.hpp.re2c

namespace json {
	typedef std::string string_t;

	/*!
	@brief calculates the extra space to escape a JSON string

	@param[in] s  the string to escape
	@return the number of characters required to escape string @a s

	@complexity Linear in the length of string @a s.
	*/
	static std::size_t extra_space(const string_t& s) noexcept
	{
		return std::accumulate(s.begin(), s.end(), size_t{},
			[](size_t res, typename string_t::value_type c)
		{
			switch (c)
			{
			case '"':
			case '\\':
			case '\b':
			case '\f':
			case '\n':
			case '\r':
			case '\t':
			{
				// from c (1 byte) to \x (2 bytes)
				return res + 1;
			}

			default:
			{
				if (c >= 0x00 and c <= 0x1f)
				{
					// from c (1 byte) to \uxxxx (6 bytes)
					return res + 5;
				}
				else
				{
					return res;
				}
			}
			}
		});
	}

	/*!
	@brief escape a string

	Escape a string by replacing certain special characters by a sequence of
	an escape character (backslash) and another character and other control
	characters by a sequence of "\u" followed by a four-digit hex
	representation.

	@param[in] s  the string to escape
	@return  the escaped string

	@complexity Linear in the length of string @a s.
	*/
	static string_t escape_string(const string_t& s)
	{
		const auto space = extra_space(s);
		if (space == 0)
		{
			return s;
		}

		// create a result string of necessary size
		string_t result(s.size() + space, '\\');
		std::size_t pos = 0;

		for (const auto& c : s)
		{
			switch (c)
			{
				// quotation mark (0x22)
			case '"':
			{
				result[pos + 1] = '"';
				pos += 2;
				break;
			}

			// reverse solidus (0x5c)
			case '\\':
			{
				// nothing to change
				pos += 2;
				break;
			}

			// backspace (0x08)
			case '\b':
			{
				result[pos + 1] = 'b';
				pos += 2;
				break;
			}

			// formfeed (0x0c)
			case '\f':
			{
				result[pos + 1] = 'f';
				pos += 2;
				break;
			}

			// newline (0x0a)
			case '\n':
			{
				result[pos + 1] = 'n';
				pos += 2;
				break;
			}

			// carriage return (0x0d)
			case '\r':
			{
				result[pos + 1] = 'r';
				pos += 2;
				break;
			}

			// horizontal tab (0x09)
			case '\t':
			{
				result[pos + 1] = 't';
				pos += 2;
				break;
			}

			default:
			{
				if (c >= 0x00 and c <= 0x1f)
				{
					// convert a number 0..15 to its hex representation
					// (0..f)
					static const char hexify[16] =
					{
						'0', '1', '2', '3', '4', '5', '6', '7',
						'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
					};

					// print character c as \uxxxx
					for (const char m :
					{ 'u', '0', '0', hexify[c >> 4], hexify[c & 0x0f]
					})
					{
						result[++pos] = m;
					}

					++pos;
				}
				else
				{
					// all other characters are added as-is
					result[pos++] = c;
				}
				break;
			}
			}
		}

		return result;
	}
}