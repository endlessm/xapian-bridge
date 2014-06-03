const Soup = imports.gi.Soup;

const RoutedServer = imports.routedServer.RoutedServer;
const DatabaseManager = imports.databaseManager.DatabaseManager;

const DEFAULT_PORT = 3004;
const MIME_JSON = 'application/json';

function main () {
    let server = new RoutedServer({
        port: DEFAULT_PORT
    });

    let manager = new DatabaseManager();

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
                manager.create_db(index_name, path);
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
            manager.remove_db(index_name);
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
