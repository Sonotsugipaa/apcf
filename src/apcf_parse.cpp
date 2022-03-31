#include "apcf_.hpp"

#include <fstream>
#include <cstring>



namespace apcf_parse {

	using namespace apcf_util;


	void fwd(apcf::io::Reader& rd, const std::string& expected) {
		if(! rd.fwdOrEof()) throw apcf::UnexpectedEof(expected);
	}


	bool skipWhitespaces(ParseData& pd) {
		if(! isWhitespace(pd.src.getChar())) return false;
		do {
			// NOP
		} while(pd.src.fwdOrEof() && isWhitespace(pd.src.getChar()));
		return true;
	}


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
		fwd(pd.src, secCharExpectStr);
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
				fwd(pd.src, commentEndExpectStr);
				lastChar = curChar;
				curChar = pd.src.getChar();
			} while(! (
				(lastChar == GRAMMAR_COMMENT_ML_MIDDLE) &&
				(curChar == GRAMMAR_COMMENT_EXTREME)
			));
			pd.src.fwdOrEof();
		} else {
			throw apcf::UnexpectedChar(
				pd.src.lineCounter(), pd.src.linePosition(),
				pd.src.getChar(), secCharExpectStr );
		}
		return true;
	}


	/* Does not allow EOF at the end */ /**/
	apcf::Key parseKey(ParseData& pd) {
		static const std::string& expectStr = "a key";
		std::string r;

		char c = pd.src.getChar();

		if(! isValidKeyChar(c)) {
			throw apcf::UnexpectedChar(
				pd.src.lineCounter(), pd.src.linePosition(),
				c, expectStr );
		}

		do {
			r.push_back(c);
			fwd(pd.src, expectStr);
			c = pd.src.getChar();
		} while(isValidKeyChar(c));
		assert(apcf::isKeyValid(r)); // All non-key characters count as terminators
		return apcf::Key(r);
	}


	apcf::RawData parseValueArray(ParseData& pd) {
		static const std::string expectStr = "a list of space separated values";
		std::vector<apcf::RawData> rVector;
		char curChar;

		fwd(pd.src, expectStr);
		skipWhitespacesAndComments(pd);

		curChar = pd.src.getChar();
		while(curChar != GRAMMAR_ARRAY_END) {
			rVector.emplace_back(parseValue(pd));
			skipWhitespacesAndComments(pd);
			curChar = pd.src.getChar();
		}
		pd.src.fwdOrEof();

		return apcf::RawData::moveArray(rVector.data(), rVector.size());
	}


	apcf::RawData parseValueString(ParseData& pd) {
		#define FWD_  { fwd(pd.src, expectStr); cur = pd.src.getChar(); }
		using namespace std::string_literals;
		static const std::string expectStr = "a string delimiter ("s + GRAMMAR_STRING_DELIM + ")"s;
		std::string r;
		fwd(pd.src, expectStr);
		char cur = pd.src.getChar();
		while(! (cur == GRAMMAR_STRING_DELIM)) {
			if(cur == GRAMMAR_STRING_ESCAPE) {
				// Skip the escape char, and do not check the next one before pushing it
				FWD_
			}
			r.push_back(cur);
			FWD_
		}
		pd.src.fwdOrEof();
		return apcf::RawData(std::move(r));
		#undef FWD_
	}


	apcf::RawData parseValueNumber(ParseData& pd, char begChar) {
		static const std::string expectStr = "a numerical value";
		std::string buffer;
		apcf::RawData r;

		{// Read the entire number-like string
			char c = begChar;
			if(c == '-' || c == '+') {
				buffer.push_back(c);
				fwd(pd.src, expectStr);
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
			auto result = apcf_num::parseNumber(buffer.begin(), buffer.end(), &r);
			assert(result.parsedChars <= buffer.size());
			if(
				(! buffer.empty()) &&
				(result.parsedChars != buffer.size())
			) {
				throw apcf::UnexpectedChar(pd.src.lineCounter(), pd.src.linePosition(),
					buffer[result.parsedChars],
					"a sequence of base " + std::to_string(result.base) + " digits" );
			}
		}
		return r;
	}


	apcf::RawData parseValueBool(ParseData& pd, char begChar) {
		static const std::string& expectStr = "a boolean value (true/false, yes/no, y/n)";
		auto expect = [&pd](char expected) {
			if(pd.src.getChar() != expected) {
				throw apcf::UnexpectedChar(pd.src.lineCounter(), pd.src.linePosition(),
					pd.src.getChar(), expectStr );
			}
			fwd(pd.src, expectStr);
		};
		auto expectOpt = [&pd](char expected) {
			char curChar = pd.src.getChar();
			if(isAlphanum(curChar)) {
				fwd(pd.src, expectStr);
				if(curChar != expected) {
					throw apcf::UnexpectedChar(pd.src.lineCounter(), pd.src.linePosition(),
						curChar, expectStr );
				}
				return true;
			} else {
				return false;
			}
		};
		fwd(pd.src, expectStr);
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
				throw apcf::UnexpectedChar(pd.src.lineCounter(), pd.src.linePosition(),
					pd.src.getChar(), expectStr );
			} break;
		}
	}


	apcf::RawData parseValue(ParseData& pd) {
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
			throw apcf::UnexpectedChar(pd.src.lineCounter(), pd.src.linePosition(),
				begChar, "a value" );
		}
	}


	apcf::Config parse(ParseData& pd) {
		skipWhitespaces(pd);

		/* Preemptively skip leading whitespaces and comments:
		 * during the loop condition check, the cursor points at either a key,
		 * EOF or an invalid character sequence. */
		skipWhitespacesAndComments(pd);

		while(! pd.src.isAtEof()) {
			if(pd.src.getChar() == GRAMMAR_GROUP_END) {
				if(pd.keyStack.empty()) {
					throw apcf::UnmatchedGroupClosure(pd.src.lineCounter(), pd.src.linePosition());
				} else {
					pd.keyStack.pop_back();
				}
				pd.src.fwdOrEof();
			} else {
				// Get the key for the entry or group
				auto key = parseKey(pd);
				if(! pd.keyStack.empty()) {
					key = apcf::Key(pd.keyStack.back() + '.' + key);
				}
				skipWhitespacesAndComments(pd);

				// Parse the assignment or group delimiter
				{
					static constexpr const char* expectDefStr = "an assignment or a group delimiter";
					char charAfterKey = pd.src.getChar();
					fwd(pd.src, expectDefStr);
					if(charAfterKey == GRAMMAR_GROUP_BEGIN) {
						pd.keyStack.push_back(std::move(key));
					} else
					if(charAfterKey == GRAMMAR_ASSIGN) {
						// Arbitrary space after the assignment character
						skipWhitespacesAndComments(pd);

						pd.cfg.set(key, parseValue(pd));
					} else {
						throw apcf::UnexpectedChar(pd.src.lineCounter(), pd.src.linePosition(),
							charAfterKey, expectDefStr );
					}
				}
			}

			// Space between definitions (or a definition and EOF)
			skipWhitespacesAndComments(pd);
		}

		// Check and throw for unclosed groups
		if(! pd.keyStack.empty()) {
			throw apcf::UnclosedGroup(pd.keyStack.back());
		}

		return std::move(pd.cfg);
	}

}



namespace apcf {

	using namespace apcf_parse;


	Config Config::parse(const char* cStr) {
		return parse(cStr, strlen(cStr));
	}

	Config Config::parse(const char* charSeqPtr, size_t length) {
		auto src = io::StringReader(std::span<const char>(charSeqPtr, length));
		ParseData pd = {
			.cfg = { },
			.src = src,
			.keyStack = { } };
		return apcf_parse::parse(pd);
	}

	Config Config::read(io::Reader& in) {
		ParseData pd = {
			.cfg = { },
			.src = in,
			.keyStack = { } };
		return apcf_parse::parse(pd);
	}

	Config Config::read(std::istream& in) {
		return read(in, std::numeric_limits<size_t>::max());
	}

	Config Config::read(std::istream& in, size_t count) {
		auto src = io::StdStreamReader(in, count);
		ParseData pd = {
			.cfg = { },
			.src = src,
			.keyStack = { } };
		return apcf_parse::parse(pd);
	}

}
