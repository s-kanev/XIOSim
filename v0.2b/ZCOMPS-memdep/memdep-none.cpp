/* memdep-none.cpp: Do not speculate on memory dependences; always wait until
   all previous store addresses have been resolved. */
/*
 * __COPYRIGHT__ GT
 */
#define COMPONENT_NAME "none"

#ifdef MEMDEP_PARSE_ARGS
if(!strcasecmp(COMPONENT_NAME,type))
{
  return new memdep_none_t();
}
#else

class memdep_none_t:public memdep_t
{
  public:

  /* CONSTRUCTOR */
  memdep_none_t(void)
  {
    init();
    name = strdup(COMPONENT_NAME); if(!name) fatal("failed to allocate memory for %s (strdup)",COMPONENT_NAME);
    type = strdup(COMPONENT_NAME); if(!type) fatal("failed to allocate memory for %s (strdup)",COMPONENT_NAME);
  }

  /* LOOKUP */
  MEMDEP_LOOKUP_HEADER
  {
    MEMDEP_STAT(lookups++;)
    return !sta_unknown && !partial_match;
  }
};

#endif /* MEMDEP_PARSE_ARGS */
#undef COMPONENT_NAME
