/*
 * /dev/null program
 *
 * Copyright 1995, 1996, 1997 by Gray Watson
 *
 * $Id$
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "argv.h"
#include "md5.h"

#define BUFFER_SIZE	100000			/* size of buffer */

/* argument vars */
static	int		buf_size = BUFFER_SIZE;	/* size of i/o buffer */
static	int		dot_size = 0;		/* show a dot every X */
static	char		flush_out = ARGV_FALSE;	/* flush output to files */
static	char		run_md5	= ARGV_FALSE;	/* run md5 on data */
static	char		non_block = ARGV_FALSE;	/* don't block on input */
static	char		pass	= ARGV_FALSE;	/* pass data through */
static	char		verbose	= ARGV_FALSE;	/* verbose flag */
static	char		very_verbose = ARGV_FALSE; /* very-verbose flag */
static	argv_array_t	outfiles;		/* outfiles for read data */

static	argv_t	args[] = {
  { 'b',	"buffer-size",	ARGV_SIZE,			&buf_size,
      "size",			"size of input output buffer" },
  { 'd',	"dot-blocks",	ARGV_SIZE,			&dot_size,
      "size",			"show a dot each X bytes of input" },
  { 'f',	"output-file",	ARGV_CHAR_P | ARGV_ARRAY,	&outfiles,
      "output-file",		"output file to write input" },
  { 'F',	"flush-output",	ARGV_BOOL,			&flush_out,
      NULL,			"flush output to files" },
  { 'm',	"md5",		ARGV_BOOL,			&run_md5,
      NULL,			"don't block on input" },
  { 'n',	"non-block",	ARGV_BOOL,			&non_block,
      NULL,			"don't block on input" },
  { 'p',	"pass-input",	ARGV_BOOL,			&pass,
      NULL,			"pass input data to output" },
  { 'v',	"verbose",	ARGV_BOOL,			&verbose,
      NULL,			"report on i/o bytes" },
  { 'V',	"very-verbose",	ARGV_BOOL,			&very_verbose,
      NULL,			"very verbose messages" },
  { ARGV_LAST }
};

/* return the number of bytes broken down */
static	char	*byte_size(const int count)
{
  static char	buf[80];
  
  if (count > 1024 * 1024) {
    (void)sprintf(buf, "%d bytes (%.1fm)",
		  count, (float)(count) / (float)(1024 * 1024));
  }
  else if (count > 1024) {
    (void)sprintf(buf, "%d bytes (%.1fk)", count, (float)count / 1024.0);
  }
  else {
    (void)sprintf(buf, "%d byte%s", count, (count == 1 ? "" : "s"));
  }
  
  return buf;
}

int	main(int argc, char **argv)
{
  unsigned int	pass_n, ret, file_c, dot_c;
  unsigned long	inc = 0;
  FILE		**streams = NULL;
  fd_set	listen_set;
  char		*buf, md5_result[MD5_SIZE], *md5_p;
  md5_t		md5;
  time_t	now, diff;
  
  argv_process(args, argc, argv);
  if (very_verbose)
    verbose = 1;
  
  /* open output paths if needed -- we add one to accomodate stdout */
  if (pass)
    pass_n = 1;
  else
    pass_n = 0;
  streams = (FILE **)malloc(sizeof(FILE *) * (outfiles.aa_entry_n + pass_n));
  if (pass) {
    streams[0] = stdout;
  }
  
  /* make stdin non-blocking */
  if (non_block) {
    (void)fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) | O_NONBLOCK);
  }
  
  if (outfiles.aa_entry_n > 0) {
    for (file_c = 0; file_c < outfiles.aa_entry_n; file_c++) {
      char	*path = ARGV_ARRAY_ENTRY(outfiles, char *, file_c);
      
      streams[file_c + pass_n] = fopen(path, "w");
      if (streams[file_c + pass_n] == NULL) {
	(void)fprintf(stderr, "%s: cannot fopen(%s): %s\n", 
		      argv_program, path, strerror(errno));
      }
    }
  }
  
  buf = (char *)malloc(buf_size);
  
  if (run_md5)
    md5_init(&md5);
  
  now = time(NULL);
  
  /* read in stuff and count the number */
  dot_c = dot_size;
  for (;;) {
    
    if (non_block) {
      FD_ZERO(&listen_set);
      FD_SET(0, &listen_set);
      ret = select(1, &listen_set, NULL, NULL, NULL);
      if (ret == -1) {
	(void)fprintf(stderr, "%s: select error: %s\n",
		      argv_program, strerror(errno));
	exit(1);
      }
      if (! FD_ISSET(0, &listen_set)) {
	continue;
      }
    }
    
    ret = read(0, buf, buf_size);
    if (ret == 0) {
      break;
    }
    if (ret < 0) {
      (void)fprintf(stderr, "%s: read on stdin error: %s\n",
		    argv_program, strerror(errno));
      exit(1);
    }
    inc += ret;
    
    if (very_verbose) {
      (void)fprintf(stderr, "Read %d bytes\n", ret);
    }
    if (run_md5) {
      md5_process_bytes(&md5, buf, ret);
    }
    
    if (dot_size > 0) {
      dot_c -= ret;
      while (dot_c <= 0) {
	(void)fputc('.', stderr);
	dot_c += dot_size;
      }
    }
    
    /* should we write it? */
    for (file_c = 0; file_c < outfiles.aa_entry_n + pass_n; file_c++) {
      if (streams[file_c] != NULL) {
	(void)fwrite(buf, sizeof(char), ret, streams[file_c]);
	if (flush_out) {
	  (void)fflush(streams[file_c]);
	}
      }
    }
  }
  
  diff = time(NULL) - now;
  
  if (dot_size > 0) {
    (void)fputc('\n', stderr);
  }
  
  /* write some report info */
  if (verbose) {
    (void)fprintf(stderr, "%s: processed %s in %d secs or %s/sec\n",
		  argv_program, byte_size(inc), (int)diff,
		  (diff == 0 ? byte_size(inc) : byte_size(inc / diff)));
  }
  
  if (run_md5) {
    md5_finish(&md5, md5_result);
    (void)fprintf(stderr, "%s: md5 signature of input = '", argv_program);
    for (md5_p = md5_result; md5_p < md5_result + MD5_SIZE; md5_p++) {
      (void)fprintf(stderr, "%02x", *(unsigned char *)md5_p);
    }
    (void)fputs("'\n", stderr);
  }
  
  /* close the output paths */
  for (file_c = pass_n; file_c < outfiles.aa_entry_n + pass_n; file_c++) {
    if (streams[file_c] != NULL) {
      (void)fclose(streams[file_c]);
    }
  }
  
  if (streams != NULL) {
    free(streams);
  }
  free(buf);
  argv_cleanup(args);

  exit(0);
}
