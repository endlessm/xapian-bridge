const Soup = imports.gi.Soup;
const GObject = imports.gi.GObject;
const Lang = imports.lang;

const Router = imports.router.Router;

const RoutedServer = new Lang.Class({
    Name: 'RoutedServer',
    GTypeName: 'RoutedServer',
    Extends: Soup.Server,

    SUPPORTED_HTTP_METHODS: ['GET', 'POST', 'PUT', 'DELETE', 'HEAD'],

    _init: function (props) {
        this.parent(props);

        this._router = new Router();

        // sets up methods like server.get(...), server.post(...) for all HTTP
        // methods, a la express.js
        this.SUPPORTED_HTTP_METHODS.forEach(function (method) {
            this[method.toLowerCase()] = function (path, cb) {
                this._router.add_route(method, path, cb);
            }.bind(this);
        }.bind(this));

        // setup the singleton handler for all HTTP requests; this will use
        // the router to actually delegate requests to set handlers
        this.add_handler(null, this._meta_handler.bind(this));
    },

    _meta_handler: function (server, msg, path, query, client) {
        // Soup defaults query to null when there's no querystring, which
        // doesn't make for a pleasant/typesafe API. Better to default to an
        // empty object which we can safely check properties on
        if (query === null) {
            query = {};
        }
        this._router.handle_route(msg.method, path, query, msg);
    }
});
