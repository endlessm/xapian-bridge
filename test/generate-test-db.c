#include <xapian-glib.h>
#include <stdlib.h>

int
main (int argc,
      char **argv)
{
  XapianWritableDatabase *db;
  XapianDocument *doc;
  gboolean res;
  GError *error = NULL;

  db = xapian_writable_database_new ("test/testdb", XAPIAN_DATABASE_ACTION_CREATE, &error);
  g_assert_nonnull (db);
  g_assert_no_error (error);

  doc = xapian_document_new ();
  g_assert_nonnull (doc);

  xapian_document_set_data (doc, "{ 'foo': 'bar' }");
  xapian_document_add_term (doc, "asd");
  xapian_document_add_term (doc, "saq");

  res = xapian_writable_database_add_document (db, doc, NULL, &error);
  g_assert_true (res);
  g_assert_no_error (error);
  g_object_unref (doc);

  doc = xapian_document_new ();
  g_assert_nonnull (doc);

  xapian_document_set_data (doc, "{ 'foo': 'bar' }");
  xapian_document_add_term (doc, "abc");
  xapian_document_add_term (doc, "lala");

  res = xapian_writable_database_add_document (db, doc, NULL, &error);
  g_assert_true (res);
  g_assert_no_error (error);
  g_object_unref (doc);

  res = xapian_writable_database_commit (db, &error);
  g_assert_true (res);
  g_assert_no_error (error);

  return EXIT_SUCCESS;
}
