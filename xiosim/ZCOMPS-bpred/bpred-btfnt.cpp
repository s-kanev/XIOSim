/* bpred-btfnt.cpp: Static BTFNT (backwards-taken, forward-not-taken)
   predictor [Ball and Larus, PPoPP 1993] */
/*
 * __COPYRIGHT__ GT
 */

#define COMPONENT_NAME "btfnt"

#ifdef BPRED_PARSE_ARGS
if(!strcasecmp(COMPONENT_NAME,type))
{
  return std::make_unique<bpred_btfnt_t>(core);
}
#else

class bpred_btfnt_t:public bpred_dir_t
{
  public:

  /* CREATE */
  bpred_btfnt_t(const core_t * core) : bpred_dir_t(core)
  {
    init();

    name = COMPONENT_NAME;
    type = "static btfnt";

    bits = 0;
  }

  /* LOOKUP */
  BPRED_LOOKUP_HEADER
  {
    BPRED_STAT(lookups++;)
    scvp->updated = false;
    return (tPC < PC);
  }

};

#endif /* BPRED_PARSE_ARGS */
#undef COMPONENT_NAME
