// Copyright 2009 Kwiirk
// Licensed under the terms of the GNU GPL, version 2
// http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt

#ifdef WIN32
#include <windows.h>
#endif

#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */

#ifdef WIN32
#include "win32/xgetopt.h"
#include <direct.h>
#include <io.h>
#define snprintf _snprintf
#else
#include <getopt.h>
#include <unistd.h>
#endif
#include <errno.h>

#include <sys/stat.h>
#include "tools.h"
#include "libwbfs.h"




wbfs_t *wbfs_try_open(char *disk, char *partition, int reset);
wbfs_t *wbfs_try_open_partition(char *fn, int reset);

#define GB (1024 * 1024 * 1024.)
#define MB (1024 * 1024)

#ifdef WIN32

int read_wii_disc_sector(void *_handle, u32 _offset, u32 count, void *buf)
{
	HANDLE *handle = (HANDLE *)_handle;
	LARGE_INTEGER large;
	DWORD read;
	u64 offset = _offset;
	
	offset <<= 2;
	large.QuadPart = offset;
	
	if (SetFilePointerEx(handle, large, NULL, FILE_BEGIN) == FALSE)
	{
		wbfs_error("error seeking in disc file");
		return 1;
	}
	
	read = 0;
	if ((ReadFile(handle, buf, count, &read, NULL) == FALSE) || !read)
	{
		wbfs_error("error reading wii disc sector");
		return 1;
	}

	if (read < count)
	{
		wbfs_warning("warning: requested %d, but read only %d bytes (trimmed or bad padded ISO)", count, read);
		wbfs_memset((u8*)buf+read, 0, count-read);
	}

	return 0;
}

int write_wii_disc_sector(void *_handle, u32 lba, u32 count, void *buf)
{
	HANDLE *handle = (HANDLE *)_handle;
	LARGE_INTEGER large;
	DWORD written;
	u64 offset = lba;
	
	offset *= 0x8000;
	large.QuadPart = offset;
	
	if (SetFilePointerEx(handle, large, NULL, FILE_BEGIN) == FALSE)
	{
		fprintf(stderr,"\n\n%lld %p\n", offset, handle);
		wbfs_error("error seeking in wii disc sector (write)");
		return 1;
	}
	
	written = 0;
	if (WriteFile(handle, buf, count * 0x8000, &written, NULL) == FALSE)
	{
		wbfs_error("error writing wii disc sector");
		return 1;
	}

	if (written != count * 0x8000)
	{
		wbfs_error("error writing wii disc sector (size mismatch)");
		return 1;
	}
	
	return 0;
}

#else

int read_wii_file(void*_fp,u32 offset,u32 count,void*iobuf)
{
	FILE*fp =_fp;
	u64 off = offset;
	off<<=2;

	if (fseeko(fp, off, SEEK_SET))
	{
		wbfs_error("error seeking in disc file");
                return 1;
        }
        if (fread(iobuf, count, 1, fp) != 1){
                wbfs_error("error reading disc");
                return 1;
	}
	return 0;
}

int write_wii_sector_file(void*_fp,u32 lba,u32 count,void*iobuf)
{
	FILE*fp=_fp;
	u64 off = lba;
	off *=0x8000;

	if (fseeko(fp, off, SEEK_SET))
        {
                fprintf(stderr,"\n\n%lld %p\n",off,_fp);
		wbfs_error("error seeking in written disc file");
                return 1;
        }
        if (fwrite(iobuf, count*0x8000, 1, fp) != 1){
                wbfs_error("error writing disc file");
                return 1;
        }
        return 0;
}

#endif

#ifdef WIN32
void wbfs_applet_list(wbfs_t *p, BOOL shortcmd)
#else
int wbfs_applet_ls(wbfs_t *p)
#endif
{
	int count = wbfs_count_discs(p);

	if (count == 0)
	{
		fprintf(stderr,"wbfs is empty\n");
	}
	else
	{
		int i;
		u32 size;
		u8 *b = wbfs_ioalloc(0x100);
		for (i = 0; i < count; i++)
		{
			if (!wbfs_get_disc_info(p, i, b, 0x100, &size))
			{
#ifdef WIN32
				if(shortcmd)
				{
					fprintf(stderr, "%c%c%c%c%c%c %s\n", b[0], b[1], b[2], b[3], b[4], b[5], b + 0x20);
				}
				else
				{
					fprintf(stderr, "%c%c%c%c%c%c %4dM %s\n", b[0], b[1], b[2], b[3], b[4], b[5], (u32)(size * 4ULL / (MB)), b + 0x20);
				}
#else
				fprintf(stderr, "%c%c%c%c%c%c|@|%-55s|@|%.2fG\n",b[0], b[1], b[2], b[3], b[4], b[5],
						b + 0x20,size*4ULL/(GB));
#endif
			}
		}

		wbfs_iofree(b);
	}   

#ifdef WIN32
	if(!shortcmd)
#endif
#ifndef __APPLE__
	{
		u32 blcount = wbfs_count_usedblocks(p);
		fprintf(stderr, "------------\n %u Files - Total: %.2fG, Used: %.2fG, Free: %.2fG\n",
				count,
				(float)p->n_wbfs_sec * p->wbfs_sec_sz / GB, 
				(float)(p->n_wbfs_sec-blcount) * p->wbfs_sec_sz / GB,
				(float)(blcount) * p->wbfs_sec_sz / GB);
	}
#endif
#ifndef WIN32
	return 0;
#endif
}

#ifdef WIN32
void wbfs_applet_info(wbfs_t *p, BOOL shortcmd)
#else
int wbfs_applet_df(wbfs_t *p)
#endif
{
	u32 count = wbfs_count_usedblocks(p);
	
#ifdef WIN32
	fprintf(stderr, "wbfs\n");
	fprintf(stderr, "  blocks : %u\n", count);
	fprintf(stderr, "  total  : %.2fG\n", (float)p->n_wbfs_sec * p->wbfs_sec_sz / GB);
	fprintf(stderr, "  used   : %.2fG\n", (float)(p->n_wbfs_sec-count) * p->wbfs_sec_sz / GB);
	fprintf(stderr, "  free   : %.2fG\n", (float)(count) * p->wbfs_sec_sz / GB);
	
#else
#ifdef MAC_UI_STUFF
 	fprintf(stderr, "Total: %.2fG Used: %.2fG  Free: %.2fG\n", 
 			(float)p->n_wbfs_sec*p->wbfs_sec_sz/GB,
			(float)(p->n_wbfs_sec-count)*p->wbfs_sec_sz/GB,
			(float)(count)*p->wbfs_sec_sz/GB);
#else
 	fprintf(stderr, "%.2f|@|%.2f\n", 
			(float)p->n_wbfs_sec*p->wbfs_sec_sz/GB,
			(float)(count)*p->wbfs_sec_sz/GB);
#endif

#endif

	return p != 0;
}

#ifdef WIN32
void wbfs_applet_makehbc(wbfs_t *p, BOOL shortcmd)
#else
int wbfs_applet_mkhbc(wbfs_t *p)
#endif
{
	int count = wbfs_count_discs(p);
#ifndef WIN32
	char filename[7];
#endif
	FILE *xml;
	
	if (count == 0)
	{
		fprintf(stderr,"wbfs is empty\n");
	}
	else
	{
		int i;
		u32 size;
		u8 *b = wbfs_ioalloc(0x100);
		
		for (i = 0; i < count; i++)
		{
#ifdef WIN32
			char dirname[7 + 1];
			char dolname[7 + 1 + 8 + 1];
			char pngname[7 + 1 + 8 + 1];
			char xmlname[7 + 1 + 8 + 1];

			wbfs_get_disc_info(p, i, b, 0x100, &size);

			snprintf(dirname, 7, "%c%c%c%c%c%c", b[0], b[1], b[2], b[3], b[4], b[5]);
			snprintf(dolname, 7 + 1 + 8, "%c%c%c%c%c%c\\boot.dol", b[0], b[1], b[2], b[3], b[4], b[5]);
			snprintf(pngname, 7 + 1 + 8, "%c%c%c%c%c%c\\icon.png", b[0], b[1], b[2], b[3], b[4], b[5]);
			snprintf(xmlname, 7 + 1 + 8, "%c%c%c%c%c%c\\meta.xml", b[0], b[1], b[2], b[3], b[4], b[5]);
			
			CreateDirectory(dirname, NULL);
			printf("%s\n", dirname);
			
			CopyFile("boot.dol", dolname, FALSE);
			CopyFile("icon.png", pngname, FALSE);

			xml = fopen(xmlname, "wb");

#else
			wbfs_get_disc_info(p,i,b,0x100,&size);
			snprintf(filename,7,"%c%c%c%c%c%c",b[0], b[1], b[2], b[3], b[4], b[5]);
			mkdir(filename, 0777);
			printf("%s\n",filename);
			if (chdir(filename))
					wbfs_fatal("chdir");
#ifndef MAC_UI_STUFF
			system("cp ../boot.dol .");
			system("cp ../icon.png .");
#endif

			xml = fopen("meta.xml","w");

#endif

			fprintf(xml,"<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
			fprintf(xml,"<app>\n\t<name>%s</name>\n", b + 0x20);
			fprintf(xml,"<short_description>%.2fGB on USB HD</short_description>\n", size * 4ULL / GB);
#ifdef WIN32
			fprintf(xml,"<long_description>This launches the yal wbfs game loader by Kwiirk for discid %s</long_description>\n", dirname);

#else
#ifdef MAC_UI_STUFF
			fprintf(xml,"<long_description>Click the Load Button to play %s from your USB Drive</long_description>\n", b+0x20);
#else
			fprintf(xml,"<long_description>This launches the yal wbfs game loader by Kwiirk for discid %s</long_description>\n",filename);
#endif

#endif
			fprintf(xml,"</app>");
			fclose(xml);

#ifndef WIN32
			if (chdir(".."))
					wbfs_fatal("chdir");
#endif
		}

		wbfs_iofree(b);
	}   
	
#ifndef WIN32
	return p!=0;
#endif

}

#ifdef WIN32
void wbfs_applet_init(wbfs_t *p, BOOL shortcmd)
#else
int wbfs_applet_init(wbfs_t *p)
#endif
{
        // nothing to do actually..
        // job already done by the reset flag of the wbfs_open_partition
#ifdef WIN32
	fprintf(stderr, "wbfs initialized.\n");
#else
	return p != 0;
#endif
}

#ifdef WIN32
void wbfs_applet_estimate(wbfs_t *p, BOOL shortcmd, int argc, char *argv[])
{
	HANDLE *handle = CreateFile(argv[0], GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);

	if (handle == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "unable to open disc file\n");
	}
	else
	{
		u32 estimation = wbfs_estimate_disc(p, read_wii_disc_sector, handle, ONLY_GAME_PARTITION);
		fprintf(stderr, "%.2fG\n", estimation / (GB));
	}
}
#endif
int _wbfs_disc_read(void *fp, u32 lba, u32 count, void *iobuf)
{
        int ret = wbfs_disc_read(fp, lba, iobuf, count);
        static int num_fail=0;
        if (ret)
        {
                if(num_fail==0)
                        wbfs_error("error reading lba probably the two wbfs don't have the same granularity. Ignoring...\n");
                if(num_fail++ > 0x100)
                {
                        wbfs_fatal( "too many error giving up...\n");
                        return 1;
                }
        }
        else
                num_fail = 0;
	return 0;
}
static void _spinner(int x, int y){ spinner(x, y); }
#ifdef WIN32
static void _progress(int x, int y){ progress(x, y); }
#endif

#ifdef WIN32
void wbfs_applet_add(wbfs_t *p, BOOL shortcmd, int argc, char *argv[])
#else
int wbfs_applet_add(wbfs_t *p,char*argv)
#endif
{
#ifndef WIN32
	FILE *f = fopen(argv,"r");
#endif
	u8 discinfo[7];
	wbfs_disc_t *d;
	
#ifdef WIN32
	HANDLE *handle = CreateFile(argv[0], GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	
	if (handle == INVALID_HANDLE_VALUE)
	{
		fprintf(stderr, "unable to open disc file\n");
	}
	else
	{
		DWORD read = 0;
		ReadFile(handle, discinfo, 6, &read, NULL);

		d = wbfs_open_disc(p, discinfo);
		
		if (d)
		{
			discinfo[6] = 0;
			fprintf(stderr, "%s already in disc...\n", discinfo);
			wbfs_close_disc(d);
		}
		else
		{
			wbfs_add_disc(p, read_wii_disc_sector, handle, shortcmd ? _progress : _spinner, ONLY_GAME_PARTITION, 0, (argc>1) ? argv[1] : NULL);
        }

		CloseHandle(handle);
	}

#else
        if(!f)
                wbfs_fatal("unable to open disc file");
        else
        {
                fread(discinfo,6,1,f);
                if (memcmp(discinfo, "WBFS", 4) == 0) // add all games in the wbfs partition
                {
                        fclose(f);
                        wbfs_t *src_p = wbfs_try_open_partition(argv, 0);
                        if (!src_p)
                        {
                                wbfs_fatal("incorrect wbfs file");
                                return 1;
                        }
                        int i, count = wbfs_count_discs(src_p);
                        u8 *b = wbfs_ioalloc(0x100);
                        if (count==0)
                          wbfs_fatal("no disc in wbfs");
                        for (i=0;i<count;i++)
                        {
                                if(!wbfs_get_disc_info(src_p, i, b, 0x100, NULL)){
                                        d = wbfs_open_disc(p, b);
                                        if(!d){ // not here already.
                                                d = wbfs_open_disc(src_p, b);
                                                wbfs_add_disc(p, _wbfs_disc_read, d, _spinner, ONLY_GAME_PARTITION, 0);
                                        }
                                        wbfs_close_disc(d);
                                }
                        }
                        wbfs_close(src_p);
                }
                else
                {
                        d = wbfs_open_disc(p,discinfo);
                        if(d)
                        {
                                discinfo[6]=0;
                                fprintf(stderr,"%s already in disc..\n",discinfo);
                                wbfs_close_disc(d);
                                return 0;
                        } else
                                return wbfs_add_disc(p,read_wii_file,f,_spinner,ONLY_GAME_PARTITION,0);
                }
        }
	return 1;
#endif

}


#ifdef WIN32
void wbfs_applet_remove(wbfs_t *p, BOOL shortcmd,  int argc, char *argv[])
{
	if ( wbfs_rm_disc(p, (u8 *)(argv[0])) )
	{
		wbfs_error("Couldn't find %s", argv[0]);
	}
	else
	{
		printf("Done.\n");
	}
}
#else
int wbfs_applet_rm(wbfs_t *p,char *argv)
{
	return wbfs_rm_disc(p,(u8*)argv);
}
#endif

#ifdef WIN32
void wbfs_applet_extract(wbfs_t *p, BOOL shortcmd, int argc, char *argv[])
{
	wbfs_disc_t *d;
	d = wbfs_open_disc(p, (u8 *)(argv[0]));
	
	if (d)
	{
		HANDLE *handle;
		char isoname[0x100];
		int i,len;
		
		if(argc>1)
		{
			wbfs_memset(isoname, 0, sizeof(isoname));
			strncpy(isoname, argv[1], 0x40);
			len = strlen(isoname);
		}
		else
		{
			/* get the name of the title to find out the name of the iso */
			strncpy(isoname, (char *)d->header->disc_header_copy + 0x20, 0x100);
			len = strlen(isoname);
		
			// replace silly chars by '_'
			for (i = 0; i < len; i++)
			{
				if (isoname[i] == ' ' || isoname[i] == '/' || isoname[i] == ':')
				{
					isoname[i] = '_';
				}
			}
		}
		
		strncpy(isoname + len, ".iso", 0x100 - len);
		
		handle = CreateFile(isoname, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL);
		
		if (handle == INVALID_HANDLE_VALUE)
		{
			fprintf(stderr, "unable to open disc file (%s) for writing\n", isoname);
		}
		else
		{
			LARGE_INTEGER large;

			fprintf(stderr, "writing to %s\n", isoname);

			large.QuadPart = (d->p->n_wii_sec_per_disc / 2) * 0x8000ULL;
			SetFilePointerEx(handle, large, NULL, FILE_BEGIN);
			SetEndOfFile(handle);

			wbfs_extract_disc(d,write_wii_disc_sector, handle, _spinner);
			
			CloseHandle(handle);
		}
		
		wbfs_close_disc(d);
	}
	else
	{
		wbfs_error("Couldn't find %s", argv[0]);
	}
}

#else
int wbfs_applet_extract(wbfs_t *p, char *argv)
{
        wbfs_disc_t *d;
        int ret = 1;
        d = wbfs_open_disc(p,(u8*)argv);
        if (d)
        {
                char buf[0x100];
                int i, len;
                /* get the name of the title to find out the name of the iso */
                strncpy(buf,(char*)d->header->disc_header_copy+0x20,0x100);
                len = strlen(buf);
                // replace silly chars by '_'
                for( i = 0; i < len; i++)
                        if(buf[i]==' ' || buf[i]=='/' || buf[i]==':')
                                buf[i] = '_';
                strncpy(buf+len,".iso",0x100-len);
                FILE *f=fopen(buf,"w");
			if (!f) {
				wbfs_fatal("unable to open dest file");
            }    
			else {
				fprintf(stderr,"writing to %s\n",buf);

				// write a zero at the end of the iso to ensure the correct size
				// XXX should check if the game is DVD9..
				fseeko(f,(d->p->n_wii_sec_per_disc/2)*0x8000ULL-1ULL,SEEK_SET);
				fwrite("",1,1,f);

				ret = wbfs_extract_disc(d,write_wii_sector_file,f,_spinner);

				fclose(f);
			}

			wbfs_close_disc(d);
                
        }
        else {
                fprintf(stderr, "%s not in disc in disc..\n", argv);
		}
        return ret;
}

#endif


#ifdef WIN32
void wbfs_applet_rename(wbfs_t *p, BOOL shortcmd, int argc, char *argv[])
{
	if(argc<2)
	{
		_set_errno(EINVAL);
		wbfs_error("error");
	}

	if(wbfs_ren_disc(p, argv[0], argv[1]))
	{
		wbfs_error("error");
	}

	printf("Done.\n");
}

#else

int wbfs_applet_rename(wbfs_t *p,char *arg1, char *arg2)
{
	if(wbfs_ren_disc(p,(u8*)arg1,(u8*)arg2))
	{
		wbfs_error("error");
	}
	
	return 0;
}
int wbfs_applet_create(char*argv)
{
        char buf[1024];
        strncpy(buf,argv,1019);
        strcpy(buf+strlen(buf),".wbfs");
        FILE *f=fopen(buf,"w");
        if(!f)
                wbfs_fatal("unable to open dest file");
        else{
                // reserve space for the maximum size.
                fseeko(f,143432*2*0x8000ULL-1ULL,SEEK_SET);
                fwrite("",1,1,f);
                fclose(f);
                wbfs_t *p = wbfs_try_open_partition(buf,1);
                if(p){
                        wbfs_applet_add(p,argv);
                        wbfs_trim(p);
                        ftruncate(fileno(p->callback_data),p->n_hd_sec*512ULL);
                        wbfs_close(p);
                }
        }
        return 0;
}

#endif

int wbfs_applet_nid(wbfs_t *p,char *arg1, char *arg2)
{	
	if(wbfs_nid_disc(p,(u8*)arg1, (u8*)arg2))
	{
		wbfs_error("error");
	}
	
	return 0;
}


#ifdef WIN32
struct wbfs_applets
{
	char *command;
	char *shortcmd;
	char *arglist;
	void (*function_with_argument)(wbfs_t *p, BOOL shortcmd, int argc, char *argv[]);
	void (*function)(wbfs_t *p, BOOL shortcmd);
}

#else
struct wbfs_applets
{
	char *opt;
	int (*function_with_argument)(wbfs_t *p, char *argv);
	int (*function)(wbfs_t *p);
	int (*func_args)(wbfs_t *p, char *arg1, char *arg2);
} 

#endif


#ifdef WIN32
#define APPLET(x, y, z) { #x, #y, #z, wbfs_applet_##x, NULL }
#define APPLET_NOARG(x, y) { #x, #y, "", NULL, wbfs_applet_##x }

#else
#define APPLET(x) { #x,wbfs_applet_##x,NULL,NULL}
#define APPLET_NOARG(x) { #x,NULL,wbfs_applet_##x,NULL}
#define APPLET_ARGS(x) { #x, NULL, NULL, wbfs_applet_##x}
#endif


#ifdef WIN32
wbfs_applets[] =
{
        APPLET_NOARG(list, l),
        APPLET_NOARG(info, i),
        APPLET_NOARG(makehbc, hbc),
        APPLET_NOARG(init, init),
        APPLET(estimate, e, <file>),
        APPLET(add, a, <file> [new_name]),
        APPLET(rename, r, <ID> <new_name>),
        APPLET(extract, x, <ID> [iso_name]),
        APPLET(remove, d, <ID>)
};

#undef APPLET
#undef APPLET_NOARG


#else
wbfs_applets[] = 
{
        APPLET_NOARG(ls),
        APPLET_NOARG(df),
        APPLET_NOARG(mkhbc),
        APPLET_NOARG(init),
        APPLET(add),
        APPLET(rm),
        APPLET_ARGS(rename),
        APPLET_ARGS(nid),
        APPLET(extract),
};

#endif

static int num_applets = sizeof(wbfs_applets) / sizeof(wbfs_applets[0]);

#ifdef WIN32
void usage(char **argv)
{
	int i;
	fprintf(stderr, "\nwbfs windows port build 'delta'. Mod v1.0 by Sorg.\n\nUsage:\n");
	
	for (i = 0; i < num_applets; i++)
	{
		if(!strcmp(wbfs_applets[i].command, wbfs_applets[i].shortcmd))
		{
			fprintf(stderr, "  %s <drive letter> %s %s\n", argv[0], wbfs_applets[i].command, wbfs_applets[i].arglist);
		}
		else
		{
			fprintf(stderr, "  %s <drive letter> %s|%s %s\n", argv[0], wbfs_applets[i].command, wbfs_applets[i].shortcmd, wbfs_applets[i].arglist);
		}
	}
}

#else
void usage(char **argv)
{
        int i;
        fprintf(stderr, 
        		"Usage: %s [-d disk|-p partition]\n",
                argv[0]);

	for (i = 0; i < num_applets; i++)
                fprintf(stderr, "\t %s %s %s\n",wbfs_applets[i].opt,wbfs_applets[i].function_with_argument||wbfs_applets[i].func_args
                        ?"(file|id)":"",
                        wbfs_applets[i].func_args?"(newid|newname)":"");
        exit(EXIT_FAILURE);
}
#endif

#ifdef WIN32
int main(int argc, char *argv[])
{
	int opt;
	int i;
	BOOL executed = FALSE;
	char *partition = NULL;
	char *command = NULL;
	
	while ((opt = getopt(argc, argv, "d:hf")) != -1)
	{
		switch (opt)
		{
			case 'f':
				wbfs_set_force_mode(1);
				break;
			
			case 'h':
			default: /* '?' */
				return usage(argv);
		}
	}
	
	if (optind + 2 > argc)
	{
		return usage(argv);
	}
	
	partition = argv[optind + 0];
	command = argv[optind + 1];
	
	if (partition == NULL || strlen(partition) != 1)
	{
		fprintf(stderr, "You must supply a valid drive letter.\n");
		return EXIT_FAILURE;
	}
	
	if (strcmp(command, "init") == 0)
	{
		char c;
		fprintf(stderr, "!!! Warning ALL data on drive '%s' will be lost irretrievably !!!\n\n", partition);
		fprintf(stderr, "Are you sure? (y/n): ");

		c = getchar();
		if (toupper(c) != 'Y')
		{
			fprintf(stderr, "Aborted.\n");
			return EXIT_FAILURE;
		}
		
		fprintf(stderr, "\n");
	}

	for (i = 0; i < num_applets; i++)
	{
		struct wbfs_applets *ap = &wbfs_applets[i];
		
		if(_stricmp(command, ap->command) == 0 || _stricmp(command, ap->shortcmd) == 0)
		{
			wbfs_t *p;
			
			p = wbfs_try_open(NULL, partition, ap->function == wbfs_applet_init);
			if (!p)
			{
				executed = TRUE;
				break;
			}
			
			if (ap->function_with_argument)
			{
				if (optind + 3 > argc)
				{
					break;
				}
				else
				{
					executed = TRUE;
					ap->function_with_argument(p, _stricmp(command, ap->shortcmd) == 0, argc-(optind+2), argv+(optind + 2));
				}
			}
			else
			{
				executed = TRUE;
				ap->function(p, _stricmp(command, ap->shortcmd) == 0);
			}
			
			wbfs_close(p);
			break;
		}
	}
	
	if (executed == FALSE)
	{
		return usage(argv);
	}
	
	return EXIT_SUCCESS;
}

#else
int main(int argc, char *argv[])
{
	int opt;
	int i;
	char *partition = NULL, *disk = NULL;

	while ((opt = getopt(argc, argv, "p:d:hf")) != -1) {
			switch (opt) {
			case 'p':
					partition = optarg;
					break;
			case 'd':
					disk = optarg;
					break;
			case 'f':
					wbfs_set_force_mode(1);
					break;
			case 'h':
			default: /* '?' */
				usage(argv);
			}
	}

	if (optind >= argc) {
		usage(argv);
		exit(EXIT_FAILURE);
	}
	
	if ( strcmp(argv[optind], "create") == 0 )
	{
		if (optind+1 >= argc)
				usage(argv);
		else
			return wbfs_applet_create(argv[optind+1]);			
	}

	int command_found = 0;
	
	for (i = 0; i < num_applets; i++)
	{
		struct wbfs_applets *ap = &wbfs_applets[i];
		if ( strcmp( argv[optind], ap->opt) == 0 )
		{
			command_found = 1;
			wbfs_t *p = wbfs_try_open(disk, partition,
									  ap->function== wbfs_applet_init);
			if(!p)
                                break;
                        if (ap->function_with_argument)
                        {
                                if (optind + 1 >= argc)
                                        usage(argv);
                                else
                                        while (optind + 1 < argc){
                                                ap->function_with_argument(p, argv[optind+1]);
                                                optind++;
                                        }
                        }
                        else if (ap->func_args){
                                if (optind + 2 >= argc)
                                        usage(argv);
                                else
                                        while (optind + 2 < argc){
                                                ap->func_args(p, argv[optind+1], argv[optind+2]);
                                                optind+=2;
                                        }
                        }
                        else 
                        {
                                ap->function(p);
                        }
                        
			wbfs_close(p);
			break;
		}
	}

	if (command_found) {
		exit(EXIT_SUCCESS);
	}
	else {
		printf("Unknown command: %s\n", argv[optind]);
		exit(EXIT_FAILURE);
	}
}

#endif
