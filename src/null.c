/*
 * /dev/null program
 *
 * Copyright 2020 by Gray Watson
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>

#include "conf.h"

#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <sys/time.h>

#include "argv.h"
#include "compat.h"
#include "md5.h"
#include "version.h"

#define BUFFER_SIZE	100000		/* size of buffer */
#define WRITES_PER_SEC	10		/* throttle to X writes/sec.  X > 1. */
#define PASS_CHAR	'p'		/* pass - argument */
#define STDIN_FD	0		/* stdin file descriptor */
#define BYTE_SIZE_BUF_LEN 80		/* length of the byte-size buffer */

#define PAGINATION_ESC	"null-page-"	/* special pagination string */
#define PAGINATION_START	's'	/* start character */
#define PAGINATION_MID		'm'	/* mid character */
#define PAGINATION_END		'e'	/* end character */

/* argument vars */
static	int		read_all_b = ARGV_FALSE; /* read input in before out */
static	unsigned long	buf_size = BUFFER_SIZE;	/* size of i/o buffer */
static	unsigned long	dot_size = 0;		/* show a dot every X */
static	int		flush_out_b = ARGV_FALSE; /* flush output to files */
static	int		help_b = ARGV_FALSE;	/* get help */
static	int		run_md5_b = ARGV_FALSE;	/* run md5 on data */
static	int		non_block_b = ARGV_FALSE; /* don't block on input */
static	int		pass_b = ARGV_FALSE;	/* pass data through */
static	float		rate_every_secs = 0.0;	/* rate every X decimal secs */
static	int		read_page_b = 0;	/* read pagination info */
static	unsigned long	stop_after = 0;		/* stop after X bytes */
static	unsigned long	throttle_size = 0;	/* throttle bytes/second */
static	int		verbose_b = ARGV_FALSE;	/* verbose flag */
static	int		very_verbose_b = ARGV_FALSE; /* very-verbose flag */
static	int		write_page_b = 0;	/* output pagination info */
static	char		*input_path = NULL;	/* input file we are reading */
static	argv_array_t	outfiles;		/* outfiles for read data */

static	argv_t	args[] = {
  { 'a',	"all-read",	ARGV_BOOL_INT,			&read_all_b,
    NULL,			"read all input before outputting" },
  { 'b',	"buffer-size",	ARGV_U_SIZE,			&buf_size,
    "size",			"size of input and output buffer" },
  { 'd',	"dot-blocks",	ARGV_U_SIZE,			&dot_size,
    "size",			"show a dot each X bytes of input" },
  { 'f',	"output-file",	ARGV_CHAR_P | ARGV_FLAG_ARRAY,	&outfiles,
    "output-file",		"output file(s) to write input" },
  { 'F',	"flush-output",	ARGV_BOOL_INT,			&flush_out_b,
    NULL,			"flush output to files" },
  { 'h',	"help",		ARGV_BOOL_INT,			&help_b,
    NULL,			"display help string" },
  { 'm',	"md5",		ARGV_BOOL_INT,			&run_md5_b,
    NULL,			"run input bytes through md5" },
  { 'n',	"non-block",	ARGV_BOOL_INT,			&non_block_b,
    NULL,			"don't block on input" },
  { PASS_CHAR,	"pass-input",	ARGV_BOOL_INT,			&pass_b,
    NULL,			"write input to standard output" },
  { 'r',	"read-pagination", ARGV_BOOL_INT,		&read_page_b,
    NULL,			"read pagination data (use with -w)" },
  { 'R',	"rate-every",	ARGV_FLOAT,			&rate_every_secs,
    "seconds",			"dump rate info every X decimal secs" },
  { 's',	"stop-after",	ARGV_U_SIZE,			&stop_after,
    "size",			"stop after size bytes" },
  { 't',	"throttle-size", ARGV_U_SIZE,			&throttle_size,
    "size",			"throttle output to X bytes / sec" },
  { 'v',	"verbose",	ARGV_BOOL_INT,			&verbose_b,
    NULL,			"report on i/o bytes" },
  { 'V',	"very-verbose",	ARGV_BOOL_INT,		       &very_verbose_b,
    NULL,			"very verbose messages" },
  { 'w',	"write-pagination", ARGV_BOOL_INT,		&write_page_b,
    NULL,			"write paginate data (use with -r)" },
  { ARGV_MAYBE,	"input-file",	ARGV_CHAR_P,			&input_path,
    "file",			"file we are reading else stdin" },
  { ARGV_LAST, NULL, 0, NULL, NULL, NULL }
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
 * buf -> Buffer or null to use the global-one.
 */
static	char	*byte_size(const unsigned long size, char *buf, int buf_size)
{
  static char	global_buf[BYTE_SIZE_BUF_LEN];
  
  if (buf == NULL) {
    buf = global_buf;
    buf_size = sizeof(global_buf);
  }
  if (size > 1024L * 1024L * 1024L) {
    loc_snprintf(buf, buf_size, "%.1fg (%ld)",
		 (float)(size) / (float)(1024L * 1024L * 1024L), size);
  }
  else if (size > 1024L * 1024L) {
    loc_snprintf(buf, buf_size, "%.1fm (%ld)",
		 (float)(size) / (float)(1024 * 1024), size);
  }
  else if (size > 1024L) {
    loc_snprintf(buf, buf_size, "%.1fk (%ld)", (float)size / (float)1024.0, size);
  }
  else {
    loc_snprintf(buf, buf_size, "%ldb", size);
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
  const char	*buf_p, *write_p = buf;
  
  int len = strlen(PAGINATION_ESC);
  
  const char *bounds_p = buf + buf_len;
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
    unsigned int write_len = buf_p - write_p;
    if (fwrite(write_p, sizeof(char), write_len, stdout) != write_len) {
      (void)fprintf(stderr,
		    "%s: ERROR.  Could not write full pagination block.\n",
		    argv_program);
      exit(1);
    }
    if (very_verbose_b) {
      (void)fprintf(stderr, " wrote %d paged chars\n", write_len);
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
    if (fwrite(write_p, sizeof(char), len, stdout) != (size_t)len) {
      (void)fprintf(stderr,
		    "%s: ERROR.  Could not write partial pagination block.\n",
		    argv_program);
      exit(1);
    }
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
  char		*buf_p = buf;
  
  /* if we've already reached the end then throw away all else */
  if (end_b) {
    if (very_verbose_b && buf_len > 0) {
      (void)fprintf(stderr, " trimmed %d paged end chars\n", buf_len);
    }
    *to_write_p = 0;
    return 0;
  }
  
  char *bounds_p = buf + buf_len;
  
  int len = strlen(PAGINATION_ESC);
  
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

/*
 * Add one timeval into another
 */
static	void	timeval_add(struct timeval *from_p, struct timeval *to_p) {
  to_p->tv_usec += from_p->tv_usec;
  if (to_p->tv_usec > 1000000) {
    to_p->tv_sec++;
    to_p->tv_usec -= 1000000;
  }
  to_p->tv_sec += from_p->tv_sec;
}

/*
 * Add one timeval into another
 */
static	void	timeval_subtract(struct timeval *from_p, struct timeval *to_p) {
  to_p->tv_sec -= from_p->tv_sec;
  if (to_p->tv_usec < from_p->tv_usec) {
    to_p->tv_sec--;
    to_p->tv_usec += 1000000;
  }
  to_p->tv_usec -= from_p->tv_usec;
}

/*
 * Add one timeval into another
 */
static	int	check_timeval_after(struct timeval *t1_p, struct timeval *t2_p) {
  if (t1_p->tv_sec > t2_p->tv_sec) {
    return 1;
  } else if (t1_p->tv_sec < t2_p->tv_sec) {
    return 0;
  } else if (t1_p->tv_usec > t2_p->tv_usec) {
    return 1;
  } else {
    return 0;
  }
}

int	main(int argc, char **argv)
{
  unsigned long long	write_bytes_c = 0, last_write_c = 0;
  unsigned long		read_c = 0;
  unsigned long		to_write, min_write = 0;
  unsigned long		write_size, write_c = 0;
  int			eof_b = 0, open_out_b = 1;
  FILE			**streams = NULL;
  struct timeval	next_rate, rate_every;

  argv_help_string = "Null utility.  Also try --usage.";
  argv_version_string = NULL_VERSION_STRING;
  
  argv_process(args, argc, argv);
  if (very_verbose_b) {
    verbose_b = 1;
  }
  
  if (rate_every_secs > 0.0) {
    rate_every.tv_sec = (int)rate_every_secs;
    rate_every.tv_usec = ((float)rate_every_secs - (float)rate_every.tv_sec) * 1000000.0;
    gettimeofday(&next_rate, NULL);
    timeval_add(&rate_every, &next_rate);
  }

  if (help_b) {
    (void)fprintf(stderr, "Null Utility: http://256.com/sources/null/\n");
    (void)fprintf(stderr,
                  "  This utility combines the functionality of /dev/null,\n");
    (void)fprintf(stderr,
                  "  tee, and md5sum with additional features.\n");
    (void)fprintf(stderr,
		  "  For a list of command-line options enter: %s --usage\n",
                  argv_argv[0]);
    exit(0);
  }
  
  if (write_page_b && (! pass_b)) {
    (void)fprintf(stderr,
		  "%s: disabling write pagination since pass data flag (-%c) "
		  "disabled\n",
		  argv_program, PASS_CHAR);
    write_page_b = 0;
  }

  int input_fd;
  if (input_path == NULL) {
    input_fd = STDIN_FD;
  }
  else {
    input_fd = open(input_path, O_RDONLY, 0);
    if (input_fd < 0) {
      (void)fprintf(stderr, "%s: cannot open(%s): %s\n", 
		    argv_program, input_path, strerror(errno));
      exit(1);
    }
  }
  
  /* make stdin non-blocking */
  if (non_block_b) {
    (void)fcntl(input_fd, F_SETFL, fcntl(input_fd, F_GETFL, 0) | O_NONBLOCK);
  }
  
  /* open output paths if needed -- we add one to accomodate stdout */
  if (outfiles.aa_entry_n == 0) {
    streams = NULL;
  }
  else {
    streams = (FILE **)calloc(outfiles.aa_entry_n, sizeof(FILE *));
    if (streams == NULL) {
      perror("malloc");
      exit(1);
    }
  }
  
  char *buf = (char *)malloc(buf_size);
  if (buf == NULL) {
    (void)fprintf(stderr, "could not allocate %ld bytes for buffer\n",
		  buf_size);
    exit(1);
  }
  
  md5_t md5;
  if (run_md5_b) {
    md5_init(&md5);
  }
  
  struct timeval start;
  gettimeofday(&start, NULL);
  
  if (write_page_b) {
    (void)fputs(PAGINATION_ESC, stdout);
    (void)fputc(PAGINATION_START, stdout);
    if (very_verbose_b) {
      (void)fprintf(stderr, "wrote starting pagination\n");
    }
  }
  
  if (throttle_size > 0) {
    /* if we sleep for 1/X of a second so write 1/X of throttle-size */
    min_write = throttle_size / WRITES_PER_SEC;
  }
  
  /* read in stuff and count the number */
  unsigned long buf_len = 0;
  fd_set listen_set;
  while (1) {
    
    if (eof_b) {
      to_write = buf_len;
    }
    else {
      if (non_block_b) {
	FD_ZERO(&listen_set);
	FD_SET(input_fd, &listen_set);
	int ret = select(1, &listen_set, NULL, NULL, NULL);
	if (ret == -1) {
	  (void)fprintf(stderr, "%s: select error: %s\n",
			argv_program, strerror(errno));
	  exit(1);
	}
	if (! FD_ISSET(input_fd, &listen_set)) {
	  continue;
	}
      }
      
      /* read in data from input stream */
      if (buf_size <= buf_len) {
	/* we've already processed the buffer so we don't need to paginate */
	to_write = buf_len;
      }
      else {
	unsigned long read_size = buf_size - buf_len;
	if (stop_after > 0 && stop_after - read_c < read_size) {
	  read_size = stop_after - read_c;
	}
	
	/* read from standard-in */
	int read_n = read(input_fd, buf + buf_len, read_size);
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
	  
	  /* are we stopping after X bytes */
	  if (stop_after > 0 && read_c >= stop_after) {
	    /* force the eof to be on */
	    eof_b = 1;
	  }
	  
	  if (read_page_b) {
	    buf_len = read_pagination(buf, buf_len, &to_write, 0);
	  }
	  else {
	    to_write = buf_len;
	  }
	  
	  /*
	   * if we are reading all of the input before we output then
	   * we'll have to grow the input buffer
	   */
	  if (read_all_b) {
	    if (buf_len == buf_size) {
	      /* grow our input buffer */
	      buf = realloc(buf, buf_size + buf_len);
	      if (buf == NULL) {
		(void)fprintf(stderr,
			      "could not reallocate %ld bytes for buffer\n",
			      buf_size + buf_len);
		exit(1);
	      }
	      buf_size += buf_len;
	    }
	    /* we'll write when we reach the EOF */
	    to_write = 0;
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
      struct timeval now;
      gettimeofday(&now, NULL);
      timeval_subtract(&start, &now);
      
      /* really it should be 0 but we should write something */
      if (now.tv_sec == 0 && now.tv_usec == 0) {
	now.tv_usec = 100000;
      }
      
      /* calculate what we should have written by now */
      unsigned long write_max = (float)throttle_size *
	((float)now.tv_sec + ((float)now.tv_usec / 1000000.0));
      
      /* figure out what we need to write to get the rate correct */
      if (write_max > read_c) {
	/* we need to read more */
	write_size = write_max - read_c;
      }
      else {
	/* we've read enough or are ahead so sleep */
	write_size = 0;
      }
      
      /*
       * If we are about to write some small amount then sleep and
       * write a larger amount
       */
      if (write_size < min_write) {
	/*
	 * Since we've read enough, we need to take our time with the
	 * write.  Just sleep for a certain amount of time and write
	 * out the throttled data.
	 */
	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 1000000 / WRITES_PER_SEC;
	(void)select(0, NULL, NULL, NULL, &timeout);
	
	/* write our minimal chunk */
	write_size = min_write;
      }
      
      /* make sure that we have enough in our buffer to write */
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
	  if (fwrite(buf, sizeof(char), write_size, stdout) != write_size) {
	    (void)fprintf(stderr,
			  "%s: ERROR.  Could not pass block to stdout.\n",
			  argv_program);
	    exit(1);
	  }
	}
	if (flush_out_b) {
	  (void)fflush(stdout);
	}
      }
      
      if (run_md5_b) {
	md5_process(&md5, buf, write_size);
      }
      
      /* write out to any files */
      int file_c;
      for (file_c = 0; file_c < outfiles.aa_entry_n; file_c++) {
	if (open_out_b) {
	  char	*path = ARGV_ARRAY_ENTRY(outfiles, char *, file_c);
	  
	  streams[file_c] = fopen(path, "w");
	  if (streams[file_c] == NULL) {
	    (void)fprintf(stderr, "%s: cannot fopen(%s): %s\n", 
			  argv_program, path, strerror(errno));
	  }
	}
	
	if (streams[file_c] != NULL) {
	  if (fwrite(buf, sizeof(char), write_size,
		     streams[file_c]) != write_size) {
	    (void)fprintf(stderr,
			  "%s: ERROR.  Could not write block to file.\n",
			  argv_program);
	    exit(1);
	  }
	  if (flush_out_b) {
	    (void)fflush(streams[file_c]);
	  }
	}
      }
      open_out_b = 0;
      
      if (very_verbose_b) {
	(void)fprintf(stderr, "wrote %ld bytes\n", write_size);
      }
      
      /* count the bytes */
      write_bytes_c += write_size;
      write_c++;
      
      /*
       * Print out our dots.  Because write_bytes_c might overflow and
       * make our dot-loop will go infinite, we continually reset the
       * dot sizes.
       */
      if (dot_size > 0) {
	while (write_bytes_c > dot_size) {
	  (void)fputc('.', stderr);
	  write_bytes_c -= dot_size;
	}
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
    
    if (rate_every_secs > 0.0) {
      struct timeval now;
      gettimeofday(&now, NULL);
      if (check_timeval_after(&now, &next_rate)) {
	float sec_diff = (float)(now.tv_sec - next_rate.tv_sec) + (float)(now.tv_usec - next_rate.tv_usec) / 1000000.0;
	unsigned long long diff = (float)(write_bytes_c - last_write_c) / sec_diff;
	char buf2[BYTE_SIZE_BUF_LEN];
	(void)fprintf(stderr, "\rWriting at %s per sec (total %s)      ",
		      byte_size(diff, NULL, 0), byte_size(write_bytes_c, buf2, sizeof(buf2)));
	next_rate = now;
	timeval_add(&rate_every, &next_rate);
	last_write_c = write_bytes_c;
      }
    }
  }
  
  if (rate_every_secs > 0.0) {
    (void)fputc('\n', stderr);
  }
  
  if (write_page_b) {
    (void)fputs(PAGINATION_ESC, stdout);
    (void)fputc(PAGINATION_END, stdout);
    if (very_verbose_b) {
      (void)fprintf(stderr, "wrote ending pagination\n");
    }
  }
  
  struct timeval now;
  gettimeofday(&now, NULL);
  timeval_subtract(&start, &now);
  
  if (dot_size > 0) {
    (void)fputc('\n', stderr);
  }
  
  /* write some report info */
  if (verbose_b) {
    long msecs = now.tv_usec / 1000;
    (void)fprintf(stderr, "%s: processed %s in %ld.%03ld secs",
		  argv_program, byte_size(read_c, NULL, 0), now.tv_sec, msecs);
    /* NOTE: this needs to be in a separate printf */
    float secs = ((float)now.tv_sec + ((float)now.tv_usec / 1000000.0));
    int speed;
    if (secs == 0.0) {
      speed = read_c;
    }
    else {
      speed = (float)read_c / secs;
    }
    (void)fprintf(stderr, " or %s per sec\n", byte_size(speed, NULL, 0));
  }
  
  /* close the output paths */
  int file_c;
  for (file_c = 0; file_c < outfiles.aa_entry_n; file_c++) {
    if (streams[file_c] != NULL) {
      (void)fclose(streams[file_c]);
    }
  }
  
  /* close the input file if not stdin */
  if (input_fd != STDIN_FD) {
    (void)close(input_fd);
  }
  
  /*
   * We flush the stdout before the md5 is printed to attempt to force
   * any de-archive messages before.
   */
  if (pass_b) {
    (void)fflush(stdout);
  }
  
  if (run_md5_b) {
    char md5_result[MD5_SIZE];
    md5_finish(&md5, md5_result);
    char md5_string[64];
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
