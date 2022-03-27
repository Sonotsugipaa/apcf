#include <apcf_fwd.hpp>

#include <map>
#include <set>



namespace apcf {

	class ConfigHierarchy {
	private:
		friend Config;
		std::map<Key, std::set<Key>> tree_;

		bool collapse_(KeySpan, KeySpan parent);

	public:
		ConfigHierarchy() = default;
		ConfigHierarchy(const ConfigHierarchy&) = default;
		ConfigHierarchy(ConfigHierarchy&&) = default;

		ConfigHierarchy(const std::map<Key, RawData>&);

		ConfigHierarchy& operator=(const ConfigHierarchy&) = default;
		ConfigHierarchy& operator=(ConfigHierarchy&&) = default;

		/** Insert every level of the given key into the hierarchy. */
		void putKey(const Key&);

		/** Returns a (reference to a) set of all the keys within the
		 * hierarchy whose "parent" key matches the given argument.
		 *
		 * Note that the returned subkeys do not necessarily have an
		 * associated value: if a Config contains entries with keys
		 * "a.b" and "a.c.d", the subkeys of "a" are "a.b" and "a.c". */
		const std::set<Key>& getSubkeys(const Key&) const;

		/** If the given key has exactely one subkey, recursively replaces
		 * the argument with it; returns the first subkey that has zero
		 * or multiple subkeys.
		 *
		 * The returned reference may point to a Key instance owned by the
		 * ConfigHierarchy, so it should be copied (rather than referenced)
		 * if the latter is modified or destroyed. */
		const Key& autocomplete(const Key&) const;
	};

}
