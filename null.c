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
  { 'f',	"output-file",	ARGV_CHARP | ARGV_ARRAY,	&outfiles,
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
  
  if (count > 1024 * 1024)
    (void)sprintf(buf, "%d bytes (%.1fmb)",
		  count, (float)(count) / (float)(1024 * 1024));
  else if (count > 1024)
    (void)sprintf(buf, "%d bytes (%.1fkb)", count, (float)count / 1024.0);
  else
    (void)sprintf(buf, "%d byte%s", count, (count == 1 ? "" : "s"));
  
  return buf;
}

int	main(int argc, char **argv)
{
  int		pass_n, ret, inc = 0, filec, dot_c;
  FILE		**streams = NULL;
  fd_set	listen_set;
  char		*buf, md5_result[MD5_SIZE], *md5_p;
  md5_t		md5;
  
  argv_process(args, argc, argv);
  if (very_verbose)
    verbose = 1;
  
  /* open output paths if needed -- we add one to accomodate stdout */
  if (pass)
    pass_n = 1;
  else
    pass_n = 0;
  streams = (FILE **)malloc(sizeof(FILE *) * (outfiles.aa_entryn + pass_n));
  if (pass)
    streams[0] = stdout;
  
  /* make stdin non-blocking */
  if (non_block)
    (void)fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) | O_NONBLOCK);
  
  if (outfiles.aa_entryn > 0) {
    for (filec = 0; filec < outfiles.aa_entryn; filec++) {
      char	*path = ARGV_ARRAY_ENTRY(outfiles, char *, filec);
      
      streams[filec + pass_n] = fopen(path, "w");
      if (streams[filec + pass_n] == NULL)
	(void)fprintf(stderr, "%s: cannot fopen(%s): %s\n", 
		      argv_program, path, strerror(errno));
    }
  }
  
  buf = (char *)malloc(buf_size);
  
  if (run_md5)
    md5_init(&md5);
  
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
      if (! FD_ISSET(0, &listen_set))
	continue;
    }
    
    ret = read(0, buf, buf_size);
    if (ret == 0)
      break;
    if (ret < 0) {
      (void)fprintf(stderr, "%s: read on stdin error: %s\n",
		    argv_program, strerror(errno));
      exit(1);
    }
    inc += ret;
    
    if (very_verbose)
      (void)fprintf(stderr, "Read %d bytes\n", ret);
    if (run_md5)
      md5_process_bytes(&md5, buf, ret);
    
    if (dot_size > 0) {
      dot_c -= ret;
      while (dot_c <= 0) {
	(void)fputc('.', stderr);
	dot_c += dot_size;
      }
    }
    
    /* should we write it? */
    for (filec = 0; filec < outfiles.aa_entryn + pass_n; filec++) {
      if (streams[filec] != NULL) {
	(void)fwrite(buf, sizeof(char), ret, streams[filec]);
	if (flush_out)
	  (void)fflush(streams[filec]);
      }
    }
  }
  
  if (dot_size > 0)
    (void)fputc('\n', stderr);
  
  /* write some report info */
  if (verbose)
    (void)fprintf(stderr, "%s: processed %s\n", argv_program, byte_size(inc));
  
  if (run_md5) {
    md5_finish(&md5, md5_result);
    (void)fprintf(stderr, "%s: md5 signature of input = '", argv_program);
    for (md5_p = md5_result; md5_p < md5_result + MD5_SIZE; md5_p++)
      (void)fprintf(stderr, "%02x", *(unsigned char *)md5_p);
    (void)fputs("'\n", stderr);
  }
  
  /* close the output paths */
  for (filec = pass_n; filec < outfiles.aa_entryn + pass_n; filec++) {
    if (streams[filec] != NULL)
      (void)fclose(streams[filec]);
  }
  
  if (streams != NULL)
    free(streams);
  free(buf);
  argv_cleanup(args);

  exit(0);
}
