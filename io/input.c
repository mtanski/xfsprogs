/*
 * Copyright (c) 2003-2004 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */

#include <xfs/libxfs.h>
#include "input.h"
#include "init.h"
#include "io.h"

#if defined(ENABLE_READLINE)
# include <readline/history.h>
# include <readline/readline.h>
#elif defined(ENABLE_EDITLINE)
# include <histedit.h>
#endif

static char *
get_prompt(void)
{
	static char	prompt[FILENAME_MAX + 1];

	if (!prompt[0])
		snprintf(prompt, sizeof(prompt), "%s> ", progname);
	return prompt;
}

#if defined(ENABLE_READLINE)
char *
fetchline(void)
{
	char	*line;

	line = readline(get_prompt());
	if (line && *line)
		add_history(line);
	return line;
}
#elif defined(ENABLE_EDITLINE)
static char *el_get_prompt(EditLine *e) { return get_prompt(); }
char *
fetchline(void)
{
	static EditLine	*el;
	static History	*hist;
	HistEvent	hevent;
	char		*line;
	int		count;

	if (!el) {
		hist = history_init();
		history(hist, &hevent, H_SETSIZE, 100);
		el = el_init(progname, stdin, stdout, stderr);
		el_source(el, NULL);
		el_set(el, EL_SIGNAL, 1);
		el_set(el, EL_PROMPT, el_get_prompt);
		el_set(el, EL_HIST, history, (const char *)hist);
	}
	line = strdup(el_gets(el, &count));
	if (line) {
		if (count > 0)
			line[count-1] = '\0';
		if (*line)
			history(hist, &hevent, H_ENTER, line);
	}
	return line;
}
#else
# define MAXREADLINESZ	1024
char *
fetchline(void)
{
	char	*p, *line = malloc(MAXREADLINESZ);

	if (!line)
		return NULL;
	printf(get_prompt());
	fflush(stdout);
	if (!fgets(line, MAXREADLINESZ, stdin)) {
		free(line);
		return NULL;
	}
	p = line + strlen(line);
	if (p != line && p[-1] == '\n')
		p[-1] = '\0';
	return line;
}
#endif

char **
breakline(
	char	*input,
	int	*count)
{
	int	c = 0;
	char	*p;
	char	**rval = calloc(sizeof(char *), 1);

	while ((p = strsep(&input, " ")) != NULL) {
		if (!*p)
			continue;
		c++;
		rval = realloc(rval, sizeof(*rval) * (c + 1));
		rval[c - 1] = p;
		rval[c] = NULL;
	}
	*count = c;
	return rval;
}

void
doneline(
	char	*input,
	char	**vec)
{
	free(input);
	free(vec);
}

void
init_cvtnum(
	int		*blocksize,
	int		*sectsize)
{
	if (!file || (file->flags & IO_FOREIGN)) {
		*blocksize = 4096;
		*sectsize = 512;
	} else {
		*blocksize = file->geom.blocksize;
		*sectsize = file->geom.sectsize;
	}
}

#define EXABYTES(x)	((long long)(x) << 60)
#define PETABYTES(x)	((long long)(x) << 50)
#define TERABYTES(x)	((long long)(x) << 40)
#define GIGABYTES(x)	((long long)(x) << 30)
#define MEGABYTES(x)	((long long)(x) << 20)
#define KILOBYTES(x)	((long long)(x) << 10)

long long
cvtnum(
	int		blocksize,
	int		sectorsize,
	char		*s)
{
	long long	i;
	char		*sp;

	i = strtoll(s, &sp, 0);
	if (i == 0 && sp == s)
		return -1LL;
	if (*sp == '\0')
		return i;

	if (*sp == 'b' && sp[1] == '\0')
		return i * blocksize;
	if (*sp == 's' && sp[1] == '\0')
		return i * sectorsize;
	if (*sp == 'k' && sp[1] == '\0')
		return KILOBYTES(i);
	if (*sp == 'm' && sp[1] == '\0')
		return MEGABYTES(i);
	if (*sp == 'g' && sp[1] == '\0')
		return GIGABYTES(i);
	if (*sp == 't' && sp[1] == '\0')
		return TERABYTES(i);
	if (*sp == 'p' && sp[1] == '\0')
		return PETABYTES(i);
	if (*sp == 'e' && sp[1] == '\0')
		return  EXABYTES(i);
	return -1LL;
}

#define TO_EXABYTES(x)	((x) / EXABYTES(1))
#define TO_PETABYTES(x)	((x) / PETABYTES(1))
#define TO_TERABYTES(x)	((x) / TERABYTES(1))
#define TO_GIGABYTES(x)	((x) / GIGABYTES(1))
#define TO_MEGABYTES(x)	((x) / MEGABYTES(1))
#define TO_KILOBYTES(x)	((x) / KILOBYTES(1))

void
cvtstr(
	double		value,
	char		*str,
	size_t		size)
{
	char		*fmt;
	int		precise;

	precise = ((double)value * 1000 == (double)(int)value * 1000);

	if (value >= EXABYTES(1)) {
		fmt = precise ? "%.f EiB" : "%.3f EiB";
		snprintf(str, size, fmt, TO_EXABYTES(value));
	} else if (value >= PETABYTES(1)) {
		fmt = precise ? "%.f PiB" : "%.3f PiB";
		snprintf(str, size, fmt, TO_PETABYTES(value));
	} else if (value >= TERABYTES(1)) {
		fmt = precise ? "%.f TiB" : "%.3f TiB";
		snprintf(str, size, fmt, TO_TERABYTES(value));
	} else if (value >= GIGABYTES(1)) {
		fmt = precise ? "%.f GiB" : "%.3f GiB";
		snprintf(str, size, fmt, TO_GIGABYTES(value));
	} else if (value >= MEGABYTES(1)) {
		fmt = precise ? "%.f MiB" : "%.3f MiB";
		snprintf(str, size, fmt, TO_MEGABYTES(value));
	} else if (value >= KILOBYTES(1)) {
		fmt = precise ? "%.f KiB" : "%.3f KiB";
		snprintf(str, size, fmt, TO_KILOBYTES(value));
	} else {
		snprintf(str, size, "%f bytes", value);
	}
}

struct timeval
tsub(struct timeval t1, struct timeval t2)
{
	t1.tv_usec -= t2.tv_usec;
	if (t1.tv_usec < 0) {
		t1.tv_usec += 1000000;
		t1.tv_sec--;
	}
	t1.tv_sec -= t2.tv_sec;
	return t1;
}

double
tdiv(double value, struct timeval tv)
{
	return value / ((double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0));
}

#define HOURS(sec)	((sec) / (60 * 60))
#define MINUTES(sec)	(((sec) % (60 * 60)) / 60)
#define SECONDS(sec)	((sec) % 60)

void
timestr(
	struct timeval	*tv,
	char		*ts,
	size_t		size)
{
	if (!tv->tv_sec)
		snprintf(ts, size, "%.4f sec",
			((double) tv->tv_usec / 1000000.0));
	else
		snprintf(ts, size, "%02u:%02u:%02u.%-u",
			(unsigned int) HOURS(tv->tv_sec),
			(unsigned int) MINUTES(tv->tv_sec),
			(unsigned int) SECONDS(tv->tv_sec),
			(unsigned int) tv->tv_usec);
}
