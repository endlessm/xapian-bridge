const GLib = imports.gi.GLib;

const config = imports.config;
const DatabaseCache = imports.databaseCache.DatabaseCache;

describe('Database cache', function () {
    let cache, tmp_dir_path;
    beforeEach(function () {
        tmp_dir_path = GLib.get_tmp_dir() + '/xapian-glib-tests';
        cache = new DatabaseCache({
            cache_dir: tmp_dir_path
        });
    });

    it('should be constructable', function () {
        expect(cache).toBeDefined();
    });

    it('should allow cache dir to be a construct property', function () {
        expect(cache.cache_dir).toBe(tmp_dir_path);
    });

    it('should create a xapian-glib dir in cache-dir', function () {
        let fileExists = GLib.file_test(tmp_dir_path + '/xapian-glib', GLib.FileTest.IS_DIR);

        expect(fileExists).toBe(true);
    });

    it('should read and write its entries from/to its cache', function () {
        cache.set_entry('foo', '/some/path');
        cache.set_entry('bar', '/some/other/path');

        // ensure the cache's in-memory entries are what we expect
        expect(cache.get_entries()).toEqual({
            'foo': { path: '/some/path' },
            'bar': { path: '/some/other/path' }
        });

        // make a new cache which reads the now-written cache to init its entries
        let new_cache = new DatabaseCache({
            cache_dir: cache.cache_dir
        });
        // ensure that the cached entries reflect the in-memory entries
        expect(new_cache.get_entries()).toEqual(cache.get_entries())
    });

    it('should remove its entries from the cache', function () {
        cache.set_entry('foo', '/some/path');
        cache.set_entry('bar', '/some/other/path');
        cache.remove_entry('foo');

        // ensure the cache's in-memory entries are what we expect
        expect(cache.get_entries()).toEqual({
            'bar': { path: '/some/other/path' }
        });

        // make a new cache which reads the now-written cache to init its entries
        let new_cache = new DatabaseCache({
            cache_dir: cache.cache_dir
        });
        // ensure that the cached entries reflect the in-memory entries
        expect(new_cache.get_entries()).toEqual(cache.get_entries());
    });

    it('should support a lang parameter', function () {
        cache.set_entry('foo', '/some/path', 'gallifreyan');
        cache.set_entry('bar', '/some/other/path', 'swaghili');

        // ensure the cache's in-memory entries are what we expect
        expect(cache.get_entries()).toEqual({
            'foo': { path: '/some/path', lang: 'gallifreyan' },
            'bar': { path: '/some/other/path', lang: 'swaghili' }
        });

        // make a new cache which reads the now-written cache to init its entries
        let new_cache = new DatabaseCache({
            cache_dir: cache.cache_dir
        });
        // ensure that the cached entries reflect the in-memory entries
        expect(new_cache.get_entries()).toEqual(cache.get_entries());
    });
});
