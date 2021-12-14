#pragma once

#include <apcf.hpp>



namespace apcf {

	template<typename T> std::optional<T> getCfgValue(const Config&, const Key&);

	template<> std::optional<bool> getCfgValue<bool>(const Config& cfg, const Key& key) { return cfg.getBool(key); }
	template<> std::optional<int_t> getCfgValue<int_t>(const Config& cfg, const Key& key) { return cfg.getInt(key); }
	template<> std::optional<float_t> getCfgValue<float_t>(const Config& cfg, const Key& key) { return cfg.getFloat(key); }
	template<> std::optional<string_t> getCfgValue<string_t>(const Config& cfg, const Key& key) { return cfg.getString(key); }
	template<> std::optional<array_span_t> getCfgValue<array_span_t>(const Config& cfg, const Key& key) { return cfg.getArray(key); }

	template<typename T> void setCfgValue(Config&, const Key&, T);

	template<> void setCfgValue<bool>(Config& cfg, const Key& key, bool value) { cfg.setBool(key, value); }
	template<> void setCfgValue<int_t>(Config& cfg, const Key& key, int_t value) { cfg.setInt(key, value); }
	template<> void setCfgValue<float_t>(Config& cfg, const Key& key, float_t value) { cfg.setFloat(key, value); }
	template<> void setCfgValue<string_t>(Config& cfg, const Key& key, string_t value) { cfg.setString(key, std::move(value)); }
	template<> void setCfgValue<array_t>(Config& cfg, const Key& key, array_t value) { cfg.setArray(key, std::move(value)); }


	template<typename T>
	T coalesceCfgValue(Config& cfg, const Key& key, const T& defaultValue) {
		auto r = getCfgValue<T>(cfg, key);
		if(r.has_value()) return r.value();
		else return defaultValue;
	}

	template<typename T, typename Fn, typename... FnArgs>
	T coalesceCfgValueFn(Config& cfg, const Key& key, Fn defaultValueFn, FnArgs... defaultValueFnArgs) {
		auto r = getCfgValue<T>(cfg, key);
		if(r.has_value()) return r.value();
		else return defaultValueFn(defaultValueFnArgs...);
	}


	template<typename T>
	void setDefaultCfgValue(Config& cfg, const Key& key, const T& value) {
		if(! cfg.get(key).has_value()) {
			setCfgValue(cfg, key, value);
		}
	}

	template<typename T, typename Fn, typename... FnArgs>
	void setDefaultCfgValueFn(Config& cfg, const Key& key, Fn defaultValueFn, FnArgs... defaultValueFnArgs) {
		if(! cfg.get(key).has_value()) {
			setCfgValue(cfg, key, defaultValueFn(defaultValueFnArgs...));
		}
	}

}
