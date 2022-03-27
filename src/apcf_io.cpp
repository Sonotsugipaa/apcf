#include "apcf_.hpp"



namespace apcf::io {


	#ifdef NDEBUG
		StringWriter::StringWriter() { }
	#else
		StringWriter::StringWriter(): dst(nullptr), cursor(0) { }
	#endif

	StringWriter::StringWriter(std::string* dst, size_t begin):
			dst(dst), cursor(begin)
	{ }

	void StringWriter::writeChar(char c) {
		assert(dst != nullptr);
		dst->push_back(c);
		++cursor;
	}

	void StringWriter::writeChars(
			const char* begin,
			const char* end
	) {
		assert(dst != nullptr);
		size_t distance = std::distance(begin, end);
		dst->insert(dst->size(), begin, distance);
		cursor += distance;
	}

	void StringWriter::writeChars(
			const std::string& str
	) {
		assert(dst != nullptr);
		dst->insert(dst->size(), str.data(), str.size());
		cursor += str.size();
	}


	#ifdef NDEBUG
		StdStreamWriter::StdStreamWriter() { }
	#else
		StdStreamWriter::StdStreamWriter(): dst(nullptr) { }
	#endif

	StdStreamWriter::StdStreamWriter(std::ostream& dst):
			dst(&dst)
	{ }

	void StdStreamWriter::writeChar(char c) {
		assert(dst != nullptr);
		dst->write(&c, 1);
	}

	void StdStreamWriter::writeChars(
			const char* begin,
			const char* end
	) {
		assert(dst != nullptr);
		size_t distance = std::distance(begin, end);
		dst->write(begin, distance);
	}

	void StdStreamWriter::writeChars(
			const std::string& str
	) {
		assert(dst != nullptr);
		dst->write(str.data(), str.size());
	}


	StringReader::StringReader(
			std::span<const char, std::dynamic_extent> str
	):
			str(str),
			cursor(0),
			limit(str.size())
	{ }

	bool StringReader::isAtEof() const {
		assert(cursor <= str.size());
		return cursor == limit;
	}

	char StringReader::getChar() const {
		assert(cursor <= str.size());
		char c = (cursor < limit)? str[cursor] : GRAMMAR_NULL;
		return c;
	}

	bool StringReader::fwdOrEof() {
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


	StdStreamReader::StdStreamReader(std::istream& str, size_t limit):
			str(&str),
			charsLeft(limit)
	{
		fwdOrEof();
	}

	StdStreamReader::StdStreamReader(std::istream& str):
			StdStreamReader(str, std::numeric_limits<size_t>::max())
	{ }

	bool StdStreamReader::fwdOrEof() {
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

}
