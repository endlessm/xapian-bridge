const Soup = imports.gi.Soup;

const DatabaseCache = imports.databaseCache.DatabaseCache;
const DatabaseManager = imports.databaseManager.DatabaseManager;
const RoutedServer = imports.routedServer.RoutedServer;

const DEFAULT_PORT = 3004;
const MIME_JSON = 'application/json';

// FIXME: this needs to be generated at make time, methinks. maybe PREFIX/var/cache?
const CACHE_DIR_PATH = 'cachedir';

function main () {
    let server = new RoutedServer({
        port: DEFAULT_PORT
    });

    let cache = new DatabaseCache({
        cache_dir: CACHE_DIR_PATH
    });
    let manager = new DatabaseManager();
    
    // load the cached database info and attempt to add those databases
    let stored_database_paths = cache.get_entries();
    for (let index_name in stored_database_paths) {
        let path = stored_database_paths[index_name];
        try {
            manager.create_db(index_name, path);
        } catch (e) {
            // if the database info is incorrect (i.e. no database at path),
            // remove that data from the cache
            if (e === DatabaseManager.ERR_INVALID_PATH) {
                cache.remove_entry(index_name);
            }
        }
    }

    // GET /:index_name - just returns OK if database exists, NOT_FOUND otherwise
    // Returns:
    //      200 - Database at index_name exists
    //      404 - No database was found at index_name
    server.get('/:index_name', function (params, query, msg) {
        let index_name = params.index_name;
        if (manager.has_db(index_name)) {
            return res(msg, Soup.Status.OK);
        } else {
            return res(msg, Soup.Status.NOT_FOUND);
        }
    });

    // PUT /:index_name - add/update the xapian database at index_name
    // Returns:
    //      200 - Database was successfully made
    //      400 - No path was specified
    //      403 - Database creation failed because path didn't exist
    server.put('/:index_name', function (params, query, msg) {
        let index_name = params.index_name;
        let path = query.path;
        if (typeof path === 'undefined') {
            return res(msg, Soup.Status.BAD_REQUEST);
        }

        if (!manager.has_db(index_name)) {
            try {
                // create the Xapian database and add it to the manager
                manager.create_db(index_name, path);

                // add the index_name/path entry to the cache
                cache.set_entry(index_name, path);
            } catch (e) {
                if (e === DatabaseManager.ERR_INVALID_PATH) {
                    return res(msg, Soup.Status.FORBIDDEN);
                } else {
                    print('ERR', e);
                    return res(msg, Soup.Status.INTERNAL_SERVER_ERROR);
                }
            }
        }

        return res(msg, Soup.Status.OK);
    });

    // DELETE /:index_name - remove the xapian database at index_name
    // Returns:
    //      200 - Database was successfully deleted
    //      404 - No database existed at index_name
    server.delete('/:index_name', function (params, query, msg) {
        let index_name = params.index_name;
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
    //     404 - No database was found at index_name
    server.get('/:index_name/query', function (params, query, msg) {
        let index_name = params.index_name;
        let q = query.q;
        let collapse_term = query.collapse;
        let limit = query.limit;
        try {
            let results = manager.query_db(index_name, q, collapse_term, limit);
            return res(msg, Soup.Status.OK, results);
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
function res (soup_msg, status_code, body) {
    soup_msg.set_status(status_code);
    if (typeof body !== 'undefined') {
        let body_str = JSON.stringify(body);
        soup_msg.set_response(MIME_JSON, Soup.MemoryUse.COPY, body_str, body_str.length);
    }
}
