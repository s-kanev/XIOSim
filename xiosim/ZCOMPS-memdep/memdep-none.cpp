/* memdep-none.cpp: Do not speculate on memory dependences; always wait until
   all previous store addresses have been resolved. */
/*
 * __COPYRIGHT__ GT
 */
#define COMPONENT_NAME "none"

#ifdef MEMDEP_PARSE_ARGS
if(!strcasecmp(COMPONENT_NAME,type))
{
  return std::make_unique<memdep_none_t>(core);
}
#else

class memdep_none_t:public memdep_t
{
  public:

  /* CONSTRUCTOR */
  memdep_none_t(const struct core_t * _core) : memdep_t(_core)
  {
    init();
    name = COMPONENT_NAME;
    type = COMPONENT_NAME;
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
