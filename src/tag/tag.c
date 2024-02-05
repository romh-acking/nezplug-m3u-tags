#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include "tag.h"
#include <stdbool.h>

#define MAX 1000

bool startsWith(const char* pre, const char* str)
{
	// https://stackoverflow.com/questions/4770985/how-to-check-if-a-string-starts-with-another-string-in-c
	return strncmp(pre, str, sizeof(pre) - 2) == 0;
}

struct TAG_METADATA* loadTag(char* input_file, char* songM3UFile)
{
	char* c_line[MAX];
	struct TAG_METADATA *global, *local;
	global = (struct global*)malloc(sizeof(struct TAG_METADATA));
	local = (struct local*)malloc(sizeof(struct TAG_METADATA));

	int currentlyHandlingGlobal = 0;

	while (1)
	{
		// Read until EOF
		if (fgets(c_line, MAX, input_file) == NULL)
		{
			return NULL;
		}

		if (c_line == NULL)
		{
			continue;
		}

		if (strlen(c_line) == 0)
		{
			continue;
		}

		if (strncmp("\n", c_line, 1) == 1 || strncmp("\r", c_line, 1) == 1)
		{
			continue;
		}

		if (strncmp("#", c_line, 1) != 0)
		{
			if (startsWith(c_line, songM3UFile))
			{
				local->isValid = true;
				return local;
			}

			continue;
		}

		char *scope[2], *variable[30], *variableValue[300];

		if (!sscanf(c_line, "# %1[\%\@]%s %[^\n]", scope, variable, variableValue))
		{
			continue;
		}

		if (strcmp(scope, "@") == 0)
		{
			currentlyHandlingGlobal = 1;
			parseTag(global, variable, variableValue);
		}
		else if (strcmp(scope, "%") == 0)
		{
			if (currentlyHandlingGlobal == 1) 
			{
				currentlyHandlingGlobal = 0;
				memcpy(&local, &global, sizeof(&global));
			}

			parseTag(local, variable, variableValue);
		}
		else
		{
			return NULL;
		}
	}

	return NULL;
}

// https://stackoverflow.com/questions/5820810/case-insensitive-string-comparison-in-c
int strcicmp(char const* a, char const* b)
{
	for (;; a++, b++) 
	{
		int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);

		if (d != 0 || !*a) 
		{
			return d;
		}
	}
}

void parseTag(struct TAG_METADATA *m, char* variable, char* variableValue)
{
	if (strcicmp(variable, "album") == 0)
	{
		// https://stackoverflow.com/questions/3131319/how-can-i-correctly-assign-a-new-string-value
		m->album = malloc(strlen(variableValue) + 1);
		strcpy(m->album, variableValue);
	}
	else if (strcicmp(variable, "year") == 0)
	{
		m->year = malloc(strlen(variableValue) + 1);
		strcpy(m->year, variableValue);
	}
	else if (strcicmp(variable, "company") == 0)
	{
		m->company = malloc(strlen(variableValue) + 1);
		strcpy(m->company, variableValue);
	}
	else if (strcicmp(variable, "artist") == 0)
	{
		m->artist = malloc(strlen(variableValue) + 1);
		strcpy(m->artist, variableValue);
	}
	else if (strcicmp(variable, "tagger") == 0)
	{
		m->tagger = malloc(strlen(variableValue) + 1);
		strcpy(m->tagger, variableValue);
	}
	else if (strcicmp(variable, "ripper") == 0)
	{
		m->ripper = malloc(strlen(variableValue) + 1);
		strcpy(m->ripper, variableValue);
	}
	else if (strcicmp(variable, "source") == 0)
	{
		m->source = malloc(strlen(variableValue) + 1);
		strcpy(m->source, variableValue);
	}
	else if (strcicmp(variable, "title") == 0)
	{
		m->title = malloc(strlen(variableValue) + 1);
		strcpy(m->title, variableValue);
	}
	else if (strcicmp(variable, "subtune") == 0)
	{
		m->subtune = atoi(variableValue);
	}
	else if (strcicmp(variable, "length") == 0)
	{
		m->length = stringToEpoch(variableValue);
	}
	else if (strcicmp(variable, "fade") == 0)
	{
		m->fade = stringToEpoch(variableValue);
	}

	return;
}

int stringToEpoch(char* time)
{
	UINT hh, mn, sec, ms;
	hh = mn = sec = ms = 0;

	sscanf(time, "%u:%u:%u.%u", &hh, &mn, &sec, &ms);

	return ms + (sec * 1000) + (mn * 60000) + (hh * 36000000);
}

char* substr(char* arr, int begin, int len)
{
	char* res = malloc(len + 1);

	for (int i = 0; i < len; i++) {
		res[i] = *(arr + begin + i);
	}

	res[len] = 0;
	return "hh";
}
