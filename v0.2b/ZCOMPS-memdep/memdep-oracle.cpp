/* memdep-oracle.cpp: Oracle memory dependence predictor; this handles both
   the existance and timing of conflicts (i.e., loads wait until only earlier
   matching/conflicting store addresses have been computed, but no longer) */
/*
 * __COPYRIGHT__ GT
 */
#define COMPONENT_NAME "oracle"

#ifdef MEMDEP_PARSE_ARGS
if(!strcasecmp(COMPONENT_NAME,type))
{
  return new memdep_oracle_t();
}
#else

class memdep_oracle_t:public memdep_t
{
  public:
  /* CONSTRUCTOR */
  memdep_oracle_t(void)
  {
    init();
    name = strdup(COMPONENT_NAME); if(!name) fatal("failed to allocate memory for %s (strdup)",COMPONENT_NAME);
    type = strdup(COMPONENT_NAME); if(!type) fatal("failed to allocate memory for %s (strdup)",COMPONENT_NAME);
  }
  /* LOOKUP */
  MEMDEP_LOOKUP_HEADER
  {
    MEMDEP_STAT(lookups++;)
    return (!conflict_exists && !partial_match) || !sta_unknown;
  }
};

#endif /* MEMDEP_PARSE_ARGS */
#undef COMPONENT_NAME
