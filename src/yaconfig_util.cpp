#include <yaconfig.hpp>

#include <cstring>
#include <set>



#define INVALID_VALUE_STR(TYPE_) ( \
	"cannot get " + \
	std::string(yacfg::dataTypeStringOf(TYPE_)) + \
	" value" \
)



namespace {
namespace yacfg_util {

	using yacfg::Key;
	using yacfg::KeySpan;


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


	template<typename T>
	std::vector<T> setToVec(std::set<T> set) {
		std::vector<T> r;
		r.reserve(set.size());
		for(const auto& value : set) {
			r.push_back(value);
		}
		return r;
	};


	class Ancestry {
	public:
		std::map<KeySpan, std::set<KeySpan>> tree;

		Ancestry(const std::map<yacfg::Key, yacfg::RawData>&);

		void putKey(const yacfg::Key&);

		const std::set<KeySpan>& getSubkeys(KeySpan) const;

		bool collapse(KeySpan, KeySpan parent);
		bool collapse();
	};


	Ancestry::Ancestry(const std::map<yacfg::Key, yacfg::RawData>& cfg) {
		for(const auto& entry : cfg) {
			putKey(entry.first);
		}
	}


	void Ancestry::putKey(const yacfg::Key& key) {
		constexpr auto splitKey = [](const yacfg::Key& key) {
			constexpr size_t keyBasenameSizeHeuristic = 5;
			std::vector<KeySpan> r;
			r.reserve(key.size() / keyBasenameSizeHeuristic);
			const char* beg = key.data();
			const char* end = key.data() + key.size();
			const char* cur = beg;

			assert(beg != end);
			while(cur != end) {
				++ cur;
				while(cur != end && *cur != GRAMMAR_KEY_SEPARATOR) {
					++ cur;
				}
				r.push_back(KeySpan(beg, size_t(cur-beg)));
			}

			return r;
		};

		auto split = splitKey(key);
		size_t size = split.size();
		if(! split.empty()) tree[{ }].insert(split[0]);
		for(size_t i=1; i < size; ++i) {
			tree[split[i-1]].insert(split[i]);
		}
	}


	const std::set<KeySpan>& Ancestry::getSubkeys(KeySpan ref) const {
		static const std::set<KeySpan> emptySet = { };
		auto found = tree.find(ref);
		if(found == tree.end()) return emptySet;
		return found->second;
	}


	bool Ancestry::collapse(KeySpan ref, KeySpan parent) {
		/* Recursive condition on `n` (number of subkeys mapped to `ref`)
		 * n=0  =>  Return value is false; end of recursion branch.
		 * n=1  =>  Return value is true; if possible, move the subkey mapping to `parent`.
		 * n>0  =>  Return value is true; recursively call `collapse` on every subkey. */
		if(ref.empty()) return false;
		auto subkeys0set = &getSubkeys(ref);
		bool r = false;
		if(! subkeys0set->empty()) {
			std::vector<KeySpan> subkeys0 = setToVec(*subkeys0set);
			if(subkeys0.size() == 1) {
				auto subkey0 = *subkeys0.begin();
				tree[parent].insert(subkey0);
				tree.erase(ref);
				collapse(subkey0, parent);
				r = true;
			} else {
				for(const auto& subkey0 : subkeys0) {
					r = collapse(subkey0, ref) || r;
				}
			}
		}
		return r;
	}


	bool Ancestry::collapse() {
		bool r = false;
		std::set<KeySpan> collapsed;

		std::vector<KeySpan> candidates;
		auto mkCandidates = [&]() {
			const auto& candidatesSet = tree[{ }];
			candidates.clear();
			if(candidates.capacity() < candidatesSet.size()) {
				candidates.reserve(candidatesSet.size());
			}
			for(const auto& entry : candidatesSet) {
				candidates.push_back(entry);
			}
		};

		/* Whenever `collapse(...)` returns true, the tree has been modified
		 * and the iterators are (probably?) out of date. */
		auto runPass = [&]() {
			mkCandidates();
			auto iter = candidates.begin();
			auto end = candidates.end();
			while(iter != end) {
				if(collapse(*iter, { })) {
					return true;
				}
				++iter;
			}
			return false;
		};

		while(runPass()) {
			r = true;
		}

		return r;
	}

}
}
