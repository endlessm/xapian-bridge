const GObject = imports.gi.GObject;
const Lang = imports.lang;

const PrefixStore = Lang.Class({
    Extends: GObject.Object,
    Name: 'PrefixStore',
    GTypeName: 'XbPrefixStore',

    _init: function (params) {
        this.parent(params);

        // create an associative array which holds the prefixes used in each
        // database, per language. Meta databases will use these for their
        // constructed queryparsers
        this._prefixes_by_lang = {};
    },

    // stores a pair if has not been already stored for the given lang
    _store: function (lang, pair, is_boolean) {
        if (!this._prefixes_by_lang.hasOwnProperty(lang)) {
            this._prefixes_by_lang[lang] = {
                prefixes: [],
                booleanPrefixes: []
            };
        }

        let lang_prefixes = this._prefixes_by_lang[lang];

        let prefix_set;
        if (is_boolean) {
            prefix_set = lang_prefixes.booleanPrefixes;
        } else {
            prefix_set = lang_prefixes.prefixes;
        }

        if (!this._prefix_list_contains_pair(prefix_set, pair))
            prefix_set.push(pair);
    },

    // Stores a field -> prefix association for a given lang, doing nothing if 
    // it already was stored
    store_prefix: function (lang, pair) {
        this._store(lang, pair, false);
    },

    // Same as store_prefix, but stores the pair as a booleanPrefix
    store_boolean_prefix: function (lang, pair) {
        this._store(lang, pair, true);
    },

    // Given a prefix map, store the prefixes and boolean prefixes accordingly
    store_prefix_map: function (lang, map) {
        map.prefixes.forEach(function (prefix) {
            this.store_prefix(lang, prefix);
        }.bind(this));
        map.booleanPrefixes.forEach(function (prefix) {
            this.store_boolean_prefix(lang, prefix);
        }.bind(this));
    },

    // Returns a prefix map that represents the unique field -> prefix
    // associations for a certain language 
    get: function (lang) {
        return this._prefixes_by_lang[lang];
    },

    // Same as get, but returns the unique association across all languages
    get_all: function () {
        let all_prefixes = {
            prefixes: [],
            booleanPrefixes: []
        };

        Object.keys(this._prefixes_by_lang).forEach(function (lang) {
            let prefix_obj = this._prefixes_by_lang[lang];
            let uniquePrefixes = prefix_obj.prefixes.filter(function (pair) {
                return !this._prefix_list_contains_pair(all_prefixes.prefixes, pair);
            }.bind(this));
            let uniqueBooleanPrefixes = prefix_obj.booleanPrefixes.filter(function (pair) {
                return !this._prefix_list_contains_pair(all_prefixes.booleanPrefixes, pair);
            }.bind(this));
            all_prefixes.prefixes = all_prefixes.prefixes.concat(uniquePrefixes);
            all_prefixes.booleanPrefixes = all_prefixes.booleanPrefixes.concat(uniqueBooleanPrefixes);
        }.bind(this));

        if (all_prefixes.prefixes.length === 0
            && all_prefixes.booleanPrefixes.length === 0) {
            return undefined;
        } else {
            return all_prefixes;
        }
    },

    // return whether a field -> prefix pair exists in prefix_list
    _prefix_list_contains_pair: function (prefix_list, pair) {
        let serialized_pair = JSON.stringify(pair);
        return prefix_list.map(JSON.stringify).indexOf(serialized_pair) !== -1;
    },
});
