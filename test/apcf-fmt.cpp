#include <test_tools.hpp>

#include <apcf.hpp>
#include <apcf_hierarchy.hpp>
#include <apcf_templates.hpp>

#include <iostream>
#include <fstream>
#include <set>



namespace {

	using apcf::Config;
	using apcf::Key;

	constexpr auto eFailure = utest::ResultType::eFailure;
	constexpr auto eNeutral = utest::ResultType::eNeutral;
	constexpr auto eSuccess = utest::ResultType::eSuccess;

	using TestFn = std::function<utest::ResultType (std::ostream&)>;


	constexpr const char* genericConfigSrc =
		"1 = 1\n"
		"1.1 = 0.25\n"
		"1.1p = +0.25\n"
		"1.1n = -0.25\n"
		"1.2 = 1.925\n"
		"1.2p = +1.925\n"
		"1.2n = -1.925\n"
		"1.3 = 9.5\n"
		"1.3p = +9.5\n"
		"1.3n = -9.5\n"
		"rootvalue-int = 1\n"
		"rootvalue-int.negative = -1\n"
		"rootvalue-int.positive = +1\n"
		"rootvalue-float=1.5\n"
		"rootvalue-string = \"str \\\"literal\\\"\"\n"
		"rootvalue-array-one = [\n"
		"  1  2.25  [  \"3\" \"4\"][ \"5\"  ] [ \"6\"][\"7\" \"8\"] true\n"
		"]\n"
		"rootvalue-array-two = [ ]\n"
		"rootvalue-array-three = [ ]\n"
		"rootvalue-array-four = [ \"1\" ]\n"
		"rootvalue-array-five = [ \"2\" \"3\" ]\n"
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


	bool cmpConfigs(std::ostream& out, std::string lName, const Config& cl, std::string rName, const Config& cr) {
		std::set<Key> keys;
		for(const auto& entry : cl) { keys.insert(entry.first); }
		for(const auto& entry : cr) { keys.insert(entry.first); }
		for(const Key& key : keys) {
			const apcf::RawData* l = cl.get(key).value_or(nullptr);
			const apcf::RawData* r = cr.get(key).value_or(nullptr);
			if((l == nullptr) && (r != nullptr)) {
				out << lName << " = nullptr,  " << rName << " = " << r->serialize() << std::endl;  return false;
			} else
			if((l != nullptr) && (r == nullptr)) {
				out << lName << " = " << l->serialize() << ",  " << rName << " = nullptr" << std::endl;  return false;
			} else
			if(l != nullptr) {
				std::string lStr = l->serialize();
				std::string rStr = r->serialize();
				if(lStr != rStr) {
					out << lName << " = " << l->serialize() << ",  " << rName << " = " << r->serialize() << std::endl;  return false;
				}
			}
		}
		return true;
	}


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


	template<typename T>
	bool checkValue(
			const Config& cfg, std::ostream& out,
			const Key& key, T expect
	) {
		auto got = getCfgValue<T>(cfg, key);
		if(! got.has_value()) {
			out
				<< "Expected entry `" << key << '='
				<< expect << "`, found none" << std::endl;
			return false;
		}
		if(got.value() != expect) {
			out
				<< "Expected entry `" << key << '=' << expect
				<< "`, found value " << got.value() << std::endl;
			return false;
		}
		return true;
	};


	utest::ResultType testSerialFullPretty(std::ostream& out) {
		Config cfg = Config::parse(genericConfigSrc);
		constexpr auto expect =
			"1 = 1\n"
			"1 {\n"
			"  1 = 0.25\n"
			"  1n = -0.25\n"
			"  1p = 0.25\n"
			"  2 = 1.925\n"
			"  2n = -1.925\n"
			"  2p = 1.925\n"
			"  3 = 9.5\n"
			"  3n = -9.5\n"
			"  3p = 9.5\n"
			"}\n"
			"\n"
			"group1 {\n"
			"  group2 {\n"
			"    value1 = 3\n"
			"    value2 = 4\n"
			"  }\n"
			"\n"
			"  group3.value1 = 5\n"
			"  value1 = 1\n"
			"  value2 = 2\n"
			"}\n"
			"\n"
			"long.single.line.group.1 {\n"
			"  2 {\n"
			"    value1 = 8\n"
			"    value2 = 9\n"
			"  }\n"
			"\n"
			"  value1 = 6\n"
			"  value2 = 7\n"
			"}\n"
			"\n"
			"rootvalue-int = 1\n"
			"rootvalue-int {\n"
			"  negative = -1\n"
			"  positive = 1\n"
			"}\n"
			"\n"
			"rootvalue-array-five = [\n"
			"  \"2\"\n"
			"  \"3\"\n"
			"]\n"
			"\n"
			"rootvalue-array-four = [ \"1\" ]\n"
			"\n"
			"rootvalue-array-one = [\n"
			"  1\n"
			"  2.25\n"
			"  [\n"
			"    \"3\"\n"
			"    \"4\"\n"
			"  ] [\n"
			"    \"5\"\n"
			"  ] [\n"
			"    \"6\"\n"
			"  ] [\n"
			"    \"7\"\n"
			"    \"8\"\n"
			"  ]\n"
			"  true\n"
			"]\n"
			"\n"
			"rootvalue-array-three = [ ]\n"
			"rootvalue-array-two = [ ]\n"
			"rootvalue-float = 1.5\n"
			"rootvalue-string = \"str \\\"literal\\\"\"\n";
		apcf::SerializationRules rules = { };
		rules.flags = apcf::SerializationRules::eNull;
		rules.indentationSize = 2;
		rules.maxInlineArrayLength = 7;
		auto serialized = cfg.serialize(rules);
		if(serialized != expect) {
			out << "// The serialized config is probably incorrect.\n";
			out << serialized << std::endl;
			return eNeutral;
		}
		return cmpConfigs(out, "expected", cfg, "serialized", Config::parse(serialized))? eSuccess : eFailure;
	}

	utest::ResultType testSerialFullMinimized(std::ostream& out) {
		Config cfg = Config::parse(genericConfigSrc);
		constexpr auto expect =
			"1=1 "
			"1{1=0.25 1n=-0.25 1p=0.25 2=1.925 2n=-1.925 2p=1.925 3=9.5 3n=-9.5 3p=9.5}"
			"group1{"
			"group2{value1=3 value2=4}"
			"group3.value1=5 "
			"value1=1 "
			"value2=2"
			"}"
			"long.single.line.group.1{"
			"2{value1=8 value2=9}"
			"value1=6 "
			"value2=7"
			"}"
			"rootvalue-array-five=[\"2\" \"3\"]"
			"rootvalue-array-four=[\"1\"]"
			"rootvalue-array-one=[1 2.25[\"3\" \"4\"][\"5\"][\"6\"][\"7\" \"8\"]y]"
			"rootvalue-array-three=[]"
			"rootvalue-array-two=[]"
			"rootvalue-float=1.5 "
			"rootvalue-int=1 "
			"rootvalue-int{negative=-1 positive=1}"
			"rootvalue-string=\"str \\\"literal\\\"\"";
		apcf::SerializationRules rules = { };
		rules.flags = apcf::SerializationRules::eMinimized;
		auto serialized = cfg.serialize(rules);
		if(serialized != expect) {
			out << "// The serialized config is probably incorrect.\n";
			out << serialized << std::endl;
			return eNeutral;
		}
		return cmpConfigs(out, "expected", cfg, "serialized", Config::parse(serialized))? eSuccess : eFailure;
	}

}



int main(int, char**) {
	#define RUN_(NAME_, FN_) run(NAME_, wrapExceptions(FN_))
	auto batch = utest::TestBatch(std::cout);
	batch
		.RUN_("[serial] Simple serialization (pretty)", testSerialFullPretty)
		.RUN_("[serial] Simple serialization (minimized)", testSerialFullMinimized);
	return batch.failures() == 0? EXIT_SUCCESS : EXIT_FAILURE;
	#undef RUN_
}
