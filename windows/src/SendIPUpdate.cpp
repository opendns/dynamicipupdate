#include "stdafx.h"

#include "SendIpUpdate.h"

#include "MiscUtil.h"
#include "Http.h"
#include "JsonParser.h"
#include "Prefs.h"
#include "StrUtil.h"

char* SendIpUpdate()
{
	assert(CanSendIPUpdates());
	if (!CanSendIPUpdates())
		return NULL;

	char *res = NULL;

	// host = updates.opendns.com or website.dev6.sfo.opendns.com
	// url: /nic/update?token=mytoken&api_key=mykey?hostname=
	// responses:
	//  "badauth"
	//  "nohost"
	//  "good ${ip-addr}"
	if (!g_pref_token)
		return NULL;
	assert(g_pref_hostname);

	const char *urlTxt = GetIpUpdateUrl(TRUE);
	const char *host = GetIpUpdateHost();

	HttpResult *httpResult = HttpGet(host, urlTxt, INTERNET_DEFAULT_HTTPS_PORT);
	free((void*)urlTxt);
	if (httpResult && httpResult->IsValid()) {
		res = (char*)httpResult->data.getData(NULL);
	}
	delete httpResult;
	return res;
}

char *SendDnsOmaticUpdate()
{
	assert(CanSendIPUpdates());
	if (!CanSendIPUpdates())
		return NULL;

	char *res = NULL;

	// TODO: do I need to append the following in url from GetIpUpdateUrl()?
	/*URL$ = SetURLPart(URL$, "myip", ip$)
	URL$ = SetURLPart(URL$, "wildcard", "NOCHG")
	URL$ = SetURLPart(URL$, "mx", "NOCHG")
	URL$ = SetURLPart(URL$, "backmx", "NOCHG")*/

	const char *urlTxt = GetIpUpdateUrl(TRUE);
	const char *host = GetIpUpdateDnsOMaticHost();

	HttpResult *httpResult = HttpGet(host, urlTxt, INTERNET_DEFAULT_HTTPS_PORT);
	free((void*)urlTxt);
	if (httpResult && httpResult->IsValid()) {
		res = (char*)httpResult->data.getData(NULL);
	}
	delete httpResult;
	return res;
}

IpUpdateResult IpUpdateResultFromString(const char *s)
{
	if (StrStartsWithI(s, "The service is not available"))
		return IpUpdateNotAvailable;
	if (StrStartsWithI(s, "good"))
		return IpUpdateOk;
	if (StrStartsWithI(s, "nochg"))
		return IpUpdateOk;
	if (StrStartsWithI(s, "!yours"))
		return IpUpdateNotYours;
	if (StrStartsWithI(s, "badauth"))
		return IpUpdateBadAuth;
	if (StrStartsWithI(s, "nohost"))
		return IpUpdateNoHost;

	// not sure if those really happen, they're supported by 1.3 client
	if (StrStartsWithI(s, "dnserr"))
		return IpUpdateDnsErr;

	if (StrStartsWithI(s, "911"))
		return IpUpdateDnsErr;

	if (StrStartsWithI(s, "abuse"))
		return IpUpdateMiscErr;

	if (StrStartsWithI(s, "notfqdn"))
		return IpUpdateMiscErr;

	if (StrStartsWithI(s, "numhost"))
		return IpUpdateMiscErr;

	if (StrStartsWithI(s, "badagent"))
		return IpUpdateMiscErr;

	if (StrStartsWithI(s, "!donator"))
		return IpUpdateMiscErr;

	assert(0);
	return IpUpdateOk;
}

#if 0
// another way is to use username/password and http basic auth
char* SendIpUpdate()
{
	// host = "updates.opendns.com"
	// url = "/nic/update?hostname=" + network
	// responses:
	//  "badauth"
	//  "nohost"
	//  "good ${ip-addr}"
	const char *userName = "";
	const char *pwd = "";
	HttpResult *httpResult = HttpGetWithBasicAuth("updates.opendns.com", "/nic/update", userName, pwd, true);
	if (httpResult && httpResult->IsValid()) {
		DWORD dataLen;
		char *data = (char*)httpResult->data.getData(&dataLen);
		free(data);
	}
	delete httpResult;
}
#endif

TCHAR *DownloadUpdateIfNotDownloaded(const char *url)
{
	const TCHAR *filePathStr = NULL;
	const char *fileName = StrFindLastChar(url, '/');
	if (!fileName)
		return NULL;
	++fileName;
	CString filePath = AppDataDir();
	filePath += PATH_SEP_STR;
	filePath += fileName;

	filePathStr = filePath;
	if (FileOrDirExists(filePathStr))
		return tstrdup(filePathStr);

	HttpResult *httpResult = HttpGet(url);
	if (!httpResult || !httpResult->IsValid())
		goto Error;

	DWORD size;
	void *s = httpResult->data.getData(&size);
	if (!s)
		goto Error;

	BOOL ok = FileWriteAll(filePathStr, (const char*)s, size);
	if (!ok)
		goto Error;

	return tstrdup(filePathStr);
Error:
	free((void*)filePathStr);
	return NULL;
}

// sends auto-update check. Returns url of the new version to download
// if an update is available or NULL if update is not available
// (or there was an error getting the upgrade info)
// Caller needs to free() the result.
// TODO: we ignore (and don't propagate) 'force' field in json response
char *GetUpdateUrl(const TCHAR *version, VersionUpdateCheckType type)
{
	char *downloadUrl = NULL;
	JsonEl *json = NULL;

	char *typeStr = "c";
	if (UpdateCheckInstall == type)
		typeStr = "i";
	else if (UpdateCheckUninstall == type)
		typeStr = "u";
	else if (UpdateCheckVersionCheck == type)
		typeStr = "c";
	else
		assert(0);

	CString url = AutoUpdateUrl(version, typeStr);
	HttpResult *res = HttpGet(AUTO_UPDATE_HOST, url);
	if (!res || !res->IsValid())
		return NULL;
	char *s = (char *)res->data.getData(NULL);
	json = ParseJsonToDoc(s);
	JsonEl *upgradeAvailable = GetMapElByName(json, "upgrade");
	JsonElBool *upgradeAvailableBool = JsonElAsBool(upgradeAvailable);
	if (!upgradeAvailableBool || !upgradeAvailableBool->boolVal)
		goto Exit;
	JsonEl *updateUrl = GetMapElByName(json, "download");
	if (!updateUrl)
		goto Exit;
	downloadUrl = StrDupSafe(JsonElAsStringVal(updateUrl));
Exit:
	JsonElFree(json);
	free(s);
	return downloadUrl;
}
