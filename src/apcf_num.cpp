#include "apcf_.hpp"

#include <limits>
#include <cassert>



namespace apcf_num {

	apcf::int_t baseOf(
			std::string::const_iterator strBeg,
			std::string::const_iterator strEnd,
			apcf::int_t* basePrefixLenDst
	) {
		*basePrefixLenDst = 0;
		if(strBeg == strEnd) return 10;
		if(*strBeg == '-' || *strBeg == '+') ++ strBeg;
		if(*strBeg == '0') {
			++ strBeg;
			if(
				(strBeg == strEnd) ||
				(*strBeg == '.')
			) {
				return 10;
			} else {
				*basePrefixLenDst += 2;
				switch(*strBeg) {
					case 'x': return 0x10;
					case 'b': return 0b10;
					case 'd': return 10;
					default:
						*basePrefixLenDst -= 1;
						[[fallthrough]];
					case 'o': return 010;
				}
			}
		} else {
			return 10;
		}
	}


	size_t parseNumberInt(
			std::string::const_iterator strBeg,
			std::string::const_iterator strEnd,
			apcf::int_t base,
			apcf::int_t* dstIntPart,
			bool* dstNeg
	) {
		size_t charsParsed = 0;
		*dstIntPart = 0;
		if(strBeg != strEnd) {
			*dstNeg = false;
			if(*strBeg == '-') {
				charsParsed = 1;
				++ strBeg;
				*dstNeg = true;
			} else
			if(*strBeg == '+') {
				charsParsed = 1;
				++ strBeg;
			}
			do {
				auto digit = charToDigit(*strBeg);
				if(digit >= base) {
					break;
				}
				*dstIntPart = (*dstIntPart * base) + digit;
				++ charsParsed;
				++ strBeg;
			} while(strBeg != strEnd);
			*dstIntPart *= *dstNeg? apcf::int_t(-1) : apcf::int_t(1);
		}
		return charsParsed;
	}


	size_t parseNumberFrc(
			std::string::const_iterator strBeg,
			std::string::const_iterator strEnd,
			apcf::float_t base,
			apcf::float_t* dst
	) {
		size_t charsParsed = 0;
		apcf::float_t magnitude = apcf::float_t(1) / base;
		*dst = 0;
		while(strBeg != strEnd) {
			auto digit = charToDigit(*strBeg);
			if(digit >= base) {
				break;
			}
			*dst += digit * magnitude;
			magnitude /= base;
			++ charsParsed;
			++ strBeg;
		}
		return charsParsed;
	}


	ParseResult parseNumber(
			std::string::const_iterator strBeg,
			std::string::const_iterator strEnd,
			apcf::RawData* dst
	) {
		apcf::int_t intPart;
		std::string::const_iterator strCursor = strBeg;
		bool neg;

		apcf::int_t base;
		{
			apcf::int_t basePrefixLen;
			base = baseOf(strCursor, strEnd, &basePrefixLen);
			strCursor += basePrefixLen;
		}
		size_t parsedChars = parseNumberInt(strCursor, strEnd, base, &intPart, &neg);

		strCursor += parsedChars;
		if(strCursor == strEnd || *strCursor != '.') {
			*dst = apcf::RawData(intPart);
		} else {
			apcf::float_t frcPart;
			++ strCursor;
			if(strCursor != strEnd) {
				strCursor += parseNumberFrc(strCursor, strEnd, base, &frcPart);
				frcPart *= (neg)? apcf::float_t(-1.0) : apcf::float_t(+1.0);
				*dst = apcf::RawData(apcf::float_t(intPart) + frcPart);
			} else {
				-- strCursor;
			}
		}
		return {
			size_t(std::distance(strBeg, strCursor)),
			unsigned(base)
		};
	}


	std::string serializeIntNumber(apcf::int_t n) {
		constexpr apcf::int_t base = 10;
		constexpr apcf::int_t digitsHeuristic = 10;
		std::string r;
		r.reserve(digitsHeuristic);
		if(n < 0) {
			r.push_back('-');
			n = -n;
		} else if(n == 0) {
			r.push_back('0');
			return r;
		}
		while(n > 0) {
			r.push_back(digitToChar(n % base));
			n /= base;
		}
		if(r.front() == '-') {
			std::reverse(r.begin()+1, r.end());
		} else {
			std::reverse(r.begin(), r.end());
		}
		return r;
	}


	std::string serializeFloatNumber(apcf::float_t n) {
		constexpr apcf::float_t base = 10;
		if(n > std::numeric_limits<apcf::int_t>::max()) n = std::numeric_limits<apcf::int_t>::max() / 2;
		if(n < std::numeric_limits<apcf::int_t>::min()) n = std::numeric_limits<apcf::int_t>::min() / 2;
		apcf::int_t nInt = n;
		std::string r;
		r.reserve(16);
		if(n < 0) {
			r.push_back('-');
			n = -n;
			nInt = n;
		}
		r.append(serializeIntNumber(nInt));
		n -= nInt;
		r.push_back('.');
		do {
			n *= base;
			nInt = n;
			assert(nInt < base);
			r.push_back(digitToChar(nInt));
			n -= nInt;
		} while(n > 0);
		return r;
	}


	bool roundFloatRep(std::string& str, unsigned digits) {
		apcf::int_t iterPos;
		apcf::int_t endPos = str.size();
		apcf::int_t trimPos;
		apcf::int_t prefixLen;

		apcf::int_t base = baseOf(str.begin(), str.end(), &prefixLen);
		char baseMaxChar = digitToChar(base-1);
		iterPos = prefixLen;

		// Find the first fractional digit
		while(iterPos < endPos && str[iterPos] != '.') ++ iterPos;
		if(iterPos >= endPos) return false; // Not a fractional number representation, abort
		assert(str[iterPos] == '.');
		++ iterPos;

		// Look for a 0 or a <baseMaxChar>;
		// The function may return inside this loop
		while(iterPos < endPos) {
			char repeatChar = str[iterPos];
			if(repeatChar == '0' || repeatChar == baseMaxChar) {
				// Look for <digits-1> repeated characters
				size_t repeatCharCount = 1;
				trimPos = iterPos;
				while(repeatCharCount < digits && iterPos != endPos && str[iterPos] == repeatChar) {
					++ repeatCharCount;
					++ iterPos;
				}
				assert(repeatCharCount <= digits);

				if(repeatCharCount >= digits) {
					// If <digits> repeated chars have been found, trim the string
					str.resize(trimPos);
					// If the rounded number is integer, append a zero
					assert(str.size() != 0); // Wouldn't make sense
					if(str.back() == '.') str.push_back('0');
					/* TODO: round up, if the char is <baseMaxChar> */
					return true;
				}
			}
			++ iterPos;
		}

		return false;
	}

}
