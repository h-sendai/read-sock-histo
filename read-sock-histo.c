#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <gsl/gsl_histogram.h>
#include <gsl/gsl_errno.h>

#include "my_signal.h"
#include "my_socket.h"
#include "set_timer.h"
#include "get_num.h"

int debug = 0;
gsl_histogram *histo;
unsigned long histo_overflow = 0;
unsigned long total_bytes    = 0;
unsigned long read_count     = 0;
struct timeval start, stop;
int bufsize = 2*1024*1024;

int usage()
{
    //char msg[] = "Usage: ./read-sock-histo [-b bufsize] [-w bin_width] [-B n_bin] [-t TIMEOUT] ip_address:port x_min x_max n_bin\n"
    char msg[] = "Usage: ./read-sock-histo [-b bufsize] [-t TIMEOUT] ip_address:port x_min x_max n_bin\n"
                 "example of ip_address:port\n"
                 "remote_host:1234\n"
                 "192.168.10.16:24\n"
                 "default port: 1234\n"
                 " Options:\n"
                 "    -b bufsize: suffix k for kilo (1024), m for mega(1024*1024) (default 2MB)\n"
                 "    -t TIMEOUT: seconds.  (default: 10 seconds)\n";
    fprintf(stderr, "%s\n", msg);

    return 0;
}

void sig_int(int signo)
{
    struct timeval elapse;
    gettimeofday(&stop, NULL);
    timersub(&stop, &start, &elapse);
    fprintf(stderr, "bufsize: %.3f kB\n", bufsize/1024.0);
    fprintf(stderr, "total bytes: %ld bytes\n", total_bytes);
    fprintf(stderr, "read count : %ld\n", read_count);
    fprintf(stderr, "running %ld.%06ld sec\n", elapse.tv_sec, elapse.tv_usec);
    double elapsed_time        = elapse.tv_sec + 0.000001*elapse.tv_usec;
    double transfer_rate_MB_s  = (double)total_bytes / elapsed_time / 1024.0 / 1024.0;
    double transfer_rate_Gbps  = (double)total_bytes * 8 / elapsed_time / 1000000000.0;
    double read_bytes_per_read = (double)total_bytes / (double)read_count / 1024.0;
    fprintf(stderr, "transfer_rate: %.3f MB/s %.3f Gbps\n", transfer_rate_MB_s, transfer_rate_Gbps);
    fprintf(stderr, "read_bytes_per_read: %.3f kB/read\n", read_bytes_per_read);

    gsl_histogram_fprintf(stdout, histo, "%g", "%g");
    gsl_histogram_free(histo);
    //if (histo_overflow > 0) {
    printf("# overflow: %ld\n", histo_overflow);
    //}

    exit(0);
}

int main(int argc, char *argv[])
{
    int c;
    int period = 10; /* default run time (10 seconds) */

    while ( (c = getopt(argc, argv, "b:dht:w:B:")) != -1) {
        switch (c) {
            case 'b':
                bufsize = get_num(optarg);
                break;
            case 'h':
                usage();
                exit(0);
                break; /* NOTREACHED */
            case 'd':
                debug = 1;
                break;
            case 't':
                period = strtol(optarg, NULL, 0);
                break;
            default:
                break;
        }
    }

    argc -= optind;
    argv += optind;

    if (argc != 4) {
        usage();
        exit(1);
    }

    int port = 1234;
    char *remote_host_info = argv[0];
    char *tmp = strdup(remote_host_info);
    char *remote_host = strsep(&tmp, ":");
    if (tmp != NULL) {
        port = strtol(tmp, NULL, 0);
    }

    double x_min = (double) get_num(argv[1]);
    double x_max = (double) get_num(argv[2]);
    int n_bin    = strtol(argv[3], NULL, 0);

    my_signal(SIGINT,  sig_int);
    my_signal(SIGTERM, sig_int);
    my_signal(SIGALRM, sig_int);

    histo = gsl_histogram_alloc(n_bin);
    gsl_histogram_set_ranges_uniform(histo, x_min, x_max);

    int sockfd = tcp_socket();
    if (sockfd < 0) {
        errx(1, "tcp_socket");
    }

    if (connect_tcp(sockfd, remote_host, port) < 0) {
        errx(1, "connect_tcp");
    }

    set_timer(period, 0, period, 0);
    gettimeofday(&start, NULL);

    char *buf = malloc(bufsize);
    if (buf == NULL) {
        err(1, "malloc for buf");
    }

    for ( ; ; ) {
        int n, m;
        n = read(sockfd, buf, bufsize);
        total_bytes += n;
        read_count ++;
        m = gsl_histogram_increment(histo, n);
        if (m == GSL_EDOM) {
            histo_overflow += 1;
        }
    }
}
