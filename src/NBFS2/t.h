/*Creates a new HashTable*/
static HashTable *create_table(int size);
/*Frees the table*/
static void free_table(HashTable *table);
/*Create the item*/
static void ht_insert(HashTable *table, char *key, char *value);
/*Searches the key in the hashtable returns NULL if it doesn't exist*/
static char *ht_search(HashTable *table, char *key);
/*Deletes an item from the table*/
static void ht_delete(HashTable *table, char *key);