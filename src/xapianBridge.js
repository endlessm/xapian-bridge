const Soup = imports.gi.Soup;

const RoutedServer = imports.routedServer.RoutedServer;

const DEFAULT_PORT = 3004;

function main () {
    let server = new RoutedServer({
        port: DEFAULT_PORT
    });

    // PUT /:indexName - add/update the xapian database at indexName
    server.put('/:indexName', function (params, query, msg) {
        let indexName = params.indexName;
        let path = query.path;
        // TODO: prepare/update a xapian database at path
    });

    // DELETE /:indexName - remove the xapian database at indexName
    server.delete('/:indexName', function (params, query, msg) {
        let indexName = params.indexName;
        // TODO: delete the xapian server at indexName
    });

    // GET /:indexName/query - query an index
    server.get('/:indexName/query', function (params, query, msg) {
        let indexName = params.indexName;
        let q = query.q;
        let collapse = query.collapse;
        let limit = query.limit;
        // TODO: query the xapian database at indexName, respond with json results
    });

    server.run();
}
