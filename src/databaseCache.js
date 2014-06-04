const Gio = imports.gi.Gio;
const GLib = imports.gi.GLib;
const GObject = imports.gi.GObject;
const Lang = imports.lang;

GObject.ParamFlags.READWRITE = GObject.ParamFlags.READABLE | GObject.ParamFlags.WRITABLE;

const CACHE_DIR_PERMISSIONS = parseInt('744', 8);

const DatabaseCache = Lang.Class({
    Extends: GObject.Object,
    Name: 'DatabaseCache',
    GTypeName: 'XbDatabaseCache',

    Properties: {
        'cache-dir': GObject.ParamSpec.string('cache-dir',
            'Cache directory', 'Directory in which the database cache should live',
            GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT_ONLY,
            '/var/cache'),
    },

    _init: function (params) {
        this.parent(params);

        // mkdir -p $CACHE_DIR_PATH
        GLib.mkdir_with_parents(this.cache_dir, CACHE_DIR_PERMISSIONS);

        let CACHE_FILE_PATH = this.cache_dir + '/databases.json';
        this._cache_file = Gio.File.new_for_path(CACHE_FILE_PATH);

        this._entries = this._get_cache_contents();
    },

    // Stores the path value for index_name and writes to cache
    set_entry: function (index_name, path) {
        this._entries[index_name] = path;
        this.save();
    },

    // Removes the entry for index_name and writes to cache
    remove_entry: function (index_name) {
        delete this._entries[index_name];
        this.save();
    },

    // Returns an entry object whose keys are index names and values are paths
    get_entries: function () {
        return this._entries;
    },

    // Writes the entry set to the cache, overwriting any existing data
    save: function () {
        this._write_cache_contents(JSON.stringify(this._entries));
    },

    // returns the value stored in the cache. If no cache file exists, make one
    // and initialize it to an empty JSON object
    _get_cache_contents: function () {
        let cache_data;
        try {
            cache_data = this._read_cache();
        } catch (e) {
            cache_data = this._create_and_init_cache();
        }

        return JSON.parse(cache_data);
    },

    // reads the cache file, returning a string of its contents
    _read_cache: function () {
        var fstream = this._cache_file.read(null);
        var dstream = new Gio.DataInputStream({
            base_stream: fstream
        });
        var data = dstream.read_until("", null);
        fstream.close(null);
        return data[0];
    },

    // creates a cache file, initializing it to an empty JSON object
    _create_and_init_cache: function () {
        let init_contents = JSON.stringify({});
        this._write_cache_contents(init_contents);
        return init_contents;
    },

    // overwrites the cache file with the given contents string
    _write_cache_contents: function (contents) {
        this._cache_file.replace_contents(contents, null, false,
            Gio.FileCreateFlags.REPLACE_DESTINATION, null);
    },
});
