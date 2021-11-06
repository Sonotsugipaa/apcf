#include <test_tools.hpp>

#include <yaconfig.hpp>

#include <iostream>
#include <fstream>
#include <cassert>
#include <random>
#include <chrono>



namespace {

	using yacfg::Config;

	constexpr auto eNeutral = utest::ResultType::eNeutral;

	constexpr const char cfgFilePath[] = "test/big_autogen.cfg";

	auto rng = std::minstd_rand();


	std::string genKey(unsigned depth) {
		#define PUSH_DIGITS_ { for(size_t i=0; i < levelLength; ++i) r.push_back(digits[rng() % base]); }
		constexpr unsigned levelLength = 5;
		constexpr unsigned base = 62;
		constexpr const char digits[] =
			"0123456789"
			"abcdefghijklmnopqrstuvwxyz"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		static_assert(base == sizeof(digits)-1);
		std::string r;
		r.reserve(levelLength + (depth * (levelLength + 1)));
		PUSH_DIGITS_
		while(depth > 0) {
			r.push_back('.');
			PUSH_DIGITS_;
			-- depth;
		}
		return r;
		#undef PUSH_DIGITS_
	}

	yacfg::RawData genStr() {
		constexpr unsigned length = 32;
		constexpr unsigned base = 62;
		constexpr const char digits[] =
			"0123456789"
			"abcdefghijklmnopqrstuvwxyz"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ";
		static_assert(base == sizeof(digits)-1);
		yacfg::RawData r = yacfg::RawData::allocString(length);
		for(unsigned i=0; i < length; ++i) {
			r.data.stringValue.ptr[i] = digits[rng() % base];
		}
		return r;
	}

	yacfg::RawData genFloat() {
		auto gen =
			yacfg::float_t(rng() % 0x100) +
			yacfg::float_t(0x100) / yacfg::float_t(rng() % 0x100);
		return gen;
	}

	yacfg::RawData genArray() {
		constexpr unsigned size0 = 4;
		constexpr unsigned size1 = 3;
		yacfg::RawData r = yacfg::RawData::allocArray(size0);
		for(unsigned i=0; i < size0; ++i) {
			auto& arrayVal = r.data.arrayValue[i];
			arrayVal = yacfg::RawData::allocArray(size1);
			for(unsigned j=0; j < size1; ++j) {
				arrayVal.data.arrayValue[j] = yacfg::int_t(rng() % 0x100);
			}
		}
		return r;
	};

	void genEntries(
			Config* cfg,
			const std::string& superKey,
			unsigned minDepth, unsigned maxDepth
	) {
		if(minDepth > maxDepth) return;
		if(minDepth > 0) {
			genEntries(cfg,
				superKey + '.' + genKey(0),
				minDepth - 1, maxDepth - 1);
		} else {
			constexpr unsigned REPEAT_MAX_HALF = 3;
			unsigned repeat = (rng() % REPEAT_MAX_HALF) * 4;
			for(unsigned i=0; i < repeat; ++i) {
				yacfg::Key key = yacfg::Key(superKey + '.' + genKey(0));
				switch(rng() % 3) {
					default: [[fallthrough]];
					case 0: cfg->set(key, 1 == rng() % 1); break;
					case 1: cfg->set(key, yacfg::int_t(rng())); break;
					case 2: cfg->set(key, genFloat()); break;
					case 3: cfg->set(key, genStr()); break;
					case 4: cfg->set(key, genArray()); break;
				}
			}
			genEntries(cfg,
				superKey,
				minDepth + 1, maxDepth);
		}
	}

	uint_fast64_t nowUs() {
		using Clock = std::chrono::steady_clock;
		using Us = std::chrono::duration<uint_fast64_t, std::micro>;
		using std::chrono::duration_cast;
		static const auto epoch = Clock::now();
		return duration_cast<Us>(Clock::now() - epoch).count();
	}


	utest::ResultType testPerformanceRd(std::ostream& out) {
		auto begTime = nowUs();
		Config cfg = Config::read(std::ifstream(cfgFilePath));
		auto endTime = (nowUs() - begTime);
		out
			<< "Parsing " << cfg.keyCount()
			<< " entries took " << endTime << "us" << std::endl;
		return eNeutral;
	}


	utest::ResultType testPerformanceWr(std::ostream& out) {
		using Rules = yacfg::SerializationRules;
		constexpr unsigned ROOT_GROUPS = 0x20;
		Config cfg;
		for(unsigned i=0; i < ROOT_GROUPS; ++i) {
			genEntries(&cfg, genKey(0), 0, 16);
		}
		auto begTime = nowUs();
		auto rules = Rules {
			.indentationSize = 3,
			.flags = Rules::ePretty | Rules::eCompactArrays };
		cfg.write(std::ofstream(cfgFilePath), rules);
		auto endTime = (nowUs() - begTime);
		out
			<< "Serializing " << cfg.keyCount()
			<< " entries took " << endTime << "us" << std::endl;
		return eNeutral;
	}

}



int main(int, char**) {
	auto batch = utest::TestBatch(std::cout);
	batch
		.run("Performance test (serialize)", testPerformanceWr)
		.run("Performance test (parse)", testPerformanceRd);
	return batch.failures() == 0? EXIT_SUCCESS : EXIT_FAILURE;
}
