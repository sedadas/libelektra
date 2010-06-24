#include "tests.h"

int nbError;
int nbTest;

uid_t nbUid;
gid_t nbGid;

char file [MAX_PATH_LENGTH];
char srcdir [MAX_PATH_LENGTH];

#ifdef HAVE_CLEARENV
int clearenv();
#endif

/**Does some useful startup.
 */
int init (int argc, char**argv)
{
	setlocale (LC_ALL, "");

	nbUid = getuid();
	nbGid = getgid();

	if (getenv ("srcdir"))
	{
		strncpy (srcdir, getenv ("srcdir"), sizeof(srcdir));
	} else {
		if (argc > 1)
		{
			strncpy (srcdir, argv[1], sizeof(srcdir));
		} else {
			strcpy (srcdir, ".");
			warn_if_fail (0, "srcdir not set, will try current directory");
		}
	}
#ifdef HAVE_CLEARENV
	clearenv();
#else
	unsetenv("HOME");
	unsetenv("USER");
	unsetenv("KDB_HOME");
	unsetenv("KDB_USER");
	unsetenv("KDB_DIR");
#endif

#ifdef HAVE_SETENV
	setenv("KDB_HOME",".",1);
#endif

	return 0;
}

/**Create a root key for a backend.
 *
 * @return a allocated root key */
Key * create_root_key (const char *backendName)
{
	Key *root = keyNew ("user/tests", KEY_END);
	/*Make mount point beneath root, and do all tests here*/
	keySetDir(root);
	keySetUID(root, nbUid);
	keySetGID(root, nbGid);
	keyAddBaseName (root, backendName);
	keySetString (root, backendName);
	keySetComment (root, "backend root key for tests");
	keySetString (root, backendName);
	return root;
}

/**Create a configuration keyset for a backend.
 *
 * @return a allocated configuration keyset for a backend*/
KeySet *create_conf (const char *filename)
{
	return ksNew (2,
		keyNew("system/path", KEY_VALUE, filename, KEY_END),
		KS_END);
}


int compare_line_files (const char *filename, const char *genfilename)
{
	FILE *forg, *fgen;
	char bufferorg [BUFFER_LENGTH + 1];
	char buffergen [BUFFER_LENGTH + 1];
	int line = 0;

	forg = fopen (filename, "r");
	fgen = fopen (genfilename, "r");
	exit_if_fail (forg && fgen, "could not open file");

	while (	fgets (bufferorg, BUFFER_LENGTH, forg) &&
		fgets (buffergen, BUFFER_LENGTH, fgen))
	{
		line ++;
		if (strncmp (bufferorg, buffergen, BUFFER_LENGTH))
		{
			printf ("In file %s, line %d.\n", filename, line);
			fclose (forg); fclose (fgen);
			succeed_if (0, "comparing lines failed");
			return 0;
		}
	}
	fclose (forg); fclose (fgen);
	return 1;
}


/**Compare two files line by line.
 *
 * Fails when there are any differences.
 *
 * The original file is passed as parameter.
 * It will be compared with the file -gen.
 *
 * file.xml -> file-gen.xml (xml comparator)
 * file.txt -> file-gen.txt (line comparator)
 * file.c -> file-gen.c (c comparator)
 *
 */
int compare_files (const char * filename)
{
	char genfilename [MAX_PATH_LENGTH];
	char * dot = strrchr (filename, '.');

	exit_if_fail (dot != 0, "could not find extension in file");

	strncpy (genfilename, filename, dot- filename);
	/* does not terminate string, but strcat need it, so: */
	genfilename[dot-filename] = 0;
	strcat  (genfilename, "-gen");
	if (!strcmp (dot, ".xml"))
	{
		strcat (genfilename, ".xml");
	} else if (!strcmp (dot, ".txt"))
	{
		strcat (genfilename, ".txt");
	} else if (!strcmp (dot, ".c"))
	{
		strcat (genfilename, ".c");
	}

	return compare_line_files (filename, genfilename);
}


int compare_key (Key *k1, Key *k2)
{
	int	ret;
	int	err = nbError;

	/* printf ("compare: "); keyOutput (k1, stdout); keyOutput (k2, stdout); printf ("\n"); */
	// printf ("compare value %s with %s\n", keyValue(k1), keyValue(k2));
	ret = keyCompare(k1, k2);

	succeed_if ((ret & KEY_NAME) == 0 , "compare key: NAME not equal");
	succeed_if ((ret & KEY_VALUE) == 0 , "compare key: VALUE not equal");
	succeed_if ((ret & KEY_OWNER) == 0 , "compare key: OWNER not equal");
	succeed_if ((ret & KEY_COMMENT) == 0 , "compare key: COMMENT not equal");
	succeed_if ((ret & KEY_UID) == 0 , "compare key: UID not equal");
	succeed_if ((ret & KEY_GID) == 0 , "compare key: GID not equal");
	succeed_if ((ret & KEY_MODE ) == 0 , "compare key: MODE  not equal");

	return err-nbError;
}

/**Compare two keysets.
 *
 * Compare if two keysets contain the same keys.
 * @return 0 on success
 * */
int compare_keyset (KeySet *ks, KeySet *ks2)
{
	Key	*key = 0;
	Key     *key2 = 0;
	int	size = 0;
	int	dup = 0;
	int	err = nbError;

	// I would have a _true_ ksCompare() ...
	ksSort(ks); ksRewind(ks);
	ksSort(ks2); ksRewind(ks2);

	//SYNC with ksOutput
	while ((key = ksNext(ks)) != 0)
	{
		key2 = ksNext(ks2);
		if (!key2)
		{
			succeed_if (0, "Will break, did not find corresponding key2");
			if (dup) keyDel (key);
			break;
		}

		size ++;
		compare_key (key, key2);
		if (dup) keyDel (key);
	}

	if (size == 0) succeed_if (0, "real size was 0");
	if ( size != ksGetSize(ks2) ) {
		printf ("%d, %d\n", (int)ksGetSize(ks), (int)ksGetSize(ks2));
		succeed_if( 0, "There are less keys fetched than keys which have been submitted.");
	}
	if ( err-nbError )
	{
		if (key && key2) printf ("error comparing %s - %s\n", keyName(key), keyName(key2));
		else printf ("error comparing null key\n");
	}
	return err-nbError;
}

/* return file name in srcdir.
 * No bound checking on file size, may overflow. */
char * srcdir_file(const char * fileName)
{
	strcpy(file, srcdir);
	strcat(file, "/");
	strcat(file, fileName);
	return file;
}

void clear_sync (KeySet *ks)
{
	Key *k;
	ksRewind(ks);
	while ((k = ksNext(ks)) != 0) keyClearSync(k);
}

void output (KeySet *ks)
{
	Key *k;
	ksRewind(ks);
	while ((k = ksNext(ks)) != 0)
	{
		printf ("key: %s, string: %s\n", keyName(k), keyString(k));
	}
}

void output_trie(Trie *trie)
{
	int i;
	printf ("entered output_trie with %p\n", (void*) trie);
	for (i=0; i <= MAX_UCHAR; ++i)
	{
		if (trie->value[i]) printf ("output_trie: %p\n", (void*) trie->value[i]);
		if (trie->children[i]) output_trie(trie->children[i]);
	}
	if (trie->empty_value) printf ("empty_value: %p\n", (void*) trie->empty_value);
}

void output_warnings(Key *warningKey)
{
	const Key *metaWarnings = keyGetMeta(warningKey, "warnings");
	if (!metaWarnings) return; /* There are no current warnings */
	succeed_if (0, "there were warnings issued");

	int nrWarnings = atoi(keyString(metaWarnings));
	char buffer[] = "warnings/#00\0description";

	printf ("There are %d warnings\n", nrWarnings+1);
	for (int i=0; i<=nrWarnings; ++i)
	{
		buffer[10] = i/10%10 + '0';
		buffer[11] = i%10 + '0';
		printf ("buffer is: %s\n", buffer);
		strncat(buffer, "/number" , sizeof(buffer));
		printf ("number: %s\n", keyString(keyGetMeta(warningKey, buffer)));
		buffer[12] = '\0'; strncat(buffer, "/description" , sizeof(buffer));
		printf ("description: %s\n", keyString(keyGetMeta(warningKey, buffer)));
		buffer[12] = '\0'; strncat(buffer, "/ingroup" , sizeof(buffer));
		keyGetMeta(warningKey, buffer);
		buffer[12] = '\0'; strncat(buffer, "/module" , sizeof(buffer));
		keyGetMeta(warningKey, buffer);
		buffer[12] = '\0'; strncat(buffer, "/file" , sizeof(buffer));
		keyGetMeta(warningKey, buffer);
		buffer[12] = '\0'; strncat(buffer, "/line" , sizeof(buffer));
		keyGetMeta(warningKey, buffer);
		buffer[12] = '\0'; strncat(buffer, "/reason" , sizeof(buffer));
		keyGetMeta(warningKey, buffer);
	}
}

void output_errors(Key *errorKey)
{
	const Key * metaError = keyGetMeta(errorKey, "error");
	if (!metaError) return; /* There is no current error */
	succeed_if (0, "there were errors issued");

	printf ("number: %s\n", keyString(keyGetMeta(errorKey, "error/number")));
	/*
	std::cerr << "description: " << error.getMeta<std::string>("error/description") << std::endl;
	std::cerr << "ingroup: " << error.getMeta<std::string>("error/ingroup") << std::endl;
	std::cerr << "module: " << error.getMeta<std::string>("error/module") << std::endl;
	std::cerr << "at: " << error.getMeta<std::string>("error/file") << ":" << error.getMeta<std::string>("error/line") << std::endl;
	std::cerr << "reason: " << error.getMeta<std::string>("error/reason") << std::endl;
	*/
}
