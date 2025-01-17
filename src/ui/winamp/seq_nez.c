/*
	NEZ sequencer
	seq_nez.cpp
*/

#include "in_nez.h"

/* ANSI/Windows standard headers */
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>

#include "tag/tag.h"

#include "nezvm/nezvm.h"
//#include "common/nsfsdk/nsfsdk.h"

/* Project headers */
#include "con_nez.h"
#include "sequencer.h"

#include "ui/nezplug/Dialog.h"

typedef signed short Int16;
unsigned NEZ_extract(char* lpszSrcFile, void** ppbuf);

/* NEZ sequencer struct */
typedef struct {
	SEQUENCER seq;			/* KUMAamp common interface */
	int frequency;			/* mixing frequency */
	int channel;
	int loopc;				/* zero based loopcounter */
	int loopcount;			/* max loop count */
	int fade;				/* 0:normal 1-:fading out(dB/(1 >> FADE_SHIFT)) */
	int fadetime;			/* fadeout time(ms) */
	int fadespd;

	int playedtime;
	int playedtime2;
	int playtime;
	int looptime;

	int volume;

	int isplaying;			/* 1:playing 0:stoped */

	int bufsize;
	Int16* buf;
	int bufp;

	NEZ_PLAY* hnsf;

	enum {
		CONTROLER_OFF,
		CONTROLER_ON,
		CONTROLER_HOOK
	} controler_mode;

} NEZSEQ;

/* NEZplug setting struct */
static struct {
	int frequency;			/* mixing frequency */
	int priority;			/* play thread priority */
	int loopcount;			/* max loop count */
	int fadetime;			/* fadeout time(ms) */
	int playtime;			/* default play time(ms) */

	HINSTANCE hDllInstance;
	char cfgname[MAX_PATH];
	char* lpSection;

	int disable_nsfext;
	int disable_pceext;

	int disable_nsf;
	int disable_kss;
	int disable_gbr;
	int disable_gbs;
	int disable_ay;
	int disable_sgc;

	int disable_hes;
	int realplaytime;
	int filtertype;

	int hookwinamp;
} setting;

typedef struct {
	char* key;
	int num;
} KEYINT;

#if FREQUENCY_LIMIT
static KEYINT freqlist[] = {
	{ "192000",		192000 },
	{ "176400",		176400 },
	{ "96000",		96000 },
	{ "88200",		88200 },
	{ "48000",		48000 },
	{ "44100",		44100 },
	{ "24000",		24000 },
	{ "22050",		22050 },
	{ "11025",		11025 },
	{ NULL,			44100 },
};
#endif

static KEYINT priolist[] = {
	{ "Highest",	5 },
	{ "Higher",		4 },
	{ "Normal",		3 },
	{ "Lower",		2 },
	{ "Lowest",		1 },
	{ NULL,			3 },
};

extern struct {
	char* title;
	char* artist;
	char* copyright;
	char detail[1024];
}songinfodata;

extern int (*memview_memread)(int a);


#define PTR__ ((NEZSEQ *)(this__->work))
static void Play(SEQUENCER* this__)
{

	PTR__->loopc = 0;
	if (PTR__->looptime)
		PTR__->playedtime = (PTR__->looptime - PTR__->playtime);
	else
		PTR__->playedtime = 0;
	PTR__->playedtime2 = 0;
	PTR__->fade = 0;
	PTR__->isplaying = 1;
	PTR__->volume = 256;

	NEZReset(PTR__->hnsf);
	NEZVolume(PTR__->hnsf, 0);

	PTR__->bufp = 0;
	NEZRender(PTR__->hnsf, PTR__->buf, PTR__->bufsize);
}

static void Stop(SEQUENCER* this__)
{
	PTR__->loopc = 0;
	PTR__->isplaying = 0;
	PTR__->playedtime2 = 0;
}

static void Term(SEQUENCER* this__)
{
	SetStateControler(0);
	NEZDelete(PTR__->hnsf);
	free(PTR__->buf);
	free(this__);
}

static unsigned DivFix(unsigned p1, unsigned p2, unsigned fix)
{
	unsigned ret;
	ret = p1 / p2;
	p1 = p1 % p2;/* p1 = p1 - p2 * ret; */
	while (fix--)
	{
		p1 += p1;
		ret += ret;
		if (p1 >= p2)
		{
			p1 -= p2;
			ret++;
		}
	}
	return ret;
}

#define MAX_SAMPLERATE 192000
#define FADE_SHIFT 12
static void Mix(SEQUENCER* this__, void* buf, int numsamp)
{
	Int16* d;
	int s;
	d = (Int16*)buf;
	PTR__->playedtime += numsamp;
	PTR__->playedtime2 += numsamp;
	if (PTR__->channel == 2)
	{
		while (PTR__->bufsize - PTR__->bufp <= numsamp)
		{
			while (PTR__->bufsize > PTR__->bufp)
			{
				if (PTR__->fade != 0) {
					PTR__->fade += PTR__->fadespd;
					PTR__->volume = 256 - (PTR__->fade >> FADE_SHIFT);
					if (PTR__->volume < 0) PTR__->volume = 0;
				}
				s = PTR__->buf[0 + 2 * PTR__->bufp];
				*(d++) = (s * PTR__->volume) >> 8;
				s = PTR__->buf[1 + 2 * PTR__->bufp++];
				*(d++) = (s * PTR__->volume) >> 8;
				numsamp--;
			}
			PTR__->bufp = 0;
			NEZRender(PTR__->hnsf, PTR__->buf, PTR__->bufsize);
		}
		while (numsamp)
		{
			if (PTR__->fade != 0) {
				PTR__->fade += PTR__->fadespd;
				PTR__->volume = 256 - (PTR__->fade >> FADE_SHIFT);
				if (PTR__->volume < 0) PTR__->volume = 0;
			}
			s = PTR__->buf[0 + 2 * PTR__->bufp];
			*(d++) = (s * PTR__->volume) >> 8;
			s = PTR__->buf[1 + 2 * PTR__->bufp++];
			*(d++) = (s * PTR__->volume) >> 8;
			numsamp--;
		}
	}
	else
	{
		while (PTR__->bufsize - PTR__->bufp <= numsamp)
		{
			while (PTR__->bufsize > PTR__->bufp)
			{
				if (PTR__->fade != 0) {
					PTR__->fade += PTR__->fadespd;
					PTR__->volume = 256 - (PTR__->fade >> FADE_SHIFT);
					if (PTR__->volume < 0) PTR__->volume = 0;
				}
				s = PTR__->buf[PTR__->bufp++];
				*(d++) = (s * PTR__->volume) >> 8;
				numsamp--;
			}
			PTR__->bufp = 0;
			NEZRender(PTR__->hnsf, PTR__->buf, PTR__->bufsize);
		}
		while (numsamp)
		{
			if (PTR__->fade != 0) {
				PTR__->fade += PTR__->fadespd;
				PTR__->volume = 256 - (PTR__->fade >> FADE_SHIFT);
				if (PTR__->volume < 0) PTR__->volume = 0;
			}
			s = PTR__->buf[PTR__->bufp++];
			*(d++) = (s * PTR__->volume) >> 8;
			numsamp--;
		}
	}

	if ((256 << FADE_SHIFT) < PTR__->fade)
	{
		Stop(this__);
		return;
	}
	if (PTR__->looptime)
	{
		if (PTR__->playedtime > PTR__->looptime)
		{
			PTR__->loopc++;
			PTR__->playedtime -= PTR__->looptime;
		}
	}
	else {
#if 0
		if (PTR__->playtime && PTR__->playedtime > PTR__->playtime)
		{
			Stop(this__);
			return;
		}
#else
		if (PTR__->playtime == 0 || PTR__->fade != 0) return;
		if (PTR__->playtime && PTR__->playedtime > PTR__->playtime)
		{
			if (PTR__->fadetime != 0)
			{
				PTR__->fade = 1;
				PTR__->fadespd = DivFix(256 * 1000, PTR__->fadetime * PTR__->frequency, FADE_SHIFT);
			}
			else
				Stop(this__);
			return;
		}
#endif
	}

	if (PTR__->loopcount == 0 || PTR__->fade != 0) return;
	if (PTR__->loopc < PTR__->loopcount) return;
	if (PTR__->fadetime != 0)
	{
		PTR__->fade = 1;
		PTR__->fadespd = DivFix(256 * 1000, PTR__->fadetime * PTR__->frequency, FADE_SHIFT);
	}
	else
		Stop(this__);

}

static void SetPosition(SEQUENCER* this__, int time_in_ms)
{
	int numsamp;
	int time_in_samp = MulDiv(PTR__->frequency, time_in_ms, 1000);
	if (PTR__->playedtime2 > time_in_samp)
	{
		Play(this__);
	}
	numsamp = time_in_samp - PTR__->playedtime2;
	PTR__->playedtime += numsamp;
	PTR__->playedtime2 += numsamp;
	NEZRender(PTR__->hnsf, NULL, numsamp);
	PTR__->bufp = 0;
	NEZRender(PTR__->hnsf, PTR__->buf, PTR__->bufsize);
}
static int GetPosition(SEQUENCER* this__)
{
	return MulDiv(PTR__->playedtime2, 1000, PTR__->frequency);
}
static int GetRate(SEQUENCER* this__)
{
	return PTR__->frequency;
}

static int GetChannel(SEQUENCER* this__)
{
	return PTR__->channel;
}

static int GetPriority(SEQUENCER* this__)
{
	return setting.priority;
}

static int GetLoopCount(SEQUENCER* this__)
{
	return PTR__->loopc;
}

static int IsPlaying(SEQUENCER* this__)
{
	return PTR__->isplaying;
}


#if SINGLE_BUILD
#define InitNezSequencer InitSequencer
#define QuitNezSequencer QuitSequencer
#define AboutNezSequencer AboutSequencer
#define ConfigNezSequencer ConfigSequencer
#define getNezFileInfo getFileInfo
#define loadNezFile loadFile
#define infoNezBox infoBox
#define isOurNezFile isOurFile
#endif

static void WritePrivateProfileInt(LPCSTR sect, LPCSTR key, UINT data, LPCSTR file)
{
	char temp[32], * p;
	temp[30] = '0';
	temp[31] = '\0';
	if (data == 0)
		p = &temp[30];
	else
		for (p = &temp[31]; data != 0; data /= 10)
		{
			*(--p) = '0' + (data % 10);
		}
	WritePrivateProfileString(sect, key, p, file);
}

static void GetSettingString(LPCSTR lpKey, LPCSTR lpDefault, LPSTR lpBuf, DWORD nSize)
{
	GetPrivateProfileString(setting.lpSection, lpKey, lpDefault, lpBuf, nSize, setting.cfgname);
}
static void SetSettingString(LPCSTR lpKey, LPCSTR lpStr)
{
	WritePrivateProfileString(setting.lpSection, lpKey, lpStr, setting.cfgname);
}
static int GetSettingInt(LPCSTR lpKey, int iDefault)
{
	char buf[128], dbuf[128];
	wsprintf(dbuf, "%d", iDefault);
	GetPrivateProfileString(setting.lpSection, lpKey, dbuf, buf, sizeof(buf), setting.cfgname);
	return atoi(buf);
}
static void SetSettingInt(LPCSTR lpKey, int iInt)
{
	char buf[128];
	wsprintf(buf, "%d", iInt);
	WritePrivateProfileString(setting.lpSection, lpKey, buf, setting.cfgname);
}

void InitNezSequencer(HINSTANCE hDllInstance)
{
	int i;
	char path[MAX_PATH];

	setting.hDllInstance = hDllInstance;

	{
		extern int NESAPUVolume;
		NESAPUVolume = GetSettingInt("NESAPUVolume", 64);
		if (NESAPUVolume < 0) NESAPUVolume = 0;
		if (NESAPUVolume > 255) NESAPUVolume = 255;
		SetSettingInt("NESAPUVolume", NESAPUVolume);
	}
	{
		extern int NESRealDAC;
		NESRealDAC = GetSettingInt("NESRealDAC", 1);
		SetSettingInt("NESRealDAC", NESRealDAC);
	}
	{
		extern int NSF_noise_random_reset;
		NSF_noise_random_reset = GetSettingInt("NSFNoiseRandomReset", 0);
		SetSettingInt("NSFNoiseRandomReset", NSF_noise_random_reset);
	}
	{
		extern int NSF_2A03Type;
		NSF_2A03Type = GetSettingInt("2A03Type", 1);
		SetSettingInt("2A03Type", NSF_2A03Type);
	}
	{
		extern int Namco106_Realmode;
		Namco106_Realmode = GetSettingInt("Namco106RealMode", 1);
		SetSettingInt("Namco106RealMode", Namco106_Realmode);
	}
	{
		extern int Namco106_Volume;
		Namco106_Volume = GetSettingInt("Namco106Volume", 16);
		SetSettingInt("Namco106Volume", Namco106_Volume);
	}
	{
		extern int FDS_RealMode;
		FDS_RealMode = GetSettingInt("FDSRealMode", 3);
		SetSettingInt("FDSRealMode", FDS_RealMode);
	}
	{
		extern int GBAMode;
		GBAMode = GetSettingInt("GBAMode", 0);
		SetSettingInt("GBAMode", GBAMode);
	}
	{
		extern int MSXPSGType;
		MSXPSGType = GetSettingInt("MSXPSGType", 1);
		SetSettingInt("MSXPSGType", MSXPSGType);
	}
	{
		extern int MSXPSGVolume;
		MSXPSGVolume = GetSettingInt("MSXPSGVolume", 64);
		if (MSXPSGVolume < 0) MSXPSGVolume = 0;
		if (MSXPSGVolume > 255) MSXPSGVolume = 255;
		SetSettingInt("MSXPSGVolume", MSXPSGVolume);
	}
	{
		extern int LowPassFilterLevel;
		LowPassFilterLevel = GetSettingInt("LowPassFilterLevel", 16);
		if (LowPassFilterLevel < 0) LowPassFilterLevel = 0;
		if (LowPassFilterLevel > 32) LowPassFilterLevel = 32;
		SetSettingInt("LowPassFilterLevel", LowPassFilterLevel);
	}
	{
		extern int Always_stereo;
		Always_stereo = GetSettingInt("AlwaysStereo", 0);
		SetSettingInt("AlwaysStereo", Always_stereo);
	}
	{
		extern char ColecoBIOSFilePath[0x200];
		GetSettingString("ColecoBIOSFilePath", "", ColecoBIOSFilePath, sizeof(ColecoBIOSFilePath));
		SetSettingString("ColecoBIOSFilePath", ColecoBIOSFilePath);
	}
#if FREQUENCY_LIMIT
	GetSettingString("Frequency", freqlist[1].key, path, sizeof(path));
	for (i = 0; freqlist[i].key != NULL; i++)
	{
		if (lstrcmpi(path, freqlist[i].key) == 0) break;
	}
	setting.frequency = freqlist[i].num;
	for (i = 0; freqlist[i].key != NULL; i++)
	{
		if (setting.frequency == freqlist[i].num) break;
	}
	if (freqlist[i].key != NULL) SetSettingString("Frequency", freqlist[i].key);
#else
	setting.frequency = GetSettingInt("Frequency", 44100);
	if (setting.frequency < 8000) setting.frequency = 44100;
	if (setting.frequency > 192000) setting.frequency = 44100;
	SetSettingInt("Frequency", setting.frequency);
#endif

	GetSettingString("Priority", priolist[2].key, path, sizeof(path));
	for (i = 0; priolist[i].key != NULL; i++)
	{
		if (lstrcmpi(path, priolist[i].key) == 0) break;
	}
	setting.priority = priolist[i].num;

	for (i = 0; priolist[i].key != NULL; i++)
	{
		if (setting.priority == priolist[i].num) break;
	}
	if (priolist[i].key != NULL) SetSettingString("Priority", priolist[i].key);

	setting.loopcount = GetSettingInt("LoopCount", 2);
	SetSettingInt("LoopCount", setting.loopcount);
	setting.fadetime = GetSettingInt("FadeoutTime", 5000);
	SetSettingInt("FadeoutTime", setting.fadetime);
	setting.playtime = GetSettingInt("DefaultPlayTime", 300000);
	SetSettingInt("DefaultPlayTime", setting.playtime);
	setting.realplaytime = GetSettingInt("DisplayRealPlayTime", 1);
	SetSettingInt("DisplayRealPlayTime", setting.realplaytime);
	setting.filtertype = GetSettingInt("FilterType", 0);
	SetSettingInt("FilterType", setting.filtertype);

	setting.hookwinamp = GetSettingInt("HookWinamp", 1);
	SetSettingInt("HookWinamp", setting.hookwinamp);
	InstallControler(!setting.hookwinamp);
}

void QuitNezSequencer(HINSTANCE hDllInstance)
{
	UninstallControler();
}

void AboutNezSequencer(HWND hwndParent)
{
	MessageBox(hwndParent,
		"zlib 1.2.3\n"
		"\t(C) 1995-2005 Jean-loup Gailly and Mark Adler\n\n"
		"\t\thttp://www.zlib.org/\n"
		"NEZplug\n"
		"\tby Mamiya\n"
		"\t\tmailto:mamiya@usres.sourceforge.net\n"
		"\t\thttp://nezplug.sourceforge.net/\n"
		"\n"
		"(revision by RuRuRu and OffGao)\n"
		, PLUGIN_NAME, MB_OK);
}

void ConfigNezSequencer(HWND hwndParent)
{
	ShellExecute(hwndParent, "open", setting.cfgname, NULL, NULL, SW_SHOW);
}

int infoNezBox(char* fn, HWND hwndParent)
{
	//ファイル情報呼び出し
	//instance = setting.hDllInstance;
	//ファイルをロードしてすぐ廃棄 = ファイル情報の格納
	if (memview_memread == NULL)Term(loadNezFile(fn));
	NEZFileInfoDlg(setting.hDllInstance, hwndParent);
	return 0;
}

static int isOurList2(char* fn, char* us, char* ls)
{
	char* p;
	if (fn == NULL) return 0;
	for (p = fn; *p != '\0'; p++)
	{
		if (p[0] != p[1]) continue;
		if (p[0] != ':'/* && p[0] != '\\' && p[0] != '/'*/) continue;
		if (p[2] != us[0] && p[2] != ls[0]) continue;
		if (p[3] != us[1] && p[3] != ls[1]) continue;
		return p + 4 - fn;
	}
	return 0;
}
static int isOurList(char* fn, char* us, char* ls)
{
	char* p;
	if (fn == NULL) return 0;
	for (p = fn; *p != '\0'; p++)
	{
		if (p[0] != p[1]) continue;
		if (p[0] != ':'/* && p[0] != '\\' && p[0] != '/'*/) continue;
		if (p[2] != us[0] && p[2] != ls[0]) continue;
		if (p[3] != us[1] && p[3] != ls[1]) continue;
		if (p[4] != us[2] && p[4] != ls[2]) continue;
		return p + 5 - fn;
	}
	return 0;
}

static int isOurLists(char* fn)
{
	int p;
	/* ::NEZプレイリストは常に制御を得る */
	if (!!(p = isOurList(fn, "NEZ", "nez"))) return p;
	if (!setting.disable_nsf && !!(p = isOurList(fn, "NSF", "nsf"))) return p;
	if (!setting.disable_kss && !!(p = isOurList(fn, "KSS", "kss"))) return p;
	if (!setting.disable_gbr && !!(p = isOurList(fn, "GBR", "gbr"))) return p;
	if (!setting.disable_gbs && !!(p = isOurList(fn, "GBS", "gbs"))) return p;
	if (!setting.disable_hes && !!(p = isOurList(fn, "HES", "hes"))) return p;
	if (!setting.disable_hes && !!(p = isOurList(fn, "PCE", "pce"))) return p;
	if (!setting.disable_ay && !!(p = isOurList2(fn, "AY", "ay"))) return p;
	return 0;
}


int isOurNezFile(char* fn)
{
	return isOurLists(fn);
}

static char* skipspace(char* p)
{
	for (; *p == ' ' || *p == '\t'; p++)
		if (*p == '\0') break;
	return p;
}

static char* getfnamebase(char* p)
{
	char* rp;
	for (rp = p; *p != '\0'; p++)
	{
		if (IsDBCSLeadByte(*p))
		{
			p++;
			continue;
		}
		switch (*p)
		{
		case ':': case '\\': case '/':
			rp = p + 1;
			break;
		}
	}
	return rp;
}

static char* gettext(char* p, char* pt)
{
	int loop_flag = 1;
	p = skipspace(p);
	while (loop_flag) {
		if (IsDBCSLeadByte(*p))
		{
			if (pt)
			{
				*(pt++) = p[0];
				*(pt++) = p[1];
			}
			p += 2;
			continue;
		}
		switch (*p)
		{
		case '\t':
			if (pt) *(pt++) = ' ';
			p++;
			break;
		case '\\':
			if (*(++p) != '\0') {
				if (pt) *(pt++) = *p;
				p++;
			}
			break;
		case '\0':
		case ',':
			loop_flag = 0;
			break;
		default:
			if (pt) *(pt++) = *p;
			p++;
			break;
		}
	}
	if (pt) *pt = '\0';
	return p;
}
static char* getnum_sub(char* p, int* pi, int type)
{
	int loop_flag = 1, n = 0, radix = 10;
	p = skipspace(p);
	if (*p == '$')
	{
		radix = 16;
		p++;
	}
	else if (*p == '%')
	{
		radix = 2;
		p++;
	}
	while (loop_flag)
	{
		switch (*p)
		{
		case 'A':	case 'B':	case 'C':
		case 'D':	case 'E':	case 'F':
			if (radix < 16)
			{
				loop_flag = 0;
				break;
			}
			n = n * radix + (*(p++) - 'A' + 10);
			break;
		case 'a':	case 'b':	case 'c':
		case 'd':	case 'e':	case 'f':
			if (radix < 16)
			{
				loop_flag = 0;
				break;
			}
			n = n * radix + (*(p++) - 'a' + 10);
			break;
		case '2':	case '3':	case '4':	case '5':
		case '6':	case '7':	case '8':	case '9':
			if (radix < 10)
			{
				loop_flag = 0;
				break;
			}
		case '0':	case '1':
			n = n * radix + (*(p++) - '0');
			break;
		default:
			loop_flag = 0;
			break;
		}
	}
	if (pi)
	{
		switch (type)
		{
		default:
			*pi = n;
			break;
		case 1:	/* nezamp list */
			*pi = n + (radix != 10);
			break;
		case 2:	/* in_kss list */
			*pi = n + 1;
			break;
		}
	}
	return p;
}
static char* getnum(char* p, int* pi)
{
	return getnum_sub(p, pi, 0);
}

typedef struct {
	char fn[MAX_PATH];
	int songno;
	char title[MAX_PATH];
	int playtime;
	int looptime;
	int fadetime;
	int loopcount;
} NEZINFO;

static void ExtractNezInfo(NEZINFO* pni, char* fn)
{
	char* p = fn;
	int isnezlist, isaylist;
	ZeroMemory(pni, sizeof(NEZINFO));
	pni->fadetime = setting.fadetime;
	pni->loopcount = setting.loopcount;
}

static char currentfn[MAX_PATH];

static int isKSS(char* buf)
{
	if (!memcmp(buf, "KSCC", 4)) return 0x10;
	if (!memcmp(buf, "KSSX", 4)) return 0x10 + ((unsigned char)buf[0xe]);
	return 0;
}

static void gettitle(char* fn, char* tmpbuf, int tmpbuflen, NEZINFO* nezinfo)
{
	FILE* fp = 0;
	char fbuf[512];
	tmpbuf[0] = '\0';
	do
	{
		fp = fopen(fn, "rb");
		if (!fp) break;
		if (fread(fbuf, 1, 0x40, fp) != 0x40) break;
		if (!memcmp(fbuf, "NESM", 4))
		{
			int numsongs;
			if (nezinfo->songno < 1) nezinfo->songno = GetControler();
			numsongs = ((int)fbuf[6]) & 255;
			if (nezinfo->songno < 1 || nezinfo->songno > numsongs) nezinfo->songno = fbuf[7];
			fseek(fp, 0x000e, SEEK_SET);
			fread(tmpbuf, 1, 32, fp);
			tmpbuf[32] = '\0';
			if (numsongs > 1) wsprintf(tmpbuf + lstrlen(tmpbuf), " (%d/%d)", nezinfo->songno, numsongs);
		}
		else if (!memcmp(fbuf, "GBS", 3))
		{
			int numsongs;
			if (nezinfo->songno < 1) nezinfo->songno = GetControler();
			numsongs = ((int)fbuf[4]) & 255;
			if (nezinfo->songno < 1 || nezinfo->songno > numsongs) nezinfo->songno = fbuf[5];
			fseek(fp, 0x0010, SEEK_SET);
			fread(tmpbuf, 1, 32, fp);
			tmpbuf[32] = '\0';
			if (numsongs > 1) wsprintf(tmpbuf + lstrlen(tmpbuf), " (%d/%d)", nezinfo->songno, numsongs);
		}
		else if (!memcmp(fbuf, "SGC", 3))
		{
			int numsongs, firstse, lastse;
			numsongs = ((int)fbuf[0x25]) & 255;
			firstse = fbuf[0x26];
			lastse = fbuf[0x27];
			if (firstse < numsongs
				|| firstse > lastse
				|| firstse == 0) {
				firstse = lastse = 0;
			}
			numsongs = numsongs + lastse - firstse + (firstse ? 1 : 0);
			if (nezinfo->songno < 1) nezinfo->songno = GetControler();
			if (nezinfo->songno < 1 || nezinfo->songno > numsongs) nezinfo->songno = fbuf[0x24];
			fseek(fp, 0x0040, SEEK_SET);
			fread(tmpbuf, 1, 32, fp);
			tmpbuf[32] = '\0';
			if (numsongs > 1) wsprintf(tmpbuf + lstrlen(tmpbuf), " (%d/%d)", nezinfo->songno, numsongs);
		}
		else if (isKSS(fbuf))
		{
			int numsongs, top = isKSS(fbuf);
			if (top >= 0x1c)
			{
				numsongs = (((unsigned char)fbuf[0x1b]) << 8) + ((unsigned char)fbuf[0x1a]) + 1;
			}
			else
			{
				numsongs = 256;
			}
			fseek(fp, top, SEEK_SET);
			if (fread(fbuf, 1, 0x10, fp) != 0x10) break;
			if (nezinfo->songno < 1) nezinfo->songno = GetControler();
			if (nezinfo->songno < 1 || nezinfo->songno > numsongs) nezinfo->songno = fbuf[5];
			if (!memcmp(fbuf, "MGS", 3) || !memcmp(fbuf, "MPK", 3))
			{
				int c;
				char* p = tmpbuf;
				fseek(fp, top, SEEK_SET);
				do {
					c = fgetc(fp);
				} while (c != EOF && c != 0x1a && c != 0x0a);
				do
				{
					c = fgetc(fp);
					if (c >= 0x20)
						*p++ = c;
				} while (c != EOF && c != 0x1a && c != 0x0a && p < tmpbuf + tmpbuflen - 1);
				*p = '\0';
			}
			else if (!memcmp(fbuf, "MBM", 3))
			{
				int c;
				char* p = tmpbuf;
				fseek(fp, top + 0x0010, SEEK_SET);
				do
				{
					c = fgetc(fp);
					if (c >= 0x20) *p++ = c;
				} while (c != EOF && p < tmpbuf + 0x28);
				*p = '\0';
			}
			else if (!memcmp(fbuf, "BTO KINROU 5th", 14))
			{
				int c, base, title;
				char* p = tmpbuf;
				fseek(fp, top + 0x1150, SEEK_SET);
				if (fread(fbuf, 1, 4, fp) != 4) break;
				/* MPK2KSS check */
				if (fbuf[0] != '\x80' || fbuf[1] != '\x11') break;
				base = (((unsigned char)fbuf[3]) << 8) + ((unsigned char)fbuf[2]);
				fseek(fp, top + 0x2049, SEEK_SET);
				if (fread(fbuf, 1, 3, fp) != 3) break;
				if (memcmp(fbuf, "BTO", 3)) break;
				fseek(fp, top + 0x2054, SEEK_SET);
				if (fread(fbuf, 1, 2, fp) != 2) break;
				title = (((unsigned char)fbuf[1]) << 8) + ((unsigned char)fbuf[0]);
				title = (title - base) & 0xffff;
				fseek(fp, top + 0x2000 + title, SEEK_SET);
				do
				{
					c = fgetc(fp);
					if (c >= 0x20)
						*p++ = c;
				} while (c != EOF && c != 0x00 && p < tmpbuf + tmpbuflen - 1);
				*p = '\0';
			}
			else
				break;
			if (tmpbuf[0] && numsongs > 1)
			{
				wsprintf(tmpbuf + lstrlen(tmpbuf), " (%02X/%02X)", nezinfo->songno - 1, numsongs - 1);
			}
		}
	} while (0);
	if (fp) fclose(fp);
}

int getNezFileInfo(char* fn, char* title, int* length_in_ms)
{
	NEZINFO nezinfo;
	int tmptime = 0;
	char tmpbuf[MAX_PATH];
	tmpbuf[0] = '\0';
	nezinfo.songno = 0;
	struct TAG_METADATA* tm = (struct global*)malloc(sizeof(struct TAG_METADATA));

	bool playlistNameFromM3uTag = false;

	if (fn == NULL || fn[0] == '\0')
	{
		fn = currentfn;
	}

	if (fn != NULL)
	{
		tm = loadTag2(fn);

		if (tm != NULL && tm->isValid)
		{
			*length_in_ms = tm->length;

			if (strlen(tm->title) > 0) 
			{
				playlistNameFromM3uTag = true;
				lstrcpy(tmpbuf, tm->title);
			}
		}
		else
		{
			*length_in_ms = setting.playtime + setting.fadetime;
		}
	}

	if (!playlistNameFromM3uTag && fn != NULL && tmpbuf[0] == '\0')
	{
		gettitle(fn, tmpbuf, sizeof(tmpbuf), &nezinfo);

		// Print song title for winamp playlist
		nezinfo.songno = GetControler();
		lstrcpy(tmpbuf, getfnamebase(fn));
		wsprintf(tmpbuf + lstrlen(tmpbuf), " ($%02X)", nezinfo.songno - 1);
	}

	if (title != NULL)
	{
		lstrcpy(title, tmpbuf);
	}

	return 1;
}

static BOOL IsExistFile(char* path)
{
	HANDLE hFile;
	hFile = CreateFile(
		path,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (hFile == INVALID_HANDLE_VALUE) return FALSE;
	CloseHandle(hFile);
	return TRUE;
}

static void GetPathNoExt(char* des, char* src)
{
	char* p = NULL;
	while (*src != '\0')
	{
		if (IsDBCSLeadByte(*src))
		{
			*(des++) = *(src++);
			*(des++) = *(src++);
			continue;
		}
		switch (*src)
		{
		case ':': case '\\': case '/':
			p = NULL;
			break;
		case '.':
			p = des;
			break;
		}
		*(des++) = *(src++);
	}
	if (p != NULL)
		*p = '\0';
	else
		*des = '\0';
}

static char* NezSearchPath(char* fn, char* searchPath)
{
	static char* extlist[] =
	{
		".NEZ", ".NSF", ".KSS", ".GBR", ".GBS", ".HES", ".PCE", ".AY", ".CPC", ".NSZ", ".NSD", ".ZIP", ".SGC", 0
	};
	int i;
	if (IsExistFile(fn)) return fn;
	for (i = 0; extlist[i]; i++)
	{
		GetPathNoExt(searchPath, fn);
		lstrcat(searchPath, extlist[i]);
		if (IsExistFile(searchPath)) return searchPath;
	}
	for (i = 0; extlist[i]; i++)
	{
		lstrcpy(searchPath, fn);
		lstrcat(searchPath, extlist[i]);
		if (IsExistFile(searchPath)) return searchPath;
	}
	return fn;	/* ??? */
}

LPTSTR GetDLLArgv0(void);

static char* substr(char* arr, int begin, int len)
{
	char* res = malloc(len + 1);

	for (int i = 0; i < len; i++) {
		res[i] = *(arr + begin + i);
	}

	res[len] = 0;
	return res;
}


int lastIndexOfSlash(char* arr)
{
	int lastIndex = -1;
	for (int i = 0; i < strlen(arr); i++)
	{
		if (strncmp("\\", &arr[i], 1) == 0)
		{
			lastIndex = i;
		}
	}

	return lastIndex;
}

char* concat(const char* s1, const char* s2)
{
	char* result = malloc(strlen(s1) + strlen(s2) + 1); // +1 for the null-terminator
	// in real code you would check for errors in malloc here
	strcpy(result, s1);
	strcat(result, s2);
	return result;
}

struct TAG_METADATA* loadTag2(char* fn)
{
	struct TAG_METADATA* tm = (struct tm*)malloc(sizeof(struct TAG_METADATA));

	if (fn == NULL) 
	{
		tm->isValid = false;
		return tm;
	}

	// You are trying to open an individual NSF file.
	if (!strstr(fn, ".m3u."))
	{
		tm->isValid = false;
		return tm;
	}

	int index = lastIndexOfSlash(fn);

	if (index == -1)
	{
		tm->isValid = false;
		return tm;
	}

	char* baseDirectory = substr(fn, 0, index + 1);
	char* songM3UFile = substr(fn, index + 1, strlen(fn) - index + 1);

	// https://stackoverflow.com/questions/8465006/how-do-i-concatenate-two-strings-in-c#8465083
	char* tagFile = strcat(baseDirectory, "!tags.m3u");

	FILE* input_file;
	if (input_file = fopen(tagFile, "r"))
	{
		tm = loadTag(input_file, songM3UFile);

		if (tm == NULL)
		{
			return NULL;
		}

		baseDirectory = substr(fn, 0, index + 1);
		tm->source = concat(baseDirectory, tm->source);

		fclose(input_file);
		printf("file exists");
	}

	return tm;
}

SEQUENCER* loadNezFile(char* fn)
{
	if (fn == NULL)
	{
		return;
	}

	NEZSEQ* p = NULL;
	struct TAG_METADATA* tm = (struct tm*)malloc(sizeof(struct TAG_METADATA));

	int playtime = 0, looptime = 0, fadetime = setting.fadetime, loopcount = setting.loopcount, subtune = 0;
	char searchPath[MAX_PATH];
	static char prevPath[MAX_PATH] = "";
	int difFile = 0;
	int nezsize;
	void* nezbuf;
	NEZ_PLAY* hnsf;

	lstrcpy(currentfn, fn);

	tm = loadTag2(fn);

	if (tm->isValid)
	{
		playtime = tm->length;
		fadetime = tm->fade;
		fn = tm->source;
		subtune = tm->subtune + 1;
		//looptime = tm->loop;
		//loopcount = tm->loopCount;
	}
	else
	{
		if (lstrcmpi(prevPath, fn))
		{
			lstrcpy(prevPath, fn);
			difFile = 1;
		}
	}

	fn = NezSearchPath(fn, searchPath);

	nezsize = NEZ_extract(fn, &nezbuf);
	if (nezsize == 0)
	{
		return NULL;
	}

	hnsf = NEZNew();
	NEZLoad(hnsf, nezbuf, nezsize);
	free(nezbuf);

	if (!hnsf)
	{
		return NULL;
	}

	p = (NEZSEQ*)malloc(sizeof(NEZSEQ));
	if (p == NULL)
	{
		NEZDelete(hnsf);
		return NULL;
	}
	p->hnsf = hnsf;

	p->frequency = setting.frequency;
	p->channel = NEZGetChannel(p->hnsf);

	NEZSetFrequency(p->hnsf, p->frequency);
	NEZSetChannel(p->hnsf, p->channel);

	p->bufsize = setting.frequency / 50;	/* 20 ms */
	p->bufp = 0;
	p->buf = (Int16*)malloc(p->bufsize * sizeof(Int16) * 2);
	if (p->buf == NULL)
	{
		NEZDelete(p->hnsf);
		free(p);
		return NULL;
	}

	if (subtune == 0)
	{
		SetStateControler(1);
		if (difFile)
		{
			EnableControler(NEZGetSongStart(p->hnsf), -1);
		}
		EnableControler(NEZGetSongStart(p->hnsf), NEZGetSongMax(p->hnsf));
		subtune = GetControler();
		if (setting.hookwinamp)
		{
			SubclassWinamp();
			p->controler_mode = CONTROLER_HOOK;
		}
		else
		{
			p->controler_mode = CONTROLER_ON;
		}
	}
	else
	{
		SetStateControler(0);
		DisableControler();
		p->controler_mode = CONTROLER_OFF;
	}

	if (subtune != 0)
	{
		NEZSetSongNo(p->hnsf, subtune);
	}

	if (tm->isValid)
	{
		tm->subtune = subtune;
	}

	NEZSetFilter(p->hnsf, setting.filtertype);

	p->loopc = 0;
	p->isplaying = 0;

	if (playtime = 0) 
	{
		playtime = setting.playtime;
		fadetime = setting.fadetime;
	}

	//setting.playtime = playtime;
	//setting.fadetime = fadetime;

	p->playtime = MulDiv(playtime, setting.frequency, 1000);
	p->looptime = MulDiv(looptime, setting.frequency, 1000);
	p->fadetime = fadetime;
	p->loopcount = loopcount;

	p->seq.work = p;
	p->seq.Play = Play;
	p->seq.Stop = Stop;
	p->seq.Mix = Mix;
	p->seq.Term = Term;
	p->seq.SetPosition = SetPosition;
	p->seq.GetPosition = GetPosition;
	p->seq.GetRate = GetRate;
	p->seq.GetChannel = GetChannel;
	p->seq.GetPriority = GetPriority;
	p->seq.GetLoopCount = GetLoopCount;
	p->seq.IsPlaying = IsPlaying;

	return &p->seq;
}

static char* RegistWinampFormat(char* p, char* ext, char* desc)
{
	lstrcpy(p, ext);
	p += lstrlen(p) + 1;
	lstrcpy(p, desc);
	p += lstrlen(p) + 1;
	return p;
}

char* StartWinamp(void)
{
	static char extbuf[1024];
	char* extp, * fntop;

	GetFullPathName(GetDLLArgv0(), MAX_PATH, setting.cfgname, &fntop);
	for (extp = 0; *fntop; fntop++) if (*fntop == '.') extp = fntop;
	if (!extp) extp = fntop;
	lstrcpy(extp, ".ini");

	setting.lpSection = "NEZplug";

	/* エミュレータの関連付け破壊防止(初心者用) */
	setting.disable_pceext = GetSettingInt("DisablePCEExtension", 1);
	SetSettingInt("DisablePCEExtension", setting.disable_pceext);

	setting.disable_nsf = GetSettingInt("DisableNSFSupport", 0);
	SetSettingInt("DisableNSFSupport", setting.disable_nsf);

	setting.disable_kss = GetSettingInt("DisableKSSSupport", 0);
	SetSettingInt("DisableKSSSupport", setting.disable_kss);

	setting.disable_gbr = GetSettingInt("DisableGBRSupport", 0);
	SetSettingInt("DisableGBRSupport", setting.disable_gbr);

	setting.disable_gbs = GetSettingInt("DisableGBSSupport", 0);
	SetSettingInt("DisableGBSSupport", setting.disable_gbs);

	setting.disable_hes = GetSettingInt("DisableHESSupport", 0);
	setting.disable_hes &= 1;
	SetSettingInt("DisableHESSupport", setting.disable_hes);

	setting.disable_ay = GetSettingInt("DisableAYSupport", 0);
	SetSettingInt("DisableAYSupport", setting.disable_ay);

	setting.disable_sgc = GetSettingInt("DisableSGCSupport", 0);
	SetSettingInt("DisableSGCSupport", setting.disable_sgc);

	extp = extbuf;
	if (!setting.disable_nsf && !setting.disable_nsfext)
		extp = RegistWinampFormat(extp, "NSF;NEZ;NSZ;NSD", "NES sound files (*.nsf;*.nez;*.nsz;*.nsd)");
	if (!setting.disable_nsf && setting.disable_nsfext)
		extp = RegistWinampFormat(extp, "NEZ;NSZ;NSD", "NEZplug sound files (*.nez;*.nsz;*.nsd)");
	if (!setting.disable_kss)
		extp = RegistWinampFormat(extp, "KSS", "KSS sound file (*.kss)");
	if (!setting.disable_gbr && !setting.disable_gbs)
		extp = RegistWinampFormat(extp, "GBR;GBS", "GB sound files (*.gbr;*.gbs)");
	if (!setting.disable_gbr && setting.disable_gbs)
		extp = RegistWinampFormat(extp, "GBR", "GBR sound file (*.gbr)");
	if (setting.disable_gbr && !setting.disable_gbs)
		extp = RegistWinampFormat(extp, "GBS", "GBS sound file (*.gbs)");
	if (!setting.disable_hes && !setting.disable_pceext)
		extp = RegistWinampFormat(extp, "HES;PCE", "HES sound files (*.hes;*.pce)");
	if (!setting.disable_hes && setting.disable_pceext)
		extp = RegistWinampFormat(extp, "HES", "HES sound file (*.hes)");
	if (!setting.disable_ay)
		extp = RegistWinampFormat(extp, "AY;CPC", "ZXAYEMUL sound files (*.ay;*.cpc)");
	if (!setting.disable_sgc)
		extp = RegistWinampFormat(extp, "SGC", "ZXAYEMUL sound files (*.sgc)");
	*extp = '\0';
	return extbuf;
}
