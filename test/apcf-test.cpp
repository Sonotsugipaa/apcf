#include <test_tools.hpp>

#include <apcf.hpp>
#include <apcf_hierarchy.hpp>

#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>
#include <set>



namespace {

	using apcf::Config;

	constexpr auto eFailure = utest::ResultType::eFailure;
	constexpr auto eNeutral = utest::ResultType::eNeutral;
	constexpr auto eSuccess = utest::ResultType::eSuccess;

	using TestFn = std::function<utest::ResultType (std::ostream&)>;


	constexpr const char* genericConfigSrc =
		"1 = 1\n"
		"1.1 = 1.5\n"
		"1.2 = 1.25\n"
		"rootvalue-int = 1\n"
		"rootvalue-int.negative = -1\n"
		"rootvalue-int.positive = +1\n"
		"rootvalue-float=1.5\n"
		"rootvalue-string = \"str \\\"literal\\\"\"\n"
		"rootvalue-array = [\n"
		"  1  2.25  [  \"3\" ][\"4\" \"5\"  ]\n"
		"]\n"
		"group1 {\n"
		"  value1 = 1\n"
		"  value2 = 2\n"
		"  group2.value1= 3\n"
		"  group2.value2 =4\n"
		"  group3 {\n"
		"    value1=5"
		"  }\n"
		"}\n"
		"long.single.line.group.1 {\n"
		"  value1 = 6\n"
		"  value2 = 7\n"
		"  2 { value1=8 value2=9 }\n"
		"}\n";

	constexpr const char* tmpFileBase = "test/";


	auto wrapExceptions(TestFn fn) {
		return [fn](std::ostream& cerr) -> utest::ResultType {
			using namespace apcf;
			try {
				return std::move(fn)(cerr);
			} catch(InvalidKey& err) {
				cerr << "Caught apcf::InvalidKey: " << err.what() << std::endl;
			} catch(InvalidValue& err) {
				cerr << "Caught apcf::InvalidValue: " << err.what() << std::endl;
			} catch(UnexpectedChar& err) {
				cerr << "Caught apcf::UnexpectedChar: " << err.what() << std::endl;
			} catch(UnexpectedEof& err) {
				cerr << "Caught apcf::UnexpectedEof: " << err.what() << std::endl;
			}
			return eFailure;
		};
	}


	utest::ResultType testVirtualRdWr(std::ostream&) {
		struct Reader : public apcf::io::Reader {
			bool isAtEof() const { return true; }
			char getChar() const { return '\0'; }
			bool fwdOrEof() { return false; }
		};
		struct Writer : public apcf::io::Writer {
			void writeChar(char) { }
			void writeChars(const char*, const char*) { }
			void writeChars(const std::string&) { }
		};
		auto cfg = Config::read(Reader());
		cfg.write(Writer());
		return eSuccess;
	}


	utest::ResultType testSetGetBool(std::ostream&) {
		Config cfg;
		cfg.setBool("key.subkey.bool", true);
		auto found = cfg.get("key.subkey.bool");
		return (bool(found) && found.value()->data.boolValue) ? eSuccess : eFailure;
	}

	utest::ResultType testSetGetInt(std::ostream&) {
		Config cfg;
		cfg.setInt("key.subkey.int", 7);
		auto found = cfg.get("key.subkey.int");
		return (bool(found) && found.value()->data.intValue == 7) ? eSuccess : eFailure;
	}

	utest::ResultType testSetGetFloat(std::ostream&) {
		Config cfg;
		cfg.setFloat("key.subkey.float", 7.2);
		auto found = cfg.get("key.subkey.float");
		return (bool(found) && found.value()->data.floatValue == 7.2) ? eSuccess : eFailure;
	}

	utest::ResultType testSetGetString(std::ostream&) {
		Config cfg;
		const std::string testString = "strValue";
		cfg.setString("key.subkey.string", testString);
		auto found = cfg.get("key.subkey.string");
		if(! found) {
			return eFailure;
		}
		return
			(0 == strncmp(
				testString.data(),
				found.value()->data.stringValue.ptr,
				testString.size()
			))? eSuccess : eFailure;
	}

	utest::ResultType testSetGetArray(std::ostream&) {
		Config cfg;
		const std::string testString = "strValue";
		cfg.setString("key.subkey.string", testString);
		auto found = cfg.get("key.subkey.string");
		if(! found) {
			return eFailure;
		}
		return
			(0 == strncmp(
				testString.data(),
				found.value()->data.stringValue.ptr,
				testString.size()
			))? eSuccess : eFailure;
	}


	bool testInvalidKey(const std::string& key, std::ostream& out) {
		try {
			Config cfg;
			cfg.setInt(key, key.size());
			out << "Invalid key \"" << key << "\" results valid" << std::endl;
			return false;
		} catch(apcf::InvalidKey& ex) {
			return true;
		}
	}

	bool testValidKey(const std::string& key, std::ostream& out) {
		try {
			Config cfg;
			cfg.setInt(key, key.size());
			return true;
		} catch(apcf::InvalidKey& ex) {
			out << "Valid key \"" << key << "\" results invalid" << std::endl;
			return false;
		}
	}

	utest::ResultType testValidKeys(std::ostream& out) {
		return
			(
				testValidKey("key", out) &&
				testValidKey("key.key", out) &&
				testValidKey("key.key.key", out)
			)? eSuccess : eFailure;
	}

	utest::ResultType testInvalidKeys(std::ostream& out) {
		return
			(
				testInvalidKey("key..key", out) &&
				testInvalidKey("key...key", out) &&
				testInvalidKey(".key.key", out) &&
				testInvalidKey("key.key.", out) &&
				testInvalidKey("key .key", out) &&
				testInvalidKey(".", out) &&
				testInvalidKey("..", out)
			)? eSuccess : eFailure;
	}

	utest::ResultType testReadOnelineCommentEof(std::ostream&) {
		Config cfg = Config::parse("// comment + eof */");
		return cfg.begin() == cfg.end()? eSuccess : eFailure;
	}

	utest::ResultType testReadOnelineCommentEol(std::ostream&) {
		Config cfg = Config::parse("// comment + eol */\n");
		return cfg.begin() == cfg.end()? eSuccess : eFailure;
	}

	utest::ResultType testReadOnelineCommentEmpty(std::ostream&) {
		Config cfg = Config::parse("//");
		return cfg.begin() == cfg.end()? eSuccess : eFailure;
	}


	utest::ResultType testGroups(std::ostream& out) {
		Config cfg = Config::parse(
			"nothing = 0\n"
			"group1 {\n"
			"  value1 = 1\n"
			"  value2 = 2\n"
			"  group2{value1=3 value2=4}\n"
			"  group3 {\n"
			"    group4 { value1 = 5 }"
			"  }\n"
			"}\n" );
		bool result = true;
		auto checkEntry = [&out, &cfg, &result](const apcf::Key& key, apcf::int_t value) {
			auto entry = cfg.getInt(key);
			if(! entry.has_value()) {
				out << "Missing entry for `" << key << '`' << std::endl;
				result = false;
			}
			if(entry.value() != value) {
				out << "Entry `" << key << "` has value " << entry.value() << ", expected " << value << std::endl;
				result = false;
			}
		};
		checkEntry("group1.value1", 1);
		checkEntry("group1.value2", 2);
		checkEntry("group1.group2.value1", 3);
		checkEntry("group1.group2.value2", 4);
		checkEntry("group1.group3.group4.value1", 5);
		return result? eSuccess : eFailure;
	}

	utest::ResultType testUnmatchedGroupClosure(std::ostream& out) {
		try {
			Config cfg = Config::parse("group1{group2{}} }");
			out << "Expected a UnmatchedGroupClosure error to be thrown" << std::endl;
			return eFailure;
		} catch(apcf::UnmatchedGroupClosure&) {
			return eSuccess;
		}
	}

	utest::ResultType testUnclosedGroup(std::ostream& out) {
		try {
			Config cfg = Config::parse(" group1 { group2 { } ");
			out << "Expected a UnclosedGroup error to be thrown" << std::endl;
			return eFailure;
		} catch(apcf::UnclosedGroup&) {
			return eSuccess;
		}
	}


	template<bool doMove>
	utest::ResultType testMerge(std::ostream& out) {
		constexpr const char* cfg1src = "cfg1 { value=1 override=1 }";
		constexpr const char* cfg2src = "cfg1 { override=2 } cfg2 { value=2 }";
		Config cfg;
		if constexpr(doMove) {
			cfg
				<< Config::parse(cfg1src)
				<< Config::parse(cfg2src);
		} else {
			Config cfg1 = Config::parse(cfg1src);
			Config cfg2 = Config::parse(cfg2src);
			cfg << cfg1 << cfg2;
		}
		auto result = true;
		auto checkEntry = [&out, &cfg, &result](const apcf::Key& key, apcf::int_t value) {
			auto entry = cfg.getInt(key);
			if(! entry.has_value()) {
				out << "Missing entry for `" << key << '`' << std::endl;
				result = false;
			}
			if(entry.value() != value) {
				out << "Entry `" << key << "` has value " << entry.value() << ", expected " << value << std::endl;
				result = false;
			}
		};
		checkEntry("cfg1.value", 1);
		checkEntry("cfg2.value", 2);
		checkEntry("cfg1.override", 2);
		return result? eSuccess : eFailure;
	}


	utest::ResultType testGetSubkeys(std::ostream& out) {
		constexpr unsigned expectedSubkeys = 3;
		Config cfg = Config::parse("a=1 a.b=2 a.c=3 a.d.e=4 a.d.f=5 g=6 h.i=7");
		auto hierarchy = cfg.getHierarchy();
		bool success = true;
		auto subkeys = hierarchy.getSubkeys("a");
		if(subkeys.size() != expectedSubkeys) {
			success = false;
			out
				<< "Config::getSubkeysOf returned " << subkeys.size()
				<< " keys, expected " << expectedSubkeys << '\n';
		}
		out.flush();
		return success? eSuccess : eFailure;
	}


	utest::ResultType testStrConfig(std::ostream& out) {
		Config cfg = Config::parse("nothing = \"zero\"\n generic.key = \"one \\\\ \\\"quote\\\"\"");
		auto val = cfg.getString("generic.key");
		if(val.has_value() && val.value() != std::string("one \\ \"quote\"")) {
			auto got = val.value();
			out << "Expected `" << std::string("one \\ \"quote\"") << "`, got `" << got << '`' << std::endl;
			return eFailure;
		}
		return eSuccess;
	}

	utest::ResultType testIntConfig(std::ostream& out) {
		Config cfg = Config::parse("nothing = 51\n generic.key = 62");
		auto val = cfg.getInt("generic.key");
		if(val.has_value() && val.value() != 62) {
			auto got = val.value();
			out << "Expected `" << 62 << "`, got `" << got << '`' << std::endl;
			return eFailure;
		}
		return eSuccess;
	}

	utest::ResultType testFloatConfig(std::ostream& out) {
		Config cfg = Config::parse("nothing = 51.4\n generic.key = 62.75");
		auto val = cfg.getFloat("generic.key");
		if(val.has_value() && val.value() != 62.75) {
			auto got = val.value();
			out << "Expected `" << 62.75 << "`, got `" << got << '`' << std::endl;
			return eNeutral;
		}
		return eSuccess;
	}

	utest::ResultType testBoolConfigTrue(std::ostream& out) {
		Config cfg = Config::parse("nothing = false\n generic.key = true");
		auto val = cfg.getBool("generic.key");
		if(val.has_value() && val.value() != true) {
			auto got = val.value();
			out << "Expected `" << true << "`, got `" << got << '`' << std::endl;
			return eFailure;
		}
		return eSuccess;
	}

	utest::ResultType testBoolConfigYes(std::ostream& out) {
		Config cfg = Config::parse("nothing = no\n generic.key = yes");
		auto val = cfg.getBool("generic.key");
		if(val.has_value() && val.value() != true) {
			auto got = val.value();
			out << "Expected `" << true << "`, got `" << got << '`' << std::endl;
			return eFailure;
		}
		return eSuccess;
	}

	utest::ResultType testBoolConfigY(std::ostream& out) {
		Config cfg = Config::parse("nothing = n\n generic.key = y");
		auto val = cfg.getBool("generic.key");
		if(val.has_value() && val.value() != true) {
			auto got = val.value();
			out << "Expected `" << true << "`, got `" << got << '`' << std::endl;
			return eFailure;
		}
		return eSuccess;
	}

	utest::ResultType testBoolConfigFalse(std::ostream& out) {
		Config cfg = Config::parse("nothing = true\n generic.key = false");
		auto val = cfg.getBool("generic.key");
		if(val.has_value() && val.value() != false) {
			auto got = val.value();
			out << "Expected `" << false << "`, got `" << got << '`' << std::endl;
			return eFailure;
		}
		return eSuccess;
	}

	utest::ResultType testBoolConfigNo(std::ostream& out) {
		Config cfg = Config::parse("nothing = yes\n generic.key = no");
		auto val = cfg.getBool("generic.key");
		if(val.has_value() && val.value() != false) {
			auto got = val.value();
			out << "Expected `" << false << "`, got `" << got << '`' << std::endl;
			return eFailure;
		}
		return eSuccess;
	}

	utest::ResultType testBoolConfigN(std::ostream& out) {
		Config cfg = Config::parse("nothing = y\n generic.key = n");
		auto val = cfg.getBool("generic.key");
		if(val.has_value() && val.value() != false) {
			auto got = val.value();
			out << "Expected `" << false << "`, got `" << got << '`' << std::endl;
			return eFailure;
		}
		return eSuccess;
	}


	utest::ResultType testSerialFullPretty(std::ostream& out) {
		Config cfg = Config::parse(genericConfigSrc);
		constexpr auto expect =
			"1 = 1\n"
			"1 {\n"
			"  1 = 1.5\n"
			"  2 = 1.25\n"
			"}\n"
			"group1 {\n"
			"  group2 {\n"
			"    value1 = 3\n"
			"    value2 = 4\n"
			"  }\n"
			"  group3.value1 = 5\n"
			"  value1 = 1\n"
			"  value2 = 2\n"
			"}\n"
			"long.single.line.group.1 {\n"
			"  2 {\n"
			"    value1 = 8\n"
			"    value2 = 9\n"
			"  }\n"
			"  value1 = 6\n"
			"  value2 = 7\n"
			"}\n"
			"rootvalue-array = [\n"
			"  1\n"
			"  2.25\n"
			"  [\n"
			"    \"3\"\n"
			"  ]\n"
			"  [\n"
			"    \"4\"\n"
			"    \"5\"\n"
			"  ]\n"
			"]\n"
			"rootvalue-float = 1.5\n"
			"rootvalue-int = 1\n"
			"rootvalue-int {\n"
			"  negative = -1\n"
			"  positive = 1\n"
			"}\n"
			"rootvalue-string = \"str \\\"literal\\\"\"\n";
		apcf::SerializationRules rules = { };
		rules.flags = apcf::SerializationRules::ePretty;
		rules.indentationSize = 2;
		auto serialized = cfg.serialize(rules);
		if(serialized != expect) {
			out << "// The serialized config is probably incorrect.\n";
			out << serialized << std::endl;
			return eNeutral;
		}
		return eSuccess;
	}

	utest::ResultType testSerialFullCompact(std::ostream& out) {
		Config cfg = Config::parse(genericConfigSrc);
		constexpr auto expect =
		"1=1 "
		"1{1=1.5 2=1.25}"
		"group1{"
		"group2{value1=3 value2=4}"
		"group3.value1=5 "
		"value1=1 "
		"value2=2"
		"}"
		"long.single.line.group.1{"
		"2{value1=8 value2=9"
		"}"
		"value1=6 "
		"value2=7"
		"}"
		"rootvalue-array=[1 2.25 [\"3\"] [\"4\" \"5\"]] "
		"rootvalue-float=1.5 "
		"rootvalue-int=1 "
		"rootvalue-int{negative=-1 positive=1}"
		"rootvalue-string=\"str \\\"literal\\\"\"";
		apcf::SerializationRules rules = { };
		rules.flags = apcf::SerializationRules::eNull;
		auto serialized = cfg.serialize(rules);
		if(serialized != expect) {
			out << "// The serialized config is probably incorrect.\n";
			out << serialized << std::endl;
			return eNeutral;
		}
		return eSuccess;
	}


	utest::ResultType testSerialNan(std::ostream& out) {
		Config cfg;
		bool r = true;
		cfg.setFloat("inf", INFINITY);
		cfg.setFloat("nan", NAN);

		try {
			auto serial = cfg.get("inf").value()->serialize();
			out
				<< "Positive infinity was successfully serialized as `"
				<< serial << "`,\nwithout the `eFloatNoFail` flag set." << std::endl;
			r = false;
		} catch(apcf::InvalidValue&) { }

		try {
			auto serial = cfg.get("nan").value()->serialize();
			out
				<< "NaN was successfully serialized as `"
				<< serial << "`,\nwithout the `eFloatNoFail` flag set." << std::endl;
			r = false;
		} catch(apcf::InvalidValue&) { }

		{
			apcf::SerializationRules rules = { };
			rules.flags = apcf::SerializationRules::eFloatNoFail;
			std::string serial;
			serial = cfg.get("inf").value()->serialize(rules);
			out << "Positive infinity serialized as `" << serial << "`\n";
			serial = cfg.get("nan").value()->serialize(rules);
			out << "NaN serialized as `" << serial << '`' << std::endl;
		}

		return r? eSuccess : eFailure;
	}


	utest::ResultType testFileWrite(std::ostream&) {
		using namespace std::string_literals;
		Config cfg = Config::parse(genericConfigSrc);
		apcf::SerializationRules rules = { };
		rules.flags = apcf::SerializationRules::eNull;
		cfg.write(
			std::ofstream(tmpFileBase + ".test_generic.cfg"s),
			rules );
		return eSuccess;
	}


	utest::ResultType testFileRead(std::ostream& out) {
		using namespace std::string_literals;
		Config cfgCmp = Config::parse(genericConfigSrc);
		Config cfgFile = Config::read(std::ifstream(tmpFileBase + ".test_generic.cfg"s));
		if(cfgCmp.keyCount() != cfgFile.keyCount()) {
			out
				<< "Key number mismatch: read " << cfgFile.keyCount()
				<< ", expected " << cfgCmp.keyCount() << std::endl;
			return eFailure;
		}
		for(auto& cmpEntry : cfgCmp) {
			auto fileEntryFound = cfgFile.get(cmpEntry.first);
			if(! fileEntryFound.has_value()) {
				out << "Entry not found for `" << cmpEntry.first << '`' << std::endl;
				return eFailure;
			}
			if(fileEntryFound.value()->type != cmpEntry.second.type) {
				out
					<< "Type mismatch for `" << cmpEntry.first << "`:\nread "
					<< apcf::dataTypeStringOf(fileEntryFound.value()->type)
					<< ",\nexpected " << apcf::dataTypeStringOf(cmpEntry.second.type)
					<< std::endl;
				return eFailure;
			}
		}
		return eSuccess;
	}

}



int main(int, char**) {
	#define RUN_(NAME_, FN_) run(NAME_, wrapExceptions(FN_))
	auto batch = utest::TestBatch(std::cout);
	batch
		.RUN_("Virtual reader and writer", testVirtualRdWr)
		.RUN_("Getter and setter (bool)", testSetGetBool)
		.RUN_("Getter and setter (int)", testSetGetInt)
		.RUN_("Getter and setter (float)", testSetGetFloat)
		.RUN_("Getter and setter (string)", testSetGetString)
		.RUN_("Getter and setter (array)", testSetGetArray)
		.RUN_("Valid keys", testValidKeys)
		.RUN_("Invalid keys", testInvalidKeys)
		.RUN_("Config merge (copy)", testMerge<false>)
		.RUN_("Config merge (move)", testMerge<true>)
		.RUN_("Get subkeys", testGetSubkeys)
		.RUN_("[parse] Single line comment, then EOL", testReadOnelineCommentEol)
		.RUN_("[parse] Single line comment, then EOF", testReadOnelineCommentEof)
		.RUN_("[parse] Single line empty comment", testReadOnelineCommentEmpty)
		.RUN_("[parse] String value", testStrConfig)
		.RUN_("[parse] Integer value", testIntConfig)
		.RUN_("[parse] Fractional value", testFloatConfig)
		.RUN_("[parse] Boolean value (TRUE/false)", testBoolConfigTrue)
		.RUN_("[parse] Boolean value (YES/no)", testBoolConfigYes)
		.RUN_("[parse] Boolean value (Y/n)", testBoolConfigY)
		.RUN_("[parse] Boolean value (FALSE/true)", testBoolConfigFalse)
		.RUN_("[parse] Boolean value (NO/yes)", testBoolConfigNo)
		.RUN_("[parse] Boolean value (N/y)", testBoolConfigN)
		.RUN_("[parse] Groups", testGroups)
		.RUN_("[parse] Unclosed group", testUnclosedGroup)
		.RUN_("[parse] Unmatched group closure", testUnmatchedGroupClosure)
		.RUN_("[serial] Simple serialization (pretty)", testSerialFullPretty)
		.RUN_("[serial] Simple serialization (compact)", testSerialFullCompact)
		.RUN_("[serial] Simple serialization (float NaN, infinity)", testSerialNan)
		.RUN_("[file] Write to file", testFileWrite)
		.RUN_("[file] Read from file", testFileRead);
	return batch.failures() == 0? EXIT_SUCCESS : EXIT_FAILURE;
	#undef RUN_
}
