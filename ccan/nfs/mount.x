/* copied from RFC1813 */

const MNTPATHLEN = 1024;  /* Maximum bytes in a path name */
const MNTNAMLEN  = 255;   /* Maximum bytes in a name */
const FHSIZE3    = 64;    /* Maximum bytes in a V3 file handle */


typedef opaque fhandle3<FHSIZE3>;
typedef string dirpath<MNTPATHLEN>;
typedef string name<MNTNAMLEN>;

enum mountstat3 {
	MNT3_OK = 0,                 /* no error */
	MNT3ERR_PERM = 1,            /* Not owner */
	MNT3ERR_NOENT = 2,           /* No such file or directory */
	MNT3ERR_IO = 5,              /* I/O error */
	MNT3ERR_ACCES = 13,          /* Permission denied */
	MNT3ERR_NOTDIR = 20,         /* Not a directory */
	MNT3ERR_INVAL = 22,          /* Invalid argument */
	MNT3ERR_NAMETOOLONG = 63,    /* Filename too long */
	MNT3ERR_NOTSUPP = 10004,     /* Operation not supported */
	MNT3ERR_SERVERFAULT = 10006  /* A failure on the server */
};


typedef struct mountbody *mountlist;

struct mountbody {
	name       ml_hostname;
	dirpath    ml_directory;
	mountlist  ml_next;
};

typedef struct groupnode *groups;

struct groupnode {
	name     gr_name;
	groups   gr_next;
};


typedef struct exportnode *exports;

struct exportnode {
	dirpath  ex_dir;
	groups   ex_groups;
	exports  ex_next;
};

struct mountres3_ok {
	fhandle3   fhandle;
	int        auth_flavors<>;
};

union mountres3 switch (mountstat3 fhs_status) {
	case MNT3_OK:
		mountres3_ok  mountinfo;
	default:
		void;
};

program MOUNT_PROGRAM {
	version MOUNT_V3 {
	void      MOUNT3_NULL(void)    = 0;
	mountres3 MOUNT3_MNT(dirpath)  = 1;
	mountlist MOUNT3_DUMP(void)    = 2;
	void      MOUNT3_UMNT(dirpath) = 3;
	void      MOUNT3_UMNTALL(void) = 4;
	exports   MOUNT3_EXPORT(void)  = 5;
	} = 3;
} = 100005;
