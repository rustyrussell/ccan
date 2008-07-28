/** json structure 
 * This file contains definition of json structure
 **/
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sqlite3.h>
#include "database.h"
#include "ccan/talloc/talloc.h"
#include "ccan/string/string.h"
#include "utils.h"

 struct json
 {
 	char *module;
 	char *title;
 	char *author;
 	char **desc;
 };
 
 /* Function for storing json structure to file given struct json*/ 
int storejsontofile(struct json *jsonobj, char *jsonfile);

/*Function to store in database*/
int storejsontodb(struct json *jsonobj, char *db);

/*create json structure*/
struct json * createjson(char **infofile, char *author);

/*Extract info from file*/
char ** extractinfo(char **file);
