/*
  tcprep.c

  Aaron Turner <aturner@pobox.com>

  Copyright (c) 2001 Aaron Turner.  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions
  are met:
  
  1. Redistributions of source code must retain the above copyright
     notice, this list of conditions and the following disclaimer.
  2. Redistributions in binary form must reproduce the above copyright
     notice, this list of conditions and the following disclaimer in the
     documentation and/or other materials provided with the distribution.
  3. All advertising materials mentioning features or use of this software
     must display the following acknowledgement:
        This product includes software developed by Aaron Turner <aturner@pobox.com>
  4. Neither the name of Aaron Turner nor the names of the contributors
     to this software may be used to endorse or promote products derived
     from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
  WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
  GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
  OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
  ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


  Purpose:
  1) Remove the performance bottleneck in tcpreplay for choosing an NIC
  2) Seperate code to make it more manageable
  3) Seperate code to make sending packets independant from choosing a NIC

  Support:
  Right now we support matching source IP based upon on of the following:
  - Regular expression
  - IP address is contained in one of a list of CIDR blocks
  - Auto learning of CIDR block for servers (clients all other)
*/


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pcap.h>
#include <libnet.h>
#include <regex.h>
#include <redblack.h>
#include <math.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#include "tcpreplay.h"
#include "tcpprep.h"
#include "tree.h"
#include "cache.h"
#include "cidr.h"

#define TCPPREP_VERSION "2.0 (adt9)"

/*
  global variables
*/
int debug = 0;
int info = 0;
char * ourregex = NULL;
char * cidr = NULL;
regex_t * preg = NULL;
CIDR * cidrdata = NULL;
CACHE * cachedata = NULL;
TREE * treedata = NULL;
extern struct rbtree * rbdata;
int mode = 0;
int automode = 0;
double ratio = 0.0;
int max_mask = DEF_MAX_MASK;
int min_mask = DEF_MIN_MASK;
int non_ip = 0;

/*
  usage()
*/
void usage() {
  fprintf(stderr, "Version: " TCPPREP_VERSION "\nUsage: tcpprep ");
#ifdef DEBUG
  fprintf(stderr, "[-d] ");
#endif
  fprintf(stderr, "[-h] [-a|-c CIDR,...|-r regex] [-n bridge|router] [-R ratio]\n               [-N client|server] [-i pcap file] [-o cache file]\n");
  exit (0);
}



/*
  checks to see if an ip address matches a regex.  Returns 1 for true
  0 for false
*/
int check_ip_regex(const unsigned long ip) {
  u_char src_ip[16];
  size_t nmatch = 0;
  regmatch_t *pmatch = NULL;
  int eflags = 0;
  
  memset (src_ip, '\0', 16);
  strncat(src_ip, libnet_host_lookup(ip, RESOLVE), 15);
  if (regexec(preg, src_ip, nmatch, pmatch, eflags)  == 0) {
    return(1);
  } else {
    return(0);
  }
  
}

/* 
   called by libnet for each packet
   we use this func to match against the regex or cidr to add to our cache
*/
void parse_packet(u_char * user, struct pcap_pkthdr * pcap_hdr, u_char * data) {
  struct libnet_ip_hdr * ip_hdr;
  struct libnet_ethernet_hdr * eth_hdr;
  
  eth_hdr = (struct libnet_ethernet_hdr *)data;

  if (ntohs(eth_hdr->ether_type) != ETHERTYPE_IP) {
#ifdef DEBUG
    if (debug)
      fprintf(stderr, "Packet isn't IP: 0x%.2x\n", eth_hdr->ether_type);
#endif
    if (mode != AUTO_MODE) /* we don't want to cache these packets twice */
      add_cache(non_ip);
    return;
  }

  ip_hdr = (struct libnet_ip_hdr *) (data + LIBNET_ETH_H);

  switch(mode) {
  case REGEX_MODE:
    add_cache(check_ip_regex(ip_hdr->ip_src.s_addr));
    break;
  case CIDR_MODE:
    add_cache(check_ip_CIDR(ip_hdr->ip_src.s_addr));
    break;
  case AUTO_MODE:
    /* first run through in auto mode: create tree */
    add_tree(ip_hdr->ip_src.s_addr, data);
    break;
  case ROUTER_MODE:
    add_cache(check_ip_CIDR(ip_hdr->ip_src.s_addr));
    break;
  case BRIDGE_MODE:
    /* second run through in auto mode: create bridge based cache */
    add_cache(check_ip_tree(ip_hdr->ip_src.s_addr));
    break;
  }
}

/*
  main()
*/
int main(int argc, char * argv[]) {
  pcap_t * in_file;
  int out_file;
  struct libnet_link_int * write_if = NULL; 
  char ebuf[EBUF_SIZE];
  char * infilename = NULL;
  char * outfilename = NULL;
  int ch;
  int regex_flags = 0;
  int regex_error = 0;
  extern char *optarg;
  u_int totpackets = 0;
  int mask_count = 0;

  debug = 0;
  ourregex = NULL;
  regex_flags |= REG_EXTENDED;
  regex_flags |= REG_NOSUB;
  
  preg = (regex_t *) malloc (sizeof(regex_t));
  if (preg == NULL) {
    fprintf(stderr, "Unable to malloc()\n");
    exit (-1);
  }
  
#ifdef DEBUG
  while ((ch = getopt(argc, argv, "ad:c:r:R:o:i:Ihm:M:n:N:")) != -1)
#else
    while ((ch = getopt(argc, argv, "ac:r:R:o:i:Ihm:M:n:N:")) != -1)
#endif
      switch(ch) {
      case 'a':
	mode = AUTO_MODE;
	break;
      case 'c':
	if (! parse_cidr(optarg)) {
	  usage();
	} 
	mode = CIDR_MODE;
	break;
      case 'd':
	debug = atoi(optarg);
	break;
      case 'h':
	usage();
	break;
      case 'i':
	infilename = optarg;
	break;
      case 'I':
	info = 1;
	break;
      case 'm':
	min_mask = atoi(optarg);
	mask_count ++;
	break;
      case 'M':
	max_mask = atoi(optarg);
	mask_count ++;
	break;
      case 'n':
	if (strcmp(optarg, "bridge") == 0) {
	  automode = BRIDGE_MODE;
	} else if (strcmp(optarg, "router") == 0) {
	  automode = ROUTER_MODE;
	} else {
	  fprintf(stderr, "Invalid network type: %s\n", optarg);
	  exit (1);
	}
	break;
      case 'N':
	if (strcmp(optarg, "client") == 0) {
	  non_ip = 0;
	} else if (strcmp(optarg, "server") == 0) {
	  non_ip = 1;
	} else {
	  fprintf(stderr, "-N must be client or server\n");
	  exit(1);
	}
	break;
      case 'o':
	outfilename = optarg;
	break;
      case 'r':
	ourregex = optarg;
	mode = REGEX_MODE;
	if ((regex_error = regcomp(preg, ourregex, regex_flags))) {
	  if (regerror(regex_error, preg, ebuf, EBUF_SIZE) != -1) {
	    fprintf(stderr, "Error compiling regex: %s\n", ebuf);
	  } else {
	    fprintf(stderr, "Error compiling regex.\n");
	  }
	  exit (1);
	}
	break;
      case 'R':
	ratio = atof(optarg);
	break;
      }
  
  /* process args */
  if ( (mode != CIDR_MODE) && (mode != REGEX_MODE) && (mode != AUTO_MODE) ) {
    fprintf(stderr, "You need to specifiy a vaild CIDR list, regex, or auto mode\n");
    exit (1);
  }

  if ((mask_count > 0) && (mode != AUTO_MODE)) {
    fprintf(stderr, "You can't specify a min/max mask length unless you use auto mode\n");
    exit (1);
  }

  if ((mode == AUTO_MODE) && (automode == 0)) {
    fprintf(stderr, "You must specify -n (bridge|router) with auto mode (-a)\n");
    exit(1);
  }

  if ((ratio != 0.0) && (mode != AUTO_MODE)) {
    fprintf(stderr, "Ratio (-R) only works in auto mode (-a).\n");
    exit (1);
  }


  if (ratio < 0) {
    fprintf(stderr, "Ratio must be a non-negative number.");
    exit (1);
  }

  if (info && mode == AUTO_MODE)
    fprintf(stderr, "Building auto mode pre-cache data structure...\n");

  if (info && mode == CIDR_MODE)
    fprintf(stderr, "Building cache file from CIDR list...\n");

  if (info && mode == REGEX_MODE)
    fprintf(stderr, "Building cache file from regex...\n");


  /* set ratio to the default if unspecified */
  if (ratio == 0.0)
    ratio = DEF_RATIO;

  /* open the cache file */
  out_file = open(outfilename, O_WRONLY|O_CREAT|O_TRUNC, S_IREAD|S_IWRITE|S_IRGRP|S_IWGRP|S_IROTH);
  if (out_file == -1) {
    fprintf(stderr, "Unable to open cache file %s for writing.\n", outfilename);
	exit (1);
  }

 readpcap:
  /* open the pcap file */
  in_file = pcap_open_offline(infilename, ebuf);
  if (!in_file) {
    fprintf(stderr, "Error opening %s for reading\n",argv[1]);
    fprintf(stderr, "in_file: %s\n",ebuf);
    exit (1);
  }

  /* process the pcap file */
  if (pcap_dispatch(in_file, 0, (void *)&parse_packet, (u_char *)write_if) == -1) {
    pcap_perror(in_file, argv[1]);
  }
  
  /* we need to process the pcap file twice in HASH/AUTO mode */
  if (mode == AUTO_MODE) {
    mode = automode;
    pcap_close(in_file);
    if (mode == ROUTER_MODE) {  /* do we need to convert TREE->CIDR? */
      fprintf(stderr, "Building network list from pre-cache...\n");
      if (!process_tree()) {
	fprintf(stderr, "Error: unable to build a valid list of servers. Aborting.\n");
	exit(1);
      }
    } else {
      /* 
	 in bridge mode we need to calculate client/sever manually 
	 since this is done automatically in process_tree() 
      */
      rbwalk(rbdata, tree_calculate, (void *)rbdata);
    }
#ifdef DEBUG
    if (debug)
      print_cidr(cidrdata);
#endif
    if (info)
      fprintf(stderr, "Buliding cache file...\n");
    goto readpcap; /* re-process files, but this time generate cache */
  }


  /* write cache data */
  totpackets = write_cache(out_file);
  if (info)
    fprintf(stderr, "Done.\nCached %u packets.\n", totpackets);
  
  /* close files */
  pcap_close(in_file);
  close(out_file);
  return 0;
  
}