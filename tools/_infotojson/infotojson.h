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
#include "tools/tools.h"

 struct json
 {
 	char *module;
 	char *title;
 	char *author;
	char **depends;
 	char **desc;
 };
 
 /* Function for storing json structure to file given struct json*/ 
static int storejsontofile(const struct json *jsonobj, const char *jsonfile);

/*Function to store in database*/
static int storejsontodb(const struct json *jsonobj, const char *db);

/*create json structure*/
static struct json *createjson(char **infofile, const char *author, const char *directory);

/*Extract info from file*/
static char **extractinfo(char **file);
