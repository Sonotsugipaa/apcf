#pragma once

#include <apcf.hpp>
#include <apcf_hierarchy.hpp>

#include <limits>
#include <cassert>
#include <string>
#include <map>
#include <set>



inline namespace apcf_constants {

	constexpr char GRAMMAR_NULL = '\0'; // Pragmatically equivalent to EOF in the code, but not a valid character to parse
	constexpr char GRAMMAR_KEY_SEPARATOR = '.';
	constexpr char GRAMMAR_STRING_DELIM = '"';
	constexpr char GRAMMAR_STRING_ESCAPE = '\\';
	constexpr char GRAMMAR_ASSIGN = '=';
	constexpr char GRAMMAR_NEWLINE = '\n'; // This only matters for single line comments
	constexpr char GRAMMAR_GROUP_BEGIN = '{';
	constexpr char GRAMMAR_GROUP_END = '}';
	constexpr char GRAMMAR_ARRAY_BEGIN = '[';
	constexpr char GRAMMAR_ARRAY_END = ']';
	constexpr char GRAMMAR_COMMENT_EXTREME = '/';
	constexpr char GRAMMAR_COMMENT_SL_MIDDLE = '/';
	constexpr char GRAMMAR_COMMENT_ML_MIDDLE = '*';

}



namespace apcf_num {

	struct ParseResult {
		size_t parsedChars;
		unsigned base;
	};


	apcf::int_t baseOf(
			std::string::const_iterator strBeg,
			std::string::const_iterator strEnd,
			apcf::int_t* basePrefixLenDst
	);


	constexpr apcf::int_t charToDigit(char c) {
		// Charset assumptions are already made in apcf_util.cpp
		if(c >= '0' && c <= '9') return c - '0';
		if(c >= 'a' && c <= 'z') return (c - 'a') + 0xa;
		if(c >= 'A' && c <= 'Z') return (c - 'A') + 0xa;
		return std::numeric_limits<apcf::int_t>::max();
	}

	constexpr char digitToChar(apcf::int_t digit) {
		// Charset assumptions are already made in apcf_util.cpp
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
			apcf::int_t base,
			apcf::int_t* dst
	);


	apcf::int_t baseOf(
			std::string::const_iterator strBeg,
			std::string::const_iterator strEnd,
			apcf::int_t* basePrefixLenDst
	);


	size_t parseNumberInt(
			std::string::const_iterator strBeg,
			std::string::const_iterator strEnd,
			apcf::int_t base,
			apcf::int_t* dst
	);


	size_t parseNumberFrc(
			std::string::const_iterator strBeg,
			std::string::const_iterator strEnd,
			apcf::float_t base,
			apcf::float_t* dst
	);


	ParseResult parseNumber(
			std::string::const_iterator strBeg,
			std::string::const_iterator strEnd,
			apcf::RawData* dst
	);


	std::string serializeIntNumber(apcf::int_t);
	std::string serializeFloatNumber(apcf::float_t);

}



namespace apcf_util {

	// Verify that charset assumptions are correct; if they aren't, there needs to be a better way to check character(heh)istics
	constexpr bool verifyCharSeq(const char* chars) {
		char last = *chars;
		if(last != '\0')
		while(*(++chars) != '\0') {
			if(*chars - last != 1) return false;
			last = *chars;
		}
		return true;
	};
	static_assert(verifyCharSeq("0123456789"));
	static_assert(verifyCharSeq("abcdefghijklmnopqrstuvwxyz"));
	static_assert(verifyCharSeq("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));

	constexpr bool isNumerical(char c) {
		return (c >= '0' && c <= '9');
	}

	constexpr bool isAlphanum(char c) {
		return
			(c >= '0' && c <= '9') ||
			(c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z');
	}

	constexpr bool isPlainChar(char c) {
		if(isNumerical(c) || isAlphanum(c)) return true;
		{
			constexpr const char* visibleNonAlphanumCharSet =
				"\\|!\"£$%&/()=?^'`[]{}+*@#,;.:-_<> ";
			const char* cursor = visibleNonAlphanumCharSet;
			char cmp;
			while((cmp = *cursor) != '\0') {
				if(c == cmp) return true;
				++cursor;
			}
		}
		return false;
	}

	constexpr bool isValidKeyChar(char c) {
		return isAlphanum(c) || (c == '_') || (c == '-') || (c == GRAMMAR_KEY_SEPARATOR);
	}

	constexpr char toLowerCase(char c) {
		if(c >= 'A' && c <= 'Z') return (c - 'A') + 'a';
	}


	std::string plainCharRep(char);

	size_t findKeyError(const std::string&);


	template<typename T>
	std::vector<T> setToVec(std::set<T> set) {
		std::vector<T> r;
		r.reserve(set.size());
		for(const auto& value : set) {
			r.push_back(value);
		}
		return r;
	};


	template<typename T>
	constexpr T setFlags(T src, bool value, T bits) {
		if(value) {
			return src | bits;
		} else {
			return src & (~bits);
		}
	}

	template<typename T>
	constexpr bool getFlags(T src, T bits) {
		return (src & bits) != 0;
	}

}



namespace apcf_parse {

	void fwd(apcf::io::Reader& reader, const std::string& expected);


	class StringReader : public apcf::io::Reader {
	public:
		std::span<const char, std::dynamic_extent> str;
		size_t cursor;
		size_t limit;

		StringReader() = default;

		StringReader(
				std::span<const char, std::dynamic_extent> str
		):
				str(str),
				cursor(0),
				limit(str.size())
		{ }

		bool isAtEof() const override {
			assert(cursor <= str.size());
			return cursor == limit;
		}

		char getChar() const override {
			assert(cursor <= str.size());
			char c = (cursor < limit)? str[cursor] : GRAMMAR_NULL;
			return c;
		}

		bool fwdOrEof() override {
			if(cursor < limit) {
				{
					char c = str[cursor];
					if(c == GRAMMAR_NEWLINE) {
						++ lineCtr;
						linePos = 0;
					} else {
						++ linePos;
					}
				}
				++ cursor;
				return true;
			}
			return false;
		}
	};


	class StdStreamReader : public apcf::io::Reader {
	public:
		std::istream* str;
		size_t charsLeft;
		char current;

		StdStreamReader() = default;

		StdStreamReader(std::istream& str, size_t limit):
				str(&str),
				charsLeft(limit)
		{
			fwdOrEof();
		}

		StdStreamReader(std::istream& str):
				StdStreamReader(str, std::numeric_limits<size_t>::max())
		{ }

		bool isAtEof() const override {
			return charsLeft <= 0;
		}

		char getChar() const override {
			return current;
		}

		bool fwdOrEof() override {
			if(isAtEof()) {
				return false;
			} else {
				if(charsLeft > 0) {
					if(str->read(&current, 1)) {
						if(current == GRAMMAR_NEWLINE) {
							++ lineCtr;
							linePos = 0;
						} else {
							++ linePos;
						}
					} else {
						current = GRAMMAR_NULL;
						charsLeft = 0;
						return false;
					}
				}
			}
			-- charsLeft;
			return true;
		}
	};


	struct ParseData {
		apcf::Config cfg;
		apcf::io::Reader& src;
		std::vector<apcf::Key> keyStack;
	};


	constexpr bool isWhitespace(char c) {
		constexpr std::string_view whitespaces = " \t\v\n";
		for(const auto& cCmp : whitespaces) {
			if(c == cCmp)  return true;
		}
		return false;
	}


	/** Returns `true` iff one or more characters have been skipped. */
	bool skipWhitespaces(ParseData&);

	/** Returns `true` iff a comment has been skipped. */
	bool skipComment(ParseData&);

	/** Returns `true` iff one or more comments have been skipped. */
	inline bool skipComments(ParseData& pd) {
		bool r = skipComment(pd);
		do { /* NOP */ } while(skipComment(pd));
		return r;
	}

	inline void skipWhitespacesAndComments(ParseData& pd) {
		skipComments(pd);
		do { /* NOP */ } while(skipWhitespaces(pd) && skipComments(pd));
	}


	apcf::Key parseKey(ParseData&);


	apcf::RawData parseValue(ParseData&);
	apcf::RawData parseValueArray(ParseData&);
	apcf::RawData parseValueString(ParseData&);
	apcf::RawData parseValueNumber(ParseData&);
	apcf::RawData parseValueBool(ParseData&, char firstChar);

	/** Select which type of value to parse based on the first character:
		* - arrays begin with GRAMMAR_ARRAY_BEGIN;
		* - strings begin with GRAMMAR_STRING_DELIM;
		* - numbers begin with a decimal digit;
		* - boolean values begin with 'y', 'n', 't' or 'f'. */
	apcf::RawData parseValue(ParseData&);


	apcf::Config parse(ParseData&);

}



namespace apcf_serialize {

	enum LineFlags : uint_fast8_t {
		lineFlagsGroupEndBit   = 1,
		lineFlagsArrayEndBit   = 2,
		lineFlagsOwnEntryBit   = 4,
		lineFlagsGroupEntryBit = 8
	};
	using LineFlagBits = uint_fast8_t;


	class StringWriter : public apcf::io::Writer {
	public:
		std::string* dst;
		size_t cursor;

		#ifdef NDEBUG
			StringWriter() { }
		#else
			StringWriter(): dst(nullptr), cursor(0) { }
		#endif

		StringWriter(std::string* dst, size_t begin):
				dst(dst), cursor(begin)
		{ }

		void writeChar(char c) override {
			assert(dst != nullptr);
			dst->push_back(c);
			++cursor;
		}

		void writeChars(
				const char* begin,
				const char* end
		) override {
			assert(dst != nullptr);
			size_t distance = std::distance(begin, end);
			dst->insert(dst->size(), begin, distance);
			cursor += distance;
		}

		void writeChars(
				const std::string& str
		) override {
			assert(dst != nullptr);
			dst->insert(dst->size(), str.data(), str.size());
			cursor += str.size();
		}
	};


	class StdStreamWriter : public apcf::io::Writer {
	public:
		std::ostream* dst;

		#ifdef NDEBUG
			StdStreamWriter() { }
		#else
			StdStreamWriter(): dst(nullptr) { }
		#endif

		StdStreamWriter(std::ostream& dst):
				dst(&dst)
		{ }

		void writeChar(char c) override {
			assert(dst != nullptr);
			dst->write(&c, 1);
		}

		void writeChars(
				const char* begin,
				const char* end
		) override {
			assert(dst != nullptr);
			size_t distance = std::distance(begin, end);
			dst->write(begin, distance);
		}

		void writeChars(
				const std::string& str
		) override {
			assert(dst != nullptr);
			dst->write(str.data(), str.size());
		}
	};


	struct SerializationState {
		std::string indentation;
		unsigned indentationLevel;
	};


	struct SerializeData {
		apcf::io::Writer& dst;
		apcf::SerializationRules rules;
		SerializationState& state;
		LineFlagBits lastLineFlags;
	};


	std::string mkIndent(apcf::SerializationRules, size_t levels);

	void pushIndent(apcf::SerializationRules, SerializationState&);
	void popIndent(apcf::SerializationRules, SerializationState&);

	void serializeArray(
			apcf::SerializationRules rules, SerializationState state,
			const apcf::RawArray& data, std::string& dst
	);

	std::string serializeDataRecursive(
			const apcf::RawData& rawData,
			apcf::SerializationRules rules,
			SerializationState state
	);

	void serializeLineEntry(
			SerializeData& sd,
			const apcf::Key& key,
			const apcf::RawData& entryValue
	);

	void serializeLineGroupBeg(
			SerializeData& sd,
			const apcf::Key& key
	);

	void serializeLineGroupEnd(
			SerializeData& sd
	);


	struct SerializeHierarchyParams {
		SerializeData* sd;
		const std::map<apcf::Key, apcf::RawData>* map;
		const apcf::ConfigHierarchy* hierarchy;
	};

	void serializeHierarchy(
			SerializeHierarchyParams& state,
			const apcf::Key& key, const apcf::Key& parent
	);

	void serialize(SerializeData& sd, const std::map<apcf::Key, apcf::RawData>& hierarchyMap);

}
