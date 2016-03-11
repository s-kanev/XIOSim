/* ras-perfetch.cpp: Oracle return address predictor */
/*
 * __COPYRIGHT__ GT
 */

#define COMPONENT_NAME "perfect"

#ifdef BPRED_PARSE_ARGS
if(!strcasecmp(COMPONENT_NAME,type))
{
  return std::make_unique<RAS_perfect_t>();
}
#else

class RAS_perfect_t:public RAS_t
{
  public:
  /* CREATE */
  RAS_perfect_t(void)
  {
    init();

    name = "RAS";
    type = COMPONENT_NAME;
  }

  /* DESTROY */
  ~RAS_perfect_t() {}

  int get_size(void)
  {
    return 0;
  }

  /* POP */
  RAS_POP_HEADER
  {
    return oPC;
  }

  /* RECOVER */
  RAS_RECOVER_HEADER
  {
    BPRED_STAT(num_recovers++;)
  }
};

#endif /* RAS_PARSE_ARGS */
#undef COMPONENT_NAME
