 /***********************************************************************
 *  __________________________________________________________________
 * 
 *              _____ _           ____        _       __ 
 *             / ___/(_)___ ___  / __ \____  (_)___  / /_
 *             \__ \/ / __ `__ \/ /_/ / __ \/ / __ \/ __/
 *            ___/ / / / / / / / ____/ /_/ / / / / / /_  
 *           /____/_/_/ /_/ /_/_/    \____/_/_/ /_/\__/  
 * 
 *  __________________________________________________________________
 * 
 * This file is part of the SimPoint Toolkit written by Greg Hamerly, 
 * Erez Perelman, Tim Sherwood, and Brad Calder as part of Efficient
 * Simulation Project at UCSD.  If you find this toolkit useful please 
 * cite the following paper published at ASPLOS 2002.
 *
 *  Timothy Sherwood, Erez Perelman, Greg Hamerly and Brad Calder,
 *  Automatically Characterizing Large Scale Program Behavior , In the
 *  10th International Conference on Architectural Support for Programming
 *  Languages and Operating Systems, October 2002.
 *
 * Contact info:
 *        Brad Calder <calder@cs.ucsd.edu>, (858) 822 - 1619
 *        Tim Sherwood <sherwood@cs.usd.edu>,
 *        Erez Perelman <eperelma@cs.ucsd.edu>,
 *        Greg Hamerly <ghamerly@cs.ucsd.edu>,
 *
 *        University of California, San Diego 
 *        Department of Computer Science and Engineering 
 *        9500 Gilman Drive, Dept 0114
 *        La Jolla CA 92093-0114 USA
 *
 *
 * Copyright 2001, 2002 The Regents of the University of California
 * All Rights Reserved
 *
 * Permission to use, copy, modify and distribute any part of this
 * SimPoint Toolkit for educational and non-profit purposes,
 * without fee, and without a written agreement is hereby granted,
 * provided that the above copyright notice, this paragraph and the
 * following five paragraphs appear in all copies.
 *
 * Those desiring to incorporate this SimPoint Toolkit into commercial
 * products or use for commercial purposes should contact the Technology
 * Transfer Office, University of California, San Diego, 9500 Gilman
 * Drive, La Jolla, CA 92093-0910, Ph: (619) 534-5815, FAX: (619)
 * 534-7345.
 *
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY
 * FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES,
 * INCLUDING LOST PROFITS, ARISING OUT OF THE USE OF THE SimPoint
 * Toolkit, EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * THE SimPoint Toolkit PROVIDED HEREIN IS ON AN "AS IS" BASIS, AND THE
 * UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO PROVIDE MAINTENANCE,
 * SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS. THE UNIVERSITY OF
 * CALIFORNIA MAKES NO REPRESENTATIONS AND EXTENDS NO WARRANTIES OF ANY
 * KIND, EITHER IMPLIED OR EXPRESS, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR
 * PURPOSE, OR THAT THE USE OF THE SimPoint Toolkit WILL NOT INFRINGE ANY
 * PATENT, TRADEMARK OR OTHER RIGHTS.
 * 
 * No nonprofit user may place any restrictions on the use of this
 * software, including as modified by the user, by any other authorized
 * user.
 *
 ************************************************************************/



#include "stdlib.h"
#include "stdio.h"
#include "malloc.h"
#include "bbtracker.h"

/* Size of basic block hash table. Should be increased for very 
   large programs (greater than 1 million basic blocks) */
#define bb_size 100000

typedef  bb_node * bb_node_ptr;
bb_node_ptr bb_hash[bb_size];

int bb_id_pool = bb_size;
int bb_id = 0;

FILE* bbtrace;
char finalname[450];
char *outdir;                                                                
char *outfile;                                                               

int interval_size; 
int first_interval = 1;
int interval_sum=0; 

long long dyn_inst=0;
long long total_inst = 0;
long long total_calls = 0;


void init_bb_tracker (char* dir_name, char* out_name, long m_interval_size)
{
  int i;

  outdir = dir_name;
  outfile = out_name;
  interval_size = m_interval_size;

  /* initialize hash ptr table */
  for (i=0; i<bb_size; i++)
    bb_hash[i] = NULL;

}


bb_node_ptr create_bb_node (long pc, int num_inst)
{
  bb_node_ptr temp;

  temp = (bb_node_ptr) calloc(1,sizeof(bb_node));
  
  if (temp == NULL) {
    fprintf(stderr,"OUT OF MEMORY\n");
    exit(1);
  }

  temp->pc = pc;
  temp->bb_id = bb_id++;
  temp->count = num_inst;
  temp->next = NULL;

  return temp;
}


void append_bb_node (bb_node_ptr m_bb_node, bb_node_ptr head)
{
  /* Append assumes head is non null. */

  while (head->next != NULL) 
    head = head->next;                                              

  head->next = m_bb_node;
}


/* Search for bb_node with pc, if found return 1, otherwise 
   return 0 */
int find_bb_node (bb_node_ptr head, long pc, int num_inst)
{

  if (head != NULL) {
    while ((head->next != NULL) && (head->pc!=pc)) {
      head = head->next;
    }
    
    if ((head != NULL) && (head->pc == pc)) {
      head->count += num_inst;
      return 1;
    }
  }
  return 0;
} 

void print_list (bb_node_ptr head, int array[]) 
{
  do {
    array[head->bb_id] = head->count;

    /* clear stats */
    head->count = 0;
    head = head->next;
  } while(head != NULL);
}

void print_bb_hash (bb_node_ptr hash[])
{
  int i;
  int bb_array[bb_id];

  /* initialize array for sorting bb according to bb_id */
  for(i=0; i<bb_id; i++) {
    bb_array[i] = 0;
  }

  for(i=0; i<bb_size; i++) {
    if (hash[i] != NULL)
      print_list(hash[i], bb_array);
  }

  if (first_interval) {
    first_interval = 0;
    sprintf( finalname, "gzip -c > %s/%s.bb.gz", outdir, outfile );
    bbtrace = popen(finalname,"w");
  }

  fprintf(bbtrace,"T");
 
  for(i=0; i<bb_id; i++) { 
    if(bb_array[i] > 0) {
      fprintf( bbtrace, ":%i:%i   ", i+1, bb_array[i]);
    }
  }

  fprintf( bbtrace, "\n");
  fflush( bbtrace );
}


void bb_tracker(long pc, int num_inst)
{
  /* key into bb-hash based on pc of last inst in bb*/
  int bb_key = (pc>>2)%bb_size;

  /* Increment bb with the number of instructions it contains */
  if (!find_bb_node(bb_hash[bb_key], pc, num_inst)) {

    /* new bb, need to create node for it */
    bb_node_ptr temp = create_bb_node(pc, num_inst);

    if (bb_hash[bb_key] == NULL) {                                           
      bb_hash[bb_key] = temp;                                                
    } else {
      append_bb_node(temp, bb_hash[bb_key]);                                     
    } 
  }

  dyn_inst += num_inst;
  total_inst += num_inst;

  /* if reached end of interval, dump stats and decrement counter */
  if (dyn_inst > interval_size) {
    dyn_inst -= interval_size;
    print_bb_hash(bb_hash);
  }
}
