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

		ConfigHierarchy(const std::map<apcf::Key, apcf::RawData>&);

		ConfigHierarchy& operator=(const ConfigHierarchy&) = default;
		ConfigHierarchy& operator=(ConfigHierarchy&&) = default;

		void putKey(const apcf::Key&);

		const std::set<Key>& getSubkeys(const Key&) const;

		/** Where possible, remove lines of relationships and replace them with
		 * single top-to-bottom relationships.
		 *
		 * For example:
		 * `a => a.b => a.b.c => a.b.c.d` is replaced by
		 * `a => a.b.c.d`. */
		void collapse();

		/** The opposite of `collapse`: where possible, remove relationships
		 * where keys are distant by more than 1 level and replace them with
		 * lines of level-by-level relationships.
		 *
		 * This will never have any significant effect if `collapse` is never
		 * used on the ConfigHierarchy. */
		void stretch();
	};

}
