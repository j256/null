/*
 * /dev/null program
 *
 * Copyright 2000 by Gray Watson
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

#ifdef __GNUC__
#ident "$Id$";
#else
static  char    *ident_str =
  "$Id$";
#endif

#define BUFFER_SIZE	100000		/* size of buffer */
#define WRITES_PER_SEC	10		/* throttle to X writes/sec.  X > 1. */
#define PASS_CHAR	'p'		/* pass - argument */
#define STDIN_FD	0		/* stdin file descriptor */

#define PAGINATION_ESC	"null-page-"	/* special pagination string */
#define PAGINATION_START	's'	/* start character */
#define PAGINATION_MID		'm'	/* mid character */
#define PAGINATION_END		'e'	/* end character */

/* argument vars */
static	unsigned long	buf_size = BUFFER_SIZE;	/* size of i/o buffer */
static	unsigned long	dot_size = 0;		/* show a dot every X */
static	int		flush_out_b = ARGV_FALSE; /* flush output to files */
static	int		run_md5_b = ARGV_FALSE;	/* run md5 on data */
static	int		non_block_b = ARGV_FALSE; /* don't block on input */
static	int		pass_b = ARGV_FALSE;	/* pass data through */
static	int		read_page_b = 0;	/* read pagination info */
static	unsigned long	throttle_size = 0;	/* throttle bytes/second */
static	int		verbose_b = ARGV_FALSE;	/* verbose flag */
static	int		very_verbose_b = ARGV_FALSE; /* very-verbose flag */
static	int		write_page_b = 0;	/* output pagination info */
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
  { PASS_CHAR,	"pass-input",	ARGV_BOOL_INT,			&pass_b,
    NULL,			"pass input data to output" },
  { 'r',	"read-pagination", ARGV_BOOL_INT,		&read_page_b,
    NULL,			"read pagination data" },
  { 't',	"throttle-size", ARGV_U_SIZE,			&throttle_size,
    "size",			"throttle output to X bytes / sec" },
  { 'v',	"verbose",	ARGV_BOOL_INT,			&verbose_b,
    NULL,			"report on i/o bytes" },
  { 'V',	"very-verbose",	ARGV_BOOL_INT,		       &very_verbose_b,
    NULL,			"very verbose messages" },
  { 'w',	"write-pagination", ARGV_BOOL_INT,		&write_page_b,
    NULL,			"write paginate data" },
  { ARGV_LAST }
};

/*
 * static char *byte_size
 *
 * DESCRIPTION:
 *
 * Translate a size into its string representation.
 *
 * RETURNS:
 *
 * Pointer to a byte-size character pointer.
 *
 * ARGUMENTS:
 *
 * size -> Size we are translating.
 */
static	char	*byte_size(const unsigned long size)
{
  static char	buf[80];
  
  if (size > 1024L * 1024L * 1024L) {
    (void)sprintf(buf, "%.1fg (%ld)",
		  (float)(size) / (float)(1024L * 1024L * 1024L), size);
  }
  else if (size > 1024L * 1024L) {
    (void)sprintf(buf, "%.1fm (%ld)",
		  (float)(size) / (float)(1024 * 1024), size);
  }
  else if (size > 1024L) {
    (void)sprintf(buf, "%.1fk (%ld)", (float)size / (float)1024.0, size);
  }
  else {
    (void)sprintf(buf, "%ldb", size);
  }
  
  return buf;
}

/*
 * static int write_pagination
 *
 * DESCRIPTION:
 *
 * Write making sure that we don't have any pagination information in
 * the middle by accident.
 *
 * RETURNS:
 *
 * The number of characters that it wrote.  It may not write all of
 * the buf_len characters if part of the secret string is at the end
 * of the buffer.
 *
 * ARGUMENTS:
 *
 * buf -> Buffer we are writing.
 *
 * buf_len -> Length of the buffer we are writing.
 *
 * last_b -> Set to 1 if this is the last write.
 */
static	int	write_pagination(const char *buf, const int buf_len,
				 const int last_b)
{
  const char	*buf_p, *bounds_p, *write_p = buf;
  int		len;
  
  len = strlen(PAGINATION_ESC);
  
  bounds_p = buf + buf_len;
  for (buf_p = buf; buf_p + len < bounds_p;) {
    
    /* if we don't have a pagination-escapge string then move forward */
    if (*buf_p != PAGINATION_ESC[0]
	|| memcmp(PAGINATION_ESC, buf_p, len) != 0) {
      buf_p++;
      continue;
    }
    
    /*
     * Insert a mid escape string which means to the reader that the
     * input contained a pagination escape by mistake.
     */
    buf_p += len;
    (void)fwrite(write_p, sizeof(char), buf_p - write_p, stdout);
    if (very_verbose_b) {
      (void)fprintf(stderr, " wrote %d paged chars\n", buf_p - write_p);
    }
    (void)fputc(PAGINATION_MID, stdout);
    if (very_verbose_b) {
      (void)fprintf(stderr, " wrote mid pagination\n");
    }
    
    write_p = buf_p;
  }
  
  /*
   * We are left with a partial buffer.  If it does not contain the
   * start of a partition escape string or it's the last write then we
   * will write it out.  Otherwise, more has to be read.
   */
  len = bounds_p - buf_p;
  if (len > 0 && (memcmp(PAGINATION_ESC, buf_p, len) != 0
		  || last_b)) {
    buf_p += len;
  }
  
  len = buf_p - write_p;
  if (len > 0) {
    (void)fwrite(write_p, sizeof(char), len, stdout);
    if (very_verbose_b) {
      (void)fprintf(stderr, " wrote %d paged chars\n", len);
    }
  }
  
  return buf_p - buf;
}

/*
 * static int read_pagination
 *
 * DESCRIPTION:
 *
 * Process a read buffer making sure that we don't have any pagination
 * escape strings in the midding of the data.
 *
 * RETURNS:
 *
 * The number of characters that we left in the buffer after trimming
 * out any escape characters.
 *
 * ARGUMENTS:
 *
 * buf -> Buffer we are writing.
 *
 * buf_len -> Length of the buffer we are writing.
 *
 * to_write_p <- Pointer to an integer which will be set with the
 * number of characters to be written from the buffer.
 *
 * last_b -> Set to 1 if this is the last write.
 */
static	int	read_pagination(char *buf, const int buf_len,
				unsigned long *to_write_p, const int last_b)
{
  static int	start_b = 0, end_b = 0;
  char		*buf_p = buf, *bounds_p;
  int		len;
  
  /* if we've already reached the end then throw away all else */
  if (end_b) {
    if (very_verbose_b && buf_len > 0) {
      (void)fprintf(stderr, " trimmed %d paged end chars\n", buf_len);
    }
    *to_write_p = 0;
    return 0;
  }
  
  bounds_p = buf + buf_len;
  
  len = strlen(PAGINATION_ESC);
  
  if (! start_b) {
    if (buf_len < len + 1) {
      if (last_b) {
	(void)fprintf(stderr, "%s: ERROR.  Did not get pagination start.\n",
		      argv_program);
	exit(1);
      }
      *to_write_p = 0;
      return buf_len;
    }
    
    /* if we don't have a pagination-escapge string then move forward */
    if (memcmp(PAGINATION_ESC, buf_p, len) != 0
	&& buf_p[len] != PAGINATION_START) {
      (void)fprintf(stderr, "%s: ERROR.  Did not get pagination start.\n",
		    argv_program);
      exit(1);
    }
    
    /* shift the buffer down over the start sequence */
    memmove(buf, buf + len + 1, buf_len - (len + 1));
    bounds_p -= len + 1;
    
    if (very_verbose_b) {
      (void)fprintf(stderr, " trimmed %d bytes of starting pagination\n",
		    len + 1);
    }
    
    start_b = 1;
  }
  
  for (; buf_p + len + 1 <= bounds_p;) {
    
    /* if we don't have a pagination-escapge string then move forward */
    if (*buf_p != PAGINATION_ESC[0]
	|| memcmp(PAGINATION_ESC, buf_p, len) != 0) {
      buf_p++;
      continue;
    }
    
    /* just skip a mid sequence */
    if (buf_p[len] == 'm') {
      buf_p += len;
      /* shift down over the mid sequence */
      memmove(buf_p, buf_p + 1, bounds_p - (buf_p + 1));
      bounds_p--;
      if (very_verbose_b) {
	(void)fprintf(stderr, " trimmed 1 byte of mid pagination\n");
      }
      continue;
    }
    
    /* have we reached the end? */
    if (buf_p[len] == 'e') {
      end_b = 1;
      bounds_p = buf_p;
      if (very_verbose_b) {
	(void)fprintf(stderr, " trimmed %d bytes of end pagination\n",
		      len + 1);
      }
      break;
    }
    
    (void)fprintf(stderr, "%s: ERROR.  got an invalid pagination sequence\n",
		  argv_program);
    exit(1);
  }
  
  /*
   * We are left with a partial buffer.  If it does not contain the
   * start of a partition escape string or it's the last write then we
   * will write it out.  Otherwise, more has to be read.
   */
  len = bounds_p - buf_p;
  if (len > 0 && (memcmp(PAGINATION_ESC, buf_p, len) != 0
		  || last_b)) {
    buf_p += len;
  }
  
  if (last_b && (! end_b)) {
    (void)fprintf(stderr, "%s: ERROR.  Did not get pagination end.\n",
		  argv_program);
    exit(1);
  }
  
  *to_write_p = buf_p - buf;
  
  return bounds_p - buf;
}

int	main(int argc, char **argv)
{
  int			file_c;
  unsigned long		read_c = 0, write_c = 0, dot_c = 0;
  unsigned long		buf_len, write_max, write_size, to_write;
  int			read_n, ret, eof_b = 0;
  FILE			**streams = NULL;
  fd_set		listen_set;
  char			*buf, md5_result[MD5_SIZE];
  md5_t			md5;
  struct timeval	start, now, timeout;
  
  argv_process(args, argc, argv);
  if (very_verbose_b) {
    verbose_b = 1;
  }
  
  if (write_page_b && (! pass_b)) {
    (void)fprintf(stderr,
		  "%s: disabling write pagination since no pass data (-%c)\n",
		  argv_program, PASS_CHAR);
    write_page_b = 0;
  }
  
  /* make stdin non-blocking */
  if (non_block_b) {
    (void)fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) | O_NONBLOCK);
  }
  
  /* open output paths if needed -- we add one to accomodate stdout */
  if (outfiles.aa_entry_n == 0) {
    streams = NULL;
  }
  else {
    streams = (FILE **)malloc(sizeof(FILE *) * outfiles.aa_entry_n);
    if (streams == NULL) {
      perror("malloc");
      exit(1);
    }
    
    for (file_c = 0; file_c < outfiles.aa_entry_n; file_c++) {
      char	*path = ARGV_ARRAY_ENTRY(outfiles, char *, file_c);
      
      streams[file_c] = fopen(path, "w");
      if (streams[file_c] == NULL) {
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
  
  if (write_page_b) {
    (void)fputs(PAGINATION_ESC, stdout);
    (void)fputc(PAGINATION_START, stdout);
    if (very_verbose_b) {
      (void)fprintf(stderr, "wrote starting pagination\n");
    }
  }
  
  /* read in stuff and count the number */
  buf_len = 0;
  while (1) {
    
    if (eof_b) {
      to_write = buf_len;
    }
    else {
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
	/* we've already processed the buffer so we don't need to paginate */
	to_write = buf_len;
      }
      else {
	/* read from standard-in */
	read_n = read(STDIN_FD, buf + buf_len, buf_size - buf_len);
	if (read_n < 0) {
	  (void)fprintf(stderr, "%s: read on stdin error: %s\n",
			argv_program, strerror(errno));
	  exit(1);
	}
	else if (read_n > 0) {
	  if (very_verbose_b) {
	    (void)fprintf(stderr, "read %d bytes\n", read_n);
	  }
	  
	  read_c += read_n;
	  buf_len += read_n;
	  
	  if (read_page_b) {
	    buf_len = read_pagination(buf, buf_len, &to_write, 0);
	  }
	  else {
	    to_write = buf_len;
	  }
	}
	else {
	  /* EOF on read */
	  
	  if (read_page_b) {
	    /* we do this here so it can error because of no end tag */
	    buf_len = read_pagination(buf, buf_len, &to_write, 1);
	  }
	  else {
	    to_write = buf_len;
	  }
	  
	  /* is the buffer now empty? */
	  if (buf_len == 0) {
	    break;
	  }
	  
	  eof_b = 1;
	}
      }
    }
    
    if (throttle_size == 0) {
      write_size = to_write;
    }
    else {
      /*
       * throttle the writes by sleeping
       */
      
      /* figure out how long we have been writing */
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
      if (read_c > write_max) {
	/*
	 * Since we've read enough, we need to take our time with the
	 * write.  Just sleep for a certain amount of time and write
	 * out the throttled data.
	 */
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000000 / WRITES_PER_SEC;
	(void)select(0, NULL, NULL, NULL, &timeout);
	
	/* if we sleep for 1/X of a second so write 1/X of throttle-size */
	write_size = throttle_size / WRITES_PER_SEC;
      }
      else {
	write_size = write_max - read_c;
      }
      
      if (write_size > to_write) {
	write_size = to_write;
      }
    }
    
    /* should we write it? */
    if (write_size > 0) {
      
      if (pass_b) {
	if (write_page_b) {
	  if (eof_b && write_size == buf_len) {
	    write_size = write_pagination(buf, write_size, 1);
	  }
	  else {
	    write_size = write_pagination(buf, write_size, 0);
	  }
	}
	else {
	  (void)fwrite(buf, sizeof(char), write_size, stdout);
	}
	if (flush_out_b) {
	  (void)fflush(stdout);
	}
      }
      
      if (run_md5_b) {
	md5_process(&md5, buf, write_size);
      }
      
      /* write out to any files */
      for (file_c = 0; file_c < outfiles.aa_entry_n; file_c++) {
	if (streams[file_c] != NULL) {
	  (void)fwrite(buf, sizeof(char), write_size, streams[file_c]);
	  if (flush_out_b) {
	    (void)fflush(streams[file_c]);
	  }
	}
      }
      
      if (very_verbose_b) {
	(void)fprintf(stderr, "wrote %ld bytes\n", write_size);
      }
      
      /*
       * Print out our dots.  Because write_c might overflow and make
       * our dot-loop will go infinite, we continually reset the dot
       * sizes.
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
      if (eof_b && buf_len == 0) {
	break;
      }
    }
  }
  
  if (write_page_b) {
    (void)fputs(PAGINATION_ESC, stdout);
    (void)fputc(PAGINATION_END, stdout);
    if (very_verbose_b) {
      (void)fprintf(stderr, "wrote ending pagination\n");
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
    int		speed;
    float	secs;
    
    (void)fprintf(stderr, "%s: processed %s in %ld.%02ld secs",
		  argv_program, byte_size(read_c), now.tv_sec,
		  now.tv_usec / 10000);
    /* NOTE: this needs to be in a separate printf */
    secs = ((float)now.tv_sec + ((float)now.tv_usec / 1000000.0));
    if (secs == 0.0) {
      speed = read_c;
    }
    else {
      speed = (float)read_c / secs;
    }
    (void)fprintf(stderr, " or %s per sec\n", byte_size(speed));
  }
  
  /* close the output paths */
  for (file_c = 0; file_c < outfiles.aa_entry_n; file_c++) {
    if (streams[file_c] != NULL) {
      (void)fclose(streams[file_c]);
    }
  }
  
  /*
   * We flush the stdout before the md5 is printed to attempt to force
   * any de-archive messages before.
   */
  if (pass_b) {
    (void)fflush(stdout);
  }
  
  if (run_md5_b) {
    char	md5_string[64];
    
    md5_finish(&md5, md5_result);
    md5_sig_to_string(md5_result, md5_string, sizeof(md5_string));
    
    (void)fprintf(stderr, "%s: md5 signature of input = '%s'\n",
		  argv_program, md5_string);
  }
  
  if (streams != NULL) {
    free(streams);
  }
  free(buf);
  argv_cleanup(args);

  exit(0);
}
