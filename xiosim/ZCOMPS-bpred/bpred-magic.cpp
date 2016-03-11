/* bpred-magic.cpp: Predictor with a predefined hit rate */

#define COMPONENT_NAME "magic"

#ifdef BPRED_PARSE_ARGS
if(!strcasecmp(COMPONENT_NAME,type))
{
  float hit_rate;

  if(sscanf(opt_string,"%*[^:]:%f",&hit_rate) != 1)
    fatal("bad bpred options string %s (should be \"magic:hit_rate\")",opt_string);
  return std::make_unique<bpred_magic_t>(core, hit_rate);
}
#else

class bpred_magic_t:public bpred_dir_t
{
  protected:

  float hit_rate;
  counter_t num_taken;

  public:

  /* CREATE */
  bpred_magic_t(const core_t * core, float _hit_rate) :
      bpred_dir_t(core), hit_rate(_hit_rate)
  {
    init();

    name = COMPONENT_NAME;
    type = "magic hit rate";

    bits = 0;
  }

  /* LOOKUP */
  BPRED_LOOKUP_HEADER
  {
    int pred;
    float prob = random() / float(RAND_MAX);
    if (prob < hit_rate)
      pred = outcome;
    else
      pred = !outcome;
    BPRED_STAT(lookups++;)
    scvp->updated = false;
    return pred;
  }

  /* UPDATE */
  BPRED_UPDATE_HEADER
  {
    if(!scvp->updated)
    {
      if(outcome)
        BPRED_STAT(num_taken++;)
      BPRED_STAT(updates++;)
      BPRED_STAT(num_hits += our_pred == outcome;)
      scvp->updated = true;
    }
  }

  /* REG_STATS */
  BPRED_REG_STATS_HEADER
  {
    bpred_dir_t::reg_stats(sdb,core);
  }

  /* RESET_STATS */
  BPRED_RESET_STATS_HEADER
  {
    bpred_dir_t::reset_stats();
  }

};

#endif /* BPRED_PARSE_ARGS */
#undef COMPONENT_NAME
