/* alloc-STM.cpp - Simple(r) Timing Model */
/*
 * __COPYRIGHT__ GT
 */


#ifdef ZESTO_PARSE_ARGS
  if(!strcasecmp(alloc_opt_string,"STM"))
    return new core_alloc_STM_t(core);
#else

class core_alloc_STM_t:public core_alloc_t
{
  enum alloc_stall_t {ASTALL_NONE,   /* no stall */
                      ASTALL_EMPTY,
                      ASTALL_ROB,
                      ASTALL_LDQ,
                      ASTALL_STQ,
                      ASTALL_RS,
                      ASTALL_DRAIN,
                      ASTALL_num
                     };

  public:

  core_alloc_STM_t(struct core_t * const core);
  virtual void reg_stats(struct stat_sdb_t * const sdb);

  virtual void step(void);
  virtual void recover(void);
  virtual void recover(const struct Mop_t * const Mop);

  virtual void RS_deallocate(const struct uop_t * const uop);
  virtual void start_drain(void); /* prevent allocation from proceeding to exec */

  protected:

  /* for load-balancing port binding */
  int * port_loading;

  static const char *alloc_stall_str[ASTALL_num];
};

const char *core_alloc_STM_t::alloc_stall_str[ASTALL_num] = {
  "no stall                   ",
  "no uops to allocate        ",
  "ROB is full                ",
  "LDQ is full                ",
  "STQ is full                ",
  "RS is full                 ",
  "OOO core draining          "
};

/*******************/
/* SETUP FUNCTIONS */
/*******************/

core_alloc_STM_t::core_alloc_STM_t(struct core_t * const arg_core)
{
  struct core_knobs_t * knobs = arg_core->knobs;
  core = arg_core;

  port_loading = (int*) calloc(knobs->exec.num_exec_ports,sizeof(*port_loading));
  if(!port_loading)
    fatal("couldn't calloc allocation port-loading scoreboard");
}

void
core_alloc_STM_t::reg_stats(struct stat_sdb_t * const sdb)
{
  char buf[1024];
  char buf2[1024];
  struct thread_t * arch = core->current_thread;

  stat_reg_note(sdb,"#### ALLOC STATS ####");
  sprintf(buf,"c%d.alloc_insn",arch->id);
  stat_reg_counter(sdb, true, buf, "total number of instructions alloced", &core->stat.alloc_insn, core->stat.alloc_insn, NULL);
  sprintf(buf,"c%d.alloc_uops",arch->id);
  stat_reg_counter(sdb, true, buf, "total number of uops alloced", &core->stat.alloc_uops, core->stat.alloc_uops, NULL);
  sprintf(buf,"c%d.alloc_IPC",arch->id);
  sprintf(buf2,"c%d.alloc_insn/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "IPC at alloc", buf2, NULL);
  sprintf(buf,"c%d.alloc_uPC",arch->id);
  sprintf(buf2,"c%d.alloc_uops/c%d.sim_cycle",arch->id,arch->id);
  stat_reg_formula(sdb, true, buf, "uPC at alloc", buf2, NULL);

  sprintf(buf,"c%d.alloc_stall",core->current_thread->id);
  core->stat.alloc_stall = stat_reg_dist(sdb, buf,
                                          "breakdown of stalls at alloc",
                                          /* initial value */0,
                                          /* array size */ASTALL_num,
                                          /* bucket size */1,
                                          /* print format */(PF_COUNT|PF_PDF),
                                          /* format */NULL,
                                          /* index map */alloc_stall_str,
                                          /* print fn */NULL);
}


/************************/
/* MAIN ALLOC FUNCTIONS */
/************************/

void core_alloc_STM_t::step(void)
{
  struct core_knobs_t * knobs = core->knobs;
  enum alloc_stall_t stall_reason = ASTALL_NONE;

  /*========================================================================*/
  /*== Dispatch insts if ROB, RS, and LQ/SQ entries available (as needed) ==*/
  if(core->decode->uop_available()) /* are there uops in the decode stage? */
  {
    /* if using drain flush: */
    /* is the back-end still draining from a misprediction recovery? */
    if(knobs->alloc.drain_flush && drain_in_progress)
    {
      if(!core->commit->ROB_empty())
      {
        stall_reason = ASTALL_DRAIN;
        ZESTO_STAT(stat_add_sample(core->stat.alloc_stall, (int)stall_reason);)
        return;
      }
      else
        drain_in_progress = false;
    }

    for(int i=0; i < knobs->alloc.width; i++) /* if so, scan all slots (width) of this stage */
    {
      struct uop_t * uop = core->decode->uop_peek();

      zesto_assert(uop->timing.when_allocated == TICK_T_MAX,(void)0);

      /* is the ROB full? */
      if(!core->commit->ROB_available())
      {
        stall_reason = ASTALL_ROB;
        break;
      }

      /* for loads, is the LDQ full? */
      if(uop->decode.is_load && !core->exec->LDQ_available())
      {
        stall_reason = ASTALL_LDQ;
        break;
      }
      /* for stores, allocate STQ entry on STA.  NOTE: This is different from 
         Bob Colwell's description in Shen&Lipasti Chap 7 where he describes
         allocation on STD.  We emit STA uops first since the oracle needs to
         use the STA result to feed the following STD uop. */
      if(uop->decode.is_sta && !core->exec->STQ_available())
      {
        stall_reason = ASTALL_STQ;
        break;
      }

      /* is the RS full? -- don't need to alloc for NOP's */
      if(!uop->decode.is_nop && !core->exec->RS_available())
      {
        stall_reason = ASTALL_RS;
        break;
      }

      /* ALL ALLOC STALL CONDITIONS PASSED */
      core->decode->uop_consume();

      /* place in ROB */
      core->commit->ROB_insert(uop);

      /* place in LDQ/STQ if needed */
      if(uop->decode.is_load)
        core->exec->LDQ_insert(uop);
      else if(uop->decode.is_sta)
        core->exec->STQ_insert_sta(uop);
      else if(uop->decode.is_std)
        core->exec->STQ_insert_std(uop);

      /* all store uops had better be marked is_std */
      zesto_assert((!(uop->decode.opflags & F_STORE)) || uop->decode.is_std,(void)0);
      zesto_assert((!(uop->decode.opflags & F_LOAD)) || uop->decode.is_load,(void)0);

      /* port bindings */
      if(!uop->decode.is_nop && !uop->Mop->decode.is_trap)
      {
        /* port-binding is trivial when there's only one valid port */
        if(knobs->exec.port_binding[uop->decode.FU_class].num_FUs == 1)
        {
          uop->alloc.port_assignment = knobs->exec.port_binding[uop->decode.FU_class].ports[0];
        }
        else /* else assign uop to least loaded port */
        {
          int min_load = INT_MAX;
          int index = -1;
          for(int j=0;j<knobs->exec.port_binding[uop->decode.FU_class].num_FUs;j++)
          {
            int port = knobs->exec.port_binding[uop->decode.FU_class].ports[j];
            if(port_loading[port] < min_load)
            {
              min_load = port_loading[port];
              index = port;
            }
          }
          uop->alloc.port_assignment = index;
        }
        port_loading[uop->alloc.port_assignment]++;

        core->exec->RS_insert(uop);

        /* Get input mappings - this is a proxy for explicit register numbers, which
           you can always get from idep_uop->alloc.ROB_index */
        for(int j=0;j<MAX_IDEPS;j++)
        {
          /* This use of oracle info is valid: at this point the processor would be
             looking up this information in the RAT, but this saves us having to
             explicitly store/track the RAT state. */
          uop->exec.idep_uop[j] = uop->oracle.idep_uop[j];

          /* Add self onto parent's output list.  This output list doesn't
             have a real microarchitectural counter part, but it makes the
             simulation faster by not having to perform a whole mess of
             associative searches each time any sort of broadcast is needed.
             The parent's odep list only points to uops which have dispatched
             into the OOO core (i.e. has left the alloc pipe). */
          if(uop->exec.idep_uop[j])
          {
            struct odep_t * odep = core->get_odep_link();
            odep->next = uop->exec.idep_uop[j]->exec.odep_uop;
            uop->exec.idep_uop[j]->exec.odep_uop = odep;
            odep->uop = uop;
            odep->aflags = (uop->decode.idep_name[j] == DCREG(MD_REG_AFLAGS));
            odep->op_num = j;
          }
        }

        /* check "scoreboard" for operand readiness (we're not actually
           explicitly implementing a scoreboard); if value is ready, read
           it into data-capture window or payload RAM. */
        tick_t when_ready = 0;
        for(int j=0;j<MAX_IDEPS;j++) /* for possible input argument */
        {
          if(uop->exec.idep_uop[j]) /* if the parent uop exists (i.e., still in the processor) */
          {
            uop->timing.when_ival_ready[j] = uop->exec.idep_uop[j]->timing.when_otag_ready;
            if(uop->exec.idep_uop[j]->exec.ovalue_valid)
            {
              uop->timing.when_ival_ready[j] = uop->exec.idep_uop[j]->timing.when_completed;
              uop->exec.ivalue_valid[j] = true;
              if(uop->decode.idep_name[j] == DCREG(MD_REG_AFLAGS))
                uop->exec.ivalue[j].dw = uop->exec.idep_uop[j]->exec.oflags;
              else
                uop->exec.ivalue[j] = uop->exec.idep_uop[j]->exec.ovalue;
            }
          }
          else /* read from ARF */
          {
            uop->timing.when_ival_ready[j] = sim_cycle;
            uop->exec.ivalue_valid[j] = true; /* applies to invalid (DNA) inputs as well */
            if(uop->decode.idep_name[j] != DNA)
              uop->exec.ivalue[j] = uop->oracle.ivalue[j]; /* oracle value == architected value */
          }
          if(when_ready < uop->timing.when_ival_ready[j])
            when_ready = uop->timing.when_ival_ready[j];
        }
        uop->timing.when_ready = when_ready;
        if(when_ready < TICK_T_MAX) /* add to readyQ if appropriate */
          core->exec->insert_ready_uop(uop);
      }
      else /* is_nop || is_trap */
      {
        /* NOP's don't go through exec pipeline; they go straight to the
           ROB and are immediately marked as completed (they still take
           up space in the ROB though). */
        /* Since traps/interrupts aren't really properly modeled in SimpleScalar, we just let
           it go through without doing anything. */
        uop->timing.when_ready = sim_cycle;
        uop->timing.when_issued = sim_cycle;
        uop->timing.when_completed = sim_cycle;
      }

      uop->timing.when_allocated = sim_cycle;

      /* update stats */
      if(uop->decode.EOM)
        ZESTO_STAT(core->stat.alloc_insn++;)

      ZESTO_STAT(core->stat.alloc_uops++;)

      if(!core->decode->uop_available())
        break;
    }
  }
  else
    stall_reason = ASTALL_EMPTY;


  ZESTO_STAT(stat_add_sample(core->stat.alloc_stall, (int)stall_reason);)
}

/* clean up on pipeline flush (blow everything away) */
void
core_alloc_STM_t::recover(void)
{
  /* nothing to do */
}
void
core_alloc_STM_t::recover(const struct Mop_t * const Mop)
{
  /* nothing to do */
}

void core_alloc_STM_t::RS_deallocate(const struct uop_t * const uop)
{
  zesto_assert(port_loading[uop->alloc.port_assignment] > 0,(void)0);
  port_loading[uop->alloc.port_assignment]--;
}

void core_alloc_STM_t::start_drain(void)
{
  drain_in_progress = true;
}


#endif
