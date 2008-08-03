/* This extract info from _info.c and create json file and also optionally store to db */
#include "infotojson.h"

/*creating json structure for storing to file/db*/
static struct json *createjson(char **infofile, char *author)
{
	struct json *jsonobj;
	unsigned int modulename;

	if (infofile == NULL || author == NULL) {
		printf("Error Author or Info file is NULL\n");
		exit(1);
	}

	//jsonobj = (struct json *)palloc(sizeof(struct json));
        jsonobj = talloc(NULL, struct json);
        if (!jsonobj)
		errx(1, "talloc error");
		
	jsonobj->author = author;

        /* First line should be module name and short description */
	modulename =  strchr(infofile[0], '-') - infofile[0];
	
	jsonobj->module = talloc_strndup(jsonobj, infofile[0], modulename - 1);
	 if (!jsonobj->module)
		errx(1, "talloc error");
		
	jsonobj->title = infofile[0];
	jsonobj->desc = &infofile[1];
	
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
	//(char **) palloc(size * sizeof(char *));
	
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
	char *cmd, *query, *desc;
	sqlite3 *handle;
	struct db_query *q;
	
	handle = db_open(db);
	query = talloc_asprintf(NULL, "SELECT module from search where module=\"%s\";", jsonobj->module);
	q = db_query(handle, query);
	
	desc = strjoin(NULL,jsonobj->desc,"\n");
	strreplace(desc, '\'', ' ');
	if (!q->num_rows)
		cmd = talloc_asprintf(NULL, "INSERT INTO search VALUES(\"%s\",\"%s\",\"%s\",'%s\');",
			jsonobj->module, jsonobj->author, jsonobj->title, desc);
	else
		cmd = talloc_asprintf(NULL, "UPDATE search set author=\"%s\", title=\"%s\", desc='%s\' where module=\"%s\";",
			jsonobj->author, jsonobj->title, desc, jsonobj->module);

	db_command(handle, cmd);	
	db_close(handle);
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
	if (argc < 4) {
		errx(1, "usage: infotojson infofile jsonfile author [sqlitedb]\n");
		return 1;
	}
		
	file = grab_file(NULL, argv[1]);
	if (!file)
		err(1, "Reading file %s", argv[1]);

	lines = strsplit(NULL, file, "\n", NULL);		
	
	//extract info from lines
	infofile = extractinfo(lines);
	
	//create json obj
	jsonobj = createjson(infofile, argv[3]);
	
	//store to file
	storejsontofile(jsonobj, argv[2]);
	
	if (argv[4] != NULL)
		storejsontodb(jsonobj, argv[4]);
		
	talloc_free(file);
	talloc_free(jsonobj);
	talloc_free(lines);
	talloc_free(infofile);	
	return 0;
}
