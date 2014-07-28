const Soup = imports.gi.Soup;
const GLib = imports.gi.GLib;

const DatabaseCache = imports.databaseCache;
const DatabaseManager = imports.databaseManager;
const RoutedServer = imports.routedServer;

const DEFAULT_PORT = 3004;
const MIME_JSON = 'application/json';
const SYSTEMD_LISTEN_FD = 3;

// List of the pseudo index names
var META_DATABASE_NAMES = ['_all'];
// Methods available to meta database resources. Used in Allow headers
const META_DATABASE_METHODS = 'GET';

function main () {
    let server = new RoutedServer.RoutedServer();
    let fd = NaN;
    // If this service is launched by systemd, LISTEN_PID and LISTEN_FDS will
    // be set by systemd and we should set up the server to use the fd instead
    // of a port
    if (GLib.getenv("LISTEN_PID") !== null) {
        let fd_string = GLib.getenv("LISTEN_FDS");
        if (fd_string !== null) {
            fd = parseInt(fd_string, 10);
        }
    }
    if (fd === 1) {
        server.listen_fd(SYSTEMD_LISTEN_FD, 0);
    } else {
        server.listen_local(DEFAULT_PORT, 0);
    }

    let cache = new DatabaseCache.DatabaseCache();
    let manager = new DatabaseManager.DatabaseManager();
    
    // load the cached database info and attempt to add those databases
    let stored_database_data = cache.get_entries();
    for (let index_name in stored_database_data) {
        let db_data = stored_database_data[index_name];
        try {
            manager.create_db(index_name, db_data.path, db_data.lang);
        } catch (e) {
            // error creating the entry in the cache, so delete it
            cache.remove_entry(index_name);
        }
    }

    // GET /:index_name - just returns OK if database exists, NOT_FOUND otherwise
    // Returns:
    //      200 - Database at index_name exists
    //      404 - No database was found at index_name
    server.get('/:index_name', function (params, query, msg) {
        let index_name = params.index_name;
        if (manager.has_db(index_name) || META_DATABASE_NAMES.indexOf(index_name) !== -1) {
            return res(msg, Soup.Status.OK);
        } else {
            return res(msg, Soup.Status.NOT_FOUND);
        }
    });

    // PUT /:index_name - add/update the xapian database at index_name
    // Returns:
    //      200 - Database was successfully made
    //      400 - No path was specified or lang is unsupported
    //      403 - Database creation failed because path didn't exist
    //      405 - Attempt was made to create a database at reserved index
    server.put('/:index_name', function (params, query, msg) {
        let index_name = params.index_name;
        let path = query.path;
        let lang = query.lang;

        if (typeof path === 'undefined') {
            return res(msg, Soup.Status.BAD_REQUEST);
        }

        if (typeof lang === 'undefined') {
            lang = 'none';
        }

        if (META_DATABASE_NAMES.indexOf(index_name) !== -1) {
            return res(msg, Soup.Status.METHOD_NOT_ALLOWED, {
                'Allow': META_DATABASE_METHODS
            });
        }

        try {
            // create the Xapian database and add it to the manager
            manager.create_db(index_name, path, lang);

            // add the index_name/path entry to the cache
            cache.set_entry(index_name, path, lang);

            // since a database now exists for lang, ensure a meta database name
            // exists for it
            let lang_index_name = '_' + lang;
            if (META_DATABASE_NAMES.indexOf(lang_index_name) === -1) {
                META_DATABASE_NAMES.push(lang_index_name);
            }
        } catch (e) {
            if (e === DatabaseManager.ERR_INVALID_PATH ||
                e === DatabaseManager.ERR_UNSUPPORTED_LANG) {
                return res(msg, Soup.Status.FORBIDDEN);
            } else {
                print('ERR', e);
                return res(msg, Soup.Status.INTERNAL_SERVER_ERROR);
            }
        }

        return res(msg, Soup.Status.OK);
    });

    // DELETE /:index_name - remove the xapian database at index_name
    // Returns:
    //      200 - Database was successfully deleted
    //      404 - No database existed at index_name
    //      405 - Attempt was made to delete a reserved index, like "_all"
    server.delete('/:index_name', function (params, query, msg) {
        let index_name = params.index_name;

        if (META_DATABASE_NAMES.indexOf(index_name) !== -1) {
            return res(msg, Soup.Status.METHOD_NOT_ALLOWED, {
                'Allow': META_DATABASE_METHODS
            });
        }

        try {
            // remove the database from the manager so it can't be queried
            manager.remove_db(index_name);

            // remove the index_name/path entry from the cache
            cache.remove_entry(index_name);
            return res(msg, Soup.Status.OK);
        } catch (e) {
            return res(msg, Soup.Status.NOT_FOUND);
        }
    });

    // GET /:index_name/query - query an index
    // Returns:
    //     200 - Query was successful
    //     400 - One of the required parameters wasn't specified (e.g. limit)
    //     404 - No database was found at index_name
    server.get('/:index_name/query', function (params, query, msg) {
        let index_name = params.index_name;
        let q = query.q;
        let collapse_key = query.collapse;
        let limit = parseInt(query.limit);
        let offset = parseInt(query.offset);
        if (isNaN(limit) || isNaN(offset)) {
            return res(msg, Soup.Status.BAD_REQUEST);
        }

        try {
            let results;
            if (META_DATABASE_NAMES.indexOf(index_name) !== -1) {
                if (index_name === '_all') {
                    results = manager.query_all(q, collapse_key, limit, offset);
                } else {
                    let lang = index_name.slice(1); // index_name === '_{lang}'
                    results = manager.query_lang(lang, q, collapse_key, limit, offset);
                }
            } else {
                results = manager.query_db(index_name, q, collapse_key, limit, offset);
            }
            return res(msg, Soup.Status.OK, undefined, results);
        } catch (e) {
            if (e === DatabaseManager.ERR_DATABASE_NOT_FOUND) {
                return res(msg, Soup.Status.NOT_FOUND);
            } else {
                print('ERR', e);
                return res(msg, Soup.Status.INTERNAL_SERVER_ERROR);
            }
        }
    });

    server.run();
}

// Sets up a SoupMessage to respond
function res (soup_msg, status_code, headers, body) {
    soup_msg.set_status(status_code);

    if (typeof headers !== 'undefined') {
        let res_headers = soup_msg.response_headers;
        Object.keys(headers).forEach(function (name) {
            let val = headers[name];
            res_headers.replace(name, val);
        });
    }

    if (typeof body !== 'undefined') {
        let body_str = JSON.stringify(body);
        soup_msg.set_response(MIME_JSON, Soup.MemoryUse.COPY, body_str, body_str.length);
    }
}
