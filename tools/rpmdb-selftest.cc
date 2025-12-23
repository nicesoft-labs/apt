/*
 * Simple rpmdb reader for verifying backend support.
 */

#include <config.h>

#ifdef HAVE_RPM

#include <rpm/header.h>
#include <rpm/rpmdb.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmmacro.h>
#include <rpm/rpmts.h>

#include <cstdlib>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <string>

#include "raptheader.h"

using std::cerr;
using std::cout;
using std::string;

static string ExpandMacro(const char *macro, const char *fallback)
{
   char *tmp = (char *) rpmExpand(macro, NULL);
   string val = tmp ? tmp : "";
   free(tmp);
   if (val.empty() && fallback != NULL)
      val = fallback;
   return val;
}

int main(int argc, char **argv)
{
   const char *rootDir = "/";
   int limit = 10;

   if (argc > 1)
      rootDir = argv[1];
   if (argc > 2)
      limit = atoi(argv[2]);

   rpmReadConfigFiles(NULL, NULL);

   string dbpath = ExpandMacro("%{_dbpath}", "/var/lib/rpm");
   string backend = ExpandMacro("%{_db_backend}", "");

   cout << "backend=" << backend << " dbpath=" << dbpath
	<< " root=" << rootDir << "\n";

   rpmts ts = rpmtsCreate();
   rpmtsSetVSFlags(ts, (rpmVSFlags_e)-1);
   rpmtsSetRootDir(ts, rootDir);

   int rc = rpmtsOpenDB(ts, O_RDONLY);
   if (rc != 0) {
      cerr << "Cannot open rpmdb: " << strerror(errno) << "\n";
      rpmtsFree(ts);
      return 1;
   }

   rpmdbMatchIterator mi = rpmtsInitIterator(ts, RPMDBI_PACKAGES, NULL, 0);
   if (mi == NULL) {
      cerr << "Cannot create rpmdb iterator\n";
      rpmtsFree(ts);
      return 1;
   }

   int count = 0;
   Header h;
   while ((h = rpmdbNextIterator(mi)) != NULL) {
      raptHeader hdr(h);
      string name;
      string version;
      string release;
      string arch;
      raptInt epochValue = 0;
      string epoch = "0";

      hdr.getTag(RPMTAG_NAME, name);
      hdr.getTag(RPMTAG_VERSION, version);
      hdr.getTag(RPMTAG_RELEASE, release);
      hdr.getTag(RPMTAG_ARCH, arch);
      if (hdr.getTag(RPMTAG_EPOCH, epochValue))
	 epoch = std::to_string(epochValue);

      cout << name << "-" << epoch << ":" << version
	   << "-" << release << "." << arch << "\n";

      if (++count >= limit)
	 break;
   }

   rpmdbFreeIterator(mi);
   rpmtsFree(ts);

   return 0;
}

#else

int main(int argc, char **argv)
{
   (void)argc;
   (void)argv;
   return 1;
}

#endif
