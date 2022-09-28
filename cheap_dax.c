
#include "cheap_dax.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/limits.h>
#include <errno.h>
#include <string.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>

int
cheap_devdax_get_file_size(const char *fname, size_t *size)
{
	char spath[PATH_MAX];
	char npath[PATH_MAX];
	char *rpath, *basename;
	FILE *sfile;
	u_int64_t size_i;
	struct stat st;
	int rc;

	rc = stat(fname, &st);
	if (rc < 0) {
		fprintf(stderr, "%s: failed to stat file %s (%s)\n",
			__func__, fname, strerror(errno));
		return -errno;
	}

	snprintf(spath, PATH_MAX, "/sys/dev/char/%d:%d/subsystem",
		 major(st.st_rdev), minor(st.st_rdev));

	rpath = realpath(spath, npath);
	if (!rpath) {
		fprintf(stderr, "%s: realpath on %s failed (%s)\n",
			__func__, spath, strerror(errno));
		return -errno;
	}

	/* Check if DAX device */
	basename = strrchr(rpath, '/');
	if (!basename || strcmp("dax", basename+1)) {
		fprintf(stderr, "%s: %s not a DAX device!\n",
			__func__, fname);
	}

	snprintf(spath, PATH_MAX, "/sys/dev/char/%d:%d/size",
		 major(st.st_rdev), minor(st.st_rdev));

	sfile = fopen(spath, "r");
	if (!sfile) {
		fprintf(stderr, "%s: fopen on %s failed (%s)\n",
			__func__, spath, strerror(errno));
		return -EINVAL;
	}

	rc = fscanf(sfile, "%lu", &size_i);
	if (rc < 0) {
		fprintf(stderr, "%s: fscanf on %s failed (%s)\n",
			__func__, spath, strerror(errno));
		fclose(sfile);
		return -EINVAL;
	}

	fclose(sfile);

	*size = (size_t)size_i;
	return 0;
}
