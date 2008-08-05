/* This extract info from _info.c and create json file and also optionally store to db */
#include "infotojson.h"

/*creating json structure for storing to file/db*/
static struct json *createjson(char **infofile, const char *author, const char *directory)
{
	struct json *jsonobj;
	unsigned int modulename;

	if (infofile == NULL || author == NULL) {
		printf("Error Author or Info file is NULL\n");
		exit(1);
	}

        jsonobj = talloc(NULL, struct json);
        if (!jsonobj)
		errx(1, "talloc error");
		
	jsonobj->author = talloc_strdup(jsonobj, author);

        /* First line should be module name and short description */
	modulename =  strchr(infofile[0], '-') - infofile[0];
	
	jsonobj->module = talloc_strndup(jsonobj, infofile[0], modulename - 1);
	 if (!jsonobj->module)
		errx(1, "talloc error");
		
	jsonobj->title = infofile[0];
	jsonobj->desc = &infofile[1];
	jsonobj->depends = get_deps(jsonobj, directory);
	return jsonobj;
}

/*extracting title and description from _info.c files*/
static char **extractinfo(char **file)
{
	char **infofile;
	unsigned int count = 0, j = 0, num_lines = 0;
	bool printing = false;
	
	while (file[num_lines++]);
	infofile = talloc_array(NULL, char *, num_lines);
	
	for (j = 0; j < num_lines - 1; j++) {
		if (streq(file[j], "/**")) {
			printing = true;
		} 
		else if (streq(file[j], " */"))
			printing = false;
		else if (printing) {
			if (strstarts(file[j], " * "))
				infofile[count++] = file[j] + 3;
			else if (strstarts(file[j], " *"))
				infofile[count++] = file[j] + 2;
			else {
				err(1,"Error in comments structure\n%d",j);
				exit(1);
			}
		}
	}
	infofile[count] = NULL;
	return infofile;	
}

/*storing json structure to json file*/
static int storejsontofile(const struct json *jsonobj, const char *file)
{
	FILE *fp;
	unsigned int j = 0;
	fp = fopen(file, "wt");
	fprintf(fp,"\"Module\":\"%s\",\n",jsonobj->module);
	fprintf(fp,"\"Title\":\"%s\",\n",jsonobj->title);
	fprintf(fp,"\"Author\":\"%s\",\n",jsonobj->author);

	fprintf(fp,"\"Dependencies\":[\n");	
	for (j = 0; jsonobj->depends[j]; j++)
		fprintf(fp,"{\n\"depends\":\"%s\"\n},\n",jsonobj->depends[j]);
	fprintf(fp,"]\n");


	fprintf(fp,"\"Description\":[\n");	
	for (j = 0; jsonobj->desc[j]; j++)
		fprintf(fp,"{\n\"str\":\"%s\"\n},\n",jsonobj->desc[j]);
	fprintf(fp,"]\n");
	fclose(fp);
	return 1;
}

/*storing json structure to db*/
static int storejsontodb(const struct json *jsonobj, const char *db)
{
	char *cmd, *query, *desc, *depends;
	sqlite3 *handle;
	struct db_query *q;
	
	handle = db_open(db);
	query = talloc_asprintf(NULL, "SELECT module from search where module=\"%s\";", jsonobj->module);
	q = db_query(handle, query);
	
	desc = strjoin(NULL, jsonobj->desc,"\n");
	strreplace(desc, '\'', ' ');

	depends = strjoin(NULL, jsonobj->depends,"\n");
	if (!q->num_rows)
		cmd = talloc_asprintf(NULL, "INSERT INTO search VALUES(\"%s\",\"%s\",\"%s\", \'%s\', \'%s\', 0);",
			jsonobj->module, jsonobj->author, jsonobj->title, depends, desc);
	else
		cmd = talloc_asprintf(NULL, "UPDATE search set author=\"%s\", title=\"%s\", desc=\'%s\' depends=\'%s\' where module=\"%s\";",
			jsonobj->author, jsonobj->title, desc, depends, jsonobj->module);

	db_command(handle, cmd);	
	db_close(handle);
	talloc_free(depends);
	talloc_free(query);
	talloc_free(desc);
	talloc_free(cmd);
	return 1;
}

int main(int argc, char *argv[])
{
	char *file;
	char **lines;
	char **infofile;
	struct json *jsonobj;
	
	talloc_enable_leak_report();
	if (argc < 5)
		errx(1, "usage: infotojson dir_of_module info_filename target_json_file author [sqlitedb]\n"
				 "Convert _info.c file to json file and optionally store to database");
		
	file = grab_file(NULL, argv[2]);
	if (!file)
		err(1, "Reading file %s", argv[2]);

	lines = strsplit(NULL, file, "\n", NULL);		
	
	//extract info from lines
	infofile = extractinfo(lines);
	
	//create json obj
	jsonobj = createjson(infofile, argv[4], argv[1]);
	
	//store to file
	storejsontofile(jsonobj, argv[3]);
	
	if (argv[5] != NULL)
		storejsontodb(jsonobj, argv[5]);
		
	talloc_free(file);
	talloc_free(jsonobj);
	talloc_free(lines);
	talloc_free(infofile);	
	return 0;
}
