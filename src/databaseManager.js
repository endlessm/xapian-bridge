const GObject = imports.gi.GObject;
const Lang = imports.lang;
const Xapian = imports.gi.Xapian;

const PrefixStore = imports.prefixStore.PrefixStore;

const QUERY_PARSER_FLAGS = Xapian.QueryParserFeature.DEFAULT
                         | Xapian.QueryParserFeature.WILDCARD;

// TODO: these should be configurable
const STANDARD_PREFIXES = {
    prefixes: [
        {field: 'title', prefix: 'S'}, 
        {field: 'exact_title', prefix: 'XEXACTS'}, 
    ],
    booleanPrefixes: [
        {field: 'tag', prefix: 'K'}, 
        {field: 'id', prefix: 'Q'}, 
    ]
}
const PREFIX_METADATA_KEY = 'XbPrefixes';
const STOPWORDS_METADATA_KEY = 'XbStopwords';

const ERR_DATABASE_NOT_FOUND = 0;
const ERR_INVALID_PATH = 1;
const ERR_UNSUPPORTED_LANG = 2;

const DatabaseManager = Lang.Class({
    Extends: GObject.Object,
    Name: 'DatabaseManager',
    GTypeName: 'XbDatabaseManager',

    _init: function (params) {
        this.parent(params);

        this._databases = {};
        this._database_langs = {};
        this._database_qp = {};

        // the prefix store manages stored field -> prefix associations, and
        // returns unions of unique associations for use in meta databases
        this._prefix_store = new PrefixStore();

        let noneStemmer = new Xapian.Stem();
        noneStemmer.init(null);
        this._stemmers = {
            none: noneStemmer
        };

        // setup the meta database, which will let us query all database via
        // the query_all method
        this._meta_db = this._new_meta_db();
    },

    // Returns boolean for if the manager has a database indexed at index_name
    has_db: function (index_name) {
        let databaseNames = Object.keys(this._databases);
        return (databaseNames.indexOf(index_name) !== -1);
    },

    // Returns the path used by the database indexed at index_name
    // If no such database exists, throws ERR_DATABASE_NOT_FOUND
    db_path: function (index_name) {
        let database = this._databases[index_name];
        if (typeof database !== 'undefined') {
            return database.path;
        }

        throw ERR_DATABASE_NOT_FOUND;
    },

    _register_prefixes: function (xapian_db, qp, lang, index_name) {
        try {
            // attempt to read the database's custom prefix association metadata
            let metadata_json = xapian_db.get_metadata(PREFIX_METADATA_KEY);
            let prefix_metadata = JSON.parse(metadata_json);

            // store the prefix association metadata for this given lang, so
            // the various meta databases which query over this database can
            // use its prefixes
            this._prefix_store.store_prefix_map(lang, prefix_metadata);

            // register the field -> prefix associations for this database's
            // queryparser
            this._add_queryparser_prefixes(qp, prefix_metadata);
        } catch (e) {
            // if there was an error storing the database's prefixes, log the
            // error and go with the "default" prefix map in STANDARD_PREFIXES
            printerr ('Could not register prefixes for database', index_name, e);
            this._add_queryparser_prefixes(qp, STANDARD_PREFIXES);
        }
    },

    _register_stopwords: function (xapian_db, qp, index_name) {
        try {
            let stopper = new Xapian.SimpleStopper();
            let stopwords_json = xapian_db.get_metadata(STOPWORDS_METADATA_KEY);
            let stopwords_data = JSON.parse(stopwords_json);
            stopwords_data.forEach(function (word) {
                stopper.add(word);
            });
            qp.stopper = stopper;
        } catch (e) {
            printerr('Could not add stop words for database', index_name, e);
        }
    },

    // Creates a new XapianDatabase with path, and indexes it by index_name.
    // Overwrites any existing database with the same name
    // If creation fails, throws ERR_INVALID_PATH
    create_db: function (index_name, path, lang) {
        let xapian_db, db_was_overwritten;
        try {
            // remember whether this will overwrite an existing db
            db_was_overwritten = this.has_db(index_name);

            xapian_db = new Xapian.Database({
                path: path
            });
            xapian_db.init(null);
        } catch (e) {
            throw ERR_INVALID_PATH;
        }

        let stemmer;
        try {
            if (typeof this._stemmers[lang] === 'undefined') {
                stemmer = new Xapian.Stem({
                    language: lang
                });
                stemmer.init(null);
                this._stemmers[lang] = stemmer;
            }
        } catch (e) {
            throw ERR_UNSUPPORTED_LANG;
        }

        // create a queryparser for this particular database, stemming by its
        // registered language
        let qp = new Xapian.QueryParser();
        qp.set_stemming_strategy(Xapian.StemStrategy.STEM_SOME);
        qp.set_stemmer(this._stemmers[lang]);
        qp.set_database(xapian_db);
        this._register_prefixes(xapian_db, qp, lang, index_name);
        this._register_stopwords(xapian_db, qp, index_name);

        this._databases[index_name] = xapian_db;
        this._database_langs[index_name] = lang;
        this._database_qp[index_name] = qp;

        // if we just overwrote an existing database, we need to build the
        // meta_db from scratch since there's no Xapian::Database remove_db
        // method. Otherwise we can just add this newly created database
        // to the meta_db
        if (db_was_overwritten) {
            this._meta_db = this._new_meta_db();
        } else {
            this._meta_db.add_database(xapian_db);
        }
    },
    
    // returns a new Xapian database which has all currently managed databases
    // as children to facilitate queries across all databases. If lang is
    // specified, only return databases which are registered under that lang
    _new_meta_db: function (lang) {
        // if lang was unspecified, we're selecting all databases
        let selectAll = (typeof lang === 'undefined');

        let db = new Xapian.Database();
        db.init(null);

        Object.keys(this._databases).filter(function (index_name) {
            return selectAll || this._database_langs[index_name] === lang; 
        }.bind(this)).forEach(function (index_name) {
            let child_db = this._databases[index_name];
            db.add_database(child_db);
        }.bind(this));

        return db;
    },

    // returns a new Xapian QueryParser for a given language and associates it
    // with the given database. The prefixes registered for this queryparser
    // derive from the PrefixStore's prefix map for the given lang. If lang is
    // 'all', the PrefixStore aggregates a union of all its known prefixes
    _new_meta_qp: function (lang, db) {
        let qp = new Xapian.QueryParser();
        qp.set_stemming_strategy(Xapian.StemStrategy.STEM_SOME);
        if (lang === 'all')
            qp.set_stemmer(this._stemmers.none);
        else
            qp.set_stemmer(this._stemmers[lang]);
        qp.set_database(db);

        let prefix_map;
        if (lang === 'all') {
            prefix_map = this._prefix_store.get_all();
        } else {
            prefix_map = this._prefix_store.get(lang);            
        }

        if (typeof prefix_map === 'undefined') {
            prefix_map = STANDARD_PREFIXES;
        }

        this._add_queryparser_prefixes(qp, prefix_map);

        return qp;
    },

    // registers the prefixes and booleanPrefixes in prefix_map to qp
    _add_queryparser_prefixes: function (qp, prefix_map) {
        prefix_map.prefixes.forEach(function (pair) {
            qp.add_prefix(pair.field, pair.prefix);
        });
        prefix_map.booleanPrefixes.forEach(function (pair) {
            qp.add_boolean_prefix(pair.field, pair.prefix, false);
        });
    },

    // Deletes the database indexed at index_name (if any)
    remove_db: function (index_name) {
        if (this.has_db(index_name)) {
            delete this._databases[index_name];

            // rebuild meta_db since there's no database remove_db method
            this._meta_db = this._new_meta_db();
            delete this._database_langs[index_name];
            delete this._database_qp[index_name];
        } else {
            throw ERR_DATABASE_NOT_FOUND;
        }
    },

    // If database exists at index_name, queries it with the following options:
    //     q: querystring that's parseable by a QueryParser
    //     collapse_key: read http://xapian.org/docs/collapsing.html
    //     limit: max number of results to return
    //     offset: offset from which to start returning results
    //     cutoff: percent between (0, 100) for the Enquire percent cutoff
    //
    // If no such database exists, throw ERR_DATABASE_NOT_FOUND
    query_db: function (index_name, options) {
        if (this.has_db(index_name)) {
            let db = this._databases[index_name];
            let qp = this._database_qp[index_name];
            return this._query(db, qp, options);
        }

        throw ERR_DATABASE_NOT_FOUND;
    },

    query_lang: function (lang, options) {
        let meta_lang_db = this._new_meta_db(lang);
        let meta_lang_qp = this._new_meta_qp(lang, meta_lang_db);
        return this._query(meta_lang_db, meta_lang_qp, options);
    },

    // Queries all databases
    query_all: function (options) {
        let meta_qp = this._new_meta_qp('all', this._meta_db);
        return this._query(this._meta_db, meta_qp, options);
    },

    // Checks if the given database is empty (has no documents). Empty databases
    // cause problems with Enquire, so we need to assert that a db isn't empty
    // before making an Enquire for it
    _db_is_empty: function (db) {
        // "If the backend does not support UUIDs or this database has no
        // subdatabases, the UUID will be empty."
        // read: http://xapian.org/docs/apidoc/html/classXapian_1_1Database.html
        return db.get_uuid().length === 0;
    },

    // Queries db with the given parameters, and returns an object with:
    //     numResults: integer number of results being returned
    //     offset: index from which results were gathered
    //     results: array of strings for every result doc, sorted by weight
    _query: function (db, qp, options) {
        if (this._db_is_empty(db)) {
            return {
                numResults: 0,
                offset: 0,
                totalResults: 0,
                results: []
            }
        }

        let parsed_query = qp.parse_query_full(options.q, QUERY_PARSER_FLAGS, '');

        let enquire = new Xapian.Enquire({
            database: db
        });
        enquire.init(null);

        if (!isNaN(options.cutoff)) {
            enquire.set_cutoff(options.cutoff);
        }

        if (typeof options.collapse_key !== 'undefined') {
            enquire.set_collapse_key(options.collapse_key);
        }

        enquire.set_query(parsed_query, parsed_query.get_length());
        let matches = enquire.get_mset(options.offset, options.limit);
        let iter = matches.get_begin();
        let docs = [];

        while (iter.next()) {
            docs.push(iter.get_document().get_data());
        }

        return {
            numResults: docs.length,
            offset: options.offset,
            results: docs.map(JSON.parse)
        };
    }
});
