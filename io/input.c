/*
 * Copyright (c) 2003 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <libxfs.h>
#include "input.h"
#include "init.h"

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
