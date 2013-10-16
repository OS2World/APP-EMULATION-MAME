#include "mamalleg.h"
#include "driver.h"
#include "unzip.h"
#include <sys/stat.h>
#include <unistd.h>

#define MAXPATHC 50 /* at most 50 path entries */
#define MAXPATHL 2048 /* at most 2048 character path length */

char buf1[MAXPATHL];
char buf2[MAXPATHL];

char *rompathv[MAXPATHC];
char *samplepathv[MAXPATHC];
int rompathc;
int samplepathc;
char *cfgdir, *hidir, *inpdir, *stadir, *memcarddir, *artworkdir, *screenshotdir;


char *alternate_name; /* for "-romdir" */

typedef enum
{
	kPlainFile,
	kRAMFile,
	kZippedFile
} eFileType;

typedef struct
{
	FILE			*file;
	unsigned char	*data;
	unsigned int	offset;
	unsigned int	length;
	eFileType		type;
	unsigned int	crc;
} FakeFileHandle;


extern unsigned int crc32 (unsigned int crc, const unsigned char *buf, unsigned int len);
static int checksum_file(const char* file, unsigned char **p, unsigned int *size, unsigned int* crc);

/*
 * File stat cache LRU (Last Recently Used)
 */

/* Use the file cache */
#define FILE_CACHE

#ifdef FILE_CACHE
struct file_cache_entry {
	struct stat stat_buffer;
	int result;
	char* file;
};

/* File cache buffer */
static struct file_cache_entry** file_cache_map = 0;

/* File cache size */
static unsigned int file_cache_max = 0;

/* AM 980919 */
static int cache_stat(const char *path, struct stat *statbuf) {
	if (file_cache_max) {
		unsigned i;
		struct file_cache_entry* entry;

		/* search in the cache */
		for(i=0;i<file_cache_max;++i) {
			if (file_cache_map[i]->file && strcmp(file_cache_map[i]->file,path)==0) {
				/* found */
				unsigned j;
/*
				if (errorlog)
					fprintf(errorlog,"File cache HIT  for %s\n", path);
*/

				/* store */
				entry = file_cache_map[i];

				/* shift */
				for(j=i;j>0;--j)
					file_cache_map[j] = file_cache_map[j-1];

				/* set the first entry */
				file_cache_map[0] = entry;

				if (entry->result==0)
					memcpy(statbuf,&entry->stat_buffer,sizeof(struct stat));

				return entry->result;
			}
		}
/*
		if (errorlog)
			fprintf(errorlog,"File cache FAIL for %s\n", path);
*/

		/* oldest entry */
		entry = file_cache_map[file_cache_max-1];
		free(entry->file);

		/* shift */
		for(i=file_cache_max-1;i>0;--i)
			file_cache_map[i] = file_cache_map[i-1];

		/* set the first entry */
		file_cache_map[0] = entry;

		/* file */
		entry->file = (char*)malloc( strlen(path) + 1);
		strcpy( entry->file, path );

		/* result and stat */
		entry->result = stat(path,&entry->stat_buffer);

		if (entry->result==0)
			memcpy(statbuf,&entry->stat_buffer,sizeof(struct stat));

		return entry->result;
	} else {
		return stat(path,statbuf);
	}
}

/* AM 980919 */
static void cache_allocate(unsigned entries) {
	if (entries) {
		unsigned i;

		file_cache_max = entries;
		file_cache_map = (struct file_cache_entry**)malloc( file_cache_max*sizeof(struct file_cache_entry*) );

		for(i=0;i<file_cache_max;++i) {
			file_cache_map[i] = (struct file_cache_entry*)malloc( sizeof(struct file_cache_entry) );
			memset( file_cache_map[i], 0, sizeof(struct file_cache_entry) );
		}

		if (errorlog)
			fprintf(errorlog,"File cache allocated for %d entries\n",file_cache_max);
	}
}
#else

#define cache_stat(a,b) stat(a,b)

#endif

/* helper function which decomposes a path list into a vector of paths */
int path2vector (char *path, char *buf, char **pathv)
{
	int i;
	char *token;

	/* copy the path variable, cause strtok will modify it */
	strncpy (buf, path, MAXPATHL-1);
	i = 0;
	token = strtok (buf, ";");
	while ((i < MAXPATHC) && token)
	{
		pathv[i] = token;
		i++;
		token = strtok (NULL, ";");
	}
	return i;
}

void decompose_rom_sample_path (char *rompath, char *samplepath)
{
	rompathc    = path2vector (rompath,    buf1, rompathv);
	samplepathc = path2vector (samplepath, buf2, samplepathv);

#ifdef FILE_CACHE
	/* AM 980919 */
	if (file_cache_max == 0) {
		/* (rom path directories + 1 buffer)==rompathc+1 */
		/* (dir + .zip + .zif)==3 */
		/* (clone+parent)==2 */
		cache_allocate((rompathc+1)*3*2);
	}
#endif

}

/*
 * file handling routines
 *
 * gamename holds the driver name, filename is only used for ROMs and samples.
 * if 'write' is not 0, the file is opened for write. Otherwise it is opened
 * for read.
 */

/*
 * check if roms/samples for a game exist at all
 * return index+1 of the path vector component on success, otherwise 0
 */
int osd_faccess(const char *newfilename, int filetype)
{
	char name[256];
	char **pathv;
	int  pathc;
	static int indx;
	static const char *filename;
	char *dir_name;

	/* if filename == NULL, continue the search */
	if (newfilename != NULL)
	{
		indx = 0;
		filename = newfilename;
	}
	else
		indx++;

   /* MESS modified */
	if (filetype == OSD_FILETYPE_ROM
#ifdef MESS
		 || filetype == OSD_FILETYPE_ROM_CART
		 || filetype == OSD_FILETYPE_IMAGE
#endif
		 )
   {
		 pathv = rompathv;
		 pathc = rompathc;
   }
	else if (filetype == OSD_FILETYPE_SAMPLE)
	{
		pathv = samplepathv;
		pathc = samplepathc;
	}
	else if (filetype == OSD_FILETYPE_SCREENSHOT)
	{
		void *f;

		sprintf(name,"%s/%s.png", screenshotdir, newfilename);
		f = fopen(name,"rb");
		if (f)
		{
			fclose(f);
			return 1;
		}
		else return 0;
	}
	else
		return 0;

	for (; indx < pathc; indx++)
	{
		struct stat stat_buffer;
		dir_name = pathv[indx];

		/* does such a directory (or file) exist? */
		sprintf(name,"%s/%s",dir_name,filename);
		if (cache_stat(name, &stat_buffer) == 0)
			return indx+1;

		/* try again with a .zip extension */
		sprintf(name,"%s/%s.zip", dir_name, filename);
		if (cache_stat(name, &stat_buffer) == 0)
			return indx+1;

		/* try again with a .zif extension */
		sprintf(name,"%s/%s.zif", dir_name, filename);
		if (cache_stat(name, &stat_buffer) == 0)
			return indx+1;
	}

	/* no match */
	return 0;
}

/* JB 980920 update */
/* AM 980919 update */
void *osd_fopen(const char *game,const char *filename,int filetype,int _write)
{
	char name[256];
	char *gamename;
	int found = 0;
	int  indx;
	struct stat stat_buffer;
	FakeFileHandle *f;
	int pathc;
	char** pathv;
   #ifdef MESS
		char file[MAXPATHL];
		char *extension;
   #endif

	f = (FakeFileHandle *)malloc(sizeof(FakeFileHandle));
	if (!f)
		return 0;
    memset(f,0,sizeof(FakeFileHandle));

	gamename = (char *)game;

	/* Support "-romdir" yuck. */
	if (alternate_name)
		gamename = alternate_name;

	switch (filetype)
	{
		case OSD_FILETYPE_ROM:
		case OSD_FILETYPE_SAMPLE:
#ifdef MESS
      case OSD_FILETYPE_ROM_CART:
#endif

      /* only for reading */
			if (_write)
				break;

         if (filetype==OSD_FILETYPE_ROM
#ifdef MESS
         	|| OSD_FILETYPE_ROM_CART
#endif
			)
			{
				pathc = rompathc;
				pathv = rompathv;
			} else {
				pathc = samplepathc;
				pathv = samplepathv;
			}

         for (indx=0;indx<pathc && !found; ++indx) {
				const char* dir_name = pathv[indx];

				if (!found) {
					sprintf(name,"%s/%s",dir_name,gamename);
					if (cache_stat(name,&stat_buffer)==0 && (stat_buffer.st_mode & S_IFDIR)) {
						sprintf(name,"%s/%s/%s",dir_name,gamename,filename);
						if (filetype==OSD_FILETYPE_ROM)	{
							if (checksum_file (name, &f->data, &f->length, &f->crc)==0) {
								f->type = kRAMFile;
								f->offset = 0;
								found = 1;
							}
						}
						else {
							f->type = kPlainFile;
							f->file = fopen(name,"rb");
							found = f->file!=0;
						}
					}
				}

#ifdef MESS
				/* Zip cart support for MESS */
				if (!found && filetype == OSD_FILETYPE_ROM_CART)
				{
					extension = strrchr(name, '.');		/* find extension       */
					if (extension) *extension = '\0';	/* drop extension       */
					sprintf(name,"%s.zip", name);		/* add .zip for zipfile */
					if (cache_stat(name,&stat_buffer)==0) {
						if (load_zipped_file(name, filename, &f->data, &f->length)==0) {
							if (errorlog)
								fprintf(errorlog,"Using (osd_fopen) zip file for %s\n", filename);
							f->type = kZippedFile;
							f->offset = 0;
							f->crc = crc32 (0L, f->data, f->length);
							found = 1;
						}
					}
				}

#endif


				if (!found) {
					/* try with a .zip extension */
					sprintf(name,"%s/%s.zip", dir_name, gamename);
					if (cache_stat(name,&stat_buffer)==0) {
						if (load_zipped_file(name, filename, &f->data, &f->length)==0) {
							if (errorlog)
								fprintf(errorlog,"Using (osd_fopen) zip file for %s\n", filename);
							f->type = kZippedFile;
							f->offset = 0;
							f->crc = crc32 (0L, f->data, f->length);
							found = 1;
						}
					}
				}

				if (!found) {
					/* try with a .zip directory (if ZipMagic is installed) */
					sprintf(name,"%s/%s.zip",dir_name,gamename);
					if (cache_stat(name,&stat_buffer)==0 && (stat_buffer.st_mode & S_IFDIR)) {
						sprintf(name,"%s/%s.zip/%s",dir_name,gamename,filename);
						if (filetype==OSD_FILETYPE_ROM)	{
							if (checksum_file (name, &f->data, &f->length, &f->crc)==0) {
								f->type = kRAMFile;
								f->offset = 0;
								found = 1;
							}
						}
						else {
							f->type = kPlainFile;
							f->file = fopen(name,"rb");
							found = f->file!=0;
						}
					}
				}

/* ZipFolders support disabled for rom and sample load.
   There is no reason to keep it because now zip files are fully supported. */
#if 0
				if (!found) {
					/* try with a .zif directory (if ZipFolders is installed) */
					sprintf(name,"%s/%s.zif",dir_name,gamename);
					if (cache_stat(name,&stat_buffer)==0) {
						sprintf(name,"%s/%s.zif/%s",dir_name,gamename,filename);
						if (filetype==OSD_FILETYPE_ROM)	{
							if (checksum_file (name, &f->data, &f->length, &f->crc)==0) {
								f->type = kRAMFile;
								f->offset = 0;
								found = 1;
							}
						}
						else {
							f->type = kPlainFile;
							f->file = fopen(name,"rb");
							found = f->file!=0;
						}
					}
				}
#endif
         }
			break;
#ifdef MESS
      case OSD_FILETYPE_IMAGE:

			if(errorlog) fprintf(errorlog,"Open IMAGE '%s' for %s\n", filename, game);
            strcpy(file, filename);

			do {
				for (indx=0; indx < rompathc && !found; ++indx)
            {
					const char* dir_name = rompathv[indx];

					if (!found) {
						sprintf(name, "%s/%s", dir_name, gamename);
						if(errorlog) fprintf(errorlog,"Trying %s\n", name);
						if (cache_stat(name,&stat_buffer)==0 && (stat_buffer.st_mode & S_IFDIR)) {
                            sprintf(name,"%s/%s/%s", dir_name, gamename, file);
							f->file = fopen(name,_write ? "r+b" : "rb");
							found = f->file!=0;
						}
					}

 /******************************************************/
 				/* Zip IMAGE support for MESS */
 				if (filetype == OSD_FILETYPE_IMAGE && !_write) {
 					extension = strrchr(name, '.');		/* find extension       */
 					if (extension) *extension = '\0';	/* drop extension       */
 					sprintf(name,"%s.zip", name);		/* add .zip for zipfile */
 					if (cache_stat(name,&stat_buffer)==0) {
 						if (load_zipped_file(name, filename, &f->data, &f->length)==0) {
 							if (errorlog)
 								fprintf(errorlog,"Using (osd_fopen) zip file for %s\n", filename);
 							f->type = kZippedFile;
 							f->offset = 0;
 							f->crc = crc32 (0L, f->data, f->length);
 							found = 1;
 						}
 					}
 				}

/******************************************************/

					if (!found && !_write) {
						/* try with a .zip extension */
						sprintf(name, "%s/%s.zip", dir_name, gamename);
						if (errorlog) fprintf(errorlog,"Trying %s\n", name);
                  if (cache_stat(name,&stat_buffer)==0) {
							if (load_zipped_file(name, file, &f->data, &f->length)==0) {
								if (errorlog) fprintf(errorlog,"Using (osd_fopen) zip file for %s\n", filename);
								f->type = kZippedFile;
								f->offset = 0;
                                f->crc = crc32 (0L, f->data, f->length);
								found = 1;
							}
						}
					}

					if (!found) {
						/* try with a .zip directory (if ZipMagic is installed) */
						sprintf(name, "%s/%s.zip", dir_name, gamename);
						if (errorlog) fprintf(errorlog,"Trying %s\n", name);
                  if (cache_stat(name,&stat_buffer)==0 && (stat_buffer.st_mode & S_IFDIR)) {
							sprintf(name,"%s/%s.zip/%s",dir_name,gamename,file);
							f->file = fopen(name,_write ? "r+b" : "rb");
                            found = f->file!=0;
						}
					}
               if (found)
               {
               if (errorlog) fprintf(errorlog,"IMAGE %s FOUND in %s!\n",file,name);
               }

				}

                extension = strrchr(file, '.');
				if (extension) *extension = '\0';

			} while (!found && extension);
	      break;

#endif


		case OSD_FILETYPE_HIGHSCORE:
			if (mame_highscore_enabled()) {
				if (!found) {
					sprintf(name,"%s/%s.hi",hidir,gamename);
					f->type = kPlainFile;
					f->file = fopen(name,_write ? "wb" : "rb");
					found = f->file!=0;
				}

				if (!found) {
					/* try with a .zip directory (if ZipMagic is installed) */
					sprintf(name,"%s.zip/%s.hi",hidir,gamename);
					f->type = kPlainFile;
					f->file = fopen(name,_write ? "wb" : "rb");
					found = f->file!=0;
				}

				if (!found) {
					/* try with a .zif directory (if ZipFolders is installed) */
					sprintf(name,"%s.zif/%s.hi",hidir,gamename);
					f->type = kPlainFile;
					f->file = fopen(name,_write ? "wb" : "rb");
					found = f->file!=0;
				}
			}
			break;
		case OSD_FILETYPE_CONFIG:
			sprintf(name,"%s/%s.cfg",cfgdir,gamename);
			f->type = kPlainFile;
			f->file = fopen(name,_write ? "wb" : "rb");
			found = f->file!=0;

			if (!found) {
				/* try with a .zip directory (if ZipMagic is installed) */
				sprintf(name,"%s.zip/%s.cfg",cfgdir,gamename);
				f->type = kPlainFile;
				f->file = fopen(name,_write ? "wb" : "rb");
				found = f->file!=0;
			}

			if (!found) {
				/* try with a .zif directory (if ZipFolders is installed) */
				sprintf(name,"%s.zif/%s.cfg",cfgdir,gamename);
				f->type = kPlainFile;
				f->file = fopen(name,_write ? "wb" : "rb");
				found = f->file!=0;
			}
			break;
		case OSD_FILETYPE_INPUTLOG:
			sprintf(name,"%s/%s.inp", inpdir, gamename);
			f->type = kPlainFile;
			f->file = fopen(name,_write ? "wb" : "rb");
			found = f->file!=0;
			break;
		case OSD_FILETYPE_STATE:
			sprintf(name,"%s/%s.sta",stadir,gamename);
			f->file = fopen(name,_write ? "wb" : "rb");
			found = !(f->file == 0);
			if (!found)
			{
				/* try with a .zip directory (if ZipMagic is installed) */
				sprintf(name,"%s.zip/%s.sta",stadir,gamename);
				f->file = fopen(name,_write ? "wb" : "rb");
				found = !(f->file == 0);
            }
			if (!found)
			{
				/* try with a .zif directory (if ZipFolders is installed) */
				sprintf(name,"%s.zif/%s.sta",stadir,gamename);
				f->file = fopen(name,_write ? "wb" : "rb");
				found = !(f->file == 0);
            }
			break;
		case OSD_FILETYPE_ARTWORK:
			/* only for reading */
			if (_write)
				break;

			sprintf(name,"%s/%s", artworkdir, filename);
			f->type = kPlainFile;
			f->file = fopen(name,_write ? "wb" : "rb");
			found = f->file!=0;
			break;
		case OSD_FILETYPE_MEMCARD:
			sprintf(name, "%s/%s",memcarddir,filename);
			f->type = kPlainFile;
			f->file = fopen(name,_write ? "wb" : "rb");
			found = f->file!=0;
			break;
		case OSD_FILETYPE_SCREENSHOT:
			/* only for writing */
			if (!_write)
				break;

			sprintf(name,"%s/%s.png", screenshotdir, filename);
			f->type = kPlainFile;
			f->file = fopen(name,_write ? "wb" : "rb");
			found = f->file!=0;
			break;
	}

	if (!found) {
		free(f);
		return 0;
	}

	return f;
}

/* JB 980920 update */
int osd_fread(void *file,void *buffer,int length)
{
	FakeFileHandle *f = (FakeFileHandle *)file;

	switch (f->type)
	{
		case kPlainFile:
			return fread(buffer,1,length,f->file);
			break;
		case kZippedFile:
		case kRAMFile:
			/* reading from the RAM image of a file */
			if (f->data)
			{
				if (length + f->offset > f->length)
					length = f->length - f->offset;
				memcpy(buffer, f->offset + f->data, length);
				f->offset += length;
				return length;
			}
			break;
	}

	return 0;
}

int osd_fread_swap(void *file,void *buffer,int length)
{
	int i;
	unsigned char *buf;
	unsigned char temp;
	int res;


	res = osd_fread(file,buffer,length);

	buf = buffer;
	for (i = 0;i < length;i+=2)
	{
		temp = buf[i];
		buf[i] = buf[i+1];
		buf[i+1] = temp;
	}

	return res;
}


/* AM 980919 update */
int osd_fwrite(void *file,const void *buffer,int length)
{
	FakeFileHandle *f = (FakeFileHandle *)file;

	switch (f->type)
	{
		case kPlainFile:
			return fwrite(buffer,1,length,((FakeFileHandle *)file)->file);
		default:
			return 0;
	}
}

int osd_fwrite_swap(void *file,const void *buffer,int length)
{
	int i;
	unsigned char *buf;
	unsigned char temp;
	int res;


	buf = (unsigned char *)buffer;
	for (i = 0;i < length;i+=2)
	{
		temp = buf[i];
		buf[i] = buf[i+1];
		buf[i+1] = temp;
	}

	res = osd_fwrite(file,buffer,length);

	for (i = 0;i < length;i+=2)
	{
		temp = buf[i];
		buf[i] = buf[i+1];
		buf[i+1] = temp;
	}

	return res;
}

int osd_fread_scatter(void *file,void *buffer,int length,int increment)
{
	unsigned char *buf = buffer;
	FakeFileHandle *f = (FakeFileHandle *)file;
	unsigned char tempbuf[4096];
	int totread,r,i;

	switch (f->type)
	{
		case kPlainFile:
			totread = 0;
			while (length)
			{
				r = length;
				if (r > 4096) r = 4096;
				r = fread(tempbuf,1,r,f->file);
				if (r == 0) return totread;	/* error */
				for (i = 0;i < r;i++)
				{
					*buf = tempbuf[i];
					buf += increment;
				}
				totread += r;
				length -= r;
			}
			return totread;
			break;
		case kZippedFile:
		case kRAMFile:
			/* reading from the RAM image of a file */
			if (f->data)
			{
				if (length + f->offset > f->length)
					length = f->length - f->offset;
				for (i = 0;i < length;i++)
				{
					*buf = f->data[f->offset + i];
					buf += increment;
				}
				f->offset += length;
				return length;
			}
			break;
	}

	return 0;
}


/* JB 980920 update */
int osd_fseek(void *file,int offset,int whence)
{
	FakeFileHandle *f = (FakeFileHandle *)file;
	int err = 0;

	switch (f->type)
	{
		case kPlainFile:
			return fseek(f->file,offset,whence);
			break;
		case kZippedFile:
		case kRAMFile:
			/* seeking within the RAM image of a file */
			switch (whence)
			{
				case SEEK_SET:
					f->offset = offset;
					break;
				case SEEK_CUR:
					f->offset += offset;
					break;
				case SEEK_END:
					f->offset = f->length + offset;
					break;
			}
			break;
	}

	return err;
}

/* JB 980920 update */
void osd_fclose(void *file)
{
	FakeFileHandle *f = (FakeFileHandle *) file;

	switch(f->type)
	{
		case kPlainFile:
			fclose(f->file);
			break;
		case kZippedFile:
		case kRAMFile:
			if (f->data)
				free(f->data);
			break;
	}
	free(f);
}

/* JB 980920 update */
/* AM 980919 */
static int checksum_file(const char* file, unsigned char **p, unsigned int *size, unsigned int *crc) {
	int length;
	unsigned char *data;
	FILE* f;

	f = fopen(file,"rb");
	if (!f) {
		return -1;
	}

	/* determine length of file */
	if (fseek (f, 0L, SEEK_END)!=0) {
		fclose(f);
		return -1;
	}

	length = ftell(f);
	if (length == -1L) {
		fclose(f);
		return -1;
	}

	/* allocate space for entire file */
	data = (unsigned char*)malloc(length);
	if (!data) {
		fclose(f);
		return -1;
	}

	/* read entire file into memory */
	if (fseek(f, 0L, SEEK_SET)!=0) {
		free(data);
		fclose(f);
		return -1;
	}

	if (fread(data, sizeof (unsigned char), length, f) != length) {
		free(data);
		fclose(f);
		return -1;
	}

	*size = length;
	*crc = crc32 (0L, data, length);
	if (p)
		*p = data;
	else
		free(data);

	fclose(f);

	return 0;
}

/* JB 980920 updated */
/* AM 980919 updated */
int osd_fchecksum (const char *game, const char *filename, unsigned int *length, unsigned int *sum)
{
	char name[256];
	int indx;
	struct stat stat_buffer;
	int found = 0;
	const char *gamename = game;

	/* Support "-romdir" yuck. */
	if (alternate_name)
		gamename = alternate_name;

	for (indx=0;indx<rompathc && !found; ++indx) {
		const char* dir_name = rompathv[indx];

		if (!found) {
			sprintf(name,"%s/%s",dir_name,gamename);
			if (cache_stat(name,&stat_buffer)==0 && (stat_buffer.st_mode & S_IFDIR)) {
				sprintf(name,"%s/%s/%s",dir_name,gamename,filename);
				if (checksum_file(name,0,length,sum)==0) {
					found = 1;
				}
			}
		}

		if (!found) {
			/* try with a .zip extension */
			sprintf(name,"%s/%s.zip", dir_name, gamename);
			if (cache_stat(name,&stat_buffer)==0) {
				if (checksum_zipped_file (name, filename, length, sum)==0) {
					if (errorlog)
						fprintf(errorlog,"Using (osd_fchecksum) zip file for %s\n", filename);
					found = 1;
				}
			}
		}

		if (!found) {
			/* try with a .zif directory (if ZipFolders is installed) */
			sprintf(name,"%s/%s.zif",dir_name,gamename);
			if (cache_stat(name,&stat_buffer)==0) {
				sprintf(name,"%s/%s.zif/%s",dir_name,gamename,filename);
				if (checksum_file(name,0,length,sum)==0) {
					found = 1;
				}
			}
		}
	}

	if (!found)
		return -1;

	return 0;
}

/* JB 980920 */
int osd_fsize (void *file)
{
	FakeFileHandle	*f = (FakeFileHandle *)file;

	if (f->type==kRAMFile || f->type==kZippedFile)
		return f->length;

	return 0;
}

/* JB 980920 */
unsigned int osd_fcrc (void *file)
{
	FakeFileHandle	*f = (FakeFileHandle *)file;

	return f->crc;
}



/* called while loading ROMs. It is called a last time with name == 0 to signal */
/* that the ROM loading process is finished. */
/* return non-zero to abort loading */
int osd_display_loading_rom_message(const char *name,int current,int total)
{
	if (name)
		fprintf(stdout,"loading %-12s\r",name);
	else
		fprintf(stdout,"                    \r");
	fflush(stdout);

	if (keyboard_pressed(KEYCODE_LCONTROL) && keyboard_pressed(KEYCODE_C))
		return 1;

	return 0;
}