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
#include <sys/time.h>

#include "argv.h"
#include "md5.h"

#define BUFFER_SIZE	100000			/* size of buffer */

/* argument vars */
static	unsigned long	buf_size = BUFFER_SIZE;	/* size of i/o buffer */
static	unsigned long	dot_size = 0;		/* show a dot every X */
static	int		flush_out_b = ARGV_FALSE; /* flush output to files */
static	int		run_md5_b = ARGV_FALSE;	/* run md5 on data */
static	int		non_block_b = ARGV_FALSE; /* don't block on input */
static	int		pass_b = ARGV_FALSE;	/* pass data through */
static	unsigned long	throttle_size = 0;	/* throttle bytes/second */
static	int		verbose_b = ARGV_FALSE;	/* verbose flag */
static	int		very_verbose_b = ARGV_FALSE; /* very-verbose flag */
static	argv_array_t	outfiles;		/* outfiles for read data */

static	argv_t	args[] = {
  { 'b',	"buffer-size",	ARGV_U_SIZE,			&buf_size,
    "size",			"size of input output buffer" },
  { 'd',	"dot-blocks",	ARGV_U_SIZE,			&dot_size,
    "size",			"show a dot each X bytes of input" },
  { 'f',	"output-file",	ARGV_CHAR_P | ARGV_FLAG_ARRAY,	&outfiles,
    "output-file",		"output file to write input" },
  { 'F',	"flush-output",	ARGV_BOOL_INT,			&flush_out_b,
    NULL,			"flush output to files" },
  { 'm',	"md5",		ARGV_BOOL_INT,			&run_md5_b,
    NULL,			"run input bytes through md5" },
  { 'n',	"non-block",	ARGV_BOOL_INT,			&non_block_b,
    NULL,			"don't block on input" },
  { 'p',	"pass-input",	ARGV_BOOL_INT,			&pass_b,
    NULL,			"pass input data to output" },
  { 't',	"throttle-size", ARGV_U_SIZE,			&throttle_size,
    "size",			"throttle output to X bytes / sec" },
  { 'v',	"verbose",	ARGV_BOOL_INT,			&verbose_b,
    NULL,			"report on i/o bytes" },
  { 'V',	"very-verbose",	ARGV_BOOL_INT,		       &very_verbose_b,
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
  unsigned int		pass_n, file_c;
  unsigned long		read_c = 0, write_c = 0, dot_c = 0;
  unsigned long		buf_len, write_max, write_size;
  int			last_read_n, ret;
  FILE			**streams = NULL;
  fd_set		listen_set;
  char			*buf, md5_result[MD5_SIZE], *md5_p;
  md5_t			md5;
  struct timeval	start, now;
  
  argv_process(args, argc, argv);
  if (very_verbose_b) {
    verbose_b = 1;
  }
  
  /* open output paths if needed -- we add one to accomodate stdout */
  if (pass_b) {
    pass_n = 1;
  }
  else {
    pass_n = 0;
  }
  streams = (FILE **)malloc(sizeof(FILE *) * (outfiles.aa_entry_n + pass_n));
  if (pass_b) {
    streams[0] = stdout;
  }
  
  /* make stdin non-blocking */
  if (non_block_b) {
    (void)fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) | O_NONBLOCK);
  }
  
  if (outfiles.aa_entry_n > 0) {
    for (file_c = 0; file_c < (unsigned int)outfiles.aa_entry_n; file_c++) {
      char	*path = ARGV_ARRAY_ENTRY(outfiles, char *, file_c);
      
      streams[file_c + pass_n] = fopen(path, "w");
      if (streams[file_c + pass_n] == NULL) {
	(void)fprintf(stderr, "%s: cannot fopen(%s): %s\n", 
		      argv_program, path, strerror(errno));
      }
    }
  }
  
  buf = (char *)malloc(buf_size);
  
  if (run_md5_b) {
    md5_init(&md5);
  }
  
  gettimeofday(&start, NULL);
  buf_len = 0;
  
  /* read in stuff and count the number */
  for (;;) {
    
    if (non_block_b) {
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
    
    /* read in data from input stream */
    if (buf_size <= buf_len) {
      last_read_n = 0;
    }
    else {
      last_read_n = read(0, buf + buf_len, buf_size - buf_len);
      if (last_read_n == 0) {
	break;
      }
      if (last_read_n < 0) {
	(void)fprintf(stderr, "%s: read on stdin error: %s\n",
		      argv_program, strerror(errno));
	exit(1);
      }
      
      if (very_verbose_b) {
	(void)fprintf(stderr, "Read %d bytes\n", last_read_n);
      }
      if (run_md5_b) {
	md5_buffer(buf + buf_len, last_read_n, &md5);
      }
      
      read_c += last_read_n;
      buf_len += last_read_n;
    }
    
    /* real simple throttle code */
    if (throttle_size == 0) {
      write_size = buf_len;
    }
    else {
      gettimeofday(&now, NULL);
      now.tv_sec -= start.tv_sec;
      if (now.tv_usec < start.tv_usec) {
	now.tv_sec--;
	now.tv_usec += 1000000;
      }
      now.tv_usec -= start.tv_usec;
      
      /* really it should be 0 but we should write something */
      if (now.tv_sec == 0 && now.tv_usec == 0) {
	now.tv_usec = 300000;
      }
      
      /* calculate what we should have written by now */
      write_max = (float)throttle_size *
	((float)now.tv_sec + ((float)now.tv_usec / 1000000.0));
      
      /* have we read too much? */
      if (read_c >= write_max) {
	/* if we can't write and we didn't read, no use spinning */
	if (last_read_n == 0) {
	  struct timeval	timeout;
	  float			dist;
	  
	  dist = ((float)read_c - (float)write_max) / (float)throttle_size;
	  timeout.tv_sec = dist;
	  timeout.tv_usec = ((float)dist - (float)timeout.tv_sec) * 1000000.0;
	  (void)select(0, NULL, NULL, NULL, &timeout);
	}
	write_size = 0;
      }
      else {
	write_size = write_max - read_c;
      }
      
      if (write_size > buf_len) {
	write_size = buf_len;
      }
    }
    
    /* should we write it? */
    if (write_size > 0) {
      
      /* write it out */
      for (file_c = 0; file_c < outfiles.aa_entry_n + pass_n; file_c++) {
	if (streams[file_c] != NULL) {
	  (void)fwrite(buf, sizeof(char), write_size, streams[file_c]);
	  if (flush_out_b) {
	    (void)fflush(streams[file_c]);
	  }
	}
      }

      /*
       * Print out our dots.  The problem here is when write_c
       * overflows a long-integer and therefore our dot-loop will go
       * infinite.  We therefore continually reset the dot sizes.
       */
      if (dot_size > 0) {
	write_c += write_size;
	
	while (dot_c + dot_size < write_c) {
	  (void)fputc('.', stderr);
	  dot_c += dot_size;
	}
	
	write_c -= dot_c;
	dot_c = 0;
      }
      
      /* do we need to shift the buffer down? */
      if (buf_len > write_size) {
	memmove(buf, buf + write_size, buf_len - write_size);
      }
      buf_len -= write_size;
    }
  }
  
  gettimeofday(&now, NULL);
  now.tv_sec -= start.tv_sec;
  if (now.tv_usec < start.tv_usec) {
    now.tv_sec--;
    now.tv_usec += 1000000;
  }
  now.tv_usec -= start.tv_usec;
  
  if (dot_size > 0) {
    (void)fputc('\n', stderr);
  }
  
  /* write some report info */
  if (verbose_b) {
    int	speed;
    (void)fprintf(stderr, "%s: processed %s in %ld.%02ld secs",
		  argv_program, byte_size(read_c), now.tv_sec,
		  now.tv_usec / 10000);
    /* NOTE: this needs to be in a separate printf */
    if (now.tv_sec == 0 && now.tv_usec == 0) {
      speed = read_c;
    }
    else {
      speed = (float)read_c /
	((float)now.tv_sec + ((float)now.tv_usec / 1000000.0));
    }
    (void)fprintf(stderr, " or %s/sec\n", byte_size(speed));
  }
  
  if (run_md5_b) {
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
