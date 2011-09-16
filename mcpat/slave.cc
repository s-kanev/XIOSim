/*****************************************************************************
 *                                McPAT
 *                      SOFTWARE LICENSE AGREEMENT
 *            Copyright 2009 Hewlett-Packard Development Company, L.P.
 *                          All Rights Reserved
 *
 * Permission to use, copy, and modify this software and its documentation is
 * hereby granted only under the following terms and conditions.  Both the
 * above copyright notice and this permission notice must appear in all copies
 * of the software, derivative works or modified versions, and any portions
 * thereof, and both notices must appear in supporting documentation.
 *
 * Any User of the software ("User"), by accessing and using it, agrees to the
 * terms and conditions set forth herein, and hereby grants back to Hewlett-
 * Packard Development Company, L.P. and its affiliated companies ("HP") a
 * non-exclusive, unrestricted, royalty-free right and license to copy,
 * modify, distribute copies, create derivate works and publicly display and
 * use, any changes, modifications, enhancements or extensions made to the
 * software by User, including but not limited to those affording
 * compatibility with other hardware or software, but excluding pre-existing
 * software applications that may incorporate the software.  User further
 * agrees to use its best efforts to inform HP of any such changes,
 * modifications, enhancements or extensions.
 *
 * Correspondence should be provided to HP at:
 *
 * Director of Intellectual Property Licensing
 * Office of Strategy and Technology
 * Hewlett-Packard Company
 * 1501 Page Mill Road
 * Palo Alto, California  94304
 *
 * The software may be further distributed by User (but not offered for
 * sale or transferred for compensation) to third parties, under the
 * condition that such third parties agree to abide by the terms and
 * conditions of this license.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITH ANY AND ALL ERRORS AND DEFECTS
 * AND USER ACKNOWLEDGES THAT THE SOFTWARE MAY CONTAIN ERRORS AND DEFECTS.
 * HP DISCLAIMS ALL WARRANTIES WITH REGARD TO THE SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL
 * HP BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THE SOFTWARE.
 *
 ***************************************************************************/
#include "xmlParser.h"
#include "XML_Parse.h"
#include "processor.h"
#include "globalvar.h"

#include "mcpat.h"

static Processor *proc;
static int print_level;

using namespace std;

void mcpat_initialize(ParseXML* p1, ostream *_out_file, int _print_level) {
   print_level = _print_level;

   opt_for_clk = true;
   out_file = _out_file;

   proc = new Processor(p1);
}

void mcpat_compute_energy(bool print_power, double * cores_rtp, double * uncore_rtp) {
   int i;
   /* These are stats, but are included in the dynamic parameters structures,
      we must update them from the reparsed XML data */
   for (i=0; i<proc->numCore; i++) {
      proc->cores[i]->coredynp.pipeline_duty_cycle = proc->XML->sys.core[i].pipeline_duty_cycle;
      proc->cores[i]->coredynp.total_cycles        = proc->XML->sys.core[i].total_cycles;
      proc->cores[i]->coredynp.busy_cycles         = proc->XML->sys.core[i].busy_cycles;
      proc->cores[i]->coredynp.idle_cycles         = proc->XML->sys.core[i].idle_cycles;
      proc->cores[i]->coredynp.executionTime       = proc->XML->sys.total_cycles/proc->cores[i]->coredynp.clockRate;
   }

   for (i=0; i<proc->numL2;i++)
      proc->l2array[i]->cachep.executionTime = proc->XML->sys.total_cycles/(proc->XML->sys.target_core_clockrate*1e6);
   for (i=0; i<proc->numL1Dir;i++)
      proc->l1dirarray[i]->cachep.executionTime = proc->XML->sys.total_cycles/(proc->XML->sys.target_core_clockrate*1e6);
   for (i=0; i<proc->numL3;i++)
      proc->l3array[i]->cachep.executionTime = proc->XML->sys.total_cycles/(proc->XML->sys.target_core_clockrate*1e6);
   for (i=0; i<proc->numL2Dir;i++)
      proc->l2dirarray[i]->cachep.executionTime = proc->XML->sys.total_cycles/(proc->XML->sys.target_core_clockrate*1e6);

   /* Similarly for those components, even though we don't care about their power for now */
   //XXX: Here we call set_*_param because they are quite short and don't overwrite anything important
   proc->mc->set_mc_param();
   if (proc->flashcontroller)
      proc->flashcontroller->set_fc_param();
   if (proc->niu)
      proc->niu->set_niu_param();
   if (proc->pcie)
      proc->pcie->set_pcie_param();
   for (i=0; i<proc->numNOC; i++)
      proc->nocs[i]->set_noc_param();

   /* Finally, compute power */
   proc->compute();
   
   if (print_power)
      proc->displayEnergy(2, print_level);

   /* Set return values */
   for (i=0; i<proc->numCore; i++)
      cores_rtp[i] = proc->cores[i]->rt_power.readOp.dynamic/proc->cores[i]->coredynp.executionTime;
   *uncore_rtp = proc->rt_power.readOp.dynamic - proc->core.rt_power.readOp.dynamic;
}

void mcpat_finalize() {
   delete proc;
}
