namespace apcf {

	namespace io {

		class Reader;
		class Writer;

	}

	enum class DataType;

	class Key;
	class KeySpan;

	struct SerializationRules;
	class RawString;
	class RawArray;
	struct RawData;

	class Config;
	class ConfigHierarchy;

	class ConfigError;
	class InvalidKey;
	class InvalidValue;
	class ConfigParsingError;
	class UnexpectedChar;
	class UnexpectedEof;
	class UnclosedGroup;
	class UnmatchedGroupClosure;

}
