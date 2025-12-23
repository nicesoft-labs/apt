// -*- mode: c++; mode: fold -*-
// Description							/*{{{*/
/* ######################################################################

   HTTPS Aquire Method - This is the HTTPS aquire method for APT.

   It uses libcurl for TLS handling and supports OpenSSL engines such as
   GOST via configuration.

   ##################################################################### */
							/*}}}*/
// Include Files							/*{{{*/
#include <apt-pkg/acquire-method.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/strutl.h>

#include <apti18n.h>

#include <curl/curl.h>

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
							/*}}}*/

namespace
{
struct CurlWriteContext
{
   FileFd *File;
   Hashes *Hash;
   bool ShouldTruncate;
   bool Truncated;
   bool IMSHit;
   long HttpCode;
};

size_t WriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
   CurlWriteContext *Ctx = static_cast<CurlWriteContext *>(userdata);
   size_t Total = size * nmemb;
   if (Total == 0)
      return 0;

   if (Ctx->ShouldTruncate == true && Ctx->Truncated == false &&
       Ctx->HttpCode != 304)
   {
      if (ftruncate(Ctx->File->Fd(),0) != 0)
	 return 0;
      if (lseek(Ctx->File->Fd(),0,SEEK_SET) < 0)
	 return 0;
      delete Ctx->Hash;
      Ctx->Hash = new Hashes;
      Ctx->Truncated = true;
   }

   if (Ctx->File->Write(ptr,Total) == false)
      return 0;

   if (Ctx->Hash != 0)
      Ctx->Hash->Add(reinterpret_cast<const unsigned char *>(ptr),Total);

   return Total;
}

size_t HeaderCallback(char *buffer, size_t size, size_t nitems, void *userdata)
{
   CurlWriteContext *Ctx = static_cast<CurlWriteContext *>(userdata);
   size_t Total = size * nitems;

   if (Total >= 5 && strncmp(buffer,"HTTP/",5) == 0)
   {
      const char *CodeStart = strchr(buffer,' ');
      if (CodeStart != 0)
      {
	 Ctx->HttpCode = strtol(CodeStart + 1,0,10);
	 Ctx->ShouldTruncate = true;
	 Ctx->Truncated = false;
	 Ctx->IMSHit = (Ctx->HttpCode == 304);
      }
   }

   return Total;
}
}

class HttpsMethod : public pkgAcqMethod
{
   unsigned long TimeOut;

   virtual bool Fetch(FetchItem *Itm);
   virtual bool Configuration(string Message);

   public:
   HttpsMethod() : pkgAcqMethod("1.0",SendConfig), TimeOut(120) {}
};

bool HttpsMethod::Configuration(string Message)
{
   if (pkgAcqMethod::Configuration(Message) == false)
      return false;

   TimeOut = _config->FindI("Acquire::https::Timeout",TimeOut);
   return true;
}

bool HttpsMethod::Fetch(FetchItem *Itm)
{
   URI Get = Itm->Uri;
   FetchResult Res;
   Res.Filename = Itm->DestFile;

   FileFd File(Itm->DestFile,FileFd::WriteAny);
   if (_error->PendingError() == true)
      return false;

   CurlWriteContext Context;
   Context.File = &File;
   Context.Hash = new Hashes;
   Context.ShouldTruncate = false;
   Context.Truncated = false;
   Context.IMSHit = false;
   Context.HttpCode = 0;

   CURL *Curl = curl_easy_init();
   if (Curl == 0)
   {
      delete Context.Hash;
      return _error->Error(_("Unable to initialize HTTPS transport"));
   }

   curl_easy_setopt(Curl, CURLOPT_URL, Itm->Uri.c_str());
   curl_easy_setopt(Curl, CURLOPT_FOLLOWLOCATION, 1L);
   curl_easy_setopt(Curl, CURLOPT_MAXREDIRS, 5L);
   curl_easy_setopt(Curl, CURLOPT_CONNECTTIMEOUT, static_cast<long>(TimeOut));
   curl_easy_setopt(Curl, CURLOPT_TIMEOUT, static_cast<long>(TimeOut));
   curl_easy_setopt(Curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
   curl_easy_setopt(Curl, CURLOPT_HEADERDATA, &Context);
   curl_easy_setopt(Curl, CURLOPT_WRITEFUNCTION, WriteCallback);
   curl_easy_setopt(Curl, CURLOPT_WRITEDATA, &Context);
   curl_easy_setopt(Curl, CURLOPT_FILETIME, 1L);

   if (Get.User.empty() == false)
   {
      string UserPass = Get.User + ":" + Get.Password;
      curl_easy_setopt(Curl, CURLOPT_USERPWD, UserPass.c_str());
   }

   if (Itm->LastModified != 0)
   {
      curl_easy_setopt(Curl, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
      curl_easy_setopt(Curl, CURLOPT_TIMEVALUE, static_cast<long>(Itm->LastModified));
   }

   string Proxy = _config->Find("Acquire::https::Proxy");
   if (Proxy.empty() == true)
      Proxy = _config->Find("Acquire::http::Proxy");
   if (Proxy.empty() == false && Proxy != "DIRECT")
      curl_easy_setopt(Curl, CURLOPT_PROXY, Proxy.c_str());

   bool VerifyPeer = _config->FindB("Acquire::https::Verify-Peer",true);
   curl_easy_setopt(Curl, CURLOPT_SSL_VERIFYPEER, VerifyPeer ? 1L : 0L);

   bool VerifyHost = _config->FindB("Acquire::https::Verify-Host",true);
   curl_easy_setopt(Curl, CURLOPT_SSL_VERIFYHOST, VerifyHost ? 2L : 0L);

   string CaInfo = _config->Find("Acquire::https::CaInfo");
   if (CaInfo.empty() == false)
      curl_easy_setopt(Curl, CURLOPT_CAINFO, CaInfo.c_str());

   string CaPath = _config->Find("Acquire::https::CaPath");
   if (CaPath.empty() == false)
      curl_easy_setopt(Curl, CURLOPT_CAPATH, CaPath.c_str());

   string SslEngine = _config->Find("Acquire::https::SSL::Engine");
   if (SslEngine.empty() == false)
   {
      curl_easy_setopt(Curl, CURLOPT_SSLENGINE, SslEngine.c_str());
      curl_easy_setopt(Curl, CURLOPT_SSLENGINE_DEFAULT, 1L);
   }

   string CipherList = _config->Find("Acquire::https::SSL::CipherList");
   if (CipherList.empty() == false)
      curl_easy_setopt(Curl, CURLOPT_SSL_CIPHER_LIST, CipherList.c_str());

   CURLcode Code = curl_easy_perform(Curl);
   if (Code != CURLE_OK)
   {
      string Err = curl_easy_strerror(Code);
      curl_easy_cleanup(Curl);
      delete Context.Hash;
      return _error->Error(_("HTTPS fetch failed: %s"),Err.c_str());
   }

   long ResponseCode = 0;
   curl_easy_getinfo(Curl, CURLINFO_RESPONSE_CODE, &ResponseCode);
   if (ResponseCode >= 400)
   {
      curl_easy_cleanup(Curl);
      delete Context.Hash;
      return _error->Error(_("HTTPS server returned error %ld"),ResponseCode);
   }

   if (Context.ShouldTruncate == true && Context.Truncated == false &&
       Context.HttpCode != 304)
   {
      if (ftruncate(File.Fd(),0) != 0 || lseek(File.Fd(),0,SEEK_SET) < 0)
      {
         curl_easy_cleanup(Curl);
         delete Context.Hash;
         return _error->Error(_("HTTPS fetch failed to truncate output file"));
      }
      Context.Truncated = true;
   }

   long FileTime = -1;
   curl_easy_getinfo(Curl, CURLINFO_FILETIME, &FileTime);
   if (FileTime != -1)
      Res.LastModified = static_cast<time_t>(FileTime);

   Res.IMSHit = Context.IMSHit;
   if (Res.IMSHit == true && Res.LastModified == 0)
      Res.LastModified = Itm->LastModified;

   Res.Size = File.Size();
   Res.TakeHashes(*Context.Hash);

   curl_easy_cleanup(Curl);
   delete Context.Hash;

   URIDone(Res);
   return true;
}

int main()
{
   curl_global_init(CURL_GLOBAL_DEFAULT);
   HttpsMethod Mth;
   int Result = Mth.Run();
   curl_global_cleanup();
   return Result;
}
