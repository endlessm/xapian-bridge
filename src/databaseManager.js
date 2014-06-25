const GObject = imports.gi.GObject;
const Lang = imports.lang;
const Xapian = imports.gi.Xapian;

const QUERY_PARSER_FLAGS = Xapian.QueryParserFeature.DEFAULT
                         | Xapian.QueryParserFeature.WILDCARD;
const STANDARD_PREFIXES = ['S', 'C', 'D', 'K', 'S', 'T', 'Z'];

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

        let noneStemmer = new Xapian.Stem();
        noneStemmer.init(null);
        this._stemmers = {
            none: noneStemmer
        };

        // setup the meta database, which will let us query all database via
        // the query_all method
        this._meta_db = this._new_meta_db();

        this._query_parser = new Xapian.QueryParser();
        this._query_parser.set_stemming_strategy(Xapian.StemStrategy.STEM_SOME);

        // setup the QueryParser so that it recognizes the standard Xapian
        // prefixes (read http://xapian.org/docs/omega/termprefixes.html)
        STANDARD_PREFIXES.forEach(function (prefix) {
            this._query_parser.add_prefix(prefix, prefix);
        }.bind(this));
        this._query_parser.add_boolean_prefix('Q', 'Q', false);
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

        this._databases[index_name] = xapian_db;
        this._database_langs[index_name] = lang;

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

    // Deletes the database indexed at index_name (if any)
    remove_db: function (index_name) {
        if (this.has_db(index_name)) {
            delete this._databases[index_name];

            // rebuild meta_db since there's no database remove_db method
            this._meta_db = this._new_meta_db();
            delete this._database_langs[index_name];
        } else {
            throw ERR_DATABASE_NOT_FOUND;
        }
    },

    // If database exists at index_name, queries it with:
    //     q: querystring that's parseable by a QueryParser
    //     collapse_key: read http://xapian.org/docs/collapsing.html
    //     limit: max number of results to return
    //
    // If no such database exists, throw ERR_DATABASE_NOT_FOUND
    query_db: function (index_name, q, collapse_key, limit) {
        if (this.has_db(index_name)) {
            let lang = this._database_langs[index_name];
            let db = this._databases[index_name];
            return this._query(db, q, collapse_key, limit, lang);
        }

        throw ERR_DATABASE_NOT_FOUND;
    },

    query_lang: function (lang, q, collapse_key, limit) {
        let meta_lang_db = this._new_meta_db(lang);
        return this._query(meta_lang_db, q, collapse_key, limit, lang);
    },

    // Queries all databases
    query_all: function (q, collapse_key, limit) {
        // default language for _all queries is none
        let lang = 'none';
        return this._query(this._meta_db, q, collapse_key, limit, lang);
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
    //     results: array of strings for every result doc, sorted by weight
    _query: function (db, q, collapse_key, limit, lang) {
        if (this._db_is_empty(db)) {
            return {
                numResults: 0,
                results: []
            }
        }

        let stemmer = this._stemmers[lang];

        this._query_parser.database = db;
        this._query_parser.set_stemmer(stemmer);
        let parsed_query = this._query_parser.parse_query_full(q, QUERY_PARSER_FLAGS, '');

        let enquire = new Xapian.Enquire({
            database: db
        });
        enquire.init(null);

        if (typeof collapse_key !== 'undefined') {
            enquire.set_collapse_key(collapse_key);
        }

        enquire.set_query(parsed_query, parsed_query.get_length());

        let matches = enquire.get_mset(0, limit);
        let iter = matches.get_begin();
        let docs = [];

        while (iter.next()) {
            docs.push(iter.get_document().get_data());
        }

        return {
            numResults: docs.length,
            results: docs.map(JSON.parse)
        };
    }
});
