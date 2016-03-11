/* bpred-perfect.cpp: Oracle/perfect predictor 
   NOTE: for branch direction only (not target) */
/*
 * __COPYRIGHT__ GT
 */

#define COMPONENT_NAME "perfect"

#ifdef BPRED_PARSE_ARGS
if(!strcasecmp(COMPONENT_NAME,type))
{
  return std::make_unique<bpred_perfect_t>(core);
}
#else

class bpred_perfect_t:public bpred_dir_t
{
  public:

  bpred_perfect_t(const core_t * core) : bpred_dir_t(core)
  {
    init();

    name = COMPONENT_NAME;
    type = "perfect/oracle";

    bits = 0;
  }

  /* LOOKUP */
  BPRED_LOOKUP_HEADER
  {
    BPRED_STAT(lookups++;)
    scvp->updated = false;
    return outcome;
  }

};

#endif /* BPRED_PARSE_ARGS */
#undef COMPONENT_NAME
