/* Test stand-in for curl, built by tools/check_installer.sh.  Never touches the
 * network.  STUB_MODE picks the behaviour (serve, partial, slow, fail22); any
 * other value (or none) is a tripwire: it records STUB_TRIP and exits 6, so a
 * test that reaches an unexpected downloader is noticed. Plain C so
 * the same source builds on Unix and MinGW. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define STUB_PID() ((long)_getpid())
#define STUB_SLEEP(seconds) Sleep((seconds) * 1000)
#else
#include <unistd.h>
#define STUB_PID() ((long)getpid())
#define STUB_SLEEP(seconds) sleep(seconds)
#endif

static int copyBytes(const char *from, const char *to, long limit)
{
	FILE *in = fopen(from, "rb"), *out = fopen(to, "wb");
	char buffer[4096];
	size_t got;
	int ok = in != NULL && out != NULL;
	while (ok && limit != 0 && (got = fread(buffer, 1, sizeof buffer, in)) > 0)
	{
		if (limit > 0 && got > (size_t)limit)
			got = (size_t)limit;
		ok = fwrite(buffer, 1, got, out) == got;
		if (limit > 0)
			limit -= (long)got;
	}
	if (in != NULL) fclose(in);
	if (out != NULL && fclose(out) != 0) ok = 0;
	return ok;
}

static void writeText(const char *path, const char *mode, const char *text)
{
	FILE *file = path != NULL ? fopen(path, mode) : NULL;
	if (file != NULL)
	{
		fputs(text, file);
		fclose(file);
	}
}

int main(int argc, char **argv)
{
	const char *mode = getenv("STUB_MODE"), *zip = getenv("STUB_ZIP"), *out = NULL;
	char line[8192] = "", pid[32];
	int i;

	if (mode == NULL || mode[0] == '\0' || strchr("spf", mode[0]) == NULL)
	{
		writeText(getenv("STUB_TRIP"), "ab", "unexpected downloader call\n");
		return 6;
	}
	for (i = 1; i < argc; ++i)
	{
		if (strlen(line) + strlen(argv[i]) + 2 < sizeof line)
		{
			strcat(line, i > 1 ? " " : "");
			strcat(line, argv[i]);
		}
		if (strcmp(argv[i], "--output") == 0 && i + 1 < argc)
			out = argv[i + 1];
	}
	strcat(line, "\n");
	writeText(getenv("STUB_LOG"), "ab", line);
	sprintf(pid, "%ld\n", STUB_PID());
	writeText(getenv("STUB_PID"), "wb", pid);

	if (strcmp(mode, "serve") == 0)
		return zip != NULL && out != NULL && copyBytes(zip, out, -1) ? 0 : 1;
	if (strcmp(mode, "partial") == 0)
	{
		if (zip != NULL && out != NULL) copyBytes(zip, out, 1000);
		return 18;
	}
	if (strcmp(mode, "slow") == 0)
	{
		if (zip != NULL && out != NULL) copyBytes(zip, out, 1000);
		STUB_SLEEP(30);
		return 0;
	}
	if (strcmp(mode, "fail22") == 0)
		return 22;
	return 6;
}
