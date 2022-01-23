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
			apcf::int_t* dst
	) {
		size_t charsParsed = 0;
		*dst = 0;
		if(strBeg != strEnd) {
			apcf::int_t signMul = 1;
			if(*strBeg == '-') {
				charsParsed = 1;
				++ strBeg;
				signMul = -1;
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
				*dst = (*dst * base) + digit;
				++ charsParsed;
				++ strBeg;
			} while(strBeg != strEnd);
			*dst *= signMul;
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

		apcf::int_t base;
		{
			apcf::int_t basePrefixLen;
			base = baseOf(strCursor, strEnd, &basePrefixLen);
			strCursor += basePrefixLen;
		}
		size_t parsedChars = parseNumberInt(strCursor, strEnd, base, &intPart);

		strCursor += parsedChars;
		if(strCursor == strEnd || *strCursor != '.') {
			*dst = apcf::RawData(intPart);
		} else {
			apcf::float_t frcPart;
			++ strCursor;
			if(strCursor != strEnd) {
				strCursor += parseNumberFrc(strCursor, strEnd, base, &frcPart);
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
		std::string r = serializeIntNumber(nInt);
		if(n < 0) {
			n = -n;
			nInt = n;
		}
		n -= nInt;
		r.push_back('.');
		do {
			n *= base;
			nInt = n;
			assert(nInt < base);
			r.push_back(digitToChar(nInt));
			n -= nInt;
		} while(n > 0);
		// if(nInt != 0) r.push_back(digitToChar(nInt));
		return r;
	}

}
