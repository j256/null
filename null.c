/*
 * /dev/null program
 *
 * Copyright 1991 by the Antaire Corporation
 *
 * $Id$
 */

#include <stdio.h>

#include "argv.h"

#define EXPORT
#define IMPORT	extern
#define LOCAL	static

#undef NULL
#define NULL	0L

#define BUFFER_SIZE	100000			/* size of buffer */

/* argument vars */
LOCAL	char		verbose	= ARGV_FALSE;	/* verbose flag */
LOCAL	char		pass	= ARGV_FALSE;	/* pass data through */
LOCAL	char		*outpath= NULL;		/* outfile for read data */

LOCAL	argv_t	args[] = {
  { 'f',	"output-file",	ARGV_CHARP,	&outpath,
      "output-file",		"output file to write input" },
  { 'p',	"pass-input",	ARGV_BOOL,	&pass,
      NULL,			"pass input data to output" },
  { 'v',	"verbose",	ARGV_BOOL,	&verbose,
      NULL,			"report on i/o bytes" },
  { ARGV_LAST }
};

/* return the number of bytes broken down */
LOCAL	char	*byte_size(int count)
{
  char		buf[80];
  
  if (count > 1024 * 1024)
    (void)sprintf(buf, "%d bytes (%.1fmb)",
		  count, (float)(count) / (float)(1024 * 1024));
  else if (count > 1024)
    (void)sprintf(buf, "%d bytes (%.1fkb)", count, (float)count / 1024.0);
  else
    (void)sprintf(buf, "%d byte%s", count, (count == 1 ? "" : "s"));
  
  return buf;
}

EXPORT	int	main(int argc, char **argv)
{
  int		ret, inc = 0, outc = 0;
  FILE		*outfile;
  char		*buf;
  
  argv_process(args, argc, argv);
  
  /* open outpath if needed */
  if (outpath != NULL) {
    outfile = fopen(outpath, "w");
    if (outfile == NULL) {
      perror(outpath);
      exit(1);
    }
  }
  
  buf = (char *)malloc(BUFFER_SIZE);
  
  /* read in stuff and count the number */
  for (;;) {
    
    ret = fread(buf, sizeof(char), BUFFER_SIZE, stdin);
    if (ret <= 0)
      break;
    inc += ret;
    
    if (outpath)
      (void)fwrite(buf, sizeof(char), ret, outfile);
    
    /* should we write it? */
    if (pass) {
      ret = fwrite(buf, sizeof(char), ret, stdout);
      if (ret > 0)
	outc += ret;
    }
  }
  
  /* write some report info */
  if (verbose) {
    (void)fprintf(stderr, "%s: read %s", argv_program, byte_size(inc));
    
    if (pass)
      (void)fprintf(stderr, ", wrote %s", byte_size(outc));
    
    (void)fprintf(stderr, "\n");
  }
  
  /* close the output path */
  if (outpath)
    (void)fclose(outfile);
  
  free(buf);
  argv_cleanup(args);

  exit(0);
}
