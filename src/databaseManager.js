const GObject = imports.gi.GObject;
const Lang = imports.lang;
const Xapian = imports.gi.Xapian;

const QUERY_PARSER_FLAGS = Xapian.QueryParserFeature.DEFAULT | Xapian.QueryParserFeature.WILDCARD;
const STANDARD_PREFIXES = ['S', 'C', 'D', 'K', 'S', 'T', 'Z'];

const ERR_DATABASE_NOT_FOUND = 0;
const ERR_INVALID_PATH = 1;

const DatabaseManager = Lang.Class({
    Extends: GObject.Object,
    Name: 'DatabaseManager',
    GTypeName: 'XbDatabaseManager',

    _init: function (params) {
        this.parent(params);

        this._databases = {};
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
    create_db: function (index_name, path) {
        try {
            let xapian_db = new Xapian.Database({
                path: path
            });
            xapian_db.init(null);
            this._databases[index_name] = xapian_db;
        } catch (e) {
            throw ERR_INVALID_PATH;
        }
    },

    // Deletes the database indexed at index_name (if any)
    remove_db: function (index_name) {
        if (this.has_db(index_name)) {
            delete this._databases[index_name];
        } else {
            throw ERR_DATABASE_NOT_FOUND;
        }
    },

    // If database exists at index_name, queries it with:
    //     q: querystring that's parseable by a QueryParser
    //     collapse_term: read http://xapian.org/docs/collapsing.html
    //     limit: max number of results to return
    // and then returns a results object with properties:
    //     numResults: integer number of results being returned
    //     results: array of strings for every result, sorted by weight
    //
    // If no such database exists, throw ERR_DATABASE_NOT_FOUND
    query_db: function (index_name, q, collapse_term, limit) {
        if (this.has_db(index_name)) {
            let db = this._databases[index_name];

            // setup the QueryParser so that it recognizes the standard Xapian
            // prefixes (read http://xapian.org/docs/omega/termprefixes.html)
            let query_parser = new Xapian.QueryParser({
                database: db    
            });
            STANDARD_PREFIXES.forEach(function (prefix) {
                query_parser.add_prefix(prefix, prefix);
            });
            query_parser.add_boolean_prefix('Q', 'Q', false);
            let enquire = new Xapian.Enquire({
                database: db
            });
            enquire.init(null);

            let parsed_query = query_parser.parse_query(q, QUERY_PARSER_FLAGS);
            enquire.set_query(parsed_query, parsed_query.get_length());

            let matches = enquire.get_mset(0, limit);
            let iter = matches.get_begin();

            let ret = {
                numResults: matches.get_size(),
                results: []
            };

            while (iter.next()) {
                ret.results.push(JSON.parse(iter.get_document().get_data()));
                ret.numResults++;
            }

            return ret;
        }

        throw ERR_DATABASE_NOT_FOUND;
    }
});
