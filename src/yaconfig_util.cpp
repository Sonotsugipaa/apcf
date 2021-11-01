#include <yaconfig.hpp>

#include <cstring>



#define INVALID_VALUE_STR(TYPE_) ( \
	"cannot get " + \
	std::string(yacfg::dataTypeStringOf(TYPE_)) + \
	" value" \
)



namespace {
namespace yacfg_util {

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


	std::string plainCharRep(char c) {
		if(c == '`') {
			return std::string('`', 1);
		} else
		if(isPlainChar(c)) {
			return '`' + std::string(&c, 1) + '`';
		} else {
			return "(codepoint " + std::to_string((unsigned) c) + ")";
		}
	}


	size_t findKeyError(const std::string& str) {
		if(str.front() == GRAMMAR_KEY_SEPARATOR) {
		}
		size_t pos = 0;
		for(char prev = GRAMMAR_KEY_SEPARATOR; char c : str) {
			if(
				(! (
					isAlphanum(c) ||
					(c == GRAMMAR_KEY_SEPARATOR) ||
					(c == '_') || (c == '-')
				)) ||
				((prev == GRAMMAR_KEY_SEPARATOR) && (c == prev))
			) {
				return pos;
			}
			prev = c;
			++pos;
		}
		if(str.back() == GRAMMAR_KEY_SEPARATOR) return str.size() - 1;
		return pos;
	}

}
}
