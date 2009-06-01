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




/* Initializes interval size, output directory and output name, 
   as well as the basic block hash table */
void init_bb_tracker (char* m_dir_name, char* m_out_name, long m_interval_size);
 

/* Called at each CTRL op, marking the end of a basic block.  The pc of the last 
   instruction indexes into the basic block hash, and the counter is inceremented 
   by the number of instructions in the basic block. */
void bb_tracker (long m_pc, int m_num_inst);


/* basic block element */ 
typedef struct node {
  int count;
  int bb_id;
  long pc;
  struct node * next;
} bb_node;
