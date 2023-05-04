/* gcc example-create-indexes.c -o example-create-indexes $(pkg-config --cflags
 * --libs libmongoc-1.0) */

/* ./example-create-indexes [CONNECTION_STRING [COLLECTION_NAME]] */

/**

    Mongo Cxx 測試，新增刪除修改。

*/

#include <mongoc/mongoc.h>
#include <stdio.h>
#include <stdlib.h>

int
main (int argc, char *argv[])
{
    
   mongoc_client_t *client;
   const char *uri_string =
      "mongodb://127.0.0.1/?appname=create-indexes-example";
   mongoc_uri_t *uri;
   mongoc_database_t *db;

   // 新增一個資料庫及一個collection  create collection
   const char *collection_name = "testaaa";
   bson_t keys;
   char *index_name;
   bson_t *create_indexes;
   bson_t reply;
   char *reply_str;
   bson_error_t error;
   bool r;

   mongoc_init ();

   if (argc > 1) {
      uri_string = argv[1];
   }

   if (argc > 2) {
      collection_name = argv[2];
   }

   uri = mongoc_uri_new_with_error (uri_string, &error);
   if (!uri) {
      fprintf (stderr,
               "failed to parse URI: %s\n"
               "error message:       %s\n",
               uri_string,
               error.message);
      return EXIT_FAILURE;
   }

   client = mongoc_client_new_from_uri (uri);
   if (!client) {
      return EXIT_FAILURE;
   }

   mongoc_client_set_error_api (client, 2);
   db = mongoc_client_get_database (client, "db");

   /* ascending index on field "x" */
   bson_init (&keys);
   BSON_APPEND_INT32 (&keys, "x", 1);
   index_name = mongoc_collection_keys_to_index_string (&keys);
   create_indexes = BCON_NEW ("createIndexes",
                              BCON_UTF8 (collection_name),
                              "indexes",
                              "[",
                              "{",
                              "key",
                              BCON_DOCUMENT (&keys),
                              "name",
                              BCON_UTF8 (index_name),
                              "}",
                              "]");

   r = mongoc_database_write_command_with_opts (
      db, create_indexes, NULL /* opts */, &reply, &error);

   reply_str = bson_as_json (&reply, NULL);
   printf ("%s\n", reply_str);


   if (!r) {
      fprintf (stderr, "Error in createIndexes: %s\n", error.message);
   }

   // 在剛新增的collection中新增一筆資料。 insert
   mongoc_collection_t* collection;
   collection = mongoc_client_get_collection(client, "db", "testaaa");
   bson_t* doc;
   bson_oid_t oid;

   doc = bson_new();
   bson_oid_init(&oid, NULL);
   BSON_APPEND_OID(doc, "_id", &oid);
   BSON_APPEND_UTF8(doc, "hello", "world");
   BSON_APPEND_UTF8(doc, "secs", "sooooo");

   if (!mongoc_collection_insert_one(
       collection, doc, NULL, NULL, &error)) {
       fprintf(stderr, "%s\n", error.message);
   }


   // 修改剛新增的資料的資料。 update
   bson_t *query, *updDoc;
   query = bson_new();
   BSON_APPEND_UTF8(query, "hello", "world");
   updDoc = bson_new();
   updDoc = BCON_NEW("$set", "{", "hello", BCON_UTF8("MMMMMMMongoDB"), "}");

   if (!mongoc_collection_update_many(collection, query, updDoc, NULL, NULL, &error))
   {
       printf("%s\n", error.message);
   }

   // 將資料讀取並印出。 read
   char* str;
   query = bson_new();
   BSON_APPEND_UTF8(query, "hello", "MMMMMMMongoDB");
   mongoc_cursor_t* cursor;
   cursor = mongoc_collection_find_with_opts(collection, query, NULL, NULL);
   while (mongoc_cursor_next(cursor, &doc)) {
       str = bson_as_canonical_extended_json(doc, NULL);
       printf("%s\n", str);
       bson_free(str);
   }

   if (mongoc_cursor_error(cursor, &error)) {
       fprintf(stderr, "An error occurred: %s\n", error.message);
   }

   // 刪除資料。 delete
   query = bson_new();
   BSON_APPEND_UTF8(query, "hello", "MMMMMMMongoDB");
   if (!mongoc_collection_delete_many(collection, query, NULL, NULL, &error))
   {
       printf("%s\n", error.message);
   }


   // 釋放記憶體
   bson_destroy(doc);
   mongoc_collection_destroy(collection);
   bson_free (index_name);
   bson_free (reply_str);
   bson_destroy (&reply);
   bson_destroy (create_indexes);
   mongoc_database_destroy (db);
   mongoc_uri_destroy (uri);
   mongoc_client_destroy (client);

   mongoc_cleanup ();

   return r ? EXIT_SUCCESS : EXIT_FAILURE;
}
