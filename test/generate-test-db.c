#include <xapian-glib.h>
#include <stdlib.h>

#define N_DOCUMENTS 5

static void
add_document (XapianWritableDatabase *db)
{
  XapianDocument *doc;
  gboolean res;
  GError *error = NULL;
  
  doc = xapian_document_new ();
  g_assert_nonnull (doc);

  xapian_document_set_data (doc, "{ 'foo': 'bar' }");
  xapian_document_add_term (doc, "asd");
  xapian_document_add_term (doc, "a");

  res = xapian_writable_database_add_document (db, doc, NULL, &error);
  g_assert_true (res);
  g_assert_no_error (error);
  g_object_unref (doc);
}

int
main (int argc,
      char **argv)
{
  XapianWritableDatabase *db;
  gboolean res;
  GError *error = NULL;
  gint idx;
  const gchar *outfile = "test/testdb";
  XapianDatabaseFlags flags = XAPIAN_DB_BACKEND_CHERT;
  gboolean glass = FALSE;

  if (argc > 1 && strcmp (argv[1], "glass") == 0)
    {
      glass = TRUE;
      outfile = "test/testdb.tmp";
      flags = XAPIAN_DB_BACKEND_GLASS;
    }

  db = g_initable_new (XAPIAN_TYPE_WRITABLE_DATABASE, NULL, &error,
                       "path", outfile,
                       "action", XAPIAN_DATABASE_ACTION_CREATE_OR_OVERWRITE,
                       "flags", flags,
                       NULL);
  g_assert_nonnull (db);
  g_assert_no_error (error);

  for (idx = 0; idx < N_DOCUMENTS; idx++)
    add_document (db);

  res = xapian_writable_database_commit (db, &error);
  g_assert_true (res);
  g_assert_no_error (error);

  if (glass)
    xapian_database_compact_to_path (XAPIAN_DATABASE (db), "test/testdb.glass",
                                     XAPIAN_DB_COMPACT_SINGLE_FILE);
  xapian_database_close (XAPIAN_DATABASE (db));

  return EXIT_SUCCESS;
}
