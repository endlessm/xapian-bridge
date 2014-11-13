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

  db = xapian_writable_database_new ("test/testdb", XAPIAN_DATABASE_ACTION_CREATE_OR_OVERWRITE, &error);
  g_assert_nonnull (db);
  g_assert_no_error (error);

  for (idx = 0; idx < N_DOCUMENTS; idx++)
    add_document (db);

  res = xapian_writable_database_commit (db, &error);
  g_assert_true (res);
  g_assert_no_error (error);

  return EXIT_SUCCESS;
}
