#include <yaconfig.hpp>

#include <limits>



namespace {
namespace yacfg_parse {
namespace num {

	struct ParseResult {
		size_t parsedChars;
		unsigned base;
	};


	yacfg::int_t baseOf(
			std::string::const_iterator strBeg,
			std::string::const_iterator strEnd,
			yacfg::int_t* basePrefixLenDst
	) {
		*basePrefixLenDst = 0;
		if(strBeg == strEnd) return 10;
		if(*strBeg == '-') ++strBeg;
		if(*strBeg == '0') {
			++strBeg;
			if(strBeg == strEnd) {
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


	constexpr yacfg::int_t charToDigit(char c) {
		// Charset assumptions are already made in yaconfig_util.cpp
		if(c >= '0' && c <= '9') return c - '0';
		if(c >= 'a' && c <= 'z') return (c - 'a') + 0xa;
		if(c >= 'A' && c <= 'Z') return (c - 'A') + 0xa;
		return std::numeric_limits<yacfg::int_t>::max();
	}

	constexpr char digitToChar(yacfg::int_t digit) {
		// Charset assumptions are already made in yaconfig_util.cpp
		constexpr bool useCapitalLetters = false;
		assert(digit >= 0);
		if(digit <= 9) return digit + '0';
		if constexpr(useCapitalLetters) {
			if(digit >= 0xa && digit <= 0xf) return (digit + 0xa) - 'f';
		} else {
			if(digit >= 0xa && digit <= 0xf) return (digit + 0xa) - 'F';
		}
		return '\0';
	}


	size_t parseNumberInt(
			std::string::const_iterator strBeg,
			std::string::const_iterator strEnd,
			yacfg::int_t base,
			yacfg::int_t* dst
	) {
		size_t charsParsed = 0;
		*dst = 0;
		if(strBeg != strEnd) {
			yacfg::int_t signMul = 1;
			if(*strBeg == '-') {
				charsParsed = 1;
				++strBeg;
				signMul = -1;
			}
			do {
				auto digit = charToDigit(*strBeg);
				if(digit >= base) {
					break;
				}
				*dst = (*dst * base) + digit;
				++charsParsed;
				++strBeg;
			} while(strBeg != strEnd);
			*dst *= signMul;
		}
		return charsParsed;
	}


	size_t parseNumberFrc(
			std::string::const_iterator strBeg,
			std::string::const_iterator strEnd,
			yacfg::float_t base,
			yacfg::float_t* dst
	) {
		size_t charsParsed = 0;
		yacfg::float_t magnitude = yacfg::float_t(1) / base;
		*dst = 0;
		while(strBeg != strEnd) {
			auto digit = charToDigit(*strBeg);
			if(digit >= base) {
				break;
			}
			*dst += digit * magnitude;
			magnitude /= base;
			++charsParsed;
			++strBeg;
		}
		return charsParsed;
	}


	ParseResult parseNumber(
			std::string::const_iterator strBeg,
			std::string::const_iterator strEnd,
			yacfg::RawData* dst
	) {
		yacfg::int_t intPart;
		std::string::const_iterator strCursor = strBeg;

		yacfg::int_t base;
		{
			yacfg::int_t basePrefixLen;
			base = baseOf(strCursor, strEnd, &basePrefixLen);
			strCursor += basePrefixLen;
		}
		size_t parsedChars = parseNumberInt(strCursor, strEnd, base, &intPart);

		strCursor += parsedChars;
		if(*strCursor != '.') {
			*dst = yacfg::RawData(intPart);
		} else {
			yacfg::float_t frcPart;
			++strCursor;
			strCursor += parseNumberFrc(strCursor, strEnd, base, &frcPart);
			*dst = yacfg::RawData(yacfg::float_t(intPart) + frcPart);
		}
		return {
			size_t(std::distance(strBeg, strCursor)),
			unsigned(base)
		};
	}


	std::string serializeIntNumber(
			yacfg::int_t n
	) {
		constexpr yacfg::int_t base = 10;
		constexpr yacfg::int_t digitsHeuristic = 10;
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


	std::string serializeFloatNumber(
			yacfg::float_t n
	) {
		constexpr yacfg::float_t base = 10;
		yacfg::int_t nInt = n;
		std::string r = serializeIntNumber(nInt);
		n -= nInt;
		r.push_back('.');
		if(n == 0) {
			r.push_back('0');
			return r;
		}
		while(n != 0) {
			n *= base;
			nInt = n;
			assert(nInt < base);
			r.push_back(digitToChar(nInt));
			n -= nInt;
		}
		return r;
	}

}
}
}
