/* bpred-taken.cpp: Static always taken predictor */
/*
 * __COPYRIGHT__ GT
 */

#define COMPONENT_NAME "taken"

#ifdef BPRED_PARSE_ARGS
if(!strcasecmp(COMPONENT_NAME,type))
{
  return std::make_unique<bpred_taken_t>(core);
}
#else

class bpred_taken_t:public bpred_dir_t
{
  public:

  /* CREATE */
  bpred_taken_t(const core_t * core) : bpred_dir_t(core)
  {
    init();

    name = COMPONENT_NAME;
    type = "static taken";

    bits = 0;
  }

  /* LOOKUP */
  BPRED_LOOKUP_HEADER
  {
    BPRED_STAT(lookups++;)
    scvp->updated = false;
    return 1;
  }

};

#endif /* BPRED_PARSE_ARGS */
#undef COMPONENT_NAME
