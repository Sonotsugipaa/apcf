#include "apcf_.hpp"

#include <cstring>
#include <set>



namespace apcf_util {

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
		// if(str.front() == GRAMMAR_KEY_SEPARATOR) {
		// }
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
