#include <yaconfig.hpp>

#include <fstream>
#include <limits>



namespace {
namespace yacfg_parse {

	using namespace yacfg_util;


	class Source {
	public:
		size_t lineCounter;
		size_t linePosition;

		Source(): lineCounter(1), linePosition(1) { }

		virtual bool isAtEof() const = 0;

		/** Returns GRAMMAR_NULL if the cursor is at EOF;
			* this allows trivial checks at the beginning of functions
			* such as `skipWhitespaces(...)`. */
		virtual char getChar() const = 0;

		/** Returns `true` iff the source's cursor was moved forward. */
		virtual bool fwdOrEof() = 0;

		void fwd(const std::string& expected) {
			if(! fwdOrEof()) throw yacfg::UnexpectedEof(expected);
		}
	};


	class StringSource : public Source {
	public:
		std::span<const char, std::dynamic_extent> str;
		size_t cursor;
		size_t limit;

		StringSource() = default;

		StringSource(
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
						++lineCounter;
						linePosition = 1;
					} else {
						++ linePosition;
					}
				}
				++ cursor;
				return true;
			}
			return false;
		}
	};


	class FileSource : public Source {
	public:
		std::istream* str;
		size_t charsLeft;
		char current;

		FileSource() = default;

		FileSource(std::istream& str, size_t limit):
				str(&str),
				charsLeft(limit)
		{
			fwdOrEof();
		}

		FileSource(std::istream& str):
				FileSource(str, std::numeric_limits<size_t>::max())
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
							++lineCounter;
							linePosition = 1;
						} else {
							++ linePosition;
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
		yacfg::Config cfg;
		Source& src;
		std::vector<yacfg::Key> keyStack;
	};


	constexpr bool isWhitespace(char c) {
		constexpr std::string_view whitespaces = " \t\v\n";
		for(const auto& cCmp : whitespaces) {
			if(c == cCmp)  return true;
		}
		return false;
	}


	/** Returns `true` iff one or more characters have been skipped. */
	bool skipWhitespaces(ParseData& pd) {
		if(! isWhitespace(pd.src.getChar())) return false;
		do {
			// NOP
		} while(pd.src.fwdOrEof() && isWhitespace(pd.src.getChar()));
		return true;
	}


	/** Returns `true` iff a comment has been skipped. */
	bool skipComment(ParseData& pd) {
		using namespace std::string_literals;
		static const std::string secCharExpectStr =
			"either `"s + GRAMMAR_COMMENT_SL_MIDDLE +
			"` or `"s + GRAMMAR_COMMENT_ML_MIDDLE + "`"s;
		static const std::string commentEndExpectStr =
			"any character sequence ending with `"s +
			GRAMMAR_COMMENT_ML_MIDDLE + std::string(GRAMMAR_COMMENT_EXTREME, 1) +
			"`"s;
		if(pd.src.getChar() != GRAMMAR_COMMENT_EXTREME) return false;
		pd.src.fwd(secCharExpectStr);
		char secondChar = pd.src.getChar();
		if(secondChar == GRAMMAR_COMMENT_SL_MIDDLE) {
			do {
				// NOP
			} while((pd.src.fwdOrEof()) && (pd.src.getChar() != GRAMMAR_NEWLINE));
		} else
		if(secondChar == GRAMMAR_COMMENT_ML_MIDDLE) {
			char lastChar;
			char curChar = GRAMMAR_COMMENT_ML_MIDDLE + 1; // It doesn't matter, it just needs to differ from GRAMMAR_COMMENT_ML_MIDDLE
			do {
				pd.src.fwd(commentEndExpectStr);
				lastChar = curChar;
				curChar = pd.src.getChar();
			} while(! (
				(lastChar == GRAMMAR_COMMENT_ML_MIDDLE) &&
				(curChar == GRAMMAR_COMMENT_EXTREME)
			));
			pd.src.fwdOrEof();
		} else {
			throw yacfg::UnexpectedChar(
				pd.src.lineCounter, pd.src.linePosition,
				pd.src.getChar(), secCharExpectStr );
		}
		return true;
	}

	/** Returns `true` iff one or more comments have been skipped. */
	bool skipComments(ParseData& pd) {
		bool r = skipComment(pd);
		do { /* NOP */ } while(skipComment(pd));
		return r;
	}

	void skipWhitespacesAndComments(ParseData& pd) {
		skipComments(pd);
		do { /* NOP */ } while(skipWhitespaces(pd) && skipComments(pd));
	}


	/* Does not allow EOF at the end */ /**/
	yacfg::Key parseKey(ParseData& pd) {
		static const std::string& expectStr = "a key";
		std::string r;

		char c = pd.src.getChar();

		if(! isValidKeyChar(c)) {
			throw yacfg::UnexpectedChar(
				pd.src.lineCounter, pd.src.linePosition,
				c, expectStr );
		}

		do {
			r.push_back(c);
			pd.src.fwd(expectStr);
			c = pd.src.getChar();
		} while(isValidKeyChar(c));
		assert(yacfg::isKeyValid(r)); // All non-key characters count as terminators
		return yacfg::Key(r);
	}


	yacfg::RawData parseValue(ParseData&); // Fwd decl needed for `parseValueArray`

	yacfg::RawData parseValueArray(ParseData& pd) {
		static const std::string expectStr = "a list of space separated values";
		std::vector<yacfg::RawData> rVector;
		char curChar;

		pd.src.fwd(expectStr);
		skipWhitespacesAndComments(pd);

		curChar = pd.src.getChar();
		while(curChar != GRAMMAR_ARRAY_END) {
			rVector.push_back(parseValue(pd));
			skipWhitespacesAndComments(pd);
			curChar = pd.src.getChar();
		}
		pd.src.fwdOrEof();

		return yacfg::RawData::moveArray(rVector.data(), rVector.size());
	}


	yacfg::RawData parseValueString(ParseData& pd) {
		using namespace std::string_literals;
		static const std::string expectStr = "a string delimiter ("s + GRAMMAR_STRING_DELIM + ")"s;
		std::string r;
		pd.src.fwd(expectStr);
		char prv;
		char cur = pd.src.getChar();
		do {
			prv = cur;
			r.push_back(cur);
			pd.src.fwd(expectStr);
			cur = pd.src.getChar();
		} while(! (
			(cur == GRAMMAR_STRING_DELIM) && (prv != GRAMMAR_STRING_ESCAPE)
		));
		pd.src.fwdOrEof();
		return yacfg::RawData(std::move(r));
	}


	yacfg::RawData parseValueNumber(ParseData& pd, char begChar) {
		static const std::string expectStr = "a numerical value";
		std::string buffer;
		yacfg::RawData r;

		{// Read the entire number-like string
			char c = begChar;
			if(c == '-' || c == '+') {
				buffer.push_back(c);
				pd.src.fwd(expectStr);
				c = pd.src.getChar();
			}
			while(
				isAlphanum(c) || (c == '.')
			) {
				buffer.push_back(c);
				pd.src.fwdOrEof();
				c = pd.src.getChar();
			}
		} { // Parse it
			auto result = num::parseNumber(buffer.begin(), buffer.end(), &r);
			assert(result.parsedChars <= buffer.size());
			if(
				(! buffer.empty()) &&
				(result.parsedChars != buffer.size())
			) {
				throw yacfg::UnexpectedChar(pd.src.lineCounter, pd.src.linePosition,
					buffer[result.parsedChars],
					"a sequence of base " + std::to_string(result.base) + " digits" );
			}
		}
		return r;
	}


	yacfg::RawData parseValueBool(ParseData& pd, char begChar) {
		static const std::string& expectStr = "a boolean value (true/false, yes/no, y/n)";
		auto expect = [&pd](char expected) {
			if(pd.src.getChar() != expected) {
				throw yacfg::UnexpectedChar(pd.src.lineCounter, pd.src.linePosition,
					pd.src.getChar(), expectStr );
			}
			pd.src.fwd(expectStr);
		};
		auto expectOpt = [&pd](char expected) {
			char curChar = pd.src.getChar();
			if(isAlphanum(curChar)) {
				pd.src.fwd(expectStr);
				if(curChar != expected) {
					throw yacfg::UnexpectedChar(pd.src.lineCounter, pd.src.linePosition,
						curChar, expectStr );
				}
				return true;
			} else {
				pd.src.fwdOrEof();
				return false;
			}
		};
		pd.src.fwd(expectStr);
		switch(begChar) {
			case 't': {
				expect('r');
				expect('u');
				expect('e');
				return true;
			} break;
			case 'f': {
				expect('a');
				expect('l');
				expect('s');
				expect('e');
				return false;
			} break;
			case 'y': {
				if(expectOpt('e')) {
					expect('s');
				}
				return true;
			} break;
			case 'n': {
				expectOpt('o');
				return false;
			} break;
			default: {
				throw yacfg::UnexpectedChar(pd.src.lineCounter, pd.src.linePosition,
					pd.src.getChar(), expectStr );
			} break;
		}
	}


	/** Select which type of value to parse based on the first character:
		* - arrays begin with GRAMMAR_ARRAY_BEGIN;
		* - strings begin with GRAMMAR_STRING_DELIM;
		* - numbers begin with a decimal digit;
		* - boolean values begin with 'y', 'n', 't' or 'f'. */
	yacfg::RawData parseValue(ParseData& pd) {
		char begChar = pd.src.getChar();
		if(begChar == GRAMMAR_ARRAY_BEGIN) {
			return parseValueArray(pd);
		} else
		if(begChar == GRAMMAR_STRING_DELIM) {
			return parseValueString(pd);
		} else
		if(isNumerical(begChar) || (begChar == '-') || (begChar == '+')) {
			return parseValueNumber(pd, begChar);
		} else
		if(
			(begChar == 'y') || (begChar == 'n') ||
			(begChar == 't') || (begChar == 'f')
		) {
			return parseValueBool(pd, begChar);
		} else {
			throw yacfg::UnexpectedChar(pd.src.lineCounter, pd.src.linePosition,
				begChar, "a value" );
		}
	}


	yacfg::Config parse(ParseData& pd) {
		skipWhitespaces(pd);

		/* Preemptively skip leading whitespaces and comments:
		 * during the loop condition check, the cursor points at either a key,
		 * EOF or an invalid character sequence. */
		skipWhitespacesAndComments(pd);

		while(! pd.src.isAtEof()) {
			if(pd.src.getChar() == GRAMMAR_GROUP_END) {
				if(pd.keyStack.empty()) {
					throw yacfg::UnmatchedGroupClosure(pd.src.lineCounter, pd.src.linePosition);
				} else {
					pd.keyStack.pop_back();
				}
				pd.src.fwdOrEof();
			} else {
				// Get the key for the entry or group
				auto key = parseKey(pd);
				if(! pd.keyStack.empty()) {
					key = yacfg::Key(pd.keyStack.back() + '.' + key);
				}
				skipWhitespacesAndComments(pd);

				// Parse the assignment or group delimiter
				{
					static constexpr const char* expectDefStr = "an assignment or a group delimiter";
					char charAfterKey = pd.src.getChar();
					pd.src.fwd(expectDefStr);
					if(charAfterKey == GRAMMAR_GROUP_BEGIN) {
						pd.keyStack.push_back(std::move(key));
					} else
					if(charAfterKey == GRAMMAR_ASSIGN) {
						// Arbitrary space after the assignment character
						skipWhitespacesAndComments(pd);

						pd.cfg.set(key, parseValue(pd));
					} else {
						throw yacfg::UnexpectedChar(pd.src.lineCounter, pd.src.linePosition,
							charAfterKey, expectDefStr );
					}
				}
			}

			// Space between definitions (or a definition and EOF)
			skipWhitespacesAndComments(pd);
		}

		// Check and throw for unclosed groups
		if(! pd.keyStack.empty()) {
			throw yacfg::UnclosedGroup(pd.keyStack.back());
		}

		return std::move(pd.cfg);
	}

}
}



namespace yacfg {

	using namespace yacfg_parse;


	Config Config::parse(const char* cStr) {
		return parse(cStr, strlen(cStr));
	}

	Config Config::parse(const char* charSeqPtr, size_t length) {
		auto src = StringSource(std::span<const char>(charSeqPtr, length));
		ParseData pd = {
			.cfg = { },
			.src = src,
			.keyStack = { } };
		return yacfg_parse::parse(pd);
	}

	Config Config::read(InputStream& in) {
		return read(in, std::numeric_limits<size_t>::max());
	}

	Config Config::read(InputStream& in, size_t count) {
		auto src = FileSource(in, count);
		ParseData pd = {
			.cfg = { },
			.src = src,
			.keyStack = { } };
		return yacfg_parse::parse(pd);
	}

}
