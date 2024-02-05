#include <stdbool.h>

struct TAG_METADATA*				loadTag(char* input_file, char* songM3UFile);
void								parseTag(struct TAG_METADATA* m, char variable, char variableValue);

typedef struct TAG_METADATA
{
	char*   isValid;
	char*	album;
	char*	year;
	char*	company;
	char*	artist;
	char*	tagger;
	char*	ripper;
	char*	source;
	char*	title;
	char*	subtune;
	char*	length;
	char*	fade;
	char*	filename;
};
