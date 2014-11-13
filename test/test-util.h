#include <glib.h>
#include <json-glib/json-glib.h>

#ifndef __TEST_UTIL_H__
#define __TEST_UTIL_H__

gchar *test_generate_json (JsonObject *object,
                           gboolean take_ownership);
void test_clear_dir (const gchar *path);

gchar *test_get_invalid_db_path (void);
gchar *test_get_sample_db_path (void);

#endif /* __TEST_UTIL_H__ */
